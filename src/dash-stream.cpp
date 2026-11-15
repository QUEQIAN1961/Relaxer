#include "dash-stream.h"
#include <QJsonArray>
namespace relaxer {
DashStream::DashStream(const QString &folder,qint64 duration,const QJsonObject &video,const QJsonObject &audio,
                       const QMap<QByteArray,QByteArray> &headers,QObject *parent)
    :QObject(parent),m_folder(folder),m_duration(duration),m_headers(headers){
    const QJsonObject tracks[]={video,audio};
    for(int i=0;i<2;++i)for(const auto &value:tracks[i].value("urls").toArray())m_urls[i].append(QUrl(value.toString()));
}
void DashStream::closeVideoReaders(){if(m_cache[0])m_cache[0]->closeReaders();}
void DashStream::closeReaders(){for(auto &cache:m_cache)if(cache)cache->closeReaders();}
void DashStream::cancel(){
    m_cancelled=true;
    for(auto &cache:m_cache)if(cache)cache->cancel();
}
void DashStream::setVideoEnabled(bool enabled){
    if(m_cancelled||m_videoEnabled==enabled)return;
    m_videoEnabled=enabled;if(!m_started)return;
    if(!enabled){
        closeVideoReaders();
        if(m_cache[0]&&!m_cache[0]->complete()){
            // Retire the interrupted cache on the next open, after its probe
            // has exited; closing the view must not wait on that process.
            m_cache[0]->cancel();m_local[0]=QUrl{};
        }
    }else if(!m_local[0].isEmpty())emit videoReady(m_local[0]);
    else if(m_urls[0].isEmpty())emit videoFailed(QStringLiteral("当前资源没有可用的视频轨"));
    else begin(0);
    emit rangesChanged();
}
void DashStream::start(){
    if(m_cancelled||m_started)return;
    if(m_urls[1].isEmpty()||m_duration<=0){emit failed(QStringLiteral("视频音轨地址不完整"));return;}
    m_started=true;begin(1);
    if(m_videoEnabled){
        if(m_urls[0].isEmpty())emit videoFailed(QStringLiteral("当前资源没有可用的视频轨"));
        else begin(0);
    }
}
void DashStream::begin(int index){
    if(m_cancelled||(index==0&&!m_videoEnabled))return;
    m_local[index]=QUrl{};
    m_cache[index]=std::make_unique<MediaPreload>(m_folder,m_duration);
    m_cache[index]->setProbeStream(index==0?QStringLiteral("v:0"):QStringLiteral("a:0"));
    connect(m_cache[index].get(),&MediaPreload::ready,this,[this,index](const QUrl &url){
        m_local[index]=url;
        if(index==0)emit videoReady(url);else emit ready(url);
    });
    connect(m_cache[index].get(),&MediaPreload::rangesChanged,this,&DashStream::rangesChanged);
    connect(m_cache[index].get(),&MediaPreload::failed,this,[this,index](const QString &message){
        if(m_local[index].isEmpty()&&m_index[index]+1<m_urls[index].size()){
            const QPointer<MediaPreload> cache=m_cache[index].get();
            ++m_index[index];QTimer::singleShot(0,this,[this,index,cache]{if(cache&&cache==m_cache[index].get())begin(index);});
        }else if(index==0)emit videoFailed(message);
        else emit failed(message);
    });
    m_cache[index]->start(m_urls[index][m_index[index]],m_headers);
}
QMediaTimeRange DashStream::ranges()const{
    if(!m_cache[1])return {};
    if(!m_videoEnabled)return m_cache[1]->complete()?QMediaTimeRange(0,m_duration):m_cache[1]->ranges();
    if(!m_cache[0])return {};
    if(m_cache[0]->complete()&&m_cache[1]->complete())return QMediaTimeRange(0,m_duration);
    const auto a=m_cache[0]->complete()?QMediaTimeRange(0,m_duration):m_cache[0]->ranges();
    const auto b=m_cache[1]->complete()?QMediaTimeRange(0,m_duration):m_cache[1]->ranges();QMediaTimeRange result;
    for(const auto &x:a.intervals())for(const auto &y:b.intervals()){const auto start=qMax(x.start(),y.start()),end=qMin(x.end(),y.end());if(end>start)result.addInterval(start,end);}
    return result;
}
}
