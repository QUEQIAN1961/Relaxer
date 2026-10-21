#include "ui-bridge/settings-bridge.h"
#include "storage/appearance-store.h"
#include "shortcuts.h"
#include "system-theme-fixture.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSignalSpy>
#include <QSettings>
#include <QtTest>
#include <memory>

using namespace relaxer;
namespace {
struct Fixture {
    QTemporaryDir directory;
    Library library{directory.filePath("data")};
    std::unique_ptr<SettingsBridge> bridge;
    bool ok = false;
    explicit Fixture(SettingsBridge::SystemThemeReader reader = {}) {
        if (!directory.isValid() || !library.open()) return;
        bridge = std::make_unique<SettingsBridge>(&library, nullptr, std::move(reader));
        ok = true;
    }
    QString image(const QString &name, const QColor &color) {
        QImage image(48, 32, QImage::Format_RGB32); image.fill(color);
        const auto path = directory.filePath(name);
        return image.save(path, "PNG") ? path : QString();
    }
};
QVariantMap allRegions(bool value) {
    return {{"songs", value}, {"lyrics", value}, {"footer", value}, {"next", value}, {"rest", value}};
}
}
class QmlSettingsParityTests final : public QObject {
    Q_OBJECT
private slots:
    void preferenceCommandsAreImmediateValidatedAndUseOriginalKeys() {
        Fixture f; QVERIFY(f.ok);
        QSignalSpy changed(f.bridge.get(), &SettingsBridge::settingsChanged);
        QVERIFY(f.bridge->setPreference("threshold", 95));
        QCOMPARE(f.library.setting("threshold").toInt(), 95);
        QVERIFY(f.bridge->setPreference("closeToTray", true));
        QVERIFY(f.library.setting("closeToTray").toBool());
        QVERIFY(f.bridge->setPreference("lyricColor", "#aabbcc"));
        QVERIFY(f.bridge->setPreference("lyricColor", ""));
        QVERIFY(f.bridge->setPreference("reduceMotion", true));
        QVERIFY(f.library.setting("qml.reduceMotion").toBool());
        QVERIFY(f.library.setting("reduceMotion").isNull() || f.library.setting("reduceMotion").isUndefined());
        const auto events = changed.size();
        QVERIFY(!f.bridge->setPreference("threshold", 69));
        QVERIFY(!f.bridge->setPreference("threshold", 80.5));
        QVERIFY(!f.bridge->setPreference("closeToTray", 1));
        QVERIFY(!f.bridge->setPreference("lyricColor", "invalid"));
        QVERIFY(!f.bridge->setPreference("unrecognized", true));
        QVERIFY(!f.bridge->setPreference("songColumnWidths", QVariantMap{{"title", 95}}));
        QVERIFY(!f.bridge->setPreference("songColumnWidths", QVariantMap{{"wrong", 100}}));
        QCOMPARE(changed.size(), events);
        QVERIFY(f.bridge->setPreference("songColumnWidths", QVariantMap{{"title", 280}, {"artist", 180}}));
        QCOMPARE(f.library.setting("songColumnWidths").toObject().value("title").toInt(), 280);
        QCOMPARE(f.library.setting("threshold").toInt(), 95);
        QVERIFY(f.bridge->edit("threshold", 90));
        QCOMPARE(f.library.setting("threshold").toInt(), 95);
        QVERIFY(f.bridge->save());
        QCOMPARE(f.library.setting("threshold").toInt(), 90);
    }
    void windowStateKeepsWidgetsCompatibleKeys() {
        Fixture f; QVERIFY(f.ok);
        QCOMPARE(f.bridge->windowBounds().value("width").toInt(), 1240);
        QCOMPARE(f.bridge->windowBounds().value("height").toInt(), 790);
        QVERIFY(f.bridge->saveWindowState(1280, 800, true));
        QCOMPARE(f.library.setting("windowWidth").toInt(), 1280);
        QCOMPARE(f.library.setting("windowHeight").toInt(), 800);
        QVERIFY(f.library.setting("windowMaximized").toBool());
        QVERIFY(!f.bridge->saveWindowState(0, 800, false));
        QCOMPARE(f.library.setting("windowWidth").toInt(), 1280);
        QVERIFY(f.library.setting("windowMaximized").toBool());
    }
    void builtinAndCustomPaletteUseOriginalLeftRightAndFooterRoles_data() {
        QTest::addColumn<bool>("systemDark");
        QTest::newRow("light-system") << false;
        QTest::newRow("dark-system") << true;
    }
    void nativeDefaultThemeUsesReadonlySystemPreference() {
        Fixture f; QVERIFY(f.ok);
        QCOMPARE(f.library.setting("theme").toInt(),2);
#ifdef Q_OS_WIN
        QSettings preference("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",QSettings::NativeFormat);
        const bool dark=preference.value("AppsUseLightTheme",1).toInt()==0;
#else
        const bool dark=QGuiApplication::palette().color(QPalette::Window).lightness()<110;
#endif
        const auto actual=f.bridge->palette().value("background").toString();
        QCOMPARE(actual,QString(dark?"#19191e":"#ffffff"));
        qInfo().noquote()<<"NATIVE_THEME_EVIDENCE"<<QJsonDocument(QJsonObject{{"source","native readonly system preference; no injection"},
            {"currentHostDark",dark},{"actualBackground",actual},{"status","PASS; opposite real host theme NOT VERIFIED"}}).toJson(QJsonDocument::Compact);
    }
    void builtinAndCustomPaletteUseOriginalLeftRightAndFooterRoles() {
        QFETCH(bool, systemDark);
        SystemThemeFixture systemTheme(systemDark);
        Fixture f(systemTheme.reader()); QVERIFY(f.ok);
        QCOMPARE(f.library.setting("theme").toInt(), 2);
        QCOMPARE(f.bridge->palette().value("background").toString(),
                 QString(systemDark ? "#19191e" : "#ffffff"));
        QVERIFY(f.bridge->setTheme(0));
        auto palette = f.bridge->palette();
        QCOMPARE(palette.value("background").toString(), QString("#ffffff"));
        QCOMPARE(palette.value("panel").toString(), QString("#f5f5f7"));
        QVERIFY(f.bridge->setTheme(1));
        palette = f.bridge->palette();
        QCOMPARE(palette.value("background").toString(), QString("#19191e"));
        QCOMPARE(palette.value("panel").toString(), QString("#222228"));
        f.bridge->beginAppearance(); QVERIFY(f.bridge->setAppearanceMode(3));
        QVERIFY(f.bridge->editAppearance("left", "#113355"));
        QVERIFY(f.bridge->editAppearance("right", "#eeeeee"));
        QVERIFY(f.bridge->editAppearance("customFooter", true));
        QVERIFY(f.bridge->editAppearance("footer", "#445566"));
        QVERIFY(f.bridge->editAppearance("customText", true));
        QVERIFY(f.bridge->editAppearance("leftInk", "#ffffff"));
        QVERIFY(f.bridge->editAppearance("rightInk", "#112233"));
        palette = f.bridge->palette();
        QCOMPARE(palette.value("panel").toString(), QString("#113355"));
        QCOMPARE(palette.value("background").toString(), QString("#eeeeee"));
        QCOMPARE(palette.value("footer").toString(), QString("#445566"));
        QCOMPARE(palette.value("sideText").toString(), QString("#ffffff"));
        QCOMPARE(palette.value("text").toString(), QString("#112233"));
        QVERIFY(!palette.value("dark").toBool());
        QCOMPARE(f.library.setting("theme").toInt(), 1);
        f.bridge->discardAppearance();
        QCOMPARE(f.bridge->palette().value("background").toString(), QString("#19191e"));
        SettingsBridge reopened(&f.library, nullptr, systemTheme.reader());
        QCOMPARE(reopened.values().value("theme").toInt(), 1);
        QCOMPARE(reopened.palette().value("background").toString(), QString("#19191e"));
        QVERIFY(reopened.setTheme(2));
        SettingsBridge following(&f.library, nullptr, systemTheme.reader());
        QCOMPARE(following.values().value("theme").toInt(), 2);
        QCOMPARE(following.palette().value("background").toString(), QString(systemDark ? "#19191e" : "#ffffff"));
        systemTheme.record({{"initial", systemDark ? "#19191e" : "#ffffff"}, {"explicitLight", "#ffffff"},
            {"explicitDark", "#19191e"}, {"reopenedFollowing", following.palette().value("background").toString()}});
    }
    void authoringPreviewSaveRenameOverwriteUseAndDeleteReuseAppearanceStore() {
        Fixture f; QVERIFY(f.ok);
        f.bridge->beginAppearance(); QVERIFY(f.bridge->setAppearanceMode(3));
        QVERIFY(f.bridge->editAppearance("right", "#112233"));
        QVERIFY(f.bridge->appearanceDirty());
        QCOMPARE(f.bridge->appearances().size(), 0);
        QVERIFY(f.bridge->saveAppearance());
        QCOMPARE(f.library.setting("theme").toInt(), 3);
        QVERIFY(!f.bridge->appearanceDirty());
        const auto id = f.bridge->activeAppearanceId(); QVERIFY(!id.isEmpty());
        QCOMPARE(f.bridge->appearances().size(), 1);
        QVERIFY(f.bridge->renameAppearance(id, "夜色"));
        QCOMPARE(f.bridge->appearances().first().toMap().value("name").toString(), QString("夜色"));
        f.bridge->beginAppearance(id);
        QVERIFY(f.bridge->editAppearance("accent", "#fedcba"));
        QVERIFY(f.bridge->saveAppearance(true));
        QCOMPARE(f.bridge->appearances().size(), 1);
        QCOMPARE(AppearanceStore(&f.library).active().value("accent").toString(), QString("#fedcba"));
        QVERIFY(f.bridge->setTheme(0));
        QVERIFY(f.bridge->useAppearance(id));
        QCOMPARE(f.library.setting("theme").toInt(), 3);
        QVERIFY(!f.bridge->removeAppearance("missing"));
        QVERIFY(f.bridge->removeAppearance(id));
        QCOMPARE(f.bridge->appearances().size(), 0);
        QCOMPARE(f.library.setting("theme").toInt(), 0);
    }
    void imageAuthoringUsesManagedCopiesAndOriginalSharingAndBlurFields() {
        Fixture f; QVERIFY(f.ok);
        const auto left = f.image("left.png", QColor("#334455"));
        const auto right = f.image("right.png", QColor("#ddeeff"));
        const auto footer = f.image("footer.png", QColor("#ccbbaa"));
        QVERIFY(!left.isEmpty()); QVERIFY(!right.isEmpty()); QVERIFY(!footer.isEmpty());
        f.bridge->beginAppearance(); QVERIFY(f.bridge->setAppearanceMode(4));
        QVERIFY(f.bridge->editAppearance("sourceLeft", QUrl::fromLocalFile(left)));
        QVERIFY(f.bridge->editAppearance("sourceRight", QUrl::fromLocalFile(right)));
        QVERIFY(f.bridge->editAppearance("shareFooter", false));
        QVERIFY(f.bridge->editAppearance("sourceFooter", QUrl::fromLocalFile(footer)));
        QVERIFY(f.bridge->editAppearance("blurLeft", 12.5));
        QVERIFY(f.bridge->editAppearance("blurRight", 25.));
        QVERIFY(f.bridge->editAppearance("blurExpanded", 37.5));
        QVERIFY(!f.bridge->editAppearance("blurLeft", 50.1));
        QVERIFY(f.bridge->saveAppearance());
        AppearanceStore store(&f.library); const auto saved = store.active();
        QCOMPARE(saved.value("blurLeft").toDouble(), 12.5);
        QCOMPARE(saved.value("blurExpanded").toDouble(), 37.5);
        QVERIFY(saved.value("imageLeft").toString().startsWith("Appearance/"));
        QVERIFY(!saved.contains("sourceLeft"));
        auto palette = f.bridge->palette();
        QVERIFY(palette.value("imageMode").toBool());
        QCOMPARE(palette.value("background").toString(), QString("#ddeeff"));
        QCOMPARE(palette.value("panel").toString(), QString("#334455"));
        QCOMPARE(palette.value("leftImage").toUrl().toLocalFile(), store.imagePath(saved, "left"));
        QCOMPARE(palette.value("footerImage").toUrl().toLocalFile(), store.imagePath(saved, "footer"));
        f.bridge->beginAppearance(); QVERIFY(f.bridge->editAppearance("shareLR", true));
        QVERIFY(f.bridge->editAppearance("shareFooter", true));
        palette = f.bridge->palette();
        QCOMPARE(palette.value("leftImage").toUrl(), palette.value("rightImage").toUrl());
        QCOMPARE(palette.value("leftImage").toUrl(), palette.value("footerImage").toUrl());
        f.bridge->discardAppearance();
        QVERIFY(f.bridge->removeAppearance(saved.value("id").toString()));
        QVERIFY(QFileInfo::exists(left)); QVERIFY(QFileInfo::exists(store.imagePath(saved, "left")));
    }
    void progressColorsRespectRecommendedVersusCustomizedModes() {
        Fixture f; QVERIFY(f.ok);
        f.bridge->beginAppearance(); QVERIFY(f.bridge->setAppearanceMode(3));
        const auto recommended = f.bridge->palette().value("progressFill").toString();
        QVERIFY(f.bridge->editAppearance("progressFill", "#123456"));
        QVERIFY(f.bridge->editAppearance("progressTrack", "#abcdef"));
        QCOMPARE(f.bridge->palette().value("progressFill").toString(), recommended);
        QVERIFY(f.bridge->editAppearance("customProgressColors", true));
        QCOMPARE(f.bridge->palette().value("progressFill").toString(), QString("#123456"));
        QCOMPARE(f.bridge->palette().value("progressTrack").toString(), QString("#abcdef"));
        QVERIFY(f.bridge->saveAppearance());
        const auto saved = AppearanceStore(&f.library).active();
        QVERIFY(saved.value("progressColorsCustomized").toBool());
        QCOMPARE(saved.value("progressTrack").toString(), QString("#abcdef"));
    }
    void fontConfigurationRetainsOriginalSchemaAndRejectsInvalidFiles() {
        Fixture f; QVERIFY(f.ok);
        QFile file(f.directory.filePath("broken.ttf")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("not a font"); file.close();
        QVERIFY(!f.bridge->saveFont(QUrl::fromLocalFile(file.fileName()), allRegions(true)));
        QVERIFY(!f.bridge->saveFont(QUrl("https://example.invalid/font.ttf"), allRegions(true)));
        QVERIFY(!f.bridge->saveFont({}, {{"lyrics", true}}));
        QVERIFY(f.bridge->saveFont({}, allRegions(false)));
        const auto saved = f.library.setting("uiFont").toObject();
        for (const auto &key : {"songs", "lyrics", "footer", "next", "rest"}) QVERIFY(saved.value(key).isBool());
        QVERIFY(saved.value("file").isString());
        QVERIFY(f.bridge->fontFamilyFor("lyrics").isEmpty());
        QVERIFY(f.bridge->resetFont());
        QVERIFY(f.library.setting("uiFont").toObject().isEmpty());
    }
    void fontImportCopiesSelectedFontIntoOriginalFontsDirectory() {
        Fixture f; QVERIFY(f.ok);
#ifdef Q_OS_WIN
        const QString source = qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/segoeui.ttf";
        if (!QFileInfo::exists(source)) QSKIP("Windows Segoe UI font fixture is unavailable");
        QVERIFY(f.bridge->saveFont(QUrl::fromLocalFile(source), allRegions(true)));
        const auto saved = f.library.setting("uiFont").toObject();
        QCOMPARE(saved.value("file").toString(), QString("Fonts/segoeui.ttf"));
        QVERIFY(QFileInfo::exists(f.library.absolute("Fonts/segoeui.ttf")));
        QVERIFY(!f.bridge->fontFamilyFor("lyrics").isEmpty());
        QVERIFY(!f.bridge->fontFamilyFor("footer").isEmpty());
        auto flags = allRegions(false); flags["songs"] = true;
        QVERIFY(f.bridge->saveFont({}, flags));
        QVERIFY(!f.bridge->fontFamilyFor("songs").isEmpty());
        QVERIFY(f.bridge->fontFamilyFor("lyrics").isEmpty());
        QVERIFY(f.bridge->resetFont());
        QVERIFY(QFileInfo::exists(f.library.absolute("Fonts/segoeui.ttf")));
#else
        QSKIP("Windows font-file fixture only");
#endif
    }
    void shortcutConfigurationUsesOriginalValidatorAndPortableText() {
        Fixture f; QVERIFY(f.ok);
        auto candidate = f.bridge->defaultShortcuts();
        QCOMPARE(QJsonObject::fromVariantMap(candidate), PlayerShortcuts::defaults());
        QVERIFY(f.bridge->saveShortcuts(candidate));
        QCOMPARE(f.library.setting("shortcuts").toObject(), PlayerShortcuts::defaults());
        QCOMPARE(f.bridge->shortcutText(Qt::Key_Right, Qt::ControlModifier), QString("Ctrl+Right"));
        auto keys = candidate.value("app").toMap(); keys["next"] = "Ctrl+O"; candidate["app"] = keys;
        QVERIFY(!f.bridge->saveShortcuts(candidate));
        QCOMPARE(f.library.setting("shortcuts").toObject(), PlayerShortcuts::defaults());
        candidate = f.bridge->defaultShortcuts(); keys = candidate.value("app").toMap(); keys["next"] = "Space"; candidate["app"] = keys;
        QVERIFY(!f.bridge->saveShortcuts(candidate));
        QCOMPARE(f.bridge->shortcutActions().size(), 5);
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv); app.setStyle("Fusion"); QmlSettingsParityTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "qml-settings-parity-tests.moc"
