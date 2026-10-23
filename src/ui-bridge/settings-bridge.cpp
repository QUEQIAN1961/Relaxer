#include "settings-bridge.h"
#include "appearance-image.h"
#include "shortcuts.h"
#include <QColor>
#include <QColorDialog>
#include <QApplication>
#include <QWindow>
#include <QDesktopServices>
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QKeySequence>
#include <QPalette>
#include <QSaveFile>
#include <QSettings>
#include <QStyleHints>
#include <QDir>
#include <cmath>

namespace relaxer {
namespace {
QStringList preferenceKeys() {
    return {"theme", "threshold", "closeToTray", "showLyricTranslation", "playerCoverBackground", "lyricColor", "reduceMotion", "songColumnWidths"};
}
QColor mixed(const QColor &first, const QColor &second, double amount) {
    return QColor(qRound(first.red() * amount + second.red() * (1 - amount)),
                  qRound(first.green() * amount + second.green() * (1 - amount)),
                  qRound(first.blue() * amount + second.blue() * (1 - amount)));
}
QColor averageImage(const QString &path, const QColor &fallback) {
    const auto image = backgroundImage(path, 32).convertToFormat(QImage::Format_RGB32);
    if (image.isNull()) return fallback;
    qint64 r = 0, g = 0, b = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const auto pixel = image.pixel(x, y);
            r += qRed(pixel); g += qGreen(pixel); b += qBlue(pixel);
        }
    const auto count = image.width() * image.height();
    return QColor(int(r / count), int(g / count), int(b / count));
}
bool systemDark() {
#ifdef Q_OS_WIN
    QSettings system("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
    return system.value("AppsUseLightTheme", 1).toInt() == 0;
#else
    return QGuiApplication::palette().color(QPalette::Window).lightness() < 110;
#endif
}
}

SettingsBridge::SettingsBridge(Library *library, QObject *parent, SystemThemeReader systemTheme)
    : QObject(parent), m_library(library), m_store(library), m_appearance(library), m_systemTheme(std::move(systemTheme)) {
    resetDraft();
    updateFont();
    if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(app, &QGuiApplication::paletteChanged, this, [this] {
            if (m_store.get("theme", 0).toInt() == 2) emit settingsChanged();
        });
        connect(app->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
            if (m_store.get("theme", 0).toInt() == 2) emit settingsChanged();
        });
    }
}
SettingsBridge::~SettingsBridge() {
    delete m_colorDialog.data();
    if (m_fontId >= 0) QFontDatabase::removeApplicationFont(m_fontId);
}

QVariantList SettingsBridge::changelog() const {QFile file(":/changelog.json");return file.open(QIODevice::ReadOnly)?QJsonDocument::fromJson(file.readAll()).toVariant().toList():QVariantList();}

QVariantMap SettingsBridge::values() const {
    return {{"theme", m_store.get("theme", 0).toInt()}, {"threshold", m_store.get("threshold", 80).toInt()},
            {"closeToTray", m_store.get("closeToTray", false).toBool()},
            {"showLyricTranslation", m_store.get("showLyricTranslation", true).toBool()},
            {"playerCoverBackground", m_store.get("playerCoverBackground", true).toBool()},
            {"lyricColor", m_store.get("lyricColor", QString()).toString()},
            {"reduceMotion", m_store.get("qml.reduceMotion", false).toBool()},
            {"songColumnWidths", m_store.get("songColumnWidths", QJsonObject()).toObject().toVariantMap()},
            {"shortcuts", m_store.get("shortcuts", PlayerShortcuts::defaults()).toObject().toVariantMap()},
            {"uiFont", m_store.get("uiFont", QJsonObject()).toObject().toVariantMap()},
            {"windowBounds", QVariantMap{{"width", m_store.get("windowWidth", 1240).toInt()},
                                          {"height", m_store.get("windowHeight", 790).toInt()},
                                          {"maximized", m_store.get("windowMaximized", false).toBool()}}}};
}

QVariantMap SettingsBridge::palette() const {
    const int theme = m_preview.isEmpty() ? m_store.get("theme", 0).toInt() : m_preview["preset"].toInt(3);
    const bool custom = theme == 3;
    const auto appearance = m_preview.isEmpty() ? m_appearance.active() : m_preview;
    bool dark = theme == 1 || (theme == 2 && (m_systemTheme ? m_systemTheme() : systemDark()));
    QColor background(dark ? "#19191e" : "#ffffff"), side(dark ? "#222228" : "#f5f5f7");
    QColor ink(dark ? "#f0edf1" : "#25252b"), sideInk(ink), muted(dark ? "#a09ca6" : "#86818a");
    QColor border(dark ? "#34343c" : "#e9e7eb"), card(dark ? "#25252d" : "#f7f6f8"), accent("#e5455f");
    QString leftImage, rightImage, footerImage;
    const auto color = [&appearance](const char *key, const QColor &fallback) {
        const QColor candidate(appearance.value(key).toString());
        return candidate.isValid() ? candidate : fallback;
    };
    const auto image = [this, &appearance](const QString &region) {
        const auto source = appearance.value("source" + region).toString();
        return source.isEmpty() ? m_appearance.imagePath(appearance, region.toLower()) : source;
    };
    const bool imageMode = custom && appearance["mode"] == "image";
    const bool shareLR = imageMode && appearance["shareLR"].toBool();
    const bool shareFooter = imageMode && appearance["shareFooter"].toBool(true);
    if (custom) {
        background = color("right", background); side = color("left", side); accent = color("accent", accent);
        if (imageMode) {
            leftImage = image("Left"); rightImage = shareLR ? leftImage : image("Right");
            background = averageImage(rightImage, background); side = averageImage(leftImage, side);
        }
        ink = appearance["customText"].toBool() ? color("rightInk", autoInk(background)) : autoInk(background);
        sideInk = appearance["customText"].toBool() ? color("leftInk", autoInk(side)) : autoInk(side);
        dark = autoInk(background).lightness() > 128;
        muted = appearance["customText"].toBool() ? color("muted", ink) : mixed(ink, background, .68);
        border = mixed(ink, background, .16);
        card = dark ? background.lighter(115) : background.darker(103);
    }
    QColor footer(custom && appearance["customFooter"].toBool() ? color("footer", card) : card);
    if (imageMode) footerImage = shareFooter ? (shareLR ? leftImage : rightImage) : image("Footer");
    const auto progress = custom ? m_appearance.withProgressColors(appearance) : QJsonObject();
    const bool manual = progress["customProgressColors"].toBool();
    const auto progressFill = custom ? progress[manual ? "progressFill" : "recommendedProgressFill"].toString() : accent.name();
    const auto progressTrack = custom ? progress[manual ? "progressTrack" : "recommendedProgressTrack"].toString() : border.name();
    return {{"dark", dark}, {"custom", custom}, {"background", background.name()}, {"panel", side.name()},
            {"card", card.name()}, {"footer", footer.name()}, {"accent", accent.name()},
            {"text", ink.name()}, {"sideText", sideInk.name()}, {"secondary", muted.name()}, {"border", border.name()},
            {"accentText", autoInk(accent).name()}, {"mutedBold", custom && appearance["mutedBold"].toBool()},
            {"progressFill", progressFill}, {"progressTrack", progressTrack},
            {"imageMode", imageMode}, {"shareLR", shareLR}, {"shareFooter", shareFooter},
            {"leftImage", leftImage.isEmpty() ? QUrl() : QUrl::fromLocalFile(leftImage)},
            {"rightImage", rightImage.isEmpty() ? QUrl() : QUrl::fromLocalFile(rightImage)},
            {"footerImage", footerImage.isEmpty() ? QUrl() : QUrl::fromLocalFile(footerImage)},
            {"expandedImage", rightImage.isEmpty() ? QUrl() : QUrl::fromLocalFile(rightImage)},
            {"blurLeft", imageMode ? appearance["blurLeft"].toDouble() : 0.},
            {"blurRight", imageMode ? appearance["blurRight"].toDouble() : 0.},
            {"blurExpanded", custom ? appearance["blurExpanded"].toDouble(15) : 25.}};
}

QVariantList SettingsBridge::appearances() const {
    QVariantList rows;
    for (const auto &value : m_appearance.state()["items"].toArray()) {
        auto item = value.toObject().toVariantMap();
        item["active"] = value.toObject()["id"] == m_appearance.state()["active"];
        const auto left = m_appearance.imagePath(value.toObject(), "left");
        const auto right = value.toObject()["shareLR"].toBool() ? left : m_appearance.imagePath(value.toObject(), "right");
        item["leftImageUrl"] = left.isEmpty() ? QUrl() : QUrl::fromLocalFile(left);
        item["rightImageUrl"] = right.isEmpty() ? QUrl() : QUrl::fromLocalFile(right);
        rows.append(item);
    }
    return rows;
}
void SettingsBridge::message(const QString &value) { m_status = value; emit statusChanged(); }
void SettingsBridge::resetDraft() {
    const auto preferences = values();
    m_draft = {};
    for (const auto &key : preferenceKeys()) m_draft[key] = QJsonValue::fromVariant(preferences.value(key));
    emit draftChanged(); message({});
}
bool SettingsBridge::edit(const QString &key, const QVariant &value) {
    if (!m_draft.contains(key)) { message("未知设置项"); return false; }
    m_draft[key] = QJsonValue::fromVariant(value); emit draftChanged(); return true;
}
QString SettingsBridge::validatePreference(const QString &key, const QJsonValue &value) const {
    if (!preferenceKeys().contains(key)) return "未知设置项";
    if (key == "theme" || key == "threshold") {
        if (!value.isDouble() || value.toDouble() != value.toInt()) return "设置需要整数";
        if (key == "theme" && (value.toInt() < 0 || value.toInt() > 3)) return "主题超出范围";
        if (key == "threshold" && (value.toInt() < 70 || value.toInt() > 100)) return "有效播放比例应为 70%～100%";
    } else if (key == "songColumnWidths") {
        if (!value.isObject()) return "列宽设置无效";
        const QStringList columns{"title", "artist", "album", "duration", "fileSize", "plays", "importDate"};
        const auto widths = value.toObject();
        for (auto it = widths.constBegin(); it != widths.constEnd(); ++it)
            if (!columns.contains(it.key()) || !it.value().isDouble() ||
                it.value().toDouble() != it.value().toInt() ||
                it.value().toInt() < (it.key() == "title" ? 96 : 40) || it.value().toInt() > 2400)
                return "列宽超出范围";
    } else if (key == "lyricColor") {
        if (!value.isString() || (!value.toString().isEmpty() && !QColor(value.toString()).isValid())) return "歌词颜色无效";
    } else if (!value.isBool()) return "设置值需要布尔类型";
    return {};
}
QString SettingsBridge::validate(const QJsonObject &value) const {
    for (const auto &key : preferenceKeys()) {
        const auto error = validatePreference(key, value[key]); if (!error.isEmpty()) return error;
    }
    return {};
}
bool SettingsBridge::save() {
    const auto error = validate(m_draft); if (!error.isEmpty()) { message(error); return false; }
    auto data = m_draft; data["qml.reduceMotion"] = data.take("reduceMotion");
    if (!m_store.setMany(data)) { message(m_library->lastError()); return false; }
    emit settingsChanged(); message("设置已保存"); return true;
}
bool SettingsBridge::setPreference(const QString &key, const QVariant &value) {
    const auto encoded = QJsonValue::fromVariant(value);
    const auto error = validatePreference(key, encoded); if (!error.isEmpty()) { message(error); return false; }
    if (!m_store.set(key == "reduceMotion" ? "qml.reduceMotion" : key, encoded)) { message(m_library->lastError()); return false; }
    m_draft[key] = encoded; emit draftChanged(); emit settingsChanged(); message({}); return true;
}
bool SettingsBridge::setTheme(int theme) { return setPreference("theme", theme); }
bool SettingsBridge::saveWindowState(int width, int height, bool maximized) {
    if (width < 640 || width > 32768 || height < 400 || height > 32768) return false;
    return m_store.setMany({{"windowWidth", width}, {"windowHeight", height}, {"windowMaximized", maximized}});
}
bool SettingsBridge::openDataRoot() { return QDesktopServices::openUrl(QUrl::fromLocalFile(dataRoot())); }

void SettingsBridge::updateFont() {
    if (m_fontId >= 0) QFontDatabase::removeApplicationFont(m_fontId);
    m_fontId = -1; m_fontFamily.clear();
    const auto file = m_store.get("uiFont").toObject()["file"].toString();
    if (file.startsWith("Fonts/") && QDir::cleanPath(file) == file) {
        const auto absolute = QFileInfo(m_library->absolute(file)).canonicalFilePath();
        const auto directory = QFileInfo(m_library->absolute("Fonts")).canonicalFilePath();
        if (!directory.isEmpty() && absolute.startsWith(directory + '/')) m_fontId = QFontDatabase::addApplicationFont(absolute);
    }
    if (m_fontId >= 0) m_fontFamily = QFontDatabase::applicationFontFamilies(m_fontId).value(0);
}
QString SettingsBridge::fontFamilyFor(const QString &region) const {
    return !m_fontFamily.isEmpty() && m_store.get("uiFont").toObject()[region].toBool() ? m_fontFamily : QString();
}
bool SettingsBridge::saveFont(const QUrl &source, const QVariantMap &regions) {
    QJsonObject font;
    for (const auto &key : {"songs", "lyrics", "footer", "next", "rest"}) {
        const auto value = QJsonValue::fromVariant(regions.value(key));
        if (!value.isBool()) { message("请选择字体应用范围"); return false; }
        font[key] = value;
    }
    QString relative = m_store.get("uiFont").toObject()["file"].toString();
    if (!source.isEmpty()) {
        const QFileInfo info(source.toLocalFile());
        const auto suffix = info.suffix().toLower();
        if (!source.isLocalFile() || !info.isFile() || info.size() > 50 * 1024 * 1024 ||
            (suffix != "ttf" && suffix != "otf" && suffix != "ttc")) { message("请选择有效的 TTF、OTF 或 TTC 字体文件"); return false; }
        QFile input(info.absoluteFilePath());
        if (!input.open(QIODevice::ReadOnly)) { message("字体文件打不开"); return false; }
        const auto bytes = input.readAll();
        input.close();
        const auto check = QFontDatabase::addApplicationFontFromData(bytes);
        if (check < 0) { message("字体文件无法加载"); return false; }
        QFontDatabase::removeApplicationFont(check);
        relative = "Fonts/" + info.fileName();
        if (!QDir().mkpath(m_library->absolute("Fonts"))) { message("无法创建字体目录"); return false; }
        QSaveFile output(m_library->absolute(relative));
        if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) { message("字体文件复制失败"); return false; }
    }
    font["file"] = relative;
    if (!m_store.set("uiFont", font)) { message(m_library->lastError()); return false; }
    updateFont(); emit settingsChanged(); message("字体已保存"); return true;
}
bool SettingsBridge::resetFont() {
    if (!m_store.set("uiFont", QJsonObject())) return false;
    updateFont(); emit settingsChanged(); message("已恢复系统字体"); return true;
}
QVariantList SettingsBridge::shortcutActions() const {
    QVariantList result; const auto keys = PlayerShortcuts::actionKeys(), names = PlayerShortcuts::actionNames();
    for (int i = 0; i < keys.size(); ++i) result.append(QVariantMap{{"key", keys[i]}, {"name", names[i]}});
    return result;
}
QString SettingsBridge::shortcutText(int key, int modifiers) const {
    return QKeySequence(QKeyCombination(Qt::KeyboardModifiers(modifiers), Qt::Key(key))).toString(QKeySequence::PortableText);
}
QVariantMap SettingsBridge::defaultShortcuts() const { return PlayerShortcuts::defaults().toVariantMap(); }
bool SettingsBridge::saveShortcuts(const QVariantMap &configuration) {
    const auto candidate = QJsonObject::fromVariantMap(configuration); QString error;
    if (!candidate["globalEnabled"].isBool() || !PlayerShortcuts::validate(candidate, &error)) { message(error.isEmpty() ? "快捷键配置无效" : error); return false; }
    if (!m_store.set("shortcuts", candidate)) { message(m_library->lastError()); return false; }
    emit settingsChanged(); message("快捷键已保存"); return true;
}

void SettingsBridge::beginAppearance(const QString &id) {
    ++m_appearanceSession;
    if (m_colorDialog) m_colorDialog->reject();
    auto appearance = m_appearance.active(); m_activeAppearanceId = m_appearance.state()["active"].toString();
    if (!id.isEmpty()) {
        for (const auto &item : m_appearance.state()["items"].toArray())
            if (item.toObject()["id"] == id) { appearance = item.toObject(); m_activeAppearanceId = id; break; }
    } else if (m_store.get("theme", 0).toInt() != 3) {
        m_activeAppearanceId.clear();
        const auto colors = palette(); appearance = AppearanceStore::defaults();
        appearance["left"] = colors.value("panel").toString(); appearance["right"] = colors.value("background").toString();
        appearance["accent"] = colors.value("accent").toString(); appearance["leftInk"] = colors.value("sideText").toString();
        appearance["rightInk"] = colors.value("text").toString(); appearance["muted"] = colors.value("secondary").toString(); appearance["customText"] = true;
    }
    m_appearanceDraft = m_appearance.withProgressColors(m_appearance.draft(appearance));
    const auto theme = id.isEmpty() ? m_store.get("theme", 0).toInt() : 3;
    m_appearanceMode = theme < 3 ? theme : m_appearanceDraft["mode"] == "image" ? 4 : 3;
    m_appearanceDirty = false; m_preview = {}; emit appearanceChanged(); emit settingsChanged();
}
void SettingsBridge::previewAppearance() {
    m_appearanceDraft = m_appearance.withProgressColors(m_appearanceDraft, true);
    m_preview = m_appearanceDraft; m_preview["preset"] = m_appearanceMode < 3 ? m_appearanceMode : 3;
    emit appearanceChanged(); emit settingsChanged();
}
bool SettingsBridge::editAppearance(const QString &key, const QVariant &value) {
    const QStringList colors{"left", "right", "accent", "leftInk", "rightInk", "muted", "footer", "progressFill", "progressTrack"};
    const QStringList flags{"customText", "mutedBold", "shareLR", "customFooter", "shareFooter", "customProgressColors"};
    const QStringList images{"sourceLeft", "sourceRight", "sourceFooter"};
    const QStringList blurs{"blurLeft", "blurRight", "blurExpanded"};
    const auto encoded = QJsonValue::fromVariant(value);
    if (colors.contains(key)) {
        if (!QColor(value.toString()).isValid()) { message("颜色无效"); return false; }
        m_appearanceDraft[key] = QColor(value.toString()).name();
        if (key == "progressFill" || key == "progressTrack") m_appearanceDraft["progressColorsCustomized"] = true;
    } else if (flags.contains(key) && encoded.isBool()) m_appearanceDraft[key] = encoded;
    else if (images.contains(key)) {
        const auto url = value.canConvert<QUrl>() ? value.toUrl() : QUrl();
        const auto path = url.isLocalFile() ? url.toLocalFile() : value.toString();
        if (!path.isEmpty() && !QFileInfo(path).isAbsolute()) { message("请选择本地背景图片"); return false; }
        m_appearanceDraft[key] = path;
    } else if (blurs.contains(key) && encoded.isDouble() && std::isfinite(encoded.toDouble()) && encoded.toDouble() >= 0 && encoded.toDouble() <= 50)
        m_appearanceDraft[key] = encoded;
    else { message("外观设置值无效"); return false; }
    m_appearanceDirty = true; previewAppearance(); return true;
}
bool SettingsBridge::setAppearanceMode(int mode) {
    if (mode < 0 || mode > 4) return false;
    m_appearanceMode = mode; if (mode >= 3) m_appearanceDraft["mode"] = mode == 4 ? "image" : "palette";
    m_appearanceDirty = true; previewAppearance(); return true;
}
bool SettingsBridge::chooseAppearanceColor(const QString &key, QObject *ownerWindow) {
    const QStringList colors{"left", "right", "accent", "leftInk", "rightInk", "muted", "footer", "progressFill", "progressTrack"};
    auto *owner = qobject_cast<QWindow *>(ownerWindow);
    if (!colors.contains(key) || !owner || !qobject_cast<QApplication *>(QCoreApplication::instance())) return false;
    if (m_colorDialog) { m_colorDialog->raise(); m_colorDialog->activateWindow(); return false; }
    auto *dialog = new QColorDialog(QColor(m_appearanceDraft[key].toString()));
    m_colorDialog = dialog;
    dialog->setObjectName("appearanceFullColorDialog");
    dialog->setWindowTitle("选择颜色");
    dialog->setOption(QColorDialog::DontUseNativeDialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::ApplicationModal);
    // Reuse the same complete Widgets chooser as the legacy appearance editor:
    // basic/custom colors, screen picker, HSV, RGB and HTML input.
    const auto look = palette();
    QPalette colorsPalette = QApplication::palette();
    colorsPalette.setColor(QPalette::Window, QColor(look["background"].toString()));
    colorsPalette.setColor(QPalette::Base, QColor(look["background"].toString()));
    colorsPalette.setColor(QPalette::Button, QColor(look["card"].toString()));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        colorsPalette.setColor(role, QColor(look["text"].toString()));
    colorsPalette.setColor(QPalette::Highlight, QColor(look["accent"].toString()));
    colorsPalette.setColor(QPalette::HighlightedText, QColor(look["accentText"].toString()));
    dialog->setPalette(colorsPalette);
    dialog->setFont(owner->property("font").value<QFont>());
    dialog->winId();
    dialog->windowHandle()->setTransientParent(owner);
    const auto session = m_appearanceSession;
    connect(dialog, &QColorDialog::colorSelected, this, [this, key, session](const QColor &color) {
        if (session == m_appearanceSession && color.isValid()) editAppearance(key, color.name());
    });
    connect(dialog, &QDialog::finished, this, [this, dialog] {
        if (m_colorDialog == dialog) m_colorDialog = nullptr;
    });
    dialog->open();
    return true;
}
void SettingsBridge::resetAppearance() {
    m_appearanceDraft = m_appearance.withProgressColors(AppearanceStore::defaults()); m_appearanceMode = 3;
    m_appearanceDirty = true; previewAppearance();
}
void SettingsBridge::discardAppearance() {
    ++m_appearanceSession;
    if (m_colorDialog) m_colorDialog->reject();
    m_preview = {}; m_appearanceDirty = false; emit appearanceChanged(); emit settingsChanged();
}
bool SettingsBridge::saveAppearance(bool overwrite) {
    QString error;
    if (overwrite && m_activeAppearanceId.isEmpty()) { message("没有可覆盖的外观"); return false; }
    const bool ok = m_appearanceMode < 3 ? m_store.set("theme", m_appearanceMode)
        : m_appearance.save(m_appearanceDraft, overwrite ? m_activeAppearanceId : QString(), &error);
    if (!ok) { message(error.isEmpty() ? m_library->lastError() : error); return false; }
    m_activeAppearanceId = m_appearance.state()["active"].toString(); m_preview = {}; m_appearanceDirty = false;
    resetDraft(); emit appearanceChanged(); emit settingsChanged(); message("外观已保存并应用"); return true;
}
bool SettingsBridge::useAppearance(const QString &id) {
    if (!m_appearance.use(id)) { message("外观不存在或保存失败"); return false; }
    m_preview = {}; m_appearanceDirty = false; resetDraft(); beginAppearance(id);
    emit settingsChanged(); message("已应用保存的外观"); return true;
}
bool SettingsBridge::renameAppearance(const QString &id, const QString &name) {
    if (!m_appearance.rename(id, name)) { message("外观重命名失败"); return false; }
    emit settingsChanged(); message("外观已重命名"); return true;
}
bool SettingsBridge::removeAppearance(const QString &id) {
    if (!m_appearance.remove(id)) { message("外观删除失败"); return false; }
    if (m_activeAppearanceId == id) { m_activeAppearanceId.clear(); m_preview = {}; m_appearanceDirty = false; emit appearanceChanged(); }
    resetDraft(); emit settingsChanged(); message("已删除保存的外观，原始图片保留"); return true;
}
bool SettingsBridge::savePalette(const QString &name, const QString &background, const QString &panel, const QString &accent) {
    if (name.trimmed().isEmpty() || !QColor(background).isValid() || !QColor(panel).isValid() || !QColor(accent).isValid()) { message("请填写名称和有效颜色"); return false; }
    auto value = AppearanceStore::defaults(); value["left"] = panel; value["right"] = background; value["accent"] = accent;
    QString error; if (!m_appearance.save(value, {}, &error)) { message(error); return false; }
    const auto id = m_appearance.state()["active"].toString();
    if (!m_appearance.rename(id, name.trimmed())) { message("配色已保存，但名称未能保存"); emit settingsChanged(); return false; }
    m_preview = {}; resetDraft(); emit settingsChanged(); message("配色已保存并应用"); return true;
}
} // namespace relaxer
