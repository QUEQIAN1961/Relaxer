#include "online-download-bridge.h"
#include "online-track-matcher.h"
#include "ncm-restore.h"
#include <QDir>
#include <QDesktopServices>
#include <QFileInfo>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUuid>
#include <QTimer>
namespace relaxer {
OnlineDownloadBridge::OnlineDownloadBridge(Library *library,QObject *parent):QAbstractListModel(parent),m_library(library),m_network(this){
    m_downloads=library->setting("online.downloads").toObject();connect(library,&Library::changed,this,[this]{m_localReady=false;m_known.clear();++m_revision;emit downloadsChanged();emit changed();});
}
OnlineDownloadBridge::~OnlineDownloadBridge(){if(m_reply){m_reply->disconnect(this);m_reply->abort();}if(m_worker){m_worker->requestInterruption();m_worker->wait();}m_file.reset();if(!m_partial.isEmpty())QFile::remove(m_partial);}
QString OnlineDownloadBridge::downloadDirectory() const {return QDir(m_library->root()).filePath("Downloads");}
bool OnlineDownloadBridge::openDownloadDirectory(){return QDir().mkpath(downloadDirectory())&&QDesktopServices::openUrl(QUrl::fromLocalFile(downloadDirectory()));}
QString OnlineDownloadBridge::destinationPath(const QVariantMap &song,const QString &extension){
    auto name=song.value("artist").toString()+" - "+song.value("title").toString();name.replace(QRegularExpression(R"([<>:"/\\|?*\x00-\x1f])"),"_");
    if(!QDir().mkpath(downloadDirectory())){m_status="无法创建下载目录";emit changed();return {};}
    return QDir(downloadDirectory()).filePath(name+(extension=="flac"?".flac":".mp3"));
}
QString OnlineDownloadBridge::songKey(const QString &provider,const QVariantMap &song) const {return song.value("key",provider=="qq"?"qqmusic:"+song.value("mid").toString():"netease:"+song.value("id").toString()).toString();}
bool OnlineDownloadBridge::isDownloaded(const QString &provider,const QVariantMap &song){
    const auto key=songKey(provider,song);const auto record=m_downloads.value(key);const auto path=record.isObject()?record.toObject().value("path").toString():record.toString();if(!path.isEmpty()&&QFileInfo(path).isFile())return true;
    if(m_known.contains(key)){const auto known=m_known.value(key);if(known.isEmpty())return false;if(QFileInfo(known).isFile())return true;m_known.remove(key);}
    if(!m_localReady){m_local.clear();for(const auto &track:m_library->tracks())if(QFileInfo(m_library->absolute(track.file)).isFile())m_local.append(track);m_localReady=true;}
    Track track;track.title=song.value("title").toString();track.artist=song.value("artist").toString();track.duration=song.value("duration").toLongLong();
    const auto match=matchOnlineTrack(track,m_local,false);QString localPath=match>0?m_library->absolute(m_library->track(match).file):QString();
    if(localPath.isEmpty())for(const auto &value:m_downloads){const auto saved=value.toObject();const auto savedPath=saved.value("path").toString();if(savedPath.isEmpty()||!QFileInfo(savedPath).isFile())continue;Track candidate;candidate.id=1;candidate.title=saved.value("title").toString();candidate.artist=saved.value("artist").toString();candidate.duration=saved.value("duration").toVariant().toLongLong();if(matchOnlineTrack(track,{candidate},false)){localPath=savedPath;break;}}
    m_known.insert(key,localPath);return !localPath.isEmpty()&&QFileInfo(localPath).isFile();
}
QHash<int,QByteArray> OnlineDownloadBridge::roleNames() const {
    return {{Qt::UserRole,"taskId"},{Qt::UserRole+1,"title"},{Qt::UserRole+2,"artist"},{Qt::UserRole+3,"cover"},{Qt::UserRole+4,"state"},{Qt::UserRole+5,"received"},{Qt::UserRole+6,"total"},{Qt::UserRole+7,"fraction"},{Qt::UserRole+8,"error"}};
}
QVariant OnlineDownloadBridge::data(const QModelIndex &index,int role) const {
    if(!index.isValid()||index.row()<0||index.row()>=m_tasks.size())return {};
    const auto &t=m_tasks[index.row()];switch(role){
    case Qt::UserRole:return t.id;case Qt::UserRole+1:return t.song.value("title");case Qt::UserRole+2:return t.song.value("artist");
    case Qt::UserRole+3:return t.song.value("thumbnail",t.song.value("cover"));case Qt::UserRole+4:return t.state;
    case Qt::UserRole+5:return t.received;case Qt::UserRole+6:return t.total;case Qt::UserRole+7:return t.progress;case Qt::UserRole+8:return t.error;default:return {};}
}
QVariantMap OnlineDownloadBridge::taskData(const Task &t) const {return {{"taskId",t.id},{"state",t.state},{"received",t.received},{"total",t.total},{"progress",t.progress},{"error",t.error}};}
QVariantMap OnlineDownloadBridge::task(const QString &provider,const QVariantMap &song) const {
    const auto key=songKey(provider,song);for(const auto &t:m_tasks)if(t.key==key)return taskData(t);return {};
}
OnlineDownloadBridge::Task *OnlineDownloadBridge::activeTask(){for(auto &t:m_tasks)if(t.id==m_activeId)return &t;return nullptr;}
void OnlineDownloadBridge::updateTask(){
    for(int i=0;i<m_tasks.size();++i)if(m_tasks[i].id==m_activeId){emit dataChanged(index(i),index(i));break;}
    ++m_taskRevision;emit tasksChanged();emit changed();
}
void OnlineDownloadBridge::enqueue(const QString &provider,const QVariantMap &song,const QString &quality){
    auto row=song;row["provider"]=provider;if(isDownloaded(provider,row))return;
    const auto previous=task(provider,row).value("state").toString();if(previous=="queued"||previous=="resolving"||previous=="downloading"||previous=="converting")return;
    const auto path=destinationPath(row,quality=="hires"||quality=="lossless"?"flac":"mp3");
    if(!path.isEmpty())enqueueTo(row,path,quality);
}
bool OnlineDownloadBridge::enqueueTo(const QVariantMap &song,const QString &path,const QString &quality,const QUrl &url,const QString &extension){
    const auto provider=song.value("provider").toString(),key=songKey(provider,song);
    if(isDownloaded(provider,song)||QFileInfo::exists(path)){m_status="已下载，已阻止重复下载";emit changed();return false;}
    for(const auto &t:m_tasks)if((t.key==key||t.path==QFileInfo(path).absoluteFilePath())&&(t.state=="queued"||t.id==m_activeId))return false;
    Task t;t.id=++m_nextId;t.song=song;t.song["_downloadTask"]=t.id;t.key=key;t.path=QFileInfo(path).absoluteFilePath();t.quality=quality;t.url=url;t.extension=extension;
    beginInsertRows({},0,0);m_tasks.prepend(t);endInsertRows();m_busy=true;++m_taskRevision;emit tasksChanged();emit changed();emit taskAdded();QTimer::singleShot(0,this,&OnlineDownloadBridge::startNext);return true;
}
void OnlineDownloadBridge::startNext(){
    if(m_activeId)return;
    for(int i=m_tasks.size()-1;i>=0;--i){
        if(m_tasks[i].state!="queued")continue;m_activeId=m_tasks[i].id;m_busy=true;m_cancelled=false;m_progress=0;
        const auto t=m_tasks[i];
        if(isDownloaded(t.song.value("provider").toString(),t.song)){complete({},"本地已有这首歌曲");return;}
        if(t.url.isEmpty()){m_tasks[i].state="resolving";m_status="正在获取下载资源…";updateTask();emit resolveRequested(t.song,t.quality);}
        else if(!startTransfer(t.song,t.url,t.extension,t.path))complete({},"无法开始下载");
        return;
    }
    m_busy=false;emit changed();
}
void OnlineDownloadBridge::chooseDestination(const QVariantMap &song,const QUrl &url,const QString &extension){
    const auto token=song.value("_downloadTask").toInt();
    if(token){if(token!=m_activeId)return;const auto t=activeTask();if(!t||t->state!="resolving")return;const auto path=t->path;if(!startTransfer(song,url,extension,path))complete({},"无法开始下载");return;}
    if(isDownloaded(song.value("provider").toString(),song))return;
    const auto path=destinationPath(song,extension);
    if(!path.isEmpty())downloadTo(song,url,extension,path);
}
void OnlineDownloadBridge::resolutionFailed(const QVariantMap &song,const QString &error){if(song.value("_downloadTask").toInt()==m_activeId&&m_activeId)complete({},error);}
bool OnlineDownloadBridge::downloadTo(const QVariantMap &song,const QUrl &url,const QString &extension,const QString &path){
    if(url.scheme()!="https"&&url.scheme()!="http")return false;return enqueueTo(song,path,{},url,extension);
}
bool OnlineDownloadBridge::startTransfer(const QVariantMap &song,const QUrl &url,const QString &extension,const QString &path){
    Q_UNUSED(extension)
    if(url.scheme()!="https"&&url.scheme()!="http")return false;
    if(isDownloaded(song.value("provider").toString(),song)||QFileInfo::exists(path))return false;
    m_song=song;m_requested=QFileInfo(path).absoluteFilePath();m_key=songKey(song.value("provider").toString(),song);m_partial=m_requested+".relaxer-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".part";
    m_file=std::make_unique<QSaveFile>(m_partial);if(!m_file->open(QIODevice::WriteOnly)){m_status="无法创建下载文件";emit changed();return false;}
    if(auto *t=activeTask())t->state="downloading";m_prefix.clear();m_busy=true;m_progress=0;m_status="正在下载…";updateTask();QNetworkRequest request(url);request.setTransferTimeout(30000);request.setRawHeader("User-Agent","Mozilla/5.0");request.setRawHeader("Referer",song.value("provider").toString()=="qq"?"https://y.qq.com/":"https://music.163.com/");
    auto *reply=m_network.get(request);m_reply=reply;
    connect(reply,&QNetworkReply::readyRead,this,[this,reply]{const auto bytes=reply->readAll();if(m_prefix.size()<32)m_prefix.append(bytes.left(32-m_prefix.size()));if(m_file&&m_file->write(bytes)!=bytes.size()){reply->abort();return;}
        if(auto *t=activeTask()){t->received=m_file?m_file->pos():t->received;t->total=qMax(t->total,reply->header(QNetworkRequest::ContentLengthHeader).toLongLong());t->progress=t->total>0?double(t->received)/t->total:0;m_progress=t->progress;updateTask();}});
    connect(reply,&QNetworkReply::downloadProgress,this,[this](qint64,qint64 total){if(auto *t=activeTask()){t->total=qMax(t->total,total);t->progress=t->total>0?double(t->received)/t->total:0;m_progress=t->progress;}updateTask();});
    connect(reply,&QNetworkReply::finished,this,[this,reply]{
        const bool ok=reply->error()==QNetworkReply::NoError;m_reply.clear();reply->deleteLater();if(!ok||!m_file||!m_file->commit()){complete({},"下载未完成，请重试");return;}m_file.reset();
        const auto info=QFileInfo(m_requested);const auto base=info.absolutePath()+"/"+info.completeBaseName();
        if(m_prefix.startsWith("CTENFDAM")){
            m_status="正在自动还原 NCM 音频…";if(auto *t=activeTask())t->state="converting";updateTask();const auto input=m_partial;
            m_worker=QThread::create([this,input,base]{const auto result=restoreNcm(input,base);QMetaObject::invokeMethod(this,[this,result]{complete(result.path,result.error);},Qt::QueuedConnection);});connect(m_worker,&QThread::finished,m_worker,&QObject::deleteLater);m_worker->start();return;
        }
        const auto format=m_prefix.startsWith("fLaC")?QString("flac"):(m_prefix.startsWith("ID3")||(m_prefix.size()>1&&quint8(m_prefix[0])==255&&(quint8(m_prefix[1])&224)==224))?QString("mp3"):QString();
        const auto target=base+"."+format;
        if(format.isEmpty()){complete({},"下载内容不是可保存的 MP3、FLAC 或 NCM 音频");return;}
        if(QFileInfo::exists(target)){complete({},"本地文件已存在，已阻止重复下载");return;}
        if(!QFile::rename(m_partial,target)){complete({},"下载文件保存失败");return;}m_partial.clear();complete(target);
    });return true;
}
void OnlineDownloadBridge::complete(const QString &savedPath,const QString &error){
    const auto path=m_cancelled?QString():savedPath;if(m_cancelled&&!savedPath.isEmpty())QFile::remove(savedPath);
    m_file.reset();if(!m_partial.isEmpty())QFile::remove(m_partial);m_partial.clear();m_busy=false;m_progress=path.isEmpty()?0:1;
    if(!path.isEmpty()){m_downloads.insert(m_key,QJsonObject{{"path",path},{"title",m_song.value("title").toString()},{"artist",m_song.value("artist").toString()},{"duration",m_song.value("duration").toLongLong()}});m_library->setSetting("online.downloads",m_downloads);m_status="已下载："+QFileInfo(path).fileName();m_known.clear();++m_revision;emit downloadsChanged();emit downloaded(path);}
    else m_status=m_cancelled?"已取消":error.isEmpty()?"下载未完成":error;
    if(auto *t=activeTask()){t->state=m_cancelled?"cancelled":path.isEmpty()?"failed":"completed";t->error=path.isEmpty()?m_status:QString();t->progress=m_progress;if(!path.isEmpty()){t->received=QFileInfo(path).size();t->total=t->received;}}
    m_busy=false;for(const auto &t:m_tasks)if(t.state=="queued"){m_busy=true;break;}
    updateTask();m_activeId=0;QTimer::singleShot(0,this,&OnlineDownloadBridge::startNext);
}
void OnlineDownloadBridge::cancelTask(int id){
    if(id==m_activeId){cancel();return;}for(int i=0;i<m_tasks.size();++i)if(m_tasks[i].id==id&&m_tasks[i].state=="queued"){m_tasks[i].state="cancelled";emit dataChanged(index(i),index(i));++m_taskRevision;emit tasksChanged();return;}
}
void OnlineDownloadBridge::cancel(){m_cancelled=true;if(m_reply)m_reply->abort();else if(auto *t=activeTask();t&&t->state=="resolving")complete({},"已取消");else if(m_worker&&m_worker->isRunning())m_worker->requestInterruption();}
}
