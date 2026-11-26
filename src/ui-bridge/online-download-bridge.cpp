#include "online-download-bridge.h"
#include "online-track-matcher.h"
#include "ncm-restore.h"
#include "import-coordinator.h"
#include <QDir>
#include <QDesktopServices>
#include <QFileInfo>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUuid>
#include <QTimer>
#include <QRunnable>
#include <QDateTime>
#include <utility>
namespace relaxer {
OnlineDownloadBridge::OnlineDownloadBridge(Library *library,QObject *parent):QAbstractListModel(parent),m_library(library),m_network(this){
    m_detectPool.setMaxThreadCount(1);m_detectPool.setExpiryTimeout(10000);
    m_detectTimer.setSingleShot(true);connect(&m_detectTimer,&QTimer::timeout,this,&OnlineDownloadBridge::detectPending);
    m_favoriteImportTimer.setSingleShot(true);
    connect(&m_favoriteImportTimer,&QTimer::timeout,this,&OnlineDownloadBridge::importNextFavoriteDownload);
    m_progressTimer.setSingleShot(true);m_progressTimer.setInterval(75);connect(&m_progressTimer,&QTimer::timeout,this,[this]{publishTask(false);});
    m_downloads=library->setting("online.downloads").toObject();connect(library,&Library::changed,this,[this]{++m_detectRevision;m_detectQueue.clear();m_known.clear();m_downloadIndex.reset();++m_revision;emit downloadsChanged();emit changed();});
}
OnlineDownloadBridge::~OnlineDownloadBridge(){m_detectPool.clear();m_detectPool.waitForDone();if(m_reply){m_reply->disconnect(this);m_reply->abort();}if(m_worker){m_worker->requestInterruption();m_worker->wait();}m_file.reset();if(!m_partial.isEmpty())QFile::remove(m_partial);}
QString OnlineDownloadBridge::downloadDirectory() const {return QDir(m_library->root()).filePath("Downloads");}
bool OnlineDownloadBridge::openDownloadDirectory(){return QDir().mkpath(downloadDirectory())&&QDesktopServices::openUrl(QUrl::fromLocalFile(downloadDirectory()));}
QString OnlineDownloadBridge::destinationPath(const QVariantMap &song,const QString &extension){
    auto name=song.value("artist").toString()+" - "+song.value("title").toString();name.replace(QRegularExpression(R"([<>:"/\\|?*\x00-\x1f])"),"_");
    if(!QDir().mkpath(downloadDirectory())){m_status="无法创建下载目录";emit changed();return {};}
    const auto format=QStringList{"flac","m4a","ogg","wav"}.contains(extension)?extension:QString("mp3");
    return QDir(downloadDirectory()).filePath(name+"."+format);
}
QString OnlineDownloadBridge::songKey(const QString &provider,const QVariantMap &song) const {
    const auto remoteId=song.value(provider=="qq"?"mid":"id").toString();
    const auto keyProvider=provider=="qq"?QString("qqmusic"):provider.isEmpty()?QString("netease"):provider;
    return song.value("key",keyProvider+":"+remoteId).toString();
}
void OnlineDownloadBridge::attachFavoriteImporter(ImportCoordinator *coordinator){
    if(m_favoriteImporter||!coordinator)return;
    m_favoriteImporter=coordinator;
    connect(m_library,&Library::onlineFavoritesChanged,this,[this]{m_favoriteImportTimer.start(0);});
    connect(this,&OnlineDownloadBridge::downloaded,this,[this]{
        m_favoriteImportAttempts.remove(m_key);
        m_favoriteImportTimer.start(0);
    });
    connect(coordinator,&ImportCoordinator::trackCommitted,this,[this](quint64 task,qint64 track,bool){
        if(task==m_favoriteImportTask)m_favoriteImportTrack=track;
    });
    connect(coordinator,&ImportCoordinator::finished,this,[this](const ImportSummary &summary){
        if(summary.taskId==m_favoriteImportTask){
            const auto key=std::exchange(m_favoriteImportKey,QString());
            const auto track=std::exchange(m_favoriteImportTrack,qint64(0));
            m_favoriteImportTask=0;
            if(!summary.cancelled){
                if(!track)emit favoriteImportFailed(summary.errors.value(0,summary.fatalError));
                else if(!m_library->linkFavoriteDownload(key,track))emit favoriteImportFailed(m_library->lastError());
            }
        }
        if(!m_favoriteImporter->isShuttingDown())m_favoriteImportTimer.start(0);
    });
    m_favoriteImportTimer.start(0);
}
void OnlineDownloadBridge::importNextFavoriteDownload(){
    if(!m_favoriteImporter||m_favoriteImporter->isBusy()||m_favoriteImporter->isShuttingDown()||
       m_favoriteImportTask||m_downloads.isEmpty())return;
    const auto folder=QFileInfo(downloadDirectory()).canonicalFilePath();
    if(folder.isEmpty())return;
    // Only a completed download's exact provider receipt can replace a favorite.
    // The fuzzy title/artist matcher still serves the download button alone.
    for(const auto &key:m_library->onlineFavoriteKeys(false)){
        const auto record=m_downloads.value(key);
        const auto path=record.isObject()?record.toObject().value("path").toString():record.toString();
        if(path.isEmpty()||m_favoriteImportAttempts.value(key)==path)continue;
        const QFileInfo file(path);
        const auto canonical=file.canonicalFilePath();
        if(!file.isFile()||file.isSymLink()||file.size()<=0||QFileInfo(canonical).absolutePath()!=folder)continue;
        m_favoriteImportAttempts.insert(key,path);
        ImportRequest request;
        request.sources={canonical};
        request.maximumWorkers=1;
        const auto id=m_favoriteImporter->start(request);
        if(id){
            m_favoriteImportTask=id;
            m_favoriteImportKey=key;
            return;
        }
    }
}
std::shared_ptr<const OnlineTrackIndex> OnlineDownloadBridge::downloadIndex(){
    if(!m_downloadIndex){QVector<Track> rows;qint64 id=1;for(const auto &value:m_downloads){const auto row=value.toObject();if(row.isEmpty())continue;Track t;t.id=id++;t.file=row.value("path").toString();t.title=row.value("title").toString();t.artist=row.value("artist").toString();t.duration=row.value("duration").toVariant().toLongLong();rows.append(t);}m_downloadIndex=std::make_shared<OnlineTrackIndex>(rows);}
    return m_downloadIndex;
}
bool OnlineDownloadBridge::downloadedNow(const QString &provider,const QVariantMap &song){
    const auto key=songKey(provider,song);const auto record=m_downloads.value(key);const auto path=record.isObject()?record.toObject().value("path").toString():record.toString();
    if(!path.isEmpty()&&QFileInfo(path).isFile()){m_known.insert(key,new Known{path,QDateTime::currentMSecsSinceEpoch()});return true;}
    if(const auto *known=m_known.object(key);known&&!known->path.isEmpty()&&QFileInfo(known->path).isFile())return true;
    Track query;query.title=song.value("title").toString();query.artist=song.value("artist").toString();query.duration=song.value("duration").toLongLong();
    auto index=sharedOnlineTrackIndex(m_library);auto id=index?index->match(query,false,true):0;QString found=id?index->path(id):QString();
    if(found.isEmpty()){index=downloadIndex();id=index->match(query,false,true);if(id)found=index->path(id);}
    m_known.insert(key,new Known{found,QDateTime::currentMSecsSinceEpoch()});return !found.isEmpty();
}
bool OnlineDownloadBridge::isDownloaded(const QString &provider,const QVariantMap &song){
    const auto key=songKey(provider,song);const auto *known=m_known.object(key);
    if(known&&QDateTime::currentMSecsSinceEpoch()-known->checked<15000)return !known->path.isEmpty();
    auto row=song;row["provider"]=provider;m_detectQueue.insert(key,row);
    while(m_detectQueue.size()>128)m_detectQueue.erase(m_detectQueue.begin());
    if(!m_detecting&&!m_detectTimer.isActive())m_detectTimer.start(0);return known&&!known->path.isEmpty();
}
void OnlineDownloadBridge::detectPending(){
    if(m_detecting||m_detectQueue.isEmpty())return;m_detecting=true;
    QHash<QString,QVariantMap> batch;while(!m_detectQueue.isEmpty()&&batch.size()<32){auto it=m_detectQueue.begin();batch.insert(it.key(),it.value());m_detectQueue.erase(it);}
    const auto local=sharedOnlineTrackIndex(m_library),downloads=downloadIndex();const auto records=m_downloads;const auto revision=m_detectRevision;
    m_detectPool.start(QRunnable::create([this,batch,local,downloads,records,revision]{
        QHash<QString,QString> results;
        for(auto it=batch.cbegin();it!=batch.cend();++it){const auto record=records.value(it.key());QString found=record.isObject()?record.toObject().value("path").toString():record.toString();
            if(found.isEmpty()||!QFileInfo(found).isFile()){Track t;t.title=it->value("title").toString();t.artist=it->value("artist").toString();t.duration=it->value("duration").toLongLong();const auto id=local?local->match(t,false,true):0;found=id?local->path(id):QString();if(found.isEmpty()){const auto saved=downloads->match(t,false,true);if(saved)found=downloads->path(saved);}}
            results.insert(it.key(),found);
        }
        QMetaObject::invokeMethod(this,[this,results,revision]{m_detecting=false;if(revision==m_detectRevision){for(auto it=results.cbegin();it!=results.cend();++it)m_known.insert(it.key(),new Known{it.value(),QDateTime::currentMSecsSinceEpoch()});++m_revision;emit downloadsChanged();}if(!m_detectQueue.isEmpty())m_detectTimer.start(0);},Qt::QueuedConnection);
    }));
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
    const auto row=m_taskRows.value(songKey(provider,song),-1);return row>=0?taskData(m_tasks[row]):QVariantMap{};
}
OnlineDownloadBridge::Task *OnlineDownloadBridge::activeTask(){const int row=m_taskIds.value(m_activeId,-1);return row>=0?&m_tasks[row]:nullptr;}
void OnlineDownloadBridge::rebuildTaskIndex(){m_taskRows.clear();m_taskIds.clear();for(int i=0;i<m_tasks.size();++i){m_taskIds[m_tasks[i].id]=i;if(!m_taskRows.contains(m_tasks[i].key))m_taskRows[m_tasks[i].key]=i;}}
void OnlineDownloadBridge::pruneTasks(){
    int terminal=0;for(int i=0;i<m_tasks.size();){const auto &t=m_tasks[i];const bool live=t.id==m_activeId||t.state=="queued"||t.state=="resolving"||t.state=="downloading"||t.state=="converting";
        if(!live&&++terminal>200){beginRemoveRows({},i,i);m_tasks.removeAt(i);endRemoveRows();}else ++i;}
    rebuildTaskIndex();
}
void OnlineDownloadBridge::publishTask(bool structural){
    const auto row=m_taskIds.value(m_activeId,-1);if(row>=0){emit dataChanged(index(row),index(row));emit taskUpdated(m_tasks[row].key,taskData(m_tasks[row]));}
    if(structural){++m_taskRevision;emit tasksChanged();}emit changed();
}
void OnlineDownloadBridge::updateTask(bool immediate){if(immediate){m_progressTimer.stop();publishTask(true);}else if(!m_progressTimer.isActive())m_progressTimer.start();}
void OnlineDownloadBridge::enqueue(const QString &provider,const QVariantMap &song,const QString &quality){
    auto row=song;row["provider"]=provider;if(downloadedNow(provider,row))return;
    const auto previous=task(provider,row).value("state").toString();if(previous=="queued"||previous=="resolving"||previous=="downloading"||previous=="converting")return;
    const auto path=destinationPath(row,quality=="hires"||quality=="lossless"?"flac":"mp3");
    if(!path.isEmpty())enqueueTo(row,path,quality);
}
bool OnlineDownloadBridge::enqueueTo(const QVariantMap &song,const QString &path,const QString &quality,const QUrl &url,const QString &extension){
    const auto provider=song.value("provider").toString(),key=songKey(provider,song);
    if(downloadedNow(provider,song)||QFileInfo::exists(path)){m_status="已下载，已阻止重复下载";emit changed();return false;}
    for(const auto &t:m_tasks)if((t.key==key||t.path==QFileInfo(path).absoluteFilePath())&&(t.state=="queued"||t.id==m_activeId))return false;
    Task t;t.id=++m_nextId;t.song=song;t.song["_downloadTask"]=t.id;t.key=key;t.path=QFileInfo(path).absoluteFilePath();t.quality=quality;t.url=url;t.extension=extension;
    beginInsertRows({},0,0);m_tasks.prepend(t);endInsertRows();pruneTasks();m_busy=true;++m_taskRevision;emit tasksChanged();emit changed();emit taskAdded();QTimer::singleShot(0,this,&OnlineDownloadBridge::startNext);return true;
}
void OnlineDownloadBridge::startNext(){
    if(m_activeId)return;
    for(int i=m_tasks.size()-1;i>=0;--i){
        if(m_tasks[i].state!="queued")continue;m_activeId=m_tasks[i].id;m_busy=true;m_cancelled=false;m_progress=0;
        const auto t=m_tasks[i];
        if(downloadedNow(t.song.value("provider").toString(),t.song)){complete({},"本地已有这首歌曲");return;}
        if(t.url.isEmpty()){m_tasks[i].state="resolving";m_status="正在获取下载资源…";updateTask();emit resolveRequested(t.song,t.quality);}
        else if(!startTransfer(t.song,t.url,t.extension,t.path))complete({},"无法开始下载");
        return;
    }
    m_busy=false;emit changed();
}
void OnlineDownloadBridge::chooseDestination(const QVariantMap &song,const QUrl &url,const QString &extension){
    const auto token=song.value("_downloadTask").toInt();
    if(token){if(token!=m_activeId)return;const auto t=activeTask();if(!t||t->state!="resolving")return;const auto path=t->path;if(!startTransfer(song,url,extension,path))complete({},"无法开始下载");return;}
    if(downloadedNow(song.value("provider").toString(),song))return;
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
    if(downloadedNow(song.value("provider").toString(),song)||QFileInfo::exists(path))return false;
    m_song=song;m_requested=QFileInfo(path).absoluteFilePath();m_key=songKey(song.value("provider").toString(),song);m_partial=m_requested+".relaxer-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".part";
    m_file=std::make_unique<QSaveFile>(m_partial);if(!m_file->open(QIODevice::WriteOnly)){m_status="无法创建下载文件";emit changed();return false;}
    if(auto *t=activeTask())t->state="downloading";m_prefix.clear();m_busy=true;m_progress=0;m_status="正在下载…";updateTask();QNetworkRequest request(url);request.setTransferTimeout(30000);request.setRawHeader("User-Agent","Mozilla/5.0");request.setRawHeader("Referer",song.value("provider").toString()=="qq"?"https://y.qq.com/":song.value("provider").toString()=="kugou"?"https://www.kugou.com/":song.value("provider").toString()=="qishui"?"https://music.douyin.com/":"https://music.163.com/");
    auto *reply=m_network.get(request);m_reply=reply;
    connect(reply,&QNetworkReply::readyRead,this,[this,reply]{const auto bytes=reply->readAll();if(m_prefix.size()<32)m_prefix.append(bytes.left(32-m_prefix.size()));if(m_file&&m_file->write(bytes)!=bytes.size()){reply->abort();return;}
        if(auto *t=activeTask()){t->received=m_file?m_file->pos():t->received;t->total=qMax(t->total,reply->header(QNetworkRequest::ContentLengthHeader).toLongLong());t->progress=t->total>0?double(t->received)/t->total:0;m_progress=t->progress;updateTask(false);}});
    connect(reply,&QNetworkReply::downloadProgress,this,[this](qint64,qint64 total){if(auto *t=activeTask()){t->total=qMax(t->total,total);t->progress=t->total>0?double(t->received)/t->total:0;m_progress=t->progress;}updateTask(false);});
    connect(reply,&QNetworkReply::finished,this,[this,reply]{
        const bool ok=reply->error()==QNetworkReply::NoError;m_reply.clear();reply->deleteLater();if(!ok||!m_file||!m_file->commit()){complete({},"下载未完成，请重试");return;}m_file.reset();
        const auto info=QFileInfo(m_requested);const auto base=info.absolutePath()+"/"+info.completeBaseName();
        if(m_prefix.startsWith("CTENFDAM")){
            m_status="正在自动还原 NCM 音频…";if(auto *t=activeTask())t->state="converting";updateTask();const auto input=m_partial;
            m_worker=QThread::create([this,input,base]{const auto result=restoreNcm(input,base);QMetaObject::invokeMethod(this,[this,result]{complete(result.path,result.error);},Qt::QueuedConnection);});connect(m_worker,&QThread::finished,m_worker,&QObject::deleteLater);m_worker->start();return;
        }
        const auto format=m_prefix.startsWith("fLaC")?QString("flac"):
            m_prefix.size()>=12&&m_prefix.mid(4,4)=="ftyp"?QString("m4a"):
            m_prefix.startsWith("OggS")?QString("ogg"):
            m_prefix.size()>=12&&m_prefix.startsWith("RIFF")&&m_prefix.mid(8,4)=="WAVE"?QString("wav"):
            (m_prefix.startsWith("ID3")||(m_prefix.size()>1&&quint8(m_prefix[0])==255&&(quint8(m_prefix[1])&224)==224))?QString("mp3"):QString();
        const auto target=base+"."+format;
        if(format.isEmpty()){complete({},"下载内容不是支持的音频格式");return;}
        if(QFileInfo::exists(target)){complete({},"本地文件已存在，已阻止重复下载");return;}
        if(!QFile::rename(m_partial,target)){complete({},"下载文件保存失败");return;}m_partial.clear();complete(target);
    });return true;
}
void OnlineDownloadBridge::complete(const QString &savedPath,const QString &error){
    const auto path=m_cancelled?QString():savedPath;if(m_cancelled&&!savedPath.isEmpty())QFile::remove(savedPath);
    m_file.reset();if(!m_partial.isEmpty())QFile::remove(m_partial);m_partial.clear();m_busy=false;m_progress=path.isEmpty()?0:1;
    if(!path.isEmpty()){m_downloads.insert(m_key,QJsonObject{{"path",path},{"title",m_song.value("title").toString()},{"artist",m_song.value("artist").toString()},{"duration",m_song.value("duration").toLongLong()}});m_library->setSetting("online.downloads",m_downloads);m_status="已下载："+QFileInfo(path).fileName();m_known.clear();m_downloadIndex.reset();++m_detectRevision;++m_revision;emit downloadsChanged();emit downloaded(path);}
    else m_status=m_cancelled?"已取消":error.isEmpty()?"下载未完成":error;
    if(auto *t=activeTask()){t->state=m_cancelled?"cancelled":path.isEmpty()?"failed":"completed";t->error=path.isEmpty()?m_status:QString();t->progress=m_progress;if(!path.isEmpty()){t->received=QFileInfo(path).size();t->total=t->received;}}
    m_busy=false;for(const auto &t:m_tasks)if(t.state=="queued"){m_busy=true;break;}
    updateTask();m_activeId=0;pruneTasks();QTimer::singleShot(0,this,&OnlineDownloadBridge::startNext);
}
void OnlineDownloadBridge::cancelTask(int id){
    if(id==m_activeId){cancel();return;}for(int i=0;i<m_tasks.size();++i)if(m_tasks[i].id==id&&m_tasks[i].state=="queued"){m_tasks[i].state="cancelled";emit dataChanged(index(i),index(i));++m_taskRevision;pruneTasks();emit tasksChanged();return;}
}
void OnlineDownloadBridge::cancel(){m_cancelled=true;if(m_reply)m_reply->abort();else if(auto *t=activeTask();t&&t->state=="resolving")complete({},"已取消");else if(m_worker&&m_worker->isRunning())m_worker->requestInterruption();}
}
