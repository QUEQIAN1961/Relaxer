#pragma once
#include "library.h"
#include "disk-cache.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QVariantMap>

namespace relaxer {
// Only an explicit resource deletion/not-found response proves disappearance;
// login failures, timeouts and empty playlists do not.
inline bool playlistReportedMissing(const QJsonObject &response){
    for(const auto &key:{"message","msg","error_msg","errmsg"}){
        const auto message=response.value(key).toString().toLower();
        for(const auto &text:{"歌单不存在","歌单已删除","歌单已被删除","收藏夹不存在","收藏夹已删除","收藏夹已被删除","playlist not found","playlist has been deleted","folder not found"})
            if(message.contains(QString::fromUtf8(text)))return true;
    }
    return false;
}
// Catalog metadata only: never persist playback URLs or account credentials.
class OnlinePageCache {
public:
    enum Action { Cached, Fetch, Validate, Pending };
    OnlinePageCache(Library *library,const QString &provider):m_library(library),m_provider(provider){}
    ~OnlinePageCache(){savePinned();}
    Action begin(const QString &key,bool force,QVariantMap &snapshot){
        if(!force&&m_pending&&m_key==key)return Pending;
        if(m_pending&&m_quiet&&m_key!=key)m_seen.remove(m_key);
        m_key=key;
        if(!m_pages.contains(key)&&key.startsWith("playlist:")){
            QFile file(path(key));if(!file.exists())file.setFileName(QDir(m_library->root()).filePath("Online/"+QFileInfo(path(key)).fileName()));if(file.open(QIODevice::ReadOnly)){const auto bytes=file.readAll();const auto value=QJsonDocument::fromJson(bytes).object().toVariantMap();if(value.contains("songs")){m_pages.insert(key,value);m_savedHashes.insert(key,QCryptographicHash::hash(bytes,QCryptographicHash::Sha256));}}
        }
        if(!force&&m_pages.contains(key)){
            snapshot=m_pages.value(key);
            m_quiet=key.startsWith("playlist:")&&!m_seen.contains(key);m_before=m_quiet?snapshot:QVariantMap{};m_pending=m_quiet;m_seen.insert(key);
            return m_quiet?Validate:Cached;
        }
        m_seen.insert(key);m_pending=true;m_quiet=false;m_before.clear();return Fetch;
    }
    void finish(const QVariantMap &snapshot){m_pages.insert(m_key,snapshot);m_pending=false;m_quiet=false;m_before.clear();if(m_key.startsWith("playlist:"))savePinned(m_key);}
    void cancelPending(){if(m_pending&&m_quiet)m_seen.remove(m_key);m_pending=false;m_quiet=false;m_before.clear();}
    bool pending() const{return m_pending;}
    bool quiet() const{return m_quiet;}
    void cancelAI(){if(m_key=="ai"){m_pending=false;m_quiet=false;m_seen.remove(m_key);}}
    QVariantMap before() const{return m_before;}
    void clearPrivate(){m_pending=false;m_quiet=false;for(const auto &key:{QString("mine"),QString("daily"),QString("ai")}){m_pages.remove(key);m_seen.remove(key);}}
    void savePinned(const QString &onlyKey={}){
        for(const auto &playlist:m_library->playlists()){
            if(playlist.provider!=m_provider)continue;const auto key="playlist:"+playlist.remoteId;if(!onlyKey.isEmpty()&&key!=onlyKey)continue;
            if(!m_pages.contains(key)){
                QVariantMap snapshot;
                for(auto it=m_pages.cbegin();it!=m_pages.cend();++it){
                    const auto metadata=it.value().value("playlist").toMap();const auto kind=metadata.value("kind").toString();
                    if(metadata.value("id").toString()==playlist.remoteId&&(kind.isEmpty()||kind=="playlist")){snapshot=it.value();break;}
                }
                if(snapshot.isEmpty())continue;m_pages.insert(key,snapshot);m_seen.insert(key);
            }
            const auto bytes=QJsonDocument(QJsonObject::fromVariantMap(m_pages.value(key))).toJson(QJsonDocument::Compact);
            const auto hash=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);
            if(m_savedHashes.value(key)==hash&&QFile::exists(path(key)))continue;QSaveFile file(path(key));if(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&file.commit()){m_savedHashes.insert(key,hash);DiskCache::trim(m_library);}
        }
    }
private:
    QString path(const QString &key) const {return QDir(DiskCache::online(m_library)).filePath("catalog-"+m_provider+"-"+QCryptographicHash::hash(key.toUtf8(),QCryptographicHash::Sha256).toHex()+".json");}
    Library *m_library;QString m_provider,m_key;QHash<QString,QVariantMap> m_pages;QHash<QString,QByteArray> m_savedHashes;QSet<QString> m_seen;QVariantMap m_before;bool m_pending=false,m_quiet=false;
};
}
