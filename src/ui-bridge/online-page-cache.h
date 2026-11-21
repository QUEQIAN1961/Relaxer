#pragma once
#include "library.h"
#include "disk-cache.h"
#include "async-json-store.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QVariantMap>
#include <limits>

namespace relaxer {
inline bool playlistReportedMissing(const QJsonObject &response){
    for(const auto &key:{"message","msg","error_msg","errmsg"}){
        const auto message=response.value(key).toString().toLower();
        for(const auto &text:{"歌单不存在","歌单已删除","歌单已被删除","收藏夹不存在","收藏夹已删除","收藏夹已被删除","playlist not found","playlist has been deleted","folder not found"})
            if(message.contains(QString::fromUtf8(text)))return true;
    }
    return false;
}
// Catalog metadata only. Session spill files contain no credentials or media
// URLs and are never restored by another run. Eviction must not refresh online.
class OnlinePageCache {
public:
    enum Action { Cached, Fetch, Validate, Pending };
    OnlinePageCache(Library *library,const QString &provider)
        :m_library(library),m_provider(provider),m_session(std::make_shared<QTemporaryDir>(QDir(DiskCache::online(library)).filePath("session-"+provider+"-XXXXXX"))){}
    ~OnlinePageCache(){savePinned();m_store.drain();}
    Action begin(const QString &key,bool force,QVariantMap &snapshot){
        if(!force&&m_pending&&m_key==key)return Pending;
        if(m_pending&&m_quiet&&m_key!=key)m_seen.remove(m_key);
        m_key=key;
        if(!force&&!m_pages.contains(key)&&m_backed.contains(key)){
            const auto value=read(sessionPath(key));if(!value.isEmpty())remember(key,value,false);
        }
        if(!m_pages.contains(key)&&key.startsWith("playlist:")){
            QFile file(path(key));if(!file.exists())file.setFileName(QDir(m_library->root()).filePath("Online/"+QFileInfo(path(key)).fileName()));
            if(file.open(QIODevice::ReadOnly)){
                const auto bytes=file.readAll();const auto value=QJsonDocument::fromJson(bytes).object().toVariantMap();
                if(value.contains("songs")){remember(key,value);m_savedHashes.insert(key,QCryptographicHash::hash(bytes,QCryptographicHash::Sha256));}
            }
        }
        if(!force&&m_pages.contains(key)){
            snapshot=m_pages.value(key);m_used[key]=++m_clock;
            m_quiet=key.startsWith("playlist:")&&!m_seen.contains(key);m_before=m_quiet?snapshot:QVariantMap{};
            m_pending=m_quiet;m_seen.insert(key);trim();return m_quiet?Validate:Cached;
        }
        m_seen.insert(key);m_pending=true;m_quiet=false;m_before.clear();return Fetch;
    }
    void finish(const QVariantMap &snapshot){remember(m_key,snapshot);m_pending=false;m_quiet=false;m_before.clear();if(m_key.startsWith("playlist:"))savePinned(m_key);trim();}
    void cancelPending(){if(m_pending&&m_quiet)m_seen.remove(m_key);m_pending=false;m_quiet=false;m_before.clear();}
    bool pending()const{return m_pending;}
    bool quiet()const{return m_quiet;}
    void cancelAI(){if(m_key=="ai"){m_pending=false;m_quiet=false;m_before.clear();m_seen.remove(m_key);}}
    QVariantMap before()const{return m_before;}
    void clearPrivate(){
        // Login/logout invalidates all account-bound session views. Playback's
        // metadata and queue are owned independently and remain untouched.
        m_pending=false;m_quiet=false;m_before.clear();m_pages.clear();m_seen.clear();m_used.clear();m_costs.clear();m_totalCost=0;
        ++m_generation;m_store.cancel();m_backed.clear();m_versions.clear();m_pinnedVersions.clear();m_savedHashes.clear();
        m_session=std::make_shared<QTemporaryDir>(QDir(DiskCache::online(m_library)).filePath("session-"+m_provider+"-XXXXXX"));
    }
    void savePinned(const QString &onlyKey={}){
        for(const auto &playlist:m_library->playlists()){
            if(playlist.provider!=m_provider)continue;const auto key="playlist:"+playlist.remoteId;
            if(!onlyKey.isEmpty()&&key!=onlyKey)continue;
            auto snapshot=m_pages.value(key);if(snapshot.isEmpty()&&m_backed.contains(key))snapshot=read(sessionPath(key));
            if(snapshot.isEmpty()){
                for(auto it=m_pages.cbegin();it!=m_pages.cend();++it){
                    const auto metadata=it.value().value("playlist").toMap();const auto kind=metadata.value("kind").toString();
                    if(metadata.value("id").toString()==playlist.remoteId&&(kind.isEmpty()||kind=="playlist")){snapshot=it.value();break;}
                }
                if(!snapshot.isEmpty()){remember(key,snapshot);m_seen.insert(key);trim();}
            }
            if(snapshot.isEmpty())continue;
            const auto version=m_versions.value(key);if(m_pinnedVersions.value(key,~quint64(0))==version)continue;
            const auto generation=m_generation;
            m_store.save(path(key),snapshot,{},[this,key,version,generation](bool ok,qint64,QByteArray hash){
                if(ok&&generation==m_generation&&m_versions.value(key)==version){m_pinnedVersions[key]=version;m_savedHashes[key]=hash;DiskCache::trim(m_library);}
            });
        }
    }
private:
    static QVariantMap read(const QString &path){QFile file(path);return file.open(QIODevice::ReadOnly)?QJsonDocument::fromJson(file.readAll()).object().toVariantMap():QVariantMap{};}
    void remember(const QString &key,const QVariantMap &snapshot,bool store=true){
        m_totalCost-=m_costs.value(key);m_costs[key]=variantMemoryCost(snapshot);m_totalCost+=m_costs.value(key);
        m_pages.insert(key,snapshot);m_used[key]=++m_clock;
        if(store){
            const auto version=++m_versions[key],generation=m_generation;m_backed.remove(key);
            if(m_session->isValid())m_store.save(sessionPath(key),snapshot,m_session,[this,key,version,generation](bool ok,qint64 size,QByteArray){
                if(generation!=m_generation||m_versions.value(key)!=version)return;
                if(ok){m_backed.insert(key);if(m_pages.contains(key)){m_totalCost-=m_costs.value(key);m_costs[key]=qMax(m_costs.value(key),size*3);m_totalCost+=m_costs.value(key);}trim();}
            });
        }
    }
    void trim(){
        while(m_pages.size()>1&&(m_pages.size()>4||m_totalCost>8*1024*1024)){
            QString oldest;quint64 age=std::numeric_limits<quint64>::max();
            for(auto it=m_pages.cbegin();it!=m_pages.cend();++it)
                if(it.key()!=m_key&&m_backed.contains(it.key())&&m_used.value(it.key())<age){oldest=it.key();age=m_used.value(it.key());}
            if(oldest.isEmpty())break; // Never refetch merely because a spill failed.
            m_totalCost-=m_costs.take(oldest);m_used.remove(oldest);m_pages.remove(oldest);
        }
    }
    QString token(const QString &key)const{return QCryptographicHash::hash(key.toUtf8(),QCryptographicHash::Sha256).toHex();}
    QString path(const QString &key)const{return QDir(DiskCache::online(m_library)).filePath("catalog-"+m_provider+"-"+token(key)+".json");}
    QString sessionPath(const QString &key)const{return QDir(m_session->path()).filePath(token(key)+".json");}
    AsyncJsonStore m_store;
    QHash<QString,quint64> m_versions,m_pinnedVersions;quint64 m_generation=0;
    Library *m_library;QString m_provider,m_key;std::shared_ptr<QTemporaryDir> m_session;
    QHash<QString,QVariantMap> m_pages;QHash<QString,QByteArray> m_savedHashes;
    QHash<QString,qint64> m_costs;QHash<QString,quint64> m_used;QSet<QString> m_seen,m_backed;
    QVariantMap m_before;qint64 m_totalCost=0;quint64 m_clock=0;bool m_pending=false,m_quiet=false;
};
}
