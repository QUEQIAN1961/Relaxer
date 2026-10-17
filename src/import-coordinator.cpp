#include "import-coordinator.h"
#include "library.h"
#include <QEventLoopLocker>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QThread>
#include <QTimer>
#include <functional>
#include <mutex>
#include <stdexcept>

namespace relaxer {
// The session outlives its caller if necessary. It never owns a Library and
// deletes its worker/thread only after the thread has actually stopped.
class ImportWorkerSession final : public QObject {
    Q_OBJECT
public:
    struct Shared {
        std::mutex mutex;
        std::shared_ptr<std::atomic_bool> cancel=std::make_shared<std::atomic_bool>(false);
        bool terminal=false,dirty=false;
        int current=0,total=0;
        QString name;
    };
    std::shared_ptr<Shared> shared=std::make_shared<Shared>();
    explicit ImportWorkerSession(QObject *worker,std::function<void()> run)
        : workerThread(new QThread(this)) {
        setObjectName("relaxerImportWorkerSession");
        workerThread->setObjectName("relaxerImportWorkerThread");
        worker->moveToThread(workerThread);
        connect(workerThread,&QThread::started,worker,[this,run=std::move(run)]{
            QString error;
            try {run();}
            catch(const std::exception &exception){error=QStringLiteral("导入任务异常：")+QString::fromUtf8(exception.what());}
            catch(...){error=QStringLiteral("导入任务发生未知异常");}
            seal(false,error);
            workerThread->quit();
        });
        connect(workerThread,&QThread::finished,worker,&QObject::deleteLater);
        connect(workerThread,&QThread::finished,this,[this]{emit exited();deleteLater();});
    }
    void launch(){workerThread->start();}
    bool requestCancel(){
        std::lock_guard<std::mutex> lock(shared->mutex);
        if(shared->terminal)return false;
        shared->cancel->store(true);return true;
    }
    void seal(bool cancelled,const QString &error={}){
        {
            std::lock_guard<std::mutex> lock(shared->mutex);
            if(shared->terminal)return;
            shared->terminal=true;cancelled=cancelled||shared->cancel->load();
        }
        emit sealed(cancelled,error);
    }
    void updateProgress(int current,int total,const QString &name){
        std::lock_guard<std::mutex> lock(shared->mutex);
        if(shared->terminal)return;
        shared->total=qMax(shared->total,total);shared->current=qMax(shared->current,current);
        shared->name=name;shared->dirty=true;
    }
signals:
    void sealed(bool cancelled,const QString &error);
    void exited();
private:
    QThread *workerThread;
    QEventLoopLocker eventLoopLocker;
};

struct ImportCoordinator::Impl {
    struct Task {
        ImportSummary summary;
        QPointer<ImportWorkerSession> session;
        bool sealed=false,exited=false;
    };
    QPointer<Library> library;
    std::unique_ptr<Task> task;
    QPointer<ImportWorkerSession> artwork;
    bool artworkSealed=false,artworkExited=false;
    int corrected=0;
    bool shuttingDown=false,readySent=false;
    quint64 nextId=0;
    QString error;
    QTimer timer;
};
ImportCoordinator::ImportCoordinator(Library *library,QObject *parent):QObject(parent),d(std::make_unique<Impl>()){
    d->library=library;d->timer.setParent(this);d->timer.setInterval(40);
    connect(&d->timer,&QTimer::timeout,this,&ImportCoordinator::flushProgress);
    qRegisterMetaType<ImportResult>();qRegisterMetaType<ArtworkResult>();qRegisterMetaType<ImportSummary>();
}
ImportCoordinator::~ImportCoordinator(){
    d->timer.stop();
    if(d->task&&d->task->session)d->task->session->requestCancel();
    if(d->artwork)d->artwork->requestCancel();
    // QObject context connections discard queued business callbacks. Sessions
    // retain just the cancellation token and clean up asynchronously.
}
bool ImportCoordinator::ownerThread() const{return QThread::currentThread()==thread();}
bool ImportCoordinator::isBusy() const{return bool(d->task);}
bool ImportCoordinator::isArtworkBusy() const{return !d->artwork.isNull();}
bool ImportCoordinator::isIdle() const{return !isBusy()&&!isArtworkBusy();}
bool ImportCoordinator::isShuttingDown() const{return d->shuttingDown;}
quint64 ImportCoordinator::activeTaskId() const{return d->task?d->task->summary.taskId:0;}
QString ImportCoordinator::lastError() const{return d->error;}
Importer *ImportCoordinator::createImporter(const ImportRequest &request,const std::shared_ptr<std::atomic_bool> &cancel){
    auto *worker=new Importer(d->library->root(),request.sources);worker->setCancellationToken(cancel);
    worker->setMaximumWorkers(request.maximumWorkers);return worker;
}
ArtworkRepair *ImportCoordinator::createArtworkRepair(const QVector<Track> &tracks,const std::shared_ptr<std::atomic_bool> &cancel){
    auto *worker=new ArtworkRepair(d->library->root(),tracks);worker->setCancellationToken(cancel);return worker;
}
quint64 ImportCoordinator::start(const ImportRequest &request){
    if(!ownerThread()){emit requestRejected(QStringLiteral("导入请求必须在服务归属线程发起"));return 0;}
    d->error.clear();
    if(!d->library||d->library->thread()!=thread()||d->shuttingDown||d->task||request.sources.isEmpty()||request.targetPlaylist<0||request.maximumWorkers<0){
        d->error=QStringLiteral("当前无法开始导入：服务忙碌、正在关闭或请求无效");emit requestRejected(d->error);return 0;
    }
    const auto token=std::make_shared<std::atomic_bool>(false);Importer *worker=nullptr;
    try {worker=createImporter(request,token);}catch(...){d->error=QStringLiteral("无法创建导入任务");}
    if(!worker||worker->parent()||worker->thread()!=thread()){
        if(worker&&worker->thread()==thread()&&!worker->parent())delete worker;
        if(d->error.isEmpty())d->error=QStringLiteral("导入 worker 的所有权或线程无效");
        emit requestRejected(d->error);return 0;
    }
    // Bind even injected workers to the same token used by the lifecycle owner.
    worker->setCancellationToken(token);
    auto *session=new ImportWorkerSession(worker,[worker]{worker->run();});session->shared->cancel=token;
    const quint64 id=++d->nextId;
    d->task=std::make_unique<Impl::Task>();auto &task=*d->task;task.session=session;
    task.summary.taskId=id;task.summary.targetPlaylist=request.targetPlaylist;task.summary.sourceCount=request.sources.size();
    connect(worker,&Importer::progress,session,[session](int current,int total,const QString &name){session->updateProgress(current,total,name);},Qt::DirectConnection);
    connect(worker,&Importer::finished,session,[session](bool cancelled){session->seal(cancelled);},Qt::DirectConnection);
    connect(worker,&Importer::scanned,this,[this,id](int total,int ignored,bool complete){
        if(!d->task||d->task->summary.taskId!=id||d->task->sealed)return;
        d->task->summary.totalFiles=total;d->task->summary.ignoredPaths=ignored;d->task->summary.scanComplete=complete;
    },Qt::QueuedConnection);
    connect(worker,&Importer::imported,this,[this,id](const ImportResult &result){
        if(!d->task||d->task->summary.taskId!=id||d->task->sealed)return;
        auto &summary=d->task->summary;++summary.processed;
        auto fail=[&](const QString &error){++summary.failed;if(summary.errors.size()<8)summary.errors<<QFileInfo(result.source).fileName()+QStringLiteral("：")+error;};
        if(!result.error.isEmpty()){fail(result.error);return;}
        if(!d->library||d->library->thread()!=QThread::currentThread()){fail(QStringLiteral("资料库已关闭或线程已变化"));cancel(id);return;}
        bool added=false;qint64 trackId=0;
        {
        QSignalBlocker batch(d->library);
        trackId=d->library->insertImportedTrack(result.track,&added);
        if(!trackId){fail(d->library->lastError());return;}
        if(added)++summary.added;else ++summary.duplicates;
        if(summary.targetPlaylist&&!d->library->addToPlaylist(summary.targetPlaylist,{trackId})){
            ++summary.playlistFailures;if(summary.errors.size()<8)summary.errors<<QStringLiteral("歌曲已导入，但未加入目标歌单：")+d->library->lastError();
        }
        }
        emit trackCommitted(id,trackId,added);
    },Qt::QueuedConnection);
    connect(session,&ImportWorkerSession::sealed,this,[this,id](bool cancelled,const QString &error){
        if(!d->task||d->task->summary.taskId!=id||d->task->sealed)return;
        d->task->sealed=true;d->task->summary.cancelled=cancelled;d->task->summary.fatalError=error;finishImport();
    },Qt::QueuedConnection);
    connect(session,&ImportWorkerSession::exited,this,[this,id]{
        if(!d->task||d->task->summary.taskId!=id)return;
        d->task->exited=true;finishImport();
    });
    d->timer.start();session->launch();return id;
}
bool ImportCoordinator::cancel(quint64 taskId){
    if(!ownerThread()||!d->task||(taskId&&d->task->summary.taskId!=taskId)||!d->task->session)return false;
    return d->task->session->requestCancel();
}
void ImportCoordinator::flushProgress(){
    if(!d->task||!d->task->session)return;
    auto state=d->task->session->shared;int current,total;QString name;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if(!state->dirty)return;
        state->dirty=false;current=state->current;total=state->total;name=state->name;
    }
    emit progress(d->task->summary.taskId,current,total,name);
}
void ImportCoordinator::finishImport(){
    if(!d->task||!d->task->sealed||!d->task->exited)return;
    QPointer<ImportCoordinator> self(this);flushProgress();
    if(!self)return;
    d->timer.stop();auto summary=d->task->summary;
    summary.unstarted=qMax(0,summary.totalFiles-summary.processed);summary.skipped=summary.ignoredPaths+summary.unstarted;
    summary.partialSuccess=summary.added+summary.duplicates>0&&(summary.failed||summary.cancelled||summary.playlistFailures||summary.unstarted||!summary.fatalError.isEmpty());
    d->task.reset();emit finished(summary);if(self)finishShutdown();
}
bool ImportCoordinator::repairLegacyArtwork(){
    if(!ownerThread()||!d->library||d->library->thread()!=thread()||d->shuttingDown||d->artwork)return false;
    QVector<Track> pending;
    for(const auto &track:d->library->tracks())if(track.coverOrigin.isEmpty()&&track.cover=="Artwork/"+track.hash+".png")pending<<track;
    if(pending.isEmpty())return false;
    auto token=std::make_shared<std::atomic_bool>(false);ArtworkRepair *worker=nullptr;
    try {worker=createArtworkRepair(pending,token);}catch(...){d->error=QStringLiteral("无法创建封面修复任务");}
    if(!worker||worker->parent()||worker->thread()!=thread()){
        if(worker&&worker->thread()==thread()&&!worker->parent())delete worker;
        return false;
    }
    worker->setCancellationToken(token);auto *session=new ImportWorkerSession(worker,[worker]{worker->run();});session->shared->cancel=token;
    d->artwork=session;d->corrected=0;d->artworkSealed=false;d->artworkExited=false;
    connect(worker,&ArtworkRepair::finished,session,[session]{session->seal(false);},Qt::DirectConnection);
    connect(worker,&ArtworkRepair::repaired,this,[this,session](const ArtworkResult &result){
        if(d->artwork!=session||d->artworkSealed||!d->library||d->library->thread()!=QThread::currentThread())return;
        bool corrected=false;
        {QSignalBlocker batch(d->library);corrected=d->library->applyArtworkRepair(result);}
        if(corrected){++d->corrected;emit artworkUpdated(result.id);}
    },Qt::QueuedConnection);
    connect(session,&ImportWorkerSession::sealed,this,[this,session](bool,const QString &error){
        if(d->artwork!=session)return;
        d->artworkSealed=true;if(!error.isEmpty())d->error=error;finishArtwork();
    },Qt::QueuedConnection);
    connect(session,&ImportWorkerSession::exited,this,[this,session]{if(d->artwork==session){d->artworkExited=true;finishArtwork();}});
    session->launch();return true;
}
void ImportCoordinator::finishArtwork(){
    if(!d->artwork||!d->artworkSealed||!d->artworkExited)return;
    QPointer<ImportCoordinator> self(this);d->artwork=nullptr;emit artworkFinished(d->corrected);if(self)finishShutdown();
}
void ImportCoordinator::shutdown(){
    if(!ownerThread())return;
    d->shuttingDown=true;cancel();if(d->artwork)d->artwork->requestCancel();finishShutdown();
}
void ImportCoordinator::finishShutdown(){
    if(d->shuttingDown&&isIdle()&&!d->readySent){d->readySent=true;emit shutdownReady();}
}
}
#include "import-coordinator.moc"
