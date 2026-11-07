#include "online-artwork-cache.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QNetworkReply>
#include <QSaveFile>

namespace relaxer {
OnlineArtworkCache::OnlineArtworkCache(Library *library,PlaybackFacade *facade,QObject *parent)
    :QObject(parent),m_library(library),m_facade(facade),m_network(this){
    m_prefetch.setSingleShot(true);
    connect(&m_prefetch,&QTimer::timeout,this,&OnlineArtworkCache::prefetch);
    const auto schedule=[this]{if(!m_prefetch.isActive())m_prefetch.start(0);};
    connect(facade,&PlaybackFacade::trackChanged,this,schedule);
    connect(facade,&PlaybackFacade::stateChanged,this,schedule);
}
OnlineArtworkCache::~OnlineArtworkCache(){for(auto *reply:m_requests){reply->disconnect(this);reply->abort();}}
QString OnlineArtworkCache::bestCover(const Entry &entry) const {
    if(entry.originalReady)return entry.originalPath;
    if(entry.thumbnailReady)return entry.thumbnailPath;
    return entry.thumbnail.toString();
}
void OnlineArtworkCache::prepare(Track &track,const QUrl &thumbnail,const QUrl &original,const QString &originalPath){
    if(thumbnail.isEmpty())return;
    auto &entry=m_entries[track.id];
    if(entry.thumbnail!=thumbnail||entry.original!=original){
        entry={};entry.thumbnail=thumbnail;entry.original=original;entry.originalPath=originalPath;
        const auto folder=QDir(m_library->root()).filePath("Online");QDir().mkpath(folder);
        entry.thumbnailPath=QDir(folder).filePath(QString("thumbnail-%1.png").arg(track.id));
        entry.thumbnailReady=QImageReader(entry.thumbnailPath).size().isValid();
        const auto size=QImageReader(originalPath).size();entry.originalReady=size.width()>=600&&size.height()>=600;
    }
    track.cover=bestCover(entry);
}
void OnlineArtworkCache::setExpanded(bool expanded){m_expanded=expanded;if(!m_prefetch.isActive())m_prefetch.start(0);}
void OnlineArtworkCache::prefetch(){
    if(!m_facade->onlineActive()||m_facade->isShutdown())return;
    const auto current=m_facade->current().id;
    fetch(current,false);
    // The panel shows five rows. Do not fetch a full playlist's artwork eagerly.
    const auto next=m_facade->upcoming();for(int i=0;i<qMin(5,int(next.size()));++i)fetch(next[i],false);
    const auto entry=m_entries.constFind(current);
    if(m_expanded&&entry!=m_entries.cend()&&entry->thumbnailReady)fetch(current,true);
}
void OnlineArtworkCache::fetch(qint64 id,bool original){
    auto it=m_entries.find(id);if(it==m_entries.end())return;auto &entry=it.value();
    if(original?(entry.originalReady||entry.originalPending):(entry.thumbnailReady||entry.thumbnailPending))return;
    const auto source=original?entry.original:entry.thumbnail;if(source.isEmpty())return;
    if(original)entry.originalPending=true;else entry.thumbnailPending=true;
    QNetworkRequest request(source);request.setTransferTimeout(15000);
    request.setRawHeader("User-Agent","Mozilla/5.0");request.setRawHeader("Referer",source.host().endsWith("gtimg.cn")?"https://y.qq.com/":"https://music.163.com/");
    auto *reply=m_network.get(request);m_requests.insert(reply);
    connect(reply,&QNetworkReply::finished,this,[this,id,original,source,reply]{
        m_requests.remove(reply);const auto bytes=reply->isOpen()?reply->readAll():QByteArray();const bool ok=reply->error()==QNetworkReply::NoError;reply->deleteLater();
        auto it=m_entries.find(id);if(it==m_entries.end())return;auto &entry=it.value();
        if(source!=(original?entry.original:entry.thumbnail))return;
        if(original)entry.originalPending=false;else entry.thumbnailPending=false;
        if(!ok)return;auto image=QImage::fromData(bytes);if(image.isNull())return;
        if(!original&&(image.width()>160||image.height()>160))image=image.scaled(160,160,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        QSaveFile file(original?entry.originalPath:entry.thumbnailPath);
        if(!file.open(QIODevice::WriteOnly)||!image.save(&file,"PNG")||!file.commit())return;
        if(original)entry.originalReady=true;else entry.thumbnailReady=true;
        publish(id);
        if(!original&&m_expanded&&m_facade->onlineActive()&&m_facade->current().id==id)fetch(id,true);
    });
}
void OnlineArtworkCache::publish(qint64 id){
    if(!m_facade->onlineActive()||!m_facade->queue().items.contains(id))return;
    auto track=m_facade->track(id);const auto cover=bestCover(m_entries.value(id));
    if(!track.id||track.cover==cover)return;track.cover=cover;m_facade->updateOnlineTrack(track);
}
}
