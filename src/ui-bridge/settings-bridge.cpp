#include "settings-bridge.h"
#include "changelog-summary.h"
#include "appearance-image.h"
#include "shortcut-rules.h"
#include "ui-font-config.h"
#include <QCryptographicHash>
#include <QColor>
#include <QColorDialog>
#include <QLabel>
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
#include <QPainterPath>
#include <QPainter>
#include <QRegion>
#include <QEvent>
#include <QStyleHints>
#include <QDir>
#include <cmath>

namespace relaxer {
namespace {
// Qt paints the color surfaces itself, so stylesheet radii alone cannot clip
// their square corners. Keep their painting and input behavior unchanged.
class ColorSurfaceCorners final : public QObject {
public:
    explicit ColorSurfaceCorners(QWidget *surface) : QObject(surface) {
        surface->installEventFilter(this);
        updateMask(surface);
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()==QEvent::Resize||event->type()==QEvent::Show)
            updateMask(static_cast<QWidget *>(object));
        return false;
    }
private:
    void updateMask(QWidget *surface) {
        QPainterPath path;path.addRoundedRect(QRectF(surface->rect()),6,6);
        surface->setMask(QRegion(path.toFillPolygon().toPolygon()));
    }
};
// Draw smooth corners over Qt's color wells without replacing their selection,
// drag/drop or keyboard focus handling. Their outer focus ring stays visible.
class ColorWellCorners final : public QWidget {
public:
    ColorWellCorners(QWidget *well,int rows,const QColor &border) : QWidget(well),m_rows(rows),m_border(border) {
        setObjectName("roundedColorSwatches");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        setGeometry(well->rect());well->installEventFilter(this);show();
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()==QEvent::Resize)setGeometry(static_cast<QWidget *>(object)->rect());
        return false;
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        // Qt's complete chooser uses eight columns, with a three-pixel cell margin.
        const auto grid=parentWidget()->sizeHint();
        const QSizeF cell(grid.width()/8.,grid.height()/qreal(m_rows));
        for(int row=0;row<m_rows;++row)for(int column=0;column<8;++column) {
            const QRectF cellFrame=QRectF(QPointF(column*cell.width(),row*cell.height()),cell).adjusted(3,3,-3,-3);
            const auto outline=cellFrame.adjusted(.5,.5,-.5,-.5);
            QPainterPath corners;corners.setFillRule(Qt::OddEvenFill);
            corners.addRect(cellFrame);corners.addRoundedRect(outline,4,4);
            painter.fillPath(corners,palette().color(QPalette::Window));
            painter.setPen(QPen(m_border,1));painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(outline,4,4);
        }
    }
private:
    int m_rows;
    QColor m_border;
};
QStringList preferenceKeys() {
    return {"theme", "threshold", "closeToTray", "showLyricTranslation", "playerCoverBackground", "lyricColor", "reduceMotion", "mutedBold", "showFavorites", "songColumnWidths"};
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
    // Invalidate before QML handles the existing notification, including live previews.
    connect(this, &SettingsBridge::settingsChanged, this, &SettingsBridge::invalidateCaches);
    connect(library, &Library::settingsUpdated, this, [this](const QStringList &keys) {
        static const QStringList relevant{"theme", "threshold", "closeToTray", "showLyricTranslation",
            "playerCoverBackground", "lyricColor", "qml.reduceMotion", "mutedBold", "showFavorites", "songColumnWidths",
            "shortcuts", "uiFont", "windowWidth", "windowHeight", "windowMaximized", "appearanceHistory"};
        for (const auto &key : keys) if (relevant.contains(key)) { invalidateCaches(); break; }
    });
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
    for(const auto id:m_fontIds)QFontDatabase::removeApplicationFont(id);
}

QVariantList SettingsBridge::changelog() const {QFile file(":/changelog.json");return file.open(QIODevice::ReadOnly)?summarizedChangelog(QJsonDocument::fromJson(file.readAll()).array()).toVariantList():QVariantList();}

QVariantMap SettingsBridge::values() const {
    if (m_valuesCache) return *m_valuesCache;
    m_valuesCache = QVariantMap{{"theme", m_store.get("theme", 0).toInt()}, {"threshold", m_store.get("threshold", 80).toInt()},
            {"closeToTray", m_store.get("closeToTray", false).toBool()},
            {"showLyricTranslation", m_store.get("showLyricTranslation", true).toBool()},
            {"playerCoverBackground", m_store.get("playerCoverBackground", true).toBool()},
            {"lyricColor", m_store.get("lyricColor", QString()).toString()},
            {"showFavorites", m_store.get("showFavorites", true).toBool()},
            {"reduceMotion", m_store.get("qml.reduceMotion", false).toBool()},
            {"mutedBold", m_store.get("mutedBold", m_appearance.active().value("mutedBold")).toBool()},
            {"songColumnWidths", m_store.get("songColumnWidths", QJsonObject()).toObject().toVariantMap()},
            {"shortcuts", ShortcutRules::withMiniPlayerBinding(m_store.get("shortcuts", ShortcutRules::defaults()).toObject()).toVariantMap()},
            {"uiFont", m_store.get("uiFont", QJsonObject()).toObject().toVariantMap()},
            {"windowBounds", QVariantMap{{"width", m_store.get("windowWidth", 1240).toInt()},
                                          {"height", m_store.get("windowHeight", 790).toInt()},
                                          {"maximized", m_store.get("windowMaximized", false).toBool()}}}};
    return *m_valuesCache;
}

QVariantMap SettingsBridge::palette() const {
    if (m_paletteCache) return *m_paletteCache;
    const int theme = m_preview.isEmpty() ? values().value("theme").toInt() : m_preview["preset"].toInt(3);
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
    m_paletteCache = QVariantMap{{"dark", dark}, {"custom", custom}, {"background", background.name()}, {"panel", side.name()},
            {"card", card.name()}, {"footer", footer.name()}, {"accent", accent.name()},
            {"text", ink.name()}, {"sideText", sideInk.name()}, {"secondary", muted.name()}, {"border", border.name()},
            {"accentText", autoInk(accent).name()}, {"mutedBold", values().value("mutedBold").toBool()},
            {"progressFill", progressFill}, {"progressTrack", progressTrack},
            {"imageMode", imageMode}, {"shareLR", shareLR}, {"shareFooter", shareFooter},
            {"leftImage", leftImage.isEmpty() ? QUrl() : QUrl::fromLocalFile(leftImage)},
            {"rightImage", rightImage.isEmpty() ? QUrl() : QUrl::fromLocalFile(rightImage)},
            {"footerImage", footerImage.isEmpty() ? QUrl() : QUrl::fromLocalFile(footerImage)},
            {"expandedImage", rightImage.isEmpty() ? QUrl() : QUrl::fromLocalFile(rightImage)},
            {"blurLeft", imageMode ? appearance["blurLeft"].toDouble() : 0.},
            {"blurRight", imageMode ? appearance["blurRight"].toDouble() : 0.},
            {"blurFooter", imageMode ? appearance["blurFooter"].toDouble(appearance["blurRight"].toDouble()) : 0.},
            {"blurExpanded", custom ? appearance["blurExpanded"].toDouble(15) : 25.}};
    return *m_paletteCache;
}

QVariantList SettingsBridge::appearances() const {
    if (m_appearancesCache) return *m_appearancesCache;
    QVariantList rows;
    const auto state = m_appearance.state();
    for (const auto &value : state["items"].toArray()) {
        auto item = value.toObject().toVariantMap();
        item["active"] = value.toObject()["id"] == state["active"];
        const auto left = m_appearance.imagePath(value.toObject(), "left");
        const auto right = value.toObject()["shareLR"].toBool() ? left : m_appearance.imagePath(value.toObject(), "right");
        item["leftImageUrl"] = left.isEmpty() ? QUrl() : QUrl::fromLocalFile(left);
        item["rightImageUrl"] = right.isEmpty() ? QUrl() : QUrl::fromLocalFile(right);
        rows.append(item);
    }
    m_appearancesCache = rows;
    return *m_appearancesCache;
}
void SettingsBridge::invalidateCaches() {
    m_valuesCache.reset(); m_paletteCache.reset(); m_appearancesCache.reset();
}
void SettingsBridge::message(const QString &value) { m_status = value; emit statusChanged(); }
void SettingsBridge::resetDraft() {
    invalidateCaches();
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
    if (width < 414 || width > 32768 || height < 400 || height > 32768) return false;
    return m_store.setMany({{"windowWidth", width}, {"windowHeight", height}, {"windowMaximized", maximized}});
}
QVariantMap SettingsBridge::miniWindowPosition()const{return m_store.get("qml.miniPosition",QJsonObject()).toObject().toVariantMap();}
bool SettingsBridge::saveMiniWindowPosition(int x,int y){return m_store.set("qml.miniPosition",QJsonObject{{"x",x},{"y",y}});}
bool SettingsBridge::openDataRoot() { return QDesktopServices::openUrl(QUrl::fromLocalFile(dataRoot())); }

void SettingsBridge::updateFont() {
    for(const auto id:m_fontIds)QFontDatabase::removeApplicationFont(id);
    m_fontIds.clear();m_fontFamilies.clear();m_fontFamily.clear();
    const auto files=uiFontFiles(m_store.get("uiFont").toObject());
    for(const auto &region:uiFontRegions()) {
        const auto file=files.value(region).toString();
        if(!file.startsWith("Fonts/")||QDir::cleanPath(file)!=file)continue;
        const auto absolute = QFileInfo(m_library->absolute(file)).canonicalFilePath();
        const auto directory = QFileInfo(m_library->absolute("Fonts")).canonicalFilePath();
        if(directory.isEmpty()||!absolute.startsWith(directory+'/'))continue;
        if(!m_fontIds.contains(absolute))m_fontIds.insert(absolute,QFontDatabase::addApplicationFont(absolute));
        const auto id=m_fontIds.value(absolute);
        if(id>=0)m_fontFamilies.insert(region,QFontDatabase::applicationFontFamilies(id).value(0));
    }
    m_fontFamily=m_fontFamilies.value("rest",m_fontFamilies.value("songs"));
}
QVariantMap SettingsBridge::fontFiles() const {return uiFontFiles(m_store.get("uiFont").toObject()).toVariantMap();}
QString SettingsBridge::fontFamilyFor(const QString &region) const {
    return m_fontFamilies.value(region);
}
bool SettingsBridge::importFont(const QUrl &source,QString &relative,bool uniqueName) {
    const QFileInfo info(source.toLocalFile());
    const auto suffix=info.suffix().toLower();
    if(!source.isLocalFile()||!info.isFile()||info.size()>50*1024*1024||
       (suffix!="ttf"&&suffix!="otf"&&suffix!="ttc")){message("请选择有效的 TTF、OTF 或 TTC 字体文件");return false;}
    QFile input(info.absoluteFilePath());
    if(!input.open(QIODevice::ReadOnly)){message("字体文件打不开");return false;}
    const auto bytes=input.readAll();input.close();
    const auto check=QFontDatabase::addApplicationFontFromData(bytes);
    if(check<0){message("字体文件无法加载");return false;}
    QFontDatabase::removeApplicationFont(check);
    relative="Fonts/"+(uniqueName?info.completeBaseName()+"-"+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex().left(12))+"."+suffix:info.fileName());
    if(!QDir().mkpath(m_library->absolute("Fonts"))){message("无法创建字体目录");return false;}
    QSaveFile output(m_library->absolute(relative));
    if(!output.open(QIODevice::WriteOnly)||output.write(bytes)!=bytes.size()||!output.commit()){message("字体文件复制失败");return false;}
    return true;
}
bool SettingsBridge::saveFont(const QUrl &source, const QVariantMap &regions) {
    QJsonObject font;
    for (const auto &key : {"songs", "lyrics", "footer", "next", "rest"}) {
        const auto value = QJsonValue::fromVariant(regions.value(key));
        if (!value.isBool()) { message("请选择字体应用范围"); return false; }
        font[key] = value;
    }
    QString relative = m_store.get("uiFont").toObject()["file"].toString();
    if (!source.isEmpty()&&!importFont(source,relative,false))return false;
    font["file"] = relative;
    if (!m_store.set("uiFont", font)) { message(m_library->lastError()); return false; }
    updateFont(); emit settingsChanged(); message("字体已保存"); return true;
}
bool SettingsBridge::saveFonts(const QVariantMap &files) {
    const auto previous=uiFontFiles(m_store.get("uiFont").toObject());
    QJsonObject next;
    for(const auto &region:uiFontRegions()) {
        if(!files.contains(region)||!files.value(region).canConvert<QString>()){message("请为每个板块选择字体或系统默认");return false;}
        auto file=files.value(region).toString();
        if(!file.isEmpty()&&file!=previous.value(region).toString()) {
            if(!importFont(QUrl(file),file,true))return false;
        }
        next[region]=file;
    }
    auto configuration=m_store.get("uiFont").toObject();configuration["files"]=next;
    if(!m_store.set("uiFont",configuration)){message(m_library->lastError());return false;}
    updateFont();emit settingsChanged();message("字体已保存");return true;
}
bool SettingsBridge::resetFont() {
    if (!m_store.set("uiFont", QJsonObject())) return false;
    updateFont(); emit settingsChanged(); message("已恢复系统字体"); return true;
}
QVariantList SettingsBridge::shortcutActions() const {
    QVariantList result; const auto keys = ShortcutRules::actionKeys(), names = ShortcutRules::actionNames();
    for (int i = 0; i < keys.size(); ++i) result.append(QVariantMap{{"key", keys[i]}, {"name", names[i]}, {"appOnly",false}});
    result.append(QVariantMap{{"key","miniPlayer"},{"name","迷你播放器"},{"appOnly",false}});
    return result;
}
QString SettingsBridge::shortcutText(int key, int modifiers) const {
    return QKeySequence(QKeyCombination(Qt::KeyboardModifiers(modifiers), Qt::Key(key))).toString(QKeySequence::PortableText);
}
QVariantMap SettingsBridge::defaultShortcuts() const { return ShortcutRules::defaults().toVariantMap(); }
bool SettingsBridge::saveShortcuts(const QVariantMap &configuration) {
    const auto candidate = QJsonObject::fromVariantMap(configuration); QString error;
    if (!candidate["globalEnabled"].isBool() || !ShortcutRules::validate(candidate, &error)) { message(error.isEmpty() ? "快捷键配置无效" : error); return false; }
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
    const QStringList blurs{"blurLeft", "blurRight", "blurFooter", "blurExpanded"};
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
    if(!colors.contains(key))return false;
    auto *dialog=createColorDialog(m_appearanceDraft[key].toString(),ownerWindow,"appearanceFullColorDialog");
    if(!dialog)return false;
    const auto session=m_appearanceSession;
    connect(dialog,&QColorDialog::colorSelected,this,[this,key,session](const QColor &color){
        if(session==m_appearanceSession&&color.isValid())editAppearance(key,color.name());
    });
    dialog->open();return true;
}
bool SettingsBridge::chooseLyricColor(QObject *ownerWindow) {
    const auto saved=m_store.get("lyricColor").toString();
    auto *dialog=createColorDialog(saved.isEmpty()?palette().value("text").toString():saved,ownerWindow,"lyricFullColorDialog");
    if(!dialog)return false;
    dialog->setWindowTitle("歌词颜色");
    connect(dialog,&QColorDialog::colorSelected,this,[this](const QColor &color){if(color.isValid())setPreference("lyricColor",color.name());});
    dialog->open();return true;
}
bool SettingsBridge::chooseLyricColorDraft(const QString &color,QObject *ownerWindow) {
    auto *dialog=createColorDialog(color,ownerWindow,"lyricDraftColorDialog");
    if(!dialog)return false;
    dialog->setWindowTitle("歌词颜色");
    connect(dialog,&QColorDialog::colorSelected,this,[this](const QColor &chosen){
        if(chosen.isValid())emit lyricColorChosen(chosen.name());
    });
    dialog->open();return true;
}
QColorDialog *SettingsBridge::createColorDialog(const QString &color,QObject *ownerWindow,const QString &name) {
    auto *owner = qobject_cast<QWindow *>(ownerWindow);
    if (!owner || !qobject_cast<QApplication *>(QCoreApplication::instance())) return nullptr;
    if (m_colorDialog) { m_colorDialog->raise(); m_colorDialog->activateWindow(); return nullptr; }
    auto *dialog = new QColorDialog(QColor(color));
    m_colorDialog = dialog;
    dialog->setObjectName(name);
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
    // Keep all of Qt's color tools, styled like the QML controls.
    dialog->setStyleSheet(QStringLiteral(
        "QColorDialog { background:%1; color:%2; }"
        "QLabel { color:%2; }"
        "QPushButton { background:%3; color:%2; border:1px solid %4; border-radius:8px; padding:6px 12px; min-height:22px; }"
        "QPushButton:hover { background:%5; }"
        "QPushButton:focus, QPushButton:default { border-color:%6; }"
        "QLineEdit, QSpinBox { background:%3; color:%2; border:1px solid %4; border-radius:6px; padding:3px 6px; min-height:24px; selection-background-color:%6; }"
        "QLineEdit:focus, QSpinBox:focus { border-color:%6; }"
        "QSpinBox { padding-right:22px; }"
        "QSpinBox::up-button { subcontrol-origin:border; subcontrol-position:top right; width:20px; border-top-right-radius:6px; }"
        "QSpinBox::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:20px; border-bottom-right-radius:6px; }"
        "QSpinBox::up-arrow { image:url(:/qml/controls/chevron-up.svg); width:10px; height:6px; }"
        "QSpinBox::down-arrow { image:url(:/qml/controls/chevron-down.svg); width:10px; height:6px; }"
        "QMenu, QToolTip { background:%1; color:%2; border:1px solid %4; border-radius:8px; padding:6px; }"
        "QMenu::item { padding:6px 18px; border-radius:6px; }"
        "QMenu::item:selected { background:%3; }"
        "QMenu::item:disabled { color:%7; }")
        .arg(look["background"].toString(),look["text"].toString(),look["card"].toString(),
             look["border"].toString(),look["panel"].toString(),look["accent"].toString(),look["secondary"].toString()));
    for(auto *surface:dialog->findChildren<QWidget *>()) {
        const auto type=QByteArray(surface->metaObject()->className());
        if(type.endsWith("QColorPicker")||type.endsWith("QColorLuminancePicker")||type.endsWith("QColorShowLabel"))
            new ColorSurfaceCorners(surface);
    }
    for(auto *label:dialog->findChildren<QLabel *>())if(auto *well=label->buddy()) {
        if(label->text()==QCoreApplication::translate("QColorDialog","&Basic colors"))
            new ColorWellCorners(well,6,QColor(look["border"].toString()));
        else if(label->text()==QCoreApplication::translate("QColorDialog","&Custom colors"))
            new ColorWellCorners(well,QColorDialog::customCount()/8,QColor(look["border"].toString()));
    }
    dialog->winId();
    dialog->windowHandle()->setTransientParent(owner);
    connect(dialog, &QDialog::finished, this, [this, dialog] {
        if (m_colorDialog == dialog) m_colorDialog = nullptr;
    });
    return dialog;
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
