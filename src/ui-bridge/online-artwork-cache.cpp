#include "online-artwork-cache.h"
#include "disk-cache.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QNetworkReply>
#include <QSaveFile>
#include <QBuffer>
#include <QDateTime>
#include <QRunnable>
#include <QCryptographicHash>

namespace relaxer {
OnlineArtworkCache::OnlineArtworkCache(Library *library,PlaybackFacade *facade,QObject *parent)
    :QObject(parent),m_library(library),m_facade(facade),m_network(this){
    m_decodePool.setMaxThreadCount(1);m_decodePool.setExpiryTimeout(10000);
    m_trim.setSingleShot(true);m_trim.setInterval(2000);
    connect(&m_trim,&QTimer::timeout,this,[this]{
        QSet<QString> keep;
        if(m_facade->onlineActive()){
            const auto current=m_entries.constFind(m_facade->current().id);
            if(current!=m_entries.cend()){keep.insert(current->originalPath);keep.insert(current->thumbnailPath);}
            const auto next=m_facade->upcoming();for(int i=0;i<qMin(5,int(next.size()));++i){const auto entry=m_entries.constFind(next[i]);if(entry!=m_entries.cend()){keep.insert(entry->originalPath);keep.insert(entry->thumbnailPath);}}
        }
        DiskCache::trim(m_library,keep);
    });
    m_prefetch.setSingleShot(true);
    connect(&m_prefetch,&QTimer::timeout,this,&OnlineArtworkCache::prefetch);
    const auto schedule=[this]{if(m_presentationActive&&!m_prefetch.isActive())m_prefetch.start(0);};
    connect(facade,&PlaybackFacade::trackChanged,this,schedule);
    connect(facade,&PlaybackFacade::trackChanged,this,&OnlineArtworkCache::thumbnailChanged);
    connect(facade,&PlaybackFacade::stateChanged,this,schedule);
}
OnlineArtworkCache::~OnlineArtworkCache(){
    m_decodeTicket->store(false);for(auto &entry:m_entries)entry.valid->store(false);
    for(auto *reply:m_requests){reply->disconnect(this);reply->abort();}
    m_decodePool.clear();m_decodePool.waitForDone();
}
QString OnlineArtworkCache::bestCover(const Entry &entry) const {
    if(entry.originalReady)return entry.originalPath;
    if(entry.thumbnailReady)return entry.thumbnailPath;
    return entry.thumbnail.toString();
}
QUrl OnlineArtworkCache::thumbnailCover() const {
    if(!m_facade->onlineActive())return {};
    const auto entry=m_entries.constFind(m_facade->current().id);
    return entry!=m_entries.cend()&&entry->thumbnailReady ? QUrl::fromLocalFile(entry->thumbnailPath) : QUrl();
}
void OnlineArtworkCache::prepare(Track &track,const QUrl &thumbnail,const QUrl &original,const QString &originalPath){
    Q_UNUSED(originalPath)
    if(thumbnail.isEmpty())return;
    auto &entry=m_entries[track.id];
    if(entry.thumbnail!=thumbnail||entry.original!=original){
        entry.valid->store(false);entry={};entry.generation=++m_generation;entry.thumbnail=thumbnail;entry.original=original;
        const auto folder=DiskCache::online(m_library);
        // Internal IDs are session-local on some platforms. Both the media
        // identity and source URLs own the cache entry, never the list position.
        const auto identity=track.hash.isEmpty()?QString::number(track.id):track.hash;
        const auto key=QCryptographicHash::hash((identity+'\n'+thumbnail.toString(QUrl::FullyEncoded)+'\n'+original.toString(QUrl::FullyEncoded)).toUtf8(),QCryptographicHash::Sha256).toHex();
        entry.thumbnailPath=QDir(folder).filePath("thumbnail-v2-"+key+".png");
        entry.originalPath=QDir(folder).filePath("cover-v2-"+key+".png");
        entry.thumbnailReady=QImageReader(entry.thumbnailPath).size().isValid();
        entry.originalReady=QImageReader(entry.originalPath).size().isValid();
    }
    track.cover=bestCover(entry);
}
void OnlineArtworkCache::setExpanded(bool expanded){if(m_expanded==expanded)return;m_expanded=expanded;if(m_presentationActive&&!m_prefetch.isActive())m_prefetch.start(0);}
void OnlineArtworkCache::setPresentationActive(bool active){
    if(m_presentationActive==active)return;m_presentationActive=active;
    if(active){m_decodeTicket=std::make_shared<std::atomic_bool>(true);m_prefetch.start(0);return;}
    m_decodeTicket->store(false);m_decodePool.clear();m_decoding=0;
    for(auto &entry:m_entries){entry.thumbnailPending=false;entry.originalPending=false;}
    m_prefetch.stop();
    // Only this object's artwork replies, never media/playback requests.
    const auto requests=m_requests;for(auto *reply:requests)reply->abort();
}
void OnlineArtworkCache::prefetch(){
    if(!m_presentationActive||!m_facade->onlineActive()||m_facade->isShutdown())return;
    const auto current=m_facade->current().id;
    const auto queue=m_facade->queue().items;QSet<qint64> keep(queue.begin(),queue.end());keep.insert(current);
    for(auto it=m_entries.begin();it!=m_entries.end();)if(!keep.contains(it.key())){it->valid->store(false);it=m_entries.erase(it);}else ++it;
    if(!m_entries.contains(current)){
        auto track=m_facade->current();const auto metadata=track.onlineMetadata;
        const QUrl original(metadata.value("cover").toString()),thumbnail(metadata.value("thumbnail").toString());
        if(!original.isEmpty()||!thumbnail.isEmpty()){
            prepare(track,thumbnail.isEmpty()?original:thumbnail,original,QDir(DiskCache::online(m_library)).filePath(QString("restored-cover-%1.png").arg(current)));
            m_facade->updateOnlineTrack(track);
        }
    }
    const auto requests=m_requests;for(auto *reply:requests)if(!keep.contains(reply->property("artworkId").toLongLong()))reply->abort();
    fetch(current,false);
    // The panel shows five rows. Do not fetch a full playlist's artwork eagerly.
    const auto next=m_facade->upcoming();for(int i=0;i<qMin(5,int(next.size()));++i)fetch(next[i],false);
    const auto entry=m_entries.constFind(current);
    if(m_expanded&&entry!=m_entries.cend()&&entry->thumbnailReady)fetch(current,true);
}
void OnlineArtworkCache::fetch(qint64 id,bool original){
    if(!m_presentationActive||m_requests.size()+m_decoding>=8)return;
    auto it=m_entries.find(id);if(it==m_entries.end())return;auto &entry=it.value();
    entry.originalReady=entry.originalReady&&QFile::exists(entry.originalPath);entry.thumbnailReady=entry.thumbnailReady&&QFile::exists(entry.thumbnailPath);
    if(original?(entry.originalReady||entry.originalPending):(entry.thumbnailReady||entry.thumbnailPending))return;
    const auto source=original?entry.original:entry.thumbnail;if(source.isEmpty()||QDateTime::currentMSecsSinceEpoch()<(original?entry.originalRetryAt:entry.thumbnailRetryAt))return;
    const auto generation=entry.generation;const auto ticket=m_decodeTicket;const auto valid=entry.valid;
    if(original)entry.originalPending=true;else entry.thumbnailPending=true;
    QNetworkRequest request(source);request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::Http2AllowedAttribute,false);
    request.setRawHeader("User-Agent","Mozilla/5.0");request.setRawHeader("Referer",source.host().endsWith("gtimg.cn")?"https://y.qq.com/":source.host().endsWith("kugou.com")?"https://www.kugou.com/":source.host().contains("douyin")||source.host().contains("byte")?"https://music.douyin.com/":"https://music.163.com/");
    auto *reply=m_network.get(request);reply->setProperty("artworkId",id);m_requests.insert(reply);
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 received,qint64 total){if(received>16*1024*1024||total>16*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,id,original,source,reply,generation,ticket,valid]{
        m_requests.remove(reply);auto bytes=reply->isOpen()?reply->readAll():QByteArray();const bool ok=reply->error()==QNetworkReply::NoError;reply->deleteLater();
        auto it=m_entries.find(id);
        if(it==m_entries.end()||it->generation!=generation||ticket!=m_decodeTicket||!ticket->load()){if(m_presentationActive&&!m_prefetch.isActive())m_prefetch.start(0);return;}
        if(!ok||bytes.size()>16*1024*1024){
            if(original){it->originalPending=false;it->originalRetryAt=QDateTime::currentMSecsSinceEpoch()+30000;}
            else{it->thumbnailPending=false;it->thumbnailRetryAt=QDateTime::currentMSecsSinceEpoch()+30000;}
            if(!m_prefetch.isActive())m_prefetch.start(0);return;
        }
        const auto destination=original?it->originalPath:it->thumbnailPath;++m_decoding;
        // One artwork-only worker bounds decode peaks. It does not access
        // Library, PlaybackFacade, media requests or any audio output object.
        m_decodePool.start(QRunnable::create([this,id,original,generation,ticket,valid,destination,bytes=std::move(bytes)]{
            bool saved=false;
            if(ticket->load()&&valid->load()){
                QBuffer input;input.setData(bytes);input.open(QIODevice::ReadOnly);
                QImageReader reader(&input);reader.setAutoTransform(true);const auto size=reader.size();const int limit=original?1600:160;
                if(size.isValid()&&qint64(size.width())*size.height()<=100000000){
                    if(qMax(size.width(),size.height())>limit)reader.setScaledSize(size.scaled(limit,limit,Qt::KeepAspectRatio));
                    auto image=reader.read();
                    if(!image.isNull()&&ticket->load()&&valid->load()){
                        if(qMax(image.width(),image.height())>limit)image=image.scaled(limit,limit,Qt::KeepAspectRatio,Qt::SmoothTransformation);
                        QSaveFile file(destination);
                        if(file.open(QIODevice::WriteOnly)&&image.save(&file,"PNG")&&ticket->load()&&valid->load())saved=file.commit();
                    }
                }
            }
            QMetaObject::invokeMethod(this,[this,id,original,generation,ticket,saved]{
                if(ticket!=m_decodeTicket||!ticket->load())return;
                m_decoding=qMax(0,m_decoding-1);auto it=m_entries.find(id);
                if(it!=m_entries.end()&&it->generation==generation){
                    if(original){it->originalPending=false;it->originalReady=saved;if(!saved)it->originalRetryAt=QDateTime::currentMSecsSinceEpoch()+30000;}
                    else{it->thumbnailPending=false;it->thumbnailReady=saved;if(!saved)it->thumbnailRetryAt=QDateTime::currentMSecsSinceEpoch()+30000;}
                    if(saved){if(!m_trim.isActive())m_trim.start();if(!original&&m_facade->current().id==id)emit thumbnailChanged();publish(id);}
                }
                if(m_presentationActive&&!m_prefetch.isActive())m_prefetch.start(0);
            },Qt::QueuedConnection);
        }));
    });
}
void OnlineArtworkCache::publish(qint64 id){
    if(!m_facade->onlineActive()||!m_facade->queue().items.contains(id))return;
    auto track=m_facade->track(id);const auto cover=bestCover(m_entries.value(id));
    if(!track.id||track.cover==cover)return;track.cover=cover;m_facade->updateOnlineTrack(track);
}
}
