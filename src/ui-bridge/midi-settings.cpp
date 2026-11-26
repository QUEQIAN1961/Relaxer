#include "settings-bridge.h"
#include "fluid-midi-backend.h"
#include "midi-synthesis-options.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
namespace relaxer {
namespace {
bool samePath(const QString &a,const QString &b){
    if(a.isEmpty()||b.isEmpty())return a==b;
#ifdef Q_OS_WIN
    constexpr auto sensitivity=Qt::CaseInsensitive;
#else
    constexpr auto sensitivity=Qt::CaseSensitive;
#endif
    return QDir::cleanPath(QFileInfo(a).absoluteFilePath()).compare(QDir::cleanPath(QFileInfo(b).absoluteFilePath()),sensitivity)==0;
}
QString fontPath(const QString &root,const QJsonObject &entry){
    const auto id=entry.value("id").toString(),file=entry.value("file").toString();
    static const QRegularExpression digest("^[0-9a-f]{64}\\.sf[23]$");
    if(!digest.match(id).hasMatch()||file!=id)return {};
    return QDir(root).filePath("soundfonts/"+file);
}
struct ImportedFont{QString error,path;QJsonObject entry;bool created=false;};
ImportedFont importFontCopy(const QString &source,const QString &root){
    ImportedFont result;
    const auto ext=QFileInfo(source).suffix().toLower();
    if(ext!="sf2"&&ext!="sf3"){result.error="请选择 SF2 或 SF3 文件；暂不支持 SF2PACK 和 SFZ";return result;}
    const auto directory=QDir(root).filePath("soundfonts");
    if(QFileInfo(directory).isSymLink()||!QDir().mkpath(directory)){result.error="无法创建音色库目录";return result;}
    QFile input(source);if(!input.open(QIODevice::ReadOnly)){result.error=input.errorString();return result;}
    const auto staged=QDir(directory).filePath(".import-"+QUuid::createUuid().toString(QUuid::Id128)+"."+ext);
    struct Cleanup{QString path;~Cleanup(){QFile::remove(path);}}cleanup{staged};
    QSaveFile output(staged);if(!output.open(QIODevice::WriteOnly)){result.error=output.errorString();return result;}
    QCryptographicHash hash(QCryptographicHash::Sha256);qint64 size=0;
    while(!input.atEnd()){
        const auto chunk=input.read(1024*1024);
        if(chunk.isEmpty()&&input.error()!=QFileDevice::NoError){result.error=input.errorString();return result;}
        if(output.write(chunk)!=chunk.size()){result.error=output.errorString();return result;}
        hash.addData(chunk);size+=chunk.size();
    }
    input.close();if(!output.commit()){result.error=output.errorString();return result;}
    result.error=FluidMidiBackend::validateSoundFont(staged);if(!result.error.isEmpty())return result;
    const QString id=QString::fromLatin1(hash.result().toHex()),file=id+"."+ext;
    result.path=QDir(directory).filePath(file);
    if(QFileInfo(result.path).exists()||QFileInfo(result.path).isSymLink()){
        QFile existing(result.path);QCryptographicHash check(QCryptographicHash::Sha256);
        if(QFileInfo(result.path).isSymLink()||!existing.open(QIODevice::ReadOnly)||!check.addData(&existing)||QString::fromLatin1(check.result().toHex())!=id){result.error="已导入的副本已损坏，请先移除后重新导入";return result;}
    }else if(!QFile::rename(staged,result.path)){result.error="无法保存音色库副本";return result;}
    else result.created=true;
    result.entry={{"id",file},{"file",file},{"name",QFileInfo(source).fileName()},{"format",ext.toUpper()},{"size",double(size)}};
    return result;
}
}
QVariantList SettingsBridge::midiSoundFonts()const{
    const auto selected=m_store.get("midiSoundFont",QString()).toString();QVariantList result;
    result.append(QVariantMap{{"id",""},{"name","GeneralUser GS（内置）"},{"format","SF2"},{"size",double(QFileInfo(QCoreApplication::applicationDirPath()+"/midi/GeneralUser-GS.sf2").size())},{"selected",selected.isEmpty()},{"active",m_midiSoundFontAtStartup.isEmpty()},{"removable",false},{"available",true},{"path",""}});
    bool selectedListed=selected.isEmpty();
    for(const auto &value:m_store.get("midiSoundFonts",QJsonArray()).toArray()){
        const auto entry=value.toObject();const auto path=fontPath(m_library->root(),entry);if(path.isEmpty())continue;
        auto row=entry.toVariantMap();row["path"]=path;row["selected"]=samePath(path,selected);row["active"]=samePath(path,m_midiSoundFontAtStartup);row["removable"]=!row["active"].toBool();row["available"]=QFileInfo(path).isFile()&&!QFileInfo(path).isSymLink();
        selectedListed|=row["selected"].toBool();result.append(row);
    }
    if(!selectedListed)result.append(QVariantMap{{"id","external"},{"name",QFileInfo(selected).fileName()+"（原有外部文件）"},{"format",QFileInfo(selected).suffix().toUpper()},{"size",double(QFileInfo(selected).size())},{"selected",true},{"active",samePath(selected,m_midiSoundFontAtStartup)},{"removable",false},{"available",QFileInfo(selected).isFile()},{"path",selected}});
    return result;
}
bool SettingsBridge::selectMidiSoundFont(const QUrl &source){
    if(m_midiSoundFontBusy)return false;
    if(!source.isLocalFile()){m_midiSoundFontStatus="请选择本地 SF2 或 SF3 文件";emit midiSoundFontChanged();return false;}
    m_midiSoundFontBusy=true;m_midiSoundFontStatus="正在复制并校验音色库…";emit midiSoundFontChanged();
    const auto path=QFileInfo(source.toLocalFile()).absoluteFilePath(),root=m_library->root();
    m_midiSoundFontPool.start([this,path,root]{
        const auto imported=importFontCopy(path,root);
        auto discard=std::shared_ptr<QString>(new QString(imported.created?imported.path:QString()),[](QString *path){if(!path->isEmpty())QFile::remove(*path);delete path;});
        QMetaObject::invokeMethod(this,[this,imported,discard]{
            m_midiSoundFontBusy=false;
            if(!imported.error.isEmpty())m_midiSoundFontStatus="音色库未更改："+imported.error;
            else{
                auto list=m_store.get("midiSoundFonts",QJsonArray()).toArray();bool found=false;
                for(const auto &row:list)if(row.toObject().value("id")==imported.entry.value("id"))found=true;
                if(!found)list.append(imported.entry);
                if(!m_store.setMany({{"midiSoundFonts",list},{"midiSoundFont",imported.path}})){
                    m_midiSoundFontStatus="音色库设置未保存："+m_library->lastError();
                }else{discard->clear();invalidateCaches();emit settingsChanged();m_midiSoundFontStatus="已导入并选择，下次启动生效";}
            }
            emit midiSoundFontChanged();
        },Qt::QueuedConnection);
    });return true;
}
bool SettingsBridge::useMidiSoundFont(const QString &id){
    if(m_midiSoundFontBusy)return false;
    if(id.isEmpty())return resetMidiSoundFont();
    for(const auto &row:midiSoundFonts()){
        const auto entry=row.toMap();if(entry.value("id").toString()!=id)continue;
        const auto path=entry.value("path").toString();
        if(id=="external")return selectMidiSoundFont(QUrl::fromLocalFile(path));
        m_midiSoundFontBusy=true;m_midiSoundFontStatus="正在校验音色库…";emit midiSoundFontChanged();
        m_midiSoundFontPool.start([this,path]{
            const auto error=FluidMidiBackend::validateSoundFont(path);
            QMetaObject::invokeMethod(this,[this,path,error]{
                m_midiSoundFontBusy=false;
                if(!error.isEmpty())m_midiSoundFontStatus="音色库未更改："+error;
                else if(!m_store.set("midiSoundFont",path))m_midiSoundFontStatus="设置未保存："+m_library->lastError();
                else{invalidateCaches();emit settingsChanged();m_midiSoundFontStatus="已选择，下次启动生效";}
                emit midiSoundFontChanged();
            },Qt::QueuedConnection);
        });return true;
    }
    m_midiSoundFontStatus="音色库不存在";emit midiSoundFontChanged();return false;
}
bool SettingsBridge::resetMidiSoundFont(){
    if(m_midiSoundFontBusy)return false;
    if(!m_store.set("midiSoundFont",QString())){m_midiSoundFontStatus="设置未保存："+m_library->lastError();emit midiSoundFontChanged();return false;}
    invalidateCaches();emit settingsChanged();m_midiSoundFontStatus=midiSoundFontRestartRequired()?"已选择内置音色库，下次启动生效":"已选择内置音色库";emit midiSoundFontChanged();return true;
}
bool SettingsBridge::removeMidiSoundFont(const QString &id){
    if(m_midiSoundFontBusy)return false;
    const auto list=m_store.get("midiSoundFonts",QJsonArray()).toArray();QJsonArray next;QString path;
    for(const auto &row:list){if(row.toObject().value("id").toString()==id)path=fontPath(m_library->root(),row.toObject());else next.append(row);}
    if(path.isEmpty())return false;
    if(samePath(path,m_midiSoundFontAtStartup)){m_midiSoundFontStatus="本次启动正在使用此音色库，请切换并重启后再移除";emit midiSoundFontChanged();return false;}
    if(QFileInfo(QDir(m_library->root()).filePath("soundfonts")).isSymLink()||QFileInfo(path).isSymLink()){m_midiSoundFontStatus="音色库目录或副本已改变，未删除文件";emit midiSoundFontChanged();return false;}
    QJsonObject settings{{"midiSoundFonts",next}};
    if(samePath(path,m_store.get("midiSoundFont",QString()).toString()))settings["midiSoundFont"]=QString();
    if(!m_store.setMany(settings)){m_midiSoundFontStatus="设置未保存："+m_library->lastError();emit midiSoundFontChanged();return false;}
    const bool removed=!QFileInfo::exists(path)||QFile::remove(path);
    invalidateCaches();emit settingsChanged();m_midiSoundFontStatus=removed?"已移除应用副本，原文件保留":"列表已移除，但副本文件未能删除";emit midiSoundFontChanged();return true;
}
QVariantMap SettingsBridge::midiSynthesis()const{return normalizedMidiSynthesis(m_store.get("midiSynthesis",QJsonObject()).toObject()).toVariantMap();}
bool SettingsBridge::midiSynthesisRestartRequired()const{return QJsonObject::fromVariantMap(midiSynthesis())!=m_midiSynthesisAtStartup;}
bool SettingsBridge::setMidiSynthesis(const QString &key,const QVariant &value){
    auto options=QJsonObject::fromVariantMap(midiSynthesis());
    if(!options.contains(key))return false;
    options[key]=QJsonValue::fromVariant(value);
    if(normalizedMidiSynthesis(options)!=options){m_midiSoundFontStatus="MIDI 声音参数超出有效范围";emit midiSoundFontChanged();return false;}
    if(!m_store.set("midiSynthesis",options)){m_midiSoundFontStatus="设置未保存："+m_library->lastError();emit midiSoundFontChanged();return false;}
    invalidateCaches();emit settingsChanged();return true;
}
bool SettingsBridge::resetMidiSynthesis(){
    if(!m_store.set("midiSynthesis",midiSynthesisDefaults())){m_midiSoundFontStatus="设置未保存："+m_library->lastError();emit midiSoundFontChanged();return false;}
    invalidateCaches();emit settingsChanged();return true;
}
}
