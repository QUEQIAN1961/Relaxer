#include <QFontDatabase>
#include "appearance.h"
#include "changelog.h"
#include "shortcuts.h"
#include "window.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
namespace {
struct BackendState {
    QMap<int, NativeKey> keys;
    unsigned blocked = 0;
    int added = 0, removed = 0;
};
class TestBackend : public GlobalKeyBackend {
    std::shared_ptr<BackendState> m_state;

  public:
    explicit TestBackend(std::shared_ptr<BackendState> state) : m_state(std::move(state)) {}
    bool supported() const override { return true; }
    bool add(int id, NativeKey key, bool) override {
        if (key.key == m_state->blocked || m_state->keys.contains(id))
            return false;
        m_state->keys[id] = key;
        ++m_state->added;
        return true;
    }
    void remove(int id) override { m_state->removed += m_state->keys.remove(id); }
};
void setBinding(QJsonObject &config, const QString &scope, const QString &action, const QString &key) {
    auto group = config[scope].toObject();
    group[action] = key;
    config[scope] = group;
}
bool copyTree(const QString &source, const QString &destination) {
    if (!QDir().mkpath(destination))
        return false;
    for (auto entry : QDir(source).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        auto target = QDir(destination).filePath(entry.fileName());
        if (entry.isDir() ? !copyTree(entry.filePath(), target) : !QFile::copy(entry.filePath(), target))
            return false;
    }
    return true;
}
bool image(const QString &path, QColor color) {
    QImage bitmap(320, 180, QImage::Format_RGB32);
    bitmap.fill(color);
    return bitmap.save(path);
}
void capture(QWidget &widget, const QString &name) {
    const auto directory = qEnvironmentVariable("RELAXER_QA_DIR");
    if (!directory.isEmpty()) {
        QDir().mkpath(directory);
        widget.grab().save(QDir(directory).filePath(name));
    }
}
} // namespace
class V11Tests : public QObject {
    Q_OBJECT
  private slots:
    void newLibraryDefaultsToSystemThemeAndKeepsSavedChoice() {
        QTemporaryDir directory;
        {
            Library library(directory.filePath("data"));QVERIFY(library.open());
            QCOMPARE(library.setting("theme").toInt(),2);
            QVERIFY(library.setSetting("theme",1));
        }
        Library reopened(directory.filePath("data"));QVERIFY(reopened.open());
        QCOMPARE(reopened.setting("theme").toInt(),1);
    }
    void initTestCase() {
        qApp->setStyle("Fusion");
        const auto font=qEnvironmentVariable("RELAXER_QA_FONT");if(!font.isEmpty())QVERIFY(QFontDatabase::addApplicationFont(font)>=0);
        qRegisterMetaType<PlayerAction>();
#ifdef RELAXER_BUILD_SOURCE_SHA256
        qInfo("Relaxer compiled source fingerprint: %s", RELAXER_BUILD_SOURCE_SHA256);
#endif
    }
    void customControlMetaObjects() {
        QWidget parent;
        ShortcutEdit editor({}, &parent);
        editor.setObjectName("shortcut-custom");
        QKeySequenceEdit baseEditor(&parent);
        baseEditor.setObjectName("shortcut-base");
        PercentSpin spin(&parent);
        spin.setObjectName("spin-custom");
        QSpinBox baseSpin(&parent);
        baseSpin.setObjectName("spin-base");
        QCOMPARE(parent.findChild<ShortcutEdit *>("shortcut-custom"), &editor);
        QCOMPARE(parent.findChild<PercentSpin *>("spin-custom"), &spin);
        QVERIFY(!parent.findChild<ShortcutEdit *>("shortcut-base"));
        QVERIFY(!parent.findChild<PercentSpin *>("spin-base"));
        QCOMPARE(qobject_cast<ShortcutEdit *>(&editor), &editor);
        QCOMPARE(qobject_cast<PercentSpin *>(&spin), &spin);
        QVERIFY(!qobject_cast<ShortcutEdit *>(&baseEditor));
        QVERIFY(!qobject_cast<PercentSpin *>(&baseSpin));
        QCOMPARE(QString::fromLatin1(editor.metaObject()->className()), QString("relaxer::ShortcutEdit"));
        QCOMPARE(QString::fromLatin1(spin.metaObject()->className()), QString("relaxer::PercentSpin"));
    }
    void bindingsValidation() {
        auto config = PlayerShortcuts::defaults();
        QString error;
        QVERIFY(PlayerShortcuts::validate(config, &error));
        setBinding(config, "app", "next", "Space");
        QVERIFY(!PlayerShortcuts::validate(config, &error));
        config = PlayerShortcuts::defaults();
        setBinding(config, "app", "next", "Ctrl+O");
        QVERIFY(!PlayerShortcuts::validate(config));
        setBinding(config, "app", "next", "Ctrl+K, Ctrl+C");
        QVERIFY(!PlayerShortcuts::validate(config));
        setBinding(config, "app", "next", "");
        QVERIFY(PlayerShortcuts::validate(config));
        setBinding(config, "global", "next", "Meta+N");
        QVERIFY(!PlayerShortcuts::validate(config));
        setBinding(config, "global", "next", "F12");
        QVERIFY(!PlayerShortcuts::validate(config));
        setBinding(config, "global", "next", "N");
        QVERIFY(!PlayerShortcuts::validate(config));
        config = PlayerShortcuts::defaults();
        setBinding(config, "app", "toggle", "Ctrl+Alt+Space");
        QVERIFY(PlayerShortcuts::validate(config));
        config["globalEnabled"] = true;
        QVERIFY(!PlayerShortcuts::validate(config));
        auto key = PlayerShortcuts::nativeKey(QKeySequence("Ctrl+Alt+Up"));
        QCOMPARE(key.modifiers, 3u);
        QCOMPARE(key.key, 0x26u);
        QCOMPARE(PlayerShortcuts::nativeKey(QKeySequence("Ctrl+Num+1")).key, 0u);
    }
    void globalRegistrationRollbackAndLifetime() {
        QWidget window;
        auto state = std::make_shared<BackendState>();
        {
            PlayerShortcuts shortcuts(&window, std::make_unique<TestBackend>(state));
            QSignalSpy spy(&shortcuts, &PlayerShortcuts::triggered);
            auto config = PlayerShortcuts::defaults();
            config["globalEnabled"] = true;
            QVERIFY(shortcuts.apply(config));
            QCOMPARE(state->keys.size(), 5);
            int first = state->keys.firstKey();
            window.hide();
            QVERIFY(shortcuts.dispatchGlobal(first));
            QCOMPARE(qvariant_cast<PlayerAction>(spy.takeFirst().first()), PlayerAction::Toggle);
            auto failed = config;
            setBinding(failed, "global", "next", "Ctrl+Alt+N");
            state->blocked = 'N';
            QString error;
            QVERIFY(!shortcuts.apply(failed, &error));
            QVERIFY(error.contains("占用"));
            QCOMPARE(shortcuts.config(), config);
            QCOMPARE(state->keys.size(), 5);
            QVERIFY(shortcuts.dispatchGlobal(first + 2));
            QCOMPARE(qvariant_cast<PlayerAction>(spy.takeFirst().first()), PlayerAction::Next);
            config["globalEnabled"] = false;
            QVERIFY(shortcuts.apply(config));
            QVERIFY(state->keys.isEmpty());
            QVERIFY(!shortcuts.dispatchGlobal(first));
            config["globalEnabled"] = true;
            QVERIFY(shortcuts.apply(config));
        }
        QVERIFY(state->keys.isEmpty());
        QCOMPARE(state->added, state->removed);
    }
    void localBindingsAndEditableFocus() {
        QWidget window;
        window.resize(400, 200);
        window.setFocusPolicy(Qt::StrongFocus);
        QLineEdit input(&window);
        input.setGeometry(10, 10, 300, 40);
        PlayerShortcuts shortcuts(&window);
        QSignalSpy spy(&shortcuts, &PlayerShortcuts::triggered);
        window.show();
        window.activateWindow();
        window.setFocus();
        QTest::qWait(40);
        QTest::keyClick(&window, Qt::Key_Up, Qt::ControlModifier);
        QTRY_COMPARE(spy.size(), 1);
        QCOMPARE(qvariant_cast<PlayerAction>(spy.takeFirst().first()), PlayerAction::VolumeUp);
        input.setFocus();
        QTest::keyClick(&input, Qt::Key_Space);
        QCOMPARE(input.text(), QString(" "));
        QVERIFY(spy.isEmpty());
        auto config = PlayerShortcuts::defaults();
        setBinding(config, "app", "volumeUp", "Alt+U");
        QVERIFY(shortcuts.apply(config));
        window.setFocus();
        QTest::keyClick(&window, Qt::Key_Up, Qt::ControlModifier);
        QVERIFY(spy.isEmpty());
        QTest::keyClick(&window, Qt::Key_U, Qt::AltModifier);
        QTRY_COMPARE(spy.size(), 1);
    }
    void singleCombinationRecordingSuspendsGlobal() {
        QWidget window;
        window.setFocusPolicy(Qt::StrongFocus);
        ShortcutEdit edit(QKeySequence("Space"), &window);
        edit.setGeometry(5, 5, 300, 40);
        auto state = std::make_shared<BackendState>();
        PlayerShortcuts shortcuts(&window, std::make_unique<TestBackend>(state));
        auto config = PlayerShortcuts::defaults();
        config["globalEnabled"] = true;
        QVERIFY(shortcuts.apply(config));
        window.show();
        window.activateWindow();
        edit.setFocus();
        QTest::qWait(30);
        QVERIFY(state->keys.isEmpty());
        QTest::keyClick(qApp->focusWidget(), Qt::Key_K, Qt::ControlModifier);
        QCOMPARE(edit.keySequence(), QKeySequence("Ctrl+K"));
        QTest::keyClick(qApp->focusWidget(), Qt::Key_U, Qt::AltModifier);
        QCOMPARE(edit.keySequence(), QKeySequence("Alt+U"));
        QCOMPARE(edit.keySequence().count(), 1);
        window.setFocus();
        QTRY_COMPARE(state->keys.size(), 5);
    }
    void managedAppearanceAndMigration() {
        QTemporaryDir directory;
        auto oldRoot = directory.filePath("old-data");
        QString id;
        {
            Library library(oldRoot);
            QVERIFY(library.open());
            Track track;
            track.hash = "migration";
            track.title = "Preserved music";
            track.file = "Music/migration.wav";
            track.duration = 1000;
            auto song = library.insert(track);
            auto list = library.createPlaylist("Keep playlist");
            QVERIFY(library.addToPlaylist(list, {song}));
            auto session = library.startSession(song, 1000);
            QVERIFY(library.saveSession(session, 800, 80));
            auto source = directory.filePath("source.png");
            QVERIFY(image(source, Qt::blue));
            auto theme = AppearanceStore::defaults();
            theme["mode"] = "image";
            theme["shareLR"] = true;
            theme["sourceLeft"] = source;
            theme["blurLeft"] = 12;
            AppearanceStore store(&library);
            QVERIFY(store.save(theme, {}));
            auto active = store.active();
            id = active["id"].toString();
            QVERIFY(!id.isEmpty());
            QCOMPARE(active["imageLeft"], active["imageRight"]);
            QVERIFY(!active.contains("sourceLeft"));
            QVERIFY(QFile::remove(source));
            QVERIFY(!backgroundImage(store.imagePath(active, "left")).isNull());
            QVERIFY(store.rename(id, "Evening"));
            auto draft = store.draft(store.active());
            draft["accent"] = "#228877";
            QVERIFY(store.save(draft, id));
            QCOMPARE(store.state()["items"].toArray().size(), 1);
            QCOMPARE(store.active()["name"].toString(), QString("Evening"));
            QCOMPARE(library.track(song).plays, 1);
            QCOMPARE(library.tracks(list).first().id, song);
            QVERIFY(library.setSetting("shortcuts", PlayerShortcuts::defaults()));
        }
        auto newRoot = directory.filePath("new-data");
        QVERIFY(copyTree(oldRoot, newRoot));
        Library migrated(newRoot);
        QVERIFY(migrated.open());
        AppearanceStore store(&migrated);
        QCOMPARE(store.active()["id"].toString(), id);
        auto picture = store.imagePath(store.active(), "right");
        QVERIFY(picture.startsWith(newRoot));
        QCOMPARE(backgroundImage(picture).pixelColor(20, 20), QColor(Qt::blue));
        QCOMPARE(migrated.totalPlays(), 1);
        QCOMPARE(migrated.totalHeard(), 800);
        QCOMPARE(migrated.playlists().size(), 1);
        QCOMPARE(migrated.setting("shortcuts").toObject(), PlayerShortcuts::defaults());
        auto bad = store.active();
        bad["imageLeft"] = "Appearance/../relaxer.sqlite";
        QVERIFY(store.imagePath(bad, "left").isEmpty());
    }
    void appearanceFailureDoesNotPartiallySave() {
        QTemporaryDir directory;
        Library library(directory.filePath("data"));
        QVERIFY(library.open());
        AppearanceStore store(&library);
        auto palette = AppearanceStore::defaults();
        QVERIFY(store.save(palette, {}));
        auto saved = store.state();
        auto source = directory.filePath("red.png");
        QVERIFY(image(source, Qt::red));
        auto theme = palette;
        theme["mode"] = "image";
        theme["sourceLeft"] = source;
        theme["sourceRight"] = directory.filePath("missing.png");
        QString error;
        QVERIFY(!store.save(theme, {}, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(store.state(), saved);
        QCOMPARE(QDir(library.absolute("Appearance")).entryList(QDir::Files).size(), 0);
        auto first = store.active()["id"].toString();
        palette["left"] = "#223344";
        QVERIFY(store.save(palette, {}));
        auto second = store.active()["id"].toString();
        QVERIFY(store.use(first));
        QVERIFY(store.remove(second));
        QCOMPARE(store.active()["id"].toString(), first);
        QVERIFY(store.remove(first));
        QCOMPARE(library.setting("theme").toInt(), 0);
        QVERIFY(store.state()["items"].toArray().isEmpty());
    }
    void continuousImageAndBlur() {
        QTemporaryDir directory;
        QImage gradient(600, 200, QImage::Format_RGB32);
        for (int y = 0; y < 200; ++y)
            for (int x = 0; x < 600; ++x)
                gradient.setPixelColor(x, y, QColor(x * 255 / 599, 70, 180));
        auto path = directory.filePath("gradient.png");
        QVERIFY(gradient.save(path));
        QWidget host;
        host.resize(600, 200);
        Surface left(&host), right(&host);
        left.setGeometry(0, 0, 200, 200);
        right.setGeometry(200, 0, 400, 200);
        left.setLook(Qt::white, path, 0, &host);
        right.setLook(Qt::white, path, 0, &host);
        host.show();
        QTest::qWait(20);
        auto full = host.grab().toImage();
        qreal dpr = full.devicePixelRatio();
        QVERIFY(qAbs(full.pixelColor(qRound(199 * dpr), qRound(80 * dpr)).red() -
                     full.pixelColor(qRound(200 * dpr), qRound(80 * dpr)).red()) <= 2);
        QVERIFY(full.pixelColor(qRound(590 * dpr), qRound(80 * dpr)).red() > 245);
        QImage check(400, 200, QImage::Format_RGB32);
        for (int y = 0; y < 200; ++y)
            for (int x = 0; x < 400; ++x)
                check.setPixelColor(x, y, ((x / 2 + y / 2) % 2) ? Qt::black : Qt::white);
        path = directory.filePath("checker.png");
        QVERIFY(check.save(path));
        right.setLook(Qt::white, path, 50);
        QTest::qWait(20);
        auto blurred = right.grab().toImage();
        int minimum = 255, maximum = 0;
        for (int y = 40; y < 160; ++y)
            for (int x = 40; x < 360; ++x) {
                int v = blurred.pixelColor(qRound(x * dpr), qRound(y * dpr)).red();
                minimum = qMin(minimum, v);
                maximum = qMax(maximum, v);
            }
        QVERIFY(maximum - minimum < 12);
    }
    void versionHistoryExpansionAndScroll() {
        QJsonArray entries;
        for (int i = 0; i < 10; ++i)
            entries.append(QJsonArray{QString("V%1").arg(10 - i), "2026.10.11",
                                      QString("A preserved release note.\n").repeated(6)});
        QScrollArea scroll;
        scroll.setWidgetResizable(true);
        scroll.resize(600, 240);
        auto *log = new ChangeLog(nullptr, entries);
        scroll.setWidget(log);
        scroll.show();
        QTest::qWait(30);
        QCOMPARE(log->findChildren<QLabel *>("versionLogTitle").size(), 3);
        scroll.verticalScrollBar()->setValue(80);
        int position = scroll.verticalScrollBar()->value();
        QTest::mouseClick(log->findChild<QPushButton *>("versionLogExpand"), Qt::LeftButton);
        QTest::qWait(30);
        QCOMPARE(log->findChildren<QLabel *>("versionLogTitle").size(), 8);
        QCOMPARE(scroll.verticalScrollBar()->value(), position);
        QTest::mouseClick(log->findChild<QPushButton *>("versionLogExpand"), Qt::LeftButton);
        QTest::qWait(30);
        QCOMPARE(log->findChildren<QLabel *>("versionLogTitle").size(), 10);
        QVERIFY(!log->findChild<QPushButton *>("versionLogExpand"));
        scroll.verticalScrollBar()->setValue(scroll.verticalScrollBar()->maximum());
        QTest::mouseClick(log->findChild<QPushButton *>("versionLogCollapse"), Qt::LeftButton);
        QTest::qWait(30);
        QCOMPARE(log->visibleCount(), 3);
        QCOMPARE(log->findChildren<QLabel *>("versionLogTitle").size(), 3);
        QCOMPARE(scroll.verticalScrollBar()->value(), scroll.verticalScrollBar()->maximum());
    }
    void settingsRowsSavedBindingAndTextRemoval() {
        QTemporaryDir directory;
        Library library(directory.filePath("data"));
        QVERIFY(library.open());
        QVERIFY(library.setSetting("volume", 50));
        Window window(&library);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))window.playback()->media()->setAudioOutput(nullptr);
        window.setFocusPolicy(Qt::StrongFocus);
        window.show();
        window.showSettings();
        for (int width : {1000, 1240, 1600}) {
            window.resize(width, 900);
            QTest::qWait(30);
            auto *theme = window.findChild<QComboBox *>("themeSetting");
            auto *threshold = window.findChild<PercentSpin *>("thresholdSetting");
            auto *close = window.findChild<QComboBox *>("closeSetting");
            QVERIFY(theme && threshold && close);
            const auto geometry = QString("window width=%1; theme y=%2 height=%3; threshold y=%4 height=%5; close y=%6 height=%7")
                                      .arg(width).arg(theme->mapTo(&window, QPoint()).y()).arg(theme->height())
                                      .arg(threshold->mapTo(&window, QPoint()).y()).arg(threshold->height())
                                      .arg(close->mapTo(&window, QPoint()).y()).arg(close->height());
            int y = theme->mapTo(&window, QPoint()).y();
            QVERIFY2(qAbs(y - threshold->mapTo(&window, QPoint()).y()) <= 3, qPrintable(geometry));
            QVERIFY2(qAbs(y - close->mapTo(&window, QPoint()).y()) <= 3, qPrintable(geometry));
            QVERIFY2(close->mapTo(&window, QPoint(close->width(), 0)).x() <= width - 20, qPrintable(geometry));
        }
        QVERIFY(!window.findChild<QLabel *>("sidebarFoot"));
        for (auto *label : window.findChildren<QLabel *>())
            QVERIFY(!label->text().contains("达到 80% 后计入"));
        auto *editor = window.findChild<ShortcutEdit *>("shortcut-app-volumeUp");
        editor->setKeySequence(QKeySequence("Alt+U"));
        QTest::mouseClick(window.findChild<QPushButton *>("shortcutsSave"), Qt::LeftButton);
        QCOMPARE(library.setting("shortcuts").toObject()["app"].toObject()["volumeUp"].toString(),
                 QString("Alt+U"));
        window.activateWindow();
        window.setFocus();
        QTest::qWait(20);
        auto *volume = window.findChild<Slider *>("volumeSlider");
        QCOMPARE(volume->value(), 50);
        QTest::keyClick(&window, Qt::Key_U, Qt::AltModifier);
        QTRY_COMPARE(volume->value(), 55);
        QTest::keyClick(&window, Qt::Key_Up, Qt::ControlModifier);
        QCOMPARE(volume->value(), 55);
        auto *scroll=window.findChild<QScrollArea *>("settingsScroll");
        auto *toggle=window.findChild<QPushButton *>("versionLogToggle");
        scroll->ensureWidgetVisible(toggle);const int previous=scroll->verticalScrollBar()->value();
        QTest::mouseClick(toggle,Qt::LeftButton);
        QCOMPARE(window.findChild<QStackedWidget *>("pages")->currentWidget()->objectName(),QString("versionLogPage"));
        QCOMPARE(window.findChildren<QLabel *>("versionLogTitle").size(),3);
        window.resize(1240,900);QTest::qWait(40);capture(window,"17-version-log.png");
        QTest::mouseClick(window.findChild<QPushButton *>("versionLogBack"),Qt::LeftButton);
        QCOMPARE(scroll->verticalScrollBar()->value(),previous);QTest::qWait(20);
        scroll->verticalScrollBar()->setValue(0);QTest::qWait(20);
        capture(window, "04-settings.png");
        window.showNowPlaying();
        QTest::qWait(40);
        capture(window, "05-playing.png");
    }
    void settingsRowWithDifferentControlHeights_data() {
        QTest::addColumn<int>("themeHeight");
        QTest::addColumn<int>("editHeight");
        QTest::addColumn<int>("thresholdHeight");
        QTest::addColumn<int>("closeHeight");
        QTest::newRow("taller-edit-button") << 40 << 72 << 40 << 40;
        QTest::newRow("taller-spin-box") << 40 << 40 << 72 << 40;
        QTest::newRow("different-combo-heights") << 60 << 40 << 40 << 72;
    }
    void settingsRowWithDifferentControlHeights() {
        QFETCH(int, themeHeight);
        QFETCH(int, editHeight);
        QFETCH(int, thresholdHeight);
        QFETCH(int, closeHeight);
        QTemporaryDir directory;
        Library library(directory.filePath("data"));
        QVERIFY(library.open());
        Window window(&library);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))window.playback()->media()->setAudioOutput(nullptr);
        window.showSettings();
        auto *theme = window.findChild<QComboBox *>("themeSetting");
        auto *edit = window.findChild<QPushButton *>("editAppearance");
        auto *threshold = window.findChild<PercentSpin *>("thresholdSetting");
        auto *close = window.findChild<QComboBox *>("closeSetting");
        QVERIFY(theme && edit && threshold && close);
        // Reproduce the unequal size hints caused by different fonts/styles.
        theme->setMinimumHeight(themeHeight);
        edit->setMinimumHeight(editHeight);
        threshold->setMinimumHeight(thresholdHeight);
        close->setMinimumHeight(closeHeight);
        window.show();
        for (int width : {1000, 1240, 1600}) {
            window.resize(width, 900);
            QTest::qWait(30);
            const auto themeRect = QRect(theme->mapTo(&window, QPoint()), theme->size());
            const auto editRect = QRect(edit->mapTo(&window, QPoint()), edit->size());
            const auto thresholdRect = QRect(threshold->mapTo(&window, QPoint()), threshold->size());
            const auto closeRect = QRect(close->mapTo(&window, QPoint()), close->size());
            const auto geometry = QString("width=%1; top: theme=%2 edit=%3 threshold=%4 close=%5; heights=%6/%7/%8/%9")
                                      .arg(width).arg(themeRect.top()).arg(editRect.top())
                                      .arg(thresholdRect.top()).arg(closeRect.top())
                                      .arg(themeRect.height()).arg(editRect.height())
                                      .arg(thresholdRect.height()).arg(closeRect.height());
            QVERIFY2(qAbs(themeRect.top() - thresholdRect.top()) <= 3, qPrintable(geometry));
            QVERIFY2(qAbs(themeRect.top() - closeRect.top()) <= 3, qPrintable(geometry));
            QVERIFY2(qAbs(themeRect.top() - editRect.top()) <= 3, qPrintable(geometry));
            QVERIFY2(themeRect.right() < editRect.left(), qPrintable(geometry));
            QVERIFY2(editRect.right() < thresholdRect.left(), qPrintable(geometry));
            QVERIFY2(thresholdRect.right() < closeRect.left(), qPrintable(geometry));
            QVERIFY2(closeRect.right() <= width - 20, qPrintable(geometry));
        }
    }
    void appearanceWindowIntegration() {
        QTemporaryDir directory;
        Library library(directory.filePath("data"));
        QVERIFY(library.open());
        AppearanceStore store(&library);
        auto theme = AppearanceStore::defaults();
        theme["left"] = "#222831";
        theme["right"] = "#eeeae2";
        theme["accent"] = "#34867b";
        QVERIFY(store.save(theme, {}));
        Window window(&library);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))window.playback()->media()->setAudioOutput(nullptr);
        window.show();
        window.showSettings();
        QTest::qWait(30);
        capture(window, "10-custom-palette.png");
        auto original = store.state();
        auto path = directory.filePath("wallpaper.png");
        QImage wallpaper(1200, 900, QImage::Format_RGB32);
        for (int y = 0; y < wallpaper.height(); ++y)
            for (int x = 0; x < wallpaper.width(); ++x)
                wallpaper.setPixelColor(x, y,
                                        QColor(208 + x * 32 / 1200, 217 + y * 22 / 900, 220 - x * 34 / 1200));
        QVERIFY(wallpaper.save(path));
        QTest::mouseClick(window.findChild<QPushButton *>("editAppearance"), Qt::LeftButton);
        auto *page=window.findChild<AppearancePage *>();QVERIFY(page);QTest::qWait(300);
        QVERIFY(!page->isWindow());capture(*page,"08-appearance-palette.png");
        page->findChild<QPushButton *>("appearanceMode4")->click();
        page->findChild<QLineEdit *>("appearanceImageLeft")->setText(path);
        page->findChild<QCheckBox *>("appearanceShareLR")->setChecked(true);
        page->findChild<BlurPercentSpin *>("appearanceBlurValueRight")->setValue(15);
        QTest::qWait(150);capture(*page,"09-appearance-image.png");
        page->findChild<QPushButton *>("appearanceNewSave")->click();
        QCOMPARE(store.state()["items"].toArray().size(), original["items"].toArray().size() + 1);
        QCOMPARE(store.active()["mode"].toString(), QString("image"));
        QCOMPARE(store.active()["blurRight"].toInt(), 15);
        QVERIFY(QFile::remove(path));
        QVERIFY(!store.imagePath(store.active(), "left").isEmpty());
        QTest::qWait(30);
        capture(window, "11-custom-image.png");
        window.showNowPlaying();
        QTest::qWait(30);
        capture(window, "12-custom-playing.png");
    }
    void appearancePageSaveAndPreviewRollback() {
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());
        QVERIFY(library.setSetting("theme",1));int previews=0,restores=0;
        auto preview=[&](const QJsonObject &){++previews;};auto restore=[&]{++restores;};
        { AppearancePage page(&library,preview,restore);bool finished=false;
          connect(&page,&AppearancePage::finished,&page,[&]{finished=true;page.hide();});
          page.show();page.findChild<QPushButton *>("appearanceMode3")->click();
          page.findChild<QPushButton *>("appearanceNewSave")->click();QVERIFY(finished); }
        QCOMPARE(library.setting("theme").toInt(),3);const auto before=AppearanceStore(&library).state();
        const auto source=directory.filePath("image.png");QVERIFY(image(source,QColor("#3c776c")));
        { AppearancePage page(&library,preview,restore);bool finished=false;
          connect(&page,&AppearancePage::finished,&page,[&]{finished=true;page.hide();});page.show();
          page.findChild<QPushButton *>("appearanceMode4")->click();
          page.findChild<QLineEdit *>("appearanceImageLeft")->setText(source);
          page.findChild<QCheckBox *>("appearanceShareLR")->setChecked(true);
          QTimer::singleShot(0,[]{for(auto *widget:QApplication::topLevelWidgets())if(auto *question=qobject_cast<QMessageBox *>(widget))for(auto *button:question->buttons())if(button->text()=="不保存")button->click();});
          page.findChild<QPushButton *>("appearanceBack")->click();QVERIFY(finished);QVERIFY(!page.isVisible()); }
        QCOMPARE(AppearanceStore(&library).state(),before);QVERIFY(previews>=4);QCOMPARE(restores,2);
    }

};
QTEST_MAIN(V11Tests)
#include "v11-tests.moc"
