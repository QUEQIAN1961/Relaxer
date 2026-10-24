#include "appearance-store.h"
#include "appearance-image.h"
#include "library.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
namespace relaxer {
QJsonObject AppearanceStore::defaults() {
    return {{"mode", "palette"},    {"left", "#f5f5f7"},     {"right", "#ffffff"}, {"accent", "#e5455f"},
            {"leftInk", "#25252b"}, {"rightInk", "#25252b"}, {"muted", "#86818a"}, {"customText", false},
            {"mutedBold", false},   {"shareLR", false},      {"blurLeft", 0},      {"blurRight", 0}, {"blurFooter", 0}, {"blurExpanded", 15}, {"customProgressColors", false},
            {"customFooter", false}, {"footer", "#f4f4f6"}, {"shareFooter", true}};
}
QJsonObject AppearanceStore::state() const {
    auto raw = m_settings.get("appearanceHistory").toObject();
    QJsonArray cleaned;
    QSet<QString> ids;
    for (auto item : raw["items"].toArray()) {
        auto theme = item.toObject();
        auto id = theme["id"].toString();
        if (id.isEmpty() || ids.contains(id) || (theme["mode"] != "palette" && theme["mode"] != "image"))
            continue;
        ids.insert(id);
        cleaned.append(theme);
    }
    return {{"items", cleaned}, {"active", raw["active"].toString()}};
}
QJsonObject AppearanceStore::active() const {
    auto current = state();
    for (auto item : current["items"].toArray())
        if (item.toObject()["id"] == current["active"])
            return item.toObject();
    return defaults();
}
QString AppearanceStore::imagePath(const QJsonObject &theme, const QString &region) const {
    auto key = region == "left" ? "imageLeft" : region == "footer" ? "imageFooter" : "imageRight";
    auto rel = theme[key].toString();
    if (!rel.startsWith("Appearance/") || QDir::cleanPath(rel) != rel)
        return {};
    auto path = QFileInfo(m_library->absolute(rel)).canonicalFilePath();
    auto directory = QFileInfo(m_library->absolute("Appearance")).canonicalFilePath();
    return !directory.isEmpty() && path.startsWith(directory + '/') ? path : QString();
}
QJsonObject AppearanceStore::draft(QJsonObject theme) const {
    if (!theme.contains("blurFooter")) theme["blurFooter"] = theme["blurRight"].toDouble();
    auto defaults = AppearanceStore::defaults();
    for (auto it = defaults.begin(); it != defaults.end(); ++it)
        if (!theme.contains(it.key()))
            theme[it.key()] = it.value();
    theme["sourceLeft"] = imagePath(theme, "left");
    theme["sourceRight"] = imagePath(theme, "right");
    theme["sourceFooter"] = imagePath(theme, "footer");
    return theme;
}
QJsonObject AppearanceStore::withProgressColors(QJsonObject theme, bool refreshRecommendation) const {
    if (!theme.contains("progressColorsCustomized")) theme["progressColorsCustomized"]=theme["customProgressColors"].toBool();
    const auto valid = [&theme](const char *key) { return QColor(theme[key].toString()).isValid(); };
    if (refreshRecommendation || !valid("recommendedProgressFill") || !valid("recommendedProgressTrack")) {
        QColor background(theme["right"].toString());
        if (!background.isValid()) background = QColor("#ffffff");
        if (theme["mode"] == "image") {
            const QString region = theme["shareLR"].toBool() ? "Left" : "Right";
            auto path = theme["source" + region].toString();
            if (path.isEmpty()) path = imagePath(theme, region.toLower());
            const auto image = backgroundImage(path, 32).convertToFormat(QImage::Format_ARGB32);
            if (!image.isNull()) {
                double r=0,g=0,b=0;
                for (int y=0;y<image.height();++y) for (int x=0;x<image.width();++x) {
                    const auto pixel=image.pixelColor(x,y);const auto a=pixel.alphaF();
                    r+=pixel.red()*a+background.red()*(1-a);
                    g+=pixel.green()*a+background.green()*(1-a);
                    b+=pixel.blue()*a+background.blue()*(1-a);
                }
                const int count=image.width()*image.height();
                background=QColor(qRound(r/count),qRound(g/count),qRound(b/count));
            }
        }
        // The footer uses the right background's derived card color in both playback modes.
        const bool dark=autoInk(background).lightness()>128;
        background=dark?background.lighter(115):background.darker(103);
        const auto ink=autoInk(background);
        const auto mix=[&](double amount) {
            return QColor(qRound(ink.red()*amount+background.red()*(1-amount)),
                          qRound(ink.green()*amount+background.green()*(1-amount)),
                          qRound(ink.blue()*amount+background.blue()*(1-amount))).name();
        };
        theme["recommendedProgressFill"]=mix(.72);
        theme["recommendedProgressTrack"]=mix(.25);
    }
    if (!theme.contains("customProgressColors")) theme["customProgressColors"]=false;
    if (!theme["progressColorsCustomized"].toBool() || !valid("progressFill")) theme["progressFill"]=theme.value("recommendedProgressFill");
    if (!theme["progressColorsCustomized"].toBool() || !valid("progressTrack")) theme["progressTrack"]=theme.value("recommendedProgressTrack");
    return theme;
}
bool AppearanceStore::initializeProgressColors() {
    auto current=state();auto items=current["items"].toArray();bool changed=false;
    for (int i=0;i<items.size();++i) {
        const auto old=items[i].toObject(),updated=withProgressColors(old);
        if (old!=updated) { items[i]=updated;changed=true; }
    }
    if (!changed) return true;
    current["items"]=items;
    return m_settings.set("appearanceHistory",current);
}
bool AppearanceStore::save(QJsonObject theme, const QString &replaceId, QString *error) {
    QStringList created;
    auto fail = [&](const QString &message) {
        for (auto path : created)
            QFile::remove(path);
        if (error)
            *error = message;
        return false;
    };
    if (theme["mode"] != "palette" && theme["mode"] != "image")
        return fail("请选择调色盘或图片模式。");
    for (const auto &key : {"left", "right", "accent", "leftInk", "rightInk", "muted"}) {
        QColor color(theme[key].toString());
        if (!color.isValid())
            return fail("颜色无效，请重新选择。");
        theme[key] = color.name();
    }
    if (theme["mode"] == "palette" && theme["customFooter"].toBool()) {
        QColor footer(theme["footer"].toString());
        if (!footer.isValid())
            return fail("底栏颜色无效，请重新选择。");
        theme["footer"] = footer.name();
    }
    auto current = state();
    auto items = current["items"].toArray();
    int replace = -1;
    for (int i = 0; i < items.size(); ++i)
        if (items[i].toObject()["id"] == replaceId)
            replace = i;
    if (!replaceId.isEmpty() && replace < 0)
        return fail("要覆盖的主题已不存在。");
    theme["id"] = replace < 0 ? QUuid::createUuid().toString(QUuid::WithoutBraces) : replaceId;
    theme["name"] =
        replace < 0 ? QString("主题 %1").arg(items.size() + 1) : items[replace].toObject()["name"].toString();
    if (theme["mode"] == "image") {
        QMap<QString, QString> reused;
        QStringList regions{"Left", "Right"};
        if (!theme["shareFooter"].toBool())
            regions << "Footer";
        for (const auto &region : regions) {
            auto source =
                theme["source" + (region == "Right" && theme["shareLR"].toBool() ? QString("Left") : region)]
                    .toString();
            if (source.isEmpty())
                return fail(region == "Footer" ? "请选择底栏背景图片。" : "请选择左侧和右侧背景图片。");
            auto image = backgroundImage(source);
            if (image.isNull())
                return fail("背景图片打不开或尺寸过大：" + source);
            QString rel = reused.value(source);
            if (rel.isEmpty()) {
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                if (!image.save(&buffer, "PNG"))
                    return fail("背景图片编码失败。");
                rel =
                    "Appearance/" +
                    QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) +
                    ".png";
                if (!QDir().mkpath(m_library->absolute("Appearance")))
                    return fail("无法创建背景图片目录。");
                auto destination = m_library->absolute(rel);
                if (!QFile::exists(destination)) {
                    QSaveFile file(destination);
                    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
                        !file.commit())
                        return fail("背景图片保存失败。");
                    created << destination;
                }
                reused[source] = rel;
            }
            theme["image" + region] = rel;
        }
    } else {
        theme.remove("imageLeft");
        theme.remove("imageRight");
        theme.remove("imageFooter");
    }
    theme=withProgressColors(theme,true);
    theme.remove("sourceLeft");
    theme.remove("sourceRight");
    theme.remove("sourceFooter");
    theme["blurLeft"] = qBound(0., theme["blurLeft"].toDouble(), 50.);
    theme["blurRight"] = qBound(0., theme["blurRight"].toDouble(), 50.);
    theme["blurFooter"] = qBound(0., theme["blurFooter"].toDouble(theme["blurRight"].toDouble()), 50.);
    theme["blurExpanded"] = qBound(0., theme["blurExpanded"].toDouble(15), 50.);
    if (replace < 0)
        items.append(theme);
    else
        items[replace] = theme;
    current["items"] = items;
    current["active"] = theme["id"];
    if (!m_settings.setMany({{"appearanceHistory", current}, {"theme", 3}}))
        return fail(m_library->lastError());
    return true;
}
bool AppearanceStore::use(const QString &id) {
    auto current = state();
    for (auto theme : current["items"].toArray())
        if (theme.toObject()["id"] == id) {
            current["active"] = id;
            return m_settings.setMany({{"appearanceHistory", current}, {"theme", 3}});
        }
    return false;
}
bool AppearanceStore::rename(const QString &id, const QString &name) {
    if (name.trimmed().isEmpty())
        return false;
    auto current = state();
    auto items = current["items"].toArray();
    for (int i = 0; i < items.size(); ++i)
        if (items[i].toObject()["id"] == id) {
            auto theme = items[i].toObject();
            theme["name"] = name.trimmed().left(80);
            items[i] = theme;
            current["items"] = items;
            return m_settings.set("appearanceHistory", current);
        }
    return false;
}
bool AppearanceStore::remove(const QString &id) {
    auto current = state();
    QJsonArray items;
    bool found = false;
    for (auto theme : current["items"].toArray()) {
        if (theme.toObject()["id"] == id)
            found = true;
        else
            items.append(theme);
    }
    if (!found)
        return false;
    current["items"] = items;
    QJsonObject changes{{"appearanceHistory", current}};
    if (current["active"] == id) {
        current["active"] = "";
        changes["appearanceHistory"] = current;
        changes["theme"] = 0;
    }
    // Keep unreferenced image files; removing a theme never deletes originals.
    return m_settings.setMany(changes);
}
} // namespace relaxer
