#include "import-coordinator.h"
#include "library.h"
#include "artwork-recovery.h"
#include <QJsonObject>
#include <QScopeGuard>
#include <QSet>
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
    ArtworkSummary artworkSummary;
    struct Plan {Track track;ArtworkFileVersion source,cover;QString identity;};
    QHash<qint64,Plan> artworkPlans;
    QSet<QString> attempted;
    bool shuttingDown=false,readySent=false;
    quint64 nextId=0;
    QString error;
    QTimer timer;
};
ImportCoordinator::ImportCoordinator(Library *library,QObject *parent):QObject(parent),d(std::make_unique<Impl>()){
    d->library=library;d->timer.setParent(this);d->timer.setInterval(40);
    connect(&d->timer,&QTimer::timeout,this,&ImportCoordinator::flushProgress);
    qRegisterMetaType<ImportResult>();qRegisterMetaType<ArtworkResult>();qRegisterMetaType<ImportSummary>();qRegisterMetaType<ArtworkSummary>();
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
bool ImportCoordinator::repairLegacyArtwork(bool retryFailed){
    if(!ownerThread()||!d->library||d->library->thread()!=thread()||d->shuttingDown||d->artwork)return false;
    QVector<Track> pending;d->artworkPlans.clear();
    // Read catalog identities only, never scan/decode every audio file at startup.
    for(const auto &track:d->library->tracks())if(isLegacyArtwork(track)) {
        const auto identity=artworkAttemptKey(d->library->root(),track);
        const auto receipt=d->library->setting(QString("artwork.recovery.v1.%1").arg(track.id)).toObject();
        const bool cooling=receipt["identity"].toString()==identity &&
            (receipt["retryAt"].toDouble()==0||receipt["retryAt"].toDouble()>QDateTime::currentMSecsSinceEpoch());
        if(!retryFailed&&(cooling||d->attempted.contains(identity)))continue;
        pending<<track;d->artworkPlans.insert(track.id,{track,artworkFileVersion(d->library->absolute(track.file)),
            artworkFileVersion(d->library->absolute(track.cover)),identity});
    }
    if(pending.isEmpty())return false;
    auto token=std::make_shared<std::atomic_bool>(false);ArtworkRepair *worker=nullptr;
    try {worker=createArtworkRepair(pending,token);}catch(...){d->error=QStringLiteral("无法创建封面修复任务");}
    if(!worker||worker->parent()||worker->thread()!=thread()){
        if(worker&&worker->thread()==thread()&&!worker->parent())delete worker;
        return false;
    }
    const auto gate=std::make_shared<QSemaphore>();worker->setResultGate(gate);
    worker->setCancellationToken(token);auto *session=new ImportWorkerSession(worker,[worker]{worker->run();});session->shared->cancel=token;
    d->artwork=session;d->artworkSummary={};d->artworkSummary.taskId=++d->nextId;d->artworkSummary.total=pending.size();
    d->artworkSealed=false;d->artworkExited=false;
    connect(worker,&ArtworkRepair::finished,session,[session]{session->seal(false);},Qt::DirectConnection);
    connect(worker,&ArtworkRepair::repaired,this,[this,session,token,gate](const ArtworkResult &result){
        const auto release=qScopeGuard([&]{gate->release();});
        if(d->artwork!=session||d->artworkSealed||!d->library||d->library->thread()!=QThread::currentThread())return;
        if(d->shuttingDown||token->load())return; // No queued writes after cancellation.
        auto &summary=d->artworkSummary;const auto plan=d->artworkPlans.take(result.id);
        if(!plan.track.id)return; // Duplicate or unexpected result cannot enlarge the frozen set.
        ++summary.processed;
        if(result.hash!=plan.track.hash||result.trackFile!=plan.track.file||result.previousCover!=plan.track.cover||
           !(result.sourceVersion==plan.source)||!(result.previousVersion==plan.cover)||
           (result.outcome!=ArtworkOutcome::Recovered&&!d->library->artworkSnapshotMatches(result))) {
            ++summary.stale;return;
        }
        d->attempted.insert(plan.identity);
        auto diagnostic=[&](const QString &error){if(summary.errors.size()<8)summary.errors<<QString("目标 %1：%2").arg(result.id).arg(error.left(1000));};
        bool changed=false,transient=false;
        switch(result.outcome) {
        case ArtworkOutcome::Recovered: {
            // applyArtworkRepair validates and holds the strong source guard;
            // avoid hashing the same source again in an unlocked preflight.
            {QSignalBlocker batch(d->library);changed=d->library->applyArtworkRepair(result);}
            if(changed)++summary.recovered;
            else {++summary.failed;transient=true;diagnostic(d->library->lastError());}
            break;
        }
        case ArtworkOutcome::NoSource:++summary.noSource;break;
        case ArtworkOutcome::InvalidImage:++summary.invalid;diagnostic(result.error);break;
        case ArtworkOutcome::ReadFailed:++summary.failed;transient=true;diagnostic(result.error);break;
        case ArtworkOutcome::Cancelled:summary.cancelled=true;return;
        }
        if(!changed) {if(result.previousUsable)++summary.retained;else ++summary.placeholder;}
        const auto receiptKey=QString("artwork.recovery.v1.%1").arg(result.id);
        const auto retryAt=transient?QDateTime::currentMSecsSinceEpoch()+24LL*60*60*1000:0;
        if(!d->library->setSetting(receiptKey,QJsonObject{{"identity",plan.identity},{"outcome",int(result.outcome)},
            {"recovered",changed},{"retryAt",double(retryAt)}}))diagnostic("封面处理回执未能保存；本次会话不会重复尝试");
        if(changed)emit artworkUpdated(result.id);
    },Qt::QueuedConnection);
    connect(session,&ImportWorkerSession::sealed,this,[this,session](bool cancelled,const QString &error){
        if(d->artwork!=session)return;
        d->artworkSealed=true;d->artworkSummary.cancelled|=cancelled;
        if(!error.isEmpty()){d->error=error;if(d->artworkSummary.errors.size()<8)d->artworkSummary.errors<<error.left(1000);}
        finishArtwork();
    },Qt::QueuedConnection);
    connect(session,&ImportWorkerSession::exited,this,[this,session]{if(d->artwork==session){d->artworkExited=true;finishArtwork();}});
    emit artworkStarted(pending.size());session->launch();return true;
}
void ImportCoordinator::finishArtwork(){
    if(!d->artwork||!d->artworkSealed||!d->artworkExited)return;
    auto summary=d->artworkSummary;summary.unstarted=qMax(0,summary.total-summary.processed);
    QPointer<ImportCoordinator> self(this);d->artwork=nullptr;d->artworkPlans.clear();
    emit artworkCompleted(summary);if(!self)return;
    emit artworkFinished(summary.recovered);if(self)finishShutdown();
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
