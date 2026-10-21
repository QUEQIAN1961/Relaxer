#include "ui-bridge/qml-shell.h"
#include "phase-c-version.h"
#include "system-theme-fixture.h"
#include <QGuiApplication>
#include <QDataStream>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QQuickItem>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
namespace {
qint64 seed(Library &library) {
    Track track;
    track.title = "C1 真正的测试曲目";
    track.artist = "Isolated fixture";
    track.hash = "phase-c-silence";
    track.file = "Music/c1.wav";
    track.duration = 4000;
    QFile file(library.absolute(track.file));
    if (!file.open(QIODevice::WriteOnly)) return 0;
    QDataStream out(&file); out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4); out << quint32(64036);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000)
        << quint16(2) << quint16(16);
    out.writeRawData("data", 4); out << quint32(64000);
    const QByteArray data(64000, '\0'); out.writeRawData(data.data(), data.size());
    file.close();
    return out.status() == QDataStream::Ok ? library.insert(track) : 0;
}
QObject *visualChild(QQuickItem *item, const QString &name) {
    if (item->objectName() == name) return item;
    for (auto *child : item->childItems()) if (auto *found = visualChild(child, name)) return found;
    return nullptr;
}
QJsonObject readJson(const QString &path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
QString appPath() {
    return qEnvironmentVariable("RELAXER_TEST_APP", QString::fromUtf8(RELAXER_APP_PATH));
}
}
class QmlFoundationTests final : public QObject {
    Q_OBJECT
  private slots:
    void resourceAndWindowContract() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto connections = QSqlDatabase::connectionNames();
        QmlShell shell(&library, nullptr, true);
        QVERIFY(QFile::exists(":/qml/Main.qml"));
        QVERIFY(QFile::exists(":/qml/theme/Theme.qml"));
        QVERIFY(shell.load()); QVERIFY(shell.load());
        auto *window = shell.window(); QVERIFY(window);
        QVERIFY(window->isVisible());
        QCOMPARE(window->title(), QString::fromLatin1(phaseC::WindowTitle) + QStringLiteral(" · QML"));
        QCOMPARE(shell.appBridge()->version(), QString::fromLatin1(phaseC::Display));
        QVERIFY(window->minimumWidth() >= 900); QVERIFY(window->minimumHeight() >= 600);
        QCOMPARE(QSqlDatabase::connectionNames(), connections);
        QCOMPARE(shell.findChildren<Playback *>().size(), 1);
        QCOMPARE(shell.findChildren<PlaybackFacade *>().size(), 1);
        QCOMPARE(shell.findChildren<Library *>().size(), 0);
        QCOMPARE(shell.appBridge()->trackCount(), 0);
        auto *tokens = window->property("designTokens").value<QObject *>(); QVERIFY(tokens);
        for (const auto *name : {"background", "panel", "text", "secondary", "accent", "radius", "spacing",
                                 "fontSize", "hover", "pressed", "disabled"}) {
            QVERIFY2(tokens->property(name).isValid(), name);
        }
        QVERIFY(shell.warnings().isEmpty());
        QVERIFY(shell.shutdown());
    }
    void navigationAndComponents_data() {
        QTest::addColumn<bool>("systemDark");
        QTest::newRow("light-system") << false;
        QTest::newRow("dark-system") << true;
    }
    void navigationAndComponents() {
        QFETCH(bool, systemDark);
        SystemThemeFixture systemTheme(systemDark); QVERIFY(systemTheme.ok());
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        QmlShell shell(&library, nullptr, true); QVERIFY(shell.load());
        auto *root = shell.window()->contentItem();
        QVERIFY(visualChild(root, "libraryListContainer"));
        QVERIFY(visualChild(root, "playerBar"));
        auto *toggle = visualChild(root, "playPauseButton"); QVERIFY(toggle);
        QVERIFY(!toggle->property("enabled").toBool());
        QSignalSpy changes(shell.appBridge(), &AppBridge::pageChanged);
        for (int page = 0; page < 4; ++page) {
            auto *button = visualChild(root, QString("navigation%1").arg(page)); QVERIFY(button);
            QVERIFY(QMetaObject::invokeMethod(button, "click"));
            QTRY_COMPARE(shell.appBridge()->page(), page);
            QCOMPARE(shell.window()->property("currentPage").toInt(), page);
        }
        QCOMPARE(changes.size(), 3);
        QVERIFY(shell.appBridge()->navigate(3)); QCOMPARE(changes.size(), 3);
        QVERIFY(!shell.appBridge()->navigate(-1)); QVERIFY(!shell.appBridge()->navigate(4));
        QCOMPARE(shell.appBridge()->page(), 3);
        auto *theme = visualChild(root, "qmlThemeSetting"); QVERIFY(theme);
        auto *tokens=shell.window()->property("designTokens").value<QObject *>();QVERIFY(tokens);
        QCOMPARE(library.setting("theme").toInt(), 2);
        QTRY_COMPARE(tokens->property("background").value<QColor>(),
                     QColor(systemDark ? "#19191e" : "#ffffff"));
        QVERIFY(theme->setProperty("currentIndex",0));QVERIFY(QMetaObject::invokeMethod(theme,"activated",Q_ARG(int,0)));
        QTRY_COMPARE(tokens->property("background").value<QColor>(), QColor("#ffffff"));
        const auto background=tokens->property("background");
        QVERIFY(theme->setProperty("currentIndex",1));QVERIFY(QMetaObject::invokeMethod(theme,"activated",Q_ARG(int,1)));
        QTRY_VERIFY(tokens->property("background")!=background);
        QTRY_COMPARE(tokens->property("background").value<QColor>(), QColor("#19191e"));
        QCOMPARE(library.setting("theme").toInt(), 1);
        QVERIFY(theme->setProperty("currentIndex",0));QVERIFY(QMetaObject::invokeMethod(theme,"activated",Q_ARG(int,0)));
        QTRY_COMPARE(tokens->property("background").value<QColor>(), QColor("#ffffff"));
        QCOMPARE(library.setting("theme").toInt(), 0);
        SettingsBridge reopened(&library);
        QCOMPARE(reopened.palette().value("background").toString(), QString("#ffffff"));
        QTest::qWait(20); QVERIFY(shell.warnings().isEmpty());
    }
    void realLibraryNotificationsAreCoalesced() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        AppBridge bridge(&library); QSignalSpy changes(&bridge, &AppBridge::trackCountChanged);
        QVERIFY(seed(library));
        QTRY_COMPARE(bridge.trackCount(), 1); QCOMPARE(changes.size(), 1);
        QVERIFY(library.setSetting("c1-not-a-track", 123));
        QTest::qWait(20); QCOMPARE(changes.size(), 1);
        QCOMPARE(library.tracks().size(), 1);
    }
    void facadeNotificationsAndRealQmlBindings() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto id = seed(library); QVERIFY(id);
        QVERIFY(library.setSettings(QJsonObject{{"queue", QJsonArray{double(id)}}, {"current", double(id)}, {"volume", 23}}));
        QmlShell shell(&library, nullptr, true); QVERIFY(shell.load());
        auto *bridge = shell.playbackBridge();
        QTRY_VERIFY(bridge->hasTrack());
        QCOMPARE(bridge->title(), library.track(id).title);
        QCOMPARE(bridge->artist(), library.track(id).artist);
        QCOMPARE(bridge->volume(), 0); // decodeOnly intentionally mutes, as in facade contract tests.
        auto *root = shell.window()->contentItem();
        auto *title = visualChild(root, "currentTitle"); QVERIFY(title);
        QCOMPARE(title->property("text").toString(), library.track(id).title);
        QSignalSpy volume(bridge, &PlaybackBridge::volumeChanged);
        shell.facade()->setVolume(41);
        QTRY_COMPARE(bridge->volume(), 41); QCOMPARE(volume.size(), 1);
        shell.facade()->setVolume(41); QTest::qWait(20); QCOMPARE(volume.size(), 1);
        auto *button = visualChild(root, "playPauseButton"); QVERIFY(button);
        QVERIFY(button->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(button, "click"));
        QTRY_VERIFY(shell.facade()->playing());
        QTRY_VERIFY(bridge->playing());
        QCOMPARE(button->property("text").toString(), QString("暂停"));
        QVERIFY(QMetaObject::invokeMethod(button, "click"));
        QTRY_VERIFY(!bridge->playing());
        QCOMPARE(bridge->position(), shell.facade()->position());
        QCOMPARE(bridge->duration(), shell.facade()->duration());
        QVERIFY(shell.shutdown()); QVERIFY(shell.shutdown());
        QVERIFY(!bridge->available()); QVERIFY(!bridge->playing());
        bridge->toggle(); QVERIFY(shell.facade()->isShutdown());
        QVERIFY(shell.warnings().isEmpty());
    }
    void queuedSnapshotsCannotRegressNewerState() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        PlaybackFacade facade(&library, nullptr, true); PlaybackBridge bridge(&facade);
        facade.setVolume(21); facade.setVolume(72);
        QCoreApplication::processEvents(); QCOMPARE(bridge.volume(), 72);
        PlaybackSnapshot stale; stale.volume = 1;
        emit facade.snapshotChanged(stale);
        QCOMPARE(bridge.volume(), 72);
    }
    void borrowedObjectsMayDisappear() {
        QTemporaryDir dir;
        auto library = std::make_unique<Library>(dir.path()); QVERIFY(library->open()); QVERIFY(seed(*library));
        auto facade = std::make_unique<PlaybackFacade>(library.get(), nullptr, true);
        PlaybackBridge playback(facade.get()); AppBridge app(library.get());
        QSignalSpy available(&playback, &PlaybackBridge::availableChanged);
        facade.reset(); QVERIFY(!playback.available()); QCOMPARE(available.size(), 1); playback.toggle();
        library.reset(); QVERIFY(!app.available()); QCOMPARE(app.trackCount(), 0);
        QVERIFY(app.navigate(1));
    }
    void bridgeDestructionCancelsPendingCallbacks() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        PlaybackFacade facade(&library, nullptr, true);
        {
            PlaybackBridge playback(&facade); AppBridge app(&library);
            facade.setVolume(12); QVERIFY(library.setSetting("pending", 1));
        }
        facade.setVolume(44); QTest::qWait(20);
        QCOMPARE(facade.volume(), 44); QVERIFY(facade.save());
    }
    void engineDestroyedBeforeBridgesAndConnectionClosed() {
        const auto before = QSqlDatabase::connectionNames();
        QTemporaryDir dir;
        {
            Library library(dir.path()); QVERIFY(library.open());
            QPointer<QQuickWindow> window;
            {
                QmlShell shell(&library, nullptr, true); QVERIFY(shell.load()); window = shell.window();
                shell.facade()->setVolume(37);
            }
            QVERIFY(window.isNull()); QCOMPARE(library.setting("volume").toInt(), 37);
        }
        QCOMPARE(QSqlDatabase::connectionNames(), before);
        QTest::qWait(20);
    }
    void processStartup_data() {
        QTest::addColumn<QString>("mode");
        QTest::newRow("default-widgets") << QString();
        QTest::newRow("explicit-widgets") << QString("widgets");
        QTest::newRow("opt-in-qml") << QString("qml");
    }
    void processStartup() {
        QFETCH(QString, mode);
        QTemporaryDir dir;
        const auto data = dir.filePath("fixture");
        {
            Library library(data); QVERIFY(library.open()); const auto id = seed(library); QVERIFY(id);
            QVERIFY(library.favorite(id, true)); QVERIFY(library.setLyricOffset(id, 250));
            QVERIFY(library.setTranslationJudgment(id, TranslationJudgment::NoTranslationNeeded));
            const auto list = library.createPlaylist("C1 retained"); QVERIFY(list); QVERIFY(library.addToPlaylist(list, {id}));
            QVERIFY(library.setSetting("c1-sentinel", "untouched"));
        }
        QFile sentinel(dir.filePath("sentinel.txt")); QVERIFY(sentinel.open(QIODevice::WriteOnly)); sentinel.write("untouched"); sentinel.close();
        const auto report = dir.filePath("run.json");
        QProcess process;
        auto env = QProcessEnvironment::systemEnvironment(); env.insert("RELAXER_DATA", dir.filePath("must-not-open"));
        process.setProcessEnvironment(env);
        QStringList args{"--data", data, "--diagnostics", report, "--quit-after", "300"};
        if (!mode.isEmpty()) args << "--ui=" + mode;
        QSignalSpy ended(&process, &QProcess::finished);
        process.start(appPath(), args);
        // Native Windows children may exchange window/theme messages with the
        // host. Keep its event loop responsive; retain the same 15s deadline.
        const bool finished = !ended.isEmpty() || ended.wait(15000);
        const auto failure = process.readAllStandardError();
        const auto detail = QJsonDocument(readJson(report)).toJson();
        QVERIFY2(finished, (QByteArray("Child timeout: ") + failure + " / " + process.errorString().toUtf8() + detail).constData());
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
        const auto result = readJson(report); QVERIFY(!result.isEmpty());
        QCOMPARE(result["mode"].toString(), mode == "qml" ? QString("qml") : QString("widgets"));
        QCOMPARE(result["databaseConnections"].toInt(), 1); QCOMPARE(result["playbackInstances"].toInt(), 1);
        QVERIFY(result["saveSucceeded"].toBool());
        QVERIFY(result["title"].toString().contains(QString::fromLatin1(phaseC::Display)));
        QVERIFY(!QFile::exists(dir.filePath("must-not-open")));
        QVERIFY(!QFile::exists(data + "/data")); QVERIFY(!QFile::exists(data + "/relaxer.lock"));
        Library reopened(data); QVERIFY(reopened.open()); QCOMPARE(reopened.tracks().size(), 1);
        const auto track = reopened.tracks().first(); QVERIFY(track.favorite);
        QCOMPARE(reopened.playlists().first().name, QString("C1 retained"));
        QCOMPARE(reopened.lyricOffset(track.id), 250);
        QCOMPARE(reopened.translationJudgment(track.id), TranslationJudgment::NoTranslationNeeded);
        QCOMPARE(reopened.setting("c1-sentinel").toString(), QString("untouched"));
        QVERIFY(sentinel.open(QIODevice::ReadOnly)); QCOMPARE(sentinel.readAll(), QByteArray("untouched"));
    }
    void invalidArgumentsDoNotOpenData_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::newRow("unknown-ui") << QStringList{"--ui=web"};
        QTest::newRow("unknown-option") << QStringList{"--not-an-option"};
        QTest::newRow("invalid-page") << QStringList{"--ui=qml", "--page=profile"};
        QTest::newRow("negative-quit") << QStringList{"--quit-after=-1"};
        QTest::newRow("unbounded-quit") << QStringList{"--quit-after=60001"};
        QTest::newRow("diagnostic-ui-conflict") << QStringList{"--ui=qml", "--self-test"};
    }
    void invalidArgumentsDoNotOpenData() {
        QFETCH(QStringList, arguments); QTemporaryDir dir; const auto data = dir.filePath("untouched");
        QProcess process; process.start(appPath(), arguments + QStringList{"--data", data});
        QVERIFY(process.waitForFinished(10000)); QVERIFY(process.exitCode() != 0);
        QVERIFY(!process.readAllStandardError().isEmpty()); QVERIFY(!QFile::exists(data));
    }
    void sameDataLockAppliesToQml() {
        QTemporaryDir dir; QLockFile lock(dir.filePath("relaxer.lock")); QVERIFY(lock.tryLock());
        QProcess process; process.start(appPath(), {"--ui=qml", "--data", dir.path(),
            "--diagnostics", dir.filePath("run.json"), "--quit-after", "100"});
        QVERIFY(process.waitForFinished(10000)); QCOMPARE(process.exitCode(), 3);
        QVERIFY(!QFile::exists(dir.filePath("relaxer.sqlite")));
    }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QmlFoundationTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "qml-foundation-tests.moc"
