#pragma once
#include <QObject>
#include <QTimer>
#include <QThreadPool>
#include <QRunnable>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QHash>
#include <QVariantMap>
#include <QCryptographicHash>
#include <atomic>
#include <memory>
#include <functional>

namespace relaxer {
// GUI-owned submission queue. Only immutable payloads and paths cross threads.
// Coalesce before serialization; draining/waiting is reserved for destruction.
class AsyncJsonStore final:public QObject {
public:
    using Completion=std::function<void(bool,qint64,QByteArray)>;
    AsyncJsonStore(){m_pool.setMaxThreadCount(1);m_pool.setExpiryTimeout(5000);m_delay.setSingleShot(true);m_delay.setInterval(120);connect(&m_delay,&QTimer::timeout,this,[this]{flush();});}
    ~AsyncJsonStore()override{drain();}
    void save(const QString &path,QVariantMap value,std::shared_ptr<void> lease={},Completion done={}){
        Job job;job.path=path;job.value=std::move(value);job.lease=std::move(lease);job.done=std::move(done);enqueue(std::move(job));
    }
    void saveJson(const QString &path,QJsonObject value,std::shared_ptr<void> lease={},Completion done={}){
        Job job;job.path=path;job.json=std::move(value);job.direct=true;job.lease=std::move(lease);job.done=std::move(done);enqueue(std::move(job));
    }
    void cancel(){m_delay.stop();for(const auto &job:m_pending)job.valid->store(false);m_pending.clear();for(const auto &valid:m_active)valid->store(false);}
    void drain(){m_delay.stop();flush();m_pool.waitForDone();}
private:
    struct Job {QString path;QVariantMap value;QJsonObject json;bool direct=false;std::shared_ptr<void> lease;Completion done;std::shared_ptr<std::atomic_bool> valid=std::make_shared<std::atomic_bool>(true);};
    QHash<QString,Job> m_pending;QHash<QString,std::shared_ptr<std::atomic_bool>> m_active;
    QThreadPool m_pool;QTimer m_delay;
    void enqueue(Job job){
        if(auto it=m_pending.find(job.path);it!=m_pending.end())it->valid->store(false);
        if(auto valid=m_active.value(job.path))valid->store(false);
        const auto path=job.path;m_pending.insert(path,std::move(job));m_delay.start();
    }
    void flush(){
        auto jobs=std::move(m_pending);m_pending.clear();
        for(auto it=jobs.begin();it!=jobs.end();++it){auto job=std::move(it.value());m_active[job.path]=job.valid;
            m_pool.start(QRunnable::create([this,job=std::move(job)]{
                bool ok=false;qint64 size=0;QByteArray hash;
                if(job.valid->load()){
                    const auto bytes=QJsonDocument(job.direct?job.json:QJsonObject::fromVariantMap(job.value)).toJson(QJsonDocument::Compact);size=bytes.size();
                    hash=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);
                    if(job.valid->load()){QSaveFile file(job.path);ok=file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&job.valid->load()&&file.commit();}
                }
                QMetaObject::invokeMethod(this,[this,job,ok,size,hash]{
                    if(m_active.value(job.path)==job.valid)m_active.remove(job.path);
                    if(job.valid->load()&&job.done)job.done(ok,size,hash);
                },Qt::QueuedConnection);
            }));
        }
    }
};
inline qint64 variantMemoryCost(const QVariant &value){
    if(value.metaType().id()==QMetaType::QVariantMap){qint64 size=96;const auto map=value.toMap();for(auto it=map.cbegin();it!=map.cend();++it)size+=96+it.key().size()*2+variantMemoryCost(it.value());return size;}
    if(value.metaType().id()==QMetaType::QVariantList){qint64 size=64;const auto list=value.toList();for(const auto &item:list)size+=32+variantMemoryCost(item);return size;}
    return value.metaType().id()==QMetaType::QString?64+value.toString().size()*2:32;
}
}
