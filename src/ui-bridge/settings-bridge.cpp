#include "settings-bridge.h"
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
namespace relaxer {
SettingsBridge::SettingsBridge(Library *library,QObject *parent):QObject(parent),m_library(library),m_store(library),m_appearance(library){
    resetDraft();
    if(auto *app=qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        connect(app,&QGuiApplication::paletteChanged,this,[this]{if(m_store.get("theme",0).toInt()==2)emit settingsChanged();});
}
QVariantMap SettingsBridge::values()const {
    return {{"theme",m_store.get("theme",0).toInt()},{"threshold",m_store.get("threshold",80).toInt()},
        {"showLyricTranslation",m_store.get("showLyricTranslation",true).toBool()},
        {"playerCoverBackground",m_store.get("playerCoverBackground",true).toBool()},
        {"lyricColor",m_store.get("lyricColor",QString()).toString()},
        {"reduceMotion",m_store.get("qml.reduceMotion",false).toBool()}};
}
QVariantMap SettingsBridge::palette()const {
    const int theme=m_store.get("theme",0).toInt();const auto appearance=m_appearance.active();
    const bool custom=theme==3;
    const bool dark=theme==1||(theme==2&&QGuiApplication::palette().color(QPalette::Window).lightness()<128)||
        (custom&&QColor(appearance["right"].toString()).lightness()<128);
    const auto ink=appearance["customText"].toBool()?appearance["rightInk"].toString():dark?QString("#f2f2f5"):QString("#292b35");
    return {{"dark",dark},{"custom",custom},{"background",appearance["left"].toString()},
        {"panel",appearance["right"].toString()},{"accent",appearance["accent"].toString()},
        {"text",ink},{"secondary",appearance["customText"].toBool()?appearance["muted"].toString():dark?QString("#b0b5c2"):QString("#707683")}};
}
QVariantList SettingsBridge::appearances()const {
    QVariantList rows;for(const auto &value:m_appearance.state()["items"].toArray()){
        const auto item=value.toObject();rows.append(QVariantMap{{"id",item["id"].toString()},{"name",item["name"].toString()}});
    }return rows;
}
void SettingsBridge::message(const QString &value){m_status=value;emit statusChanged();}
void SettingsBridge::resetDraft(){m_draft=QJsonObject::fromVariantMap(values());emit draftChanged();message({});}
bool SettingsBridge::edit(const QString &key,const QVariant &value){
    if(!m_draft.contains(key)){message("未知设置项");return false;}
    m_draft[key]=QJsonValue::fromVariant(value);emit draftChanged();return true;
}
QString SettingsBridge::validate(const QJsonObject &v)const {
    for(const auto &key:{"theme","threshold"})if(!v[key].isDouble()||v[key].toDouble()!=v[key].toInt())return "设置需要整数";
    if(v["theme"].toInt()<0||v["theme"].toInt()>3)return "主题超出范围";
    if(v["threshold"].toInt()<70||v["threshold"].toInt()>100)return "播放统计阈值应为70%～100%";
    for(const auto &key:{"showLyricTranslation","playerCoverBackground","reduceMotion"})if(!v[key].isBool())return "设置值需要布尔类型";
    if(!v["lyricColor"].isString()||(!v["lyricColor"].toString().isEmpty()&&!QColor(v["lyricColor"].toString()).isValid()))return "歌词颜色无效";
    return {};
}
bool SettingsBridge::save(){
    const auto error=validate(m_draft);if(!error.isEmpty()){message(error);return false;}
    auto data=m_draft;data["qml.reduceMotion"]=data.take("reduceMotion");
    if(!m_store.setMany(data)){message(m_library->lastError());return false;}
    emit settingsChanged();message("设置已保存");return true;
}
bool SettingsBridge::setTheme(int theme){
    if(theme<0||theme>3)return false;
    if(!m_store.set("theme",theme)){message(m_library->lastError());return false;}
    m_draft["theme"]=theme;emit draftChanged();emit settingsChanged();return true;
}
bool SettingsBridge::useAppearance(const QString &id){
    if(!m_appearance.use(id)){message("外观不存在或保存失败");return false;}
    resetDraft();emit settingsChanged();message("已应用保存的外观");return true;
}
bool SettingsBridge::savePalette(const QString &name,const QString &background,const QString &panel,const QString &accent){
    if(name.trimmed().isEmpty()||!QColor(background).isValid()||!QColor(panel).isValid()||!QColor(accent).isValid()){message("请填写名称和有效颜色");return false;}
    auto value=AppearanceStore::defaults();value["name"]=name.trimmed();value["left"]=background;value["right"]=panel;value["accent"]=accent;
    QString error;if(!m_appearance.save(value,{},&error)){message(error);return false;}
    resetDraft();emit settingsChanged();message("配色已保存并应用");return true;
}
}
