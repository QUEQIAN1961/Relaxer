#include "media-preload.h"
#include "media-preload-worker.h"
#include <QThread>
#include <QCoreApplication>
namespace relaxer {
MediaPreload::MediaPreload(const QString &folder,qint64 duration,QObject *parent):QObject(parent){
    auto *thread=new QThread;thread->setObjectName("mediaPreloadIO");
    m_worker=new MediaPreloadWorker(folder,duration);m_worker->moveToThread(thread);
    connect(thread,&QThread::finished,m_worker,&QObject::deleteLater);
    connect(thread,&QThread::finished,thread,&QObject::deleteLater);
    connect(QCoreApplication::instance(),&QCoreApplication::aboutToQuit,thread,[thread]{thread->quit();thread->wait(2000);});
    connect(m_worker,&MediaPreloadWorker::stateUpdated,this,[this](const QMediaTimeRange &ranges,bool complete,const QString &file){if(!m_cancelled){m_ranges=ranges;m_complete=complete;m_file=file;}},Qt::QueuedConnection);
    connect(m_worker,&MediaPreloadWorker::ready,this,[this](const QUrl &url){if(!m_cancelled)emit ready(url);},Qt::QueuedConnection);
    connect(m_worker,&MediaPreloadWorker::rangesChanged,this,[this]{if(!m_cancelled)emit rangesChanged();},Qt::QueuedConnection);
    connect(m_worker,&MediaPreloadWorker::failed,this,[this](const QString &error){if(!m_cancelled)emit failed(error);},Qt::QueuedConnection);
    thread->start();
}
MediaPreload::~MediaPreload(){
    if(!m_worker)return;auto *worker=m_worker.data();auto *thread=worker->thread();worker->disconnect(this);
    QMetaObject::invokeMethod(worker,[worker,thread]{worker->cancel();thread->quit();},Qt::QueuedConnection);
}
void MediaPreload::cancel(){m_cancelled=true;m_complete=false;m_ranges.clear();if(m_worker)QMetaObject::invokeMethod(m_worker,[worker=m_worker]{if(worker)worker->cancel();},Qt::QueuedConnection);}
void MediaPreload::closeReaders(){if(m_worker)QMetaObject::invokeMethod(m_worker,[worker=m_worker]{if(worker)worker->closeReaders();},Qt::QueuedConnection);}
void MediaPreload::setProbeStream(const QString &stream){if(m_worker)QMetaObject::invokeMethod(m_worker,[worker=m_worker,stream]{if(worker)worker->setProbeStream(stream);},Qt::QueuedConnection);}
void MediaPreload::start(const QUrl &source,const QMap<QByteArray,QByteArray> &headers){if(m_worker)QMetaObject::invokeMethod(m_worker,[worker=m_worker,source,headers]{if(worker)worker->start(source,headers);},Qt::QueuedConnection);}
}
