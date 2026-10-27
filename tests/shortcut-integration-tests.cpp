#include "shortcut-rules.h"
#include "ui-bridge/ui-integration.h"
#include <QQuickWindow>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QtTest>
#include <thread>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace relaxer;

#ifdef Q_OS_WIN
namespace {
// Another thread owns the reservation, so Windows itself detects conflicts.
// No key is injected and no window is displayed or activated.
class Reservation {
    QSemaphore ready, finish;
    std::thread worker;
  public:
    bool registered = false;
    explicit Reservation(UINT key) : worker([this, key] {
        registered = RegisterHotKey(nullptr, 0x4f00, MOD_CONTROL | MOD_ALT | MOD_SHIFT, key);
        ready.release();
        finish.acquire();
        if (registered) UnregisterHotKey(nullptr, 0x4f00);
    }) { ready.acquire(); }
    ~Reservation() { finish.release(); worker.join(); }
};
QJsonObject globals(const QString &first, const QString &second = {}) {
    auto config = ShortcutRules::defaults();
    auto keys = config["global"].toObject();
    for (const auto &name : ShortcutRules::actionKeys()) keys[name] = "";
    keys["volumeUp"] = first;
    keys["next"] = second;
    config["global"] = keys;
    config["globalEnabled"] = true;
    return config;
}
}
#endif

class ShortcutIntegrationTests : public QObject {
    Q_OBJECT
private slots:
    void nativeConflictRecordingAndPersistence() {
#ifndef Q_OS_WIN
        QSKIP("Windows RegisterHotKey is unavailable; native coverage was not executed");
#else
        for (const UINT key : {VK_F22, VK_F23, VK_F24}) {
            Reservation probe(key);
            if (!probe.registered)
                QSKIP("A required Ctrl+Alt+Shift+F22/F23/F24 test combination is already occupied");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto initial = globals("Ctrl+Alt+Shift+F24");
        const auto replacement = globals("Ctrl+Alt+Shift+F22", "Ctrl+Alt+Shift+F23");
        {
            Library library(directory.filePath("data"));
            QVERIFY(library.open());
            PlaybackFacade facade(&library, nullptr, true);
            PlaybackBridge playback(&facade, nullptr, &library);
            QQuickWindow window; // Never shown.
            {
                UiIntegration integration(&library, &playback);
                integration.attach(&window);
                QVERIFY(integration.saveShortcuts(initial.toVariantMap()));
                QCOMPARE(library.setting("shortcuts").toObject(), initial);
                { Reservation busy(VK_F24); QVERIFY(!busy.registered); }
                {
                    Reservation occupied(VK_F22);
                    QVERIFY(occupied.registered);
                    QVERIFY(!integration.saveShortcuts(replacement.toVariantMap()));
                    QVERIFY(integration.shortcutStatus().contains(QStringLiteral("占用")));
                    QCOMPARE(library.setting("shortcuts").toObject(), initial);
                    { Reservation rollback(VK_F24); QVERIFY(!rollback.registered); }
                    { Reservation releasedPartial(VK_F23); QVERIFY(releasedPartial.registered); }
                }
                QVERIFY(integration.saveShortcuts(replacement.toVariantMap()));
                QCOMPARE(library.setting("shortcuts").toObject(), replacement);
                { Reservation oldKey(VK_F24); QVERIFY(oldKey.registered); }
                integration.setShortcutRecording(true);
                integration.setShortcutRecording(true);
                { Reservation released(VK_F23); QVERIFY(released.registered); }
                { Reservation released(VK_F22); QVERIFY(released.registered); }
                integration.setShortcutRecording(false);
                integration.setShortcutRecording(false);
                { Reservation restored(VK_F23); QVERIFY(!restored.registered); }
                { Reservation restored(VK_F22); QVERIFY(!restored.registered); }

                // A conflicting key when recording ends is reported; once the
                // conflict clears another pause/resume restores the bindings.
                integration.setShortcutRecording(true);
                {
                    Reservation occupied(VK_F22);
                    QVERIFY(occupied.registered);
                    integration.setShortcutRecording(false);
                    QVERIFY(integration.shortcutStatus().contains(QStringLiteral("占用")));
                    { Reservation releasedPartial(VK_F23); QVERIFY(releasedPartial.registered); }
                    QCOMPARE(library.setting("shortcuts").toObject(), replacement);
                }
                integration.setShortcutRecording(true);
                integration.setShortcutRecording(false);
                { Reservation retried(VK_F23); QVERIFY(!retried.registered); }
                { Reservation retried(VK_F22); QVERIFY(!retried.registered); }
            }
            { Reservation destroyed(VK_F23); QVERIFY(destroyed.registered); }
            { Reservation destroyed(VK_F22); QVERIFY(destroyed.registered); }
        }
        {
            Library reopened(directory.filePath("data"));
            QVERIFY(reopened.open());
            QCOMPARE(reopened.setting("shortcuts").toObject(), replacement);
            PlaybackFacade facade(&reopened, nullptr, true);
            PlaybackBridge playback(&facade, nullptr, &reopened);
            QQuickWindow window;
            UiIntegration integration(&reopened, &playback);
            integration.attach(&window);
            { Reservation restored(VK_F23); QVERIFY(!restored.registered); }
            { Reservation restored(VK_F22); QVERIFY(!restored.registered); }
        }
        { Reservation finalRelease(VK_F23); QVERIFY(finalRelease.registered); }
        { Reservation finalRelease(VK_F22); QVERIFY(finalRelease.registered); }
#endif
    }
};

QTEST_MAIN(ShortcutIntegrationTests)
#include "shortcut-integration-tests.moc"
