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
#include <QPointer>
#include <QQuickImageProvider>
#include <functional>
#include <utility>

namespace relaxer {
namespace {
constexpr qint64 CoverByteLimit=16*1024*1024;

bool remoteCover(const QUrl &url) {
    return url.isValid()&&!url.host().isEmpty()&&url.userInfo().isEmpty()
        &&(url.scheme()=="https"||url.scheme()=="http");
}

QNetworkRequest coverRequest(const QUrl &source) {
    QNetworkRequest request(source);
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute,false);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,QNetworkRequest::Manual);
    const auto host=source.host();
    request.setRawHeader("User-Agent","Mozilla/5.0");
    if(host=="hdslb.com"||host.endsWith(".hdslb.com")||host.endsWith(".biliimg.com"))
        request.setRawHeader("Referer","https://www.bilibili.com/");
    else if(host.endsWith("gtimg.cn"))request.setRawHeader("Referer","https://y.qq.com/");
    else if(host.endsWith("kugou.com"))request.setRawHeader("Referer","https://www.kugou.com/");
    else if(host.contains("douyin")||host.contains("byte"))request.setRawHeader("Referer","https://music.douyin.com/");
    else if(host.endsWith("music.126.net")||host.endsWith("music.163.com"))request.setRawHeader("Referer","https://music.163.com/");
    return request;
}

QImage decodeCover(QImageReader &reader,const QSize &limit) {
    reader.setAutoTransform(true);
    const auto size=reader.size();
    if(size.isEmpty()||qint64(size.width())*size.height()>100000000)return {};
    if(size.width()>limit.width()||size.height()>limit.height())
        reader.setScaledSize(size.scaled(limit,Qt::KeepAspectRatio));
    auto image=reader.read();
    if(image.width()>limit.width()||image.height()>limit.height())
        image=image.scaled(limit,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    return image;
}

void boundCoverReply(QNetworkReply *reply) {
    reply->setReadBufferSize(CoverByteLimit+1);
    QObject::connect(reply,&QNetworkReply::readyRead,reply,[reply]{
        if(reply->bytesAvailable()>CoverByteLimit)reply->abort();
    });
    QObject::connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 received,qint64 total){
        if(received>CoverByteLimit||total>CoverByteLimit)reply->abort();
    });
}
}

class OnlineCoverResponse final:public QQuickImageResponse {
public:
    QQuickTextureFactory *textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }
    QString errorString() const override {
        return m_image.isNull()?QStringLiteral("Online cover is unavailable"):QString();
    }
    void complete(QImage image) {
        m_image=std::move(image);
        emit finished();
    }
private:
    QImage m_image;
};

namespace {
class OnlineCoverProvider final:public QQuickAsyncImageProvider {
public:
    using Load=std::function<void(const QUrl &,const QSize &,OnlineCoverResponse *)>;
    OnlineCoverProvider(QThread *thread,Load load):m_thread(thread),m_load(std::move(load)){}
    QQuickImageResponse *requestImageResponse(const QString &id,const QSize &size) override {
        auto *response=new OnlineCoverResponse;
        response->moveToThread(m_thread);
        const auto source=QUrl::fromEncoded(QByteArray::fromBase64(id.toLatin1(),QByteArray::Base64UrlEncoding));
        QMetaObject::invokeMethod(response,[response,source,size,load=m_load]{load(source,size,response);},Qt::QueuedConnection);
        return response;
    }
private:
    QThread *m_thread;
    Load m_load;
};
}

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
    const auto pending=std::exchange(m_coverResponses,{});
    for(auto *response:pending)response->complete({});
}

QUrl OnlineArtworkCache::coverImageUrl(const QUrl &source) const {
    if(!remoteCover(source))return source;
    const auto key=source.toEncoded(QUrl::RemoveFragment).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals);
    return QUrl("image://online-cover/"+QString::fromLatin1(key));
}

QQuickAsyncImageProvider *OnlineArtworkCache::createImageProvider() {
    return new OnlineCoverProvider(thread(),[cache=QPointer<OnlineArtworkCache>(this)](const QUrl &source,const QSize &size,OnlineCoverResponse *response){
        if(cache)cache->loadCover(source,size,response);
        else response->complete({});
    });
}

void OnlineArtworkCache::loadCover(const QUrl &source,const QSize &requestedSize,OnlineCoverResponse *response) {
    if(!remoteCover(source)){response->complete({});return;}
    m_coverResponses.insert(response);
    connect(response,&QObject::destroyed,this,[this,response]{m_coverResponses.remove(response);});
    const auto digest=QCryptographicHash::hash(source.toEncoded(),QCryptographicHash::Sha256).toHex();
    const auto path=QDir(DiskCache::online(m_library)).filePath("artwork-"+digest+".img");
    const QSize cap(1600,1600);
    const auto size=requestedSize.isEmpty()?cap:requestedSize.boundedTo(cap);
    const auto pending=QPointer<OnlineCoverResponse>(response);
    m_decodePool.start([this,source,size,path,pending]{
        QImage image;
        const QFileInfo info(path);
        if(info.isFile()&&!info.isSymLink()&&info.size()<=CoverByteLimit){
            QImageReader reader(path);
            image=decodeCover(reader,size);
        }
        QMetaObject::invokeMethod(this,[this,source,size,path,pending,image=std::move(image)]()mutable{
            if(!pending)return;
            if(image.isNull())downloadCover(source,path,size,pending);
            else completeCover(pending,std::move(image));
        },Qt::QueuedConnection);
    });
}

void OnlineArtworkCache::downloadCover(const QUrl &source,const QString &path,const QSize &size,OnlineCoverResponse *response) {
    auto *reply=m_network.get(coverRequest(source));
    boundCoverReply(reply);
    const auto pending=QPointer<OnlineCoverResponse>(response);
    connect(reply,&QNetworkReply::finished,this,[this,reply,path,size,pending]{
        reply->deleteLater();
        if(reply->error()!=QNetworkReply::NoError||reply->bytesAvailable()>CoverByteLimit){
            if(pending)completeCover(pending,{});
            return;
        }
        // Validate the image before storing just its bytes. The filename hashes
        // the full source URL, keeping headers and URL query strings off disk.
        m_decodePool.start([this,path,size,pending,bytes=reply->readAll()]{
            QBuffer input;
            input.setData(bytes);
            input.open(QIODevice::ReadOnly);
            QImageReader reader(&input);
            auto image=decodeCover(reader,size);
            bool saved=false;
            if(!image.isNull()&&!QFileInfo(path).isSymLink()){
                QSaveFile file(path);
                saved=file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&file.commit();
            }
            QMetaObject::invokeMethod(this,[this,pending,saved,image=std::move(image)]()mutable{
                if(saved&&!m_trim.isActive())m_trim.start();
                if(pending)completeCover(pending,std::move(image));
            },Qt::QueuedConnection);
        });
    });
}

void OnlineArtworkCache::completeCover(OnlineCoverResponse *response,QImage image) {
    if(m_coverResponses.remove(response))response->complete(std::move(image));
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
    // Queued prefetches check this ticket before decoding. Keep the jobs for
    // already requested artwork so they can finish writing their cache files.
    m_decodeTicket->store(false);m_decoding=0;
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
    auto *reply=m_network.get(coverRequest(source));reply->setProperty("artworkId",id);m_requests.insert(reply);
    boundCoverReply(reply);
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
