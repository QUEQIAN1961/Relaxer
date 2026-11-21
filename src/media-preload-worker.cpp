#include "media-preload-worker.h"
#include "disk-cache.h"
#include "importer.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <cmath>
#include <algorithm>

namespace relaxer {
MediaPreloadWorker::MediaPreloadWorker(const QString &folder,qint64 duration,QObject *parent)
    :QObject(parent),m_file(QDir(folder).filePath("playing-XXXXXX.media")),m_duration(duration){
    QDir().mkpath(folder);
    m_token=QUuid::createUuid().toByteArray(QUuid::Id128);
    m_probeTimer.setInterval(800);
    connect(&m_probeTimer,&QTimer::timeout,this,&MediaPreloadWorker::probe);
    m_probeTimeout.setSingleShot(true);m_probeTimeout.setInterval(8000);
    connect(&m_probeTimeout,&QTimer::timeout,this,[this]{m_probe.kill();});
    connect(&m_probe,&QProcess::started,this,&MediaPreloadWorker::feedProbe);
    connect(&m_probe,&QProcess::bytesWritten,this,[this]{feedProbe();});
    connect(&m_probe,&QProcess::readyReadStandardError,this,[this]{m_probe.readAllStandardError();});
    for(auto *object:{static_cast<QObject *>(&m_network),static_cast<QObject *>(&m_server),static_cast<QObject *>(&m_file),static_cast<QObject *>(&m_probeTimer),static_cast<QObject *>(&m_probeTimeout),static_cast<QObject *>(&m_probe)})object->setParent(this);
    m_probeClock.start();
    const auto publish=[this]{emit stateUpdated(m_ranges,complete(),m_file.fileName());};
    connect(this,&MediaPreloadWorker::rangesChanged,this,publish);
    connect(this,&MediaPreloadWorker::ready,this,publish);
    connect(this,&MediaPreloadWorker::failed,this,publish);
    connect(&m_probe,&QProcess::readyReadStandardOutput,this,&MediaPreloadWorker::readProbe);
    connect(&m_probe,&QProcess::finished,this,[this](int,QProcess::ExitStatus){m_probeTimeout.stop();readProbe();m_probeLines.clear();m_probe.readAllStandardError();});
    connect(&m_server,&QTcpServer::newConnection,this,[this]{while(m_server.hasPendingConnections()){
        auto *socket=m_server.nextPendingConnection();m_clients.insert(socket,{});
        connect(socket,&QTcpSocket::disconnected,this,[this,socket]{m_clients.remove(socket);socket->deleteLater();});
        connect(socket,&QTcpSocket::bytesWritten,this,[this,socket]{pump(socket);});
        connect(socket,&QTcpSocket::readyRead,this,[this,socket]{
            auto it=m_clients.find(socket);if(it==m_clients.end())return;auto &client=it.value();
            if(client.parsed){socket->readAll();return;}
            client.request+=socket->readAll();
            if(client.request.size()>16384){socket->disconnectFromHost();return;}
            if(!client.request.contains("\r\n\r\n"))return;
            const auto first=client.request.left(client.request.indexOf("\r\n")).split(' ');
            if(first.size()!=3||(first[0]!="GET"&&first[0]!="HEAD")||first[1]!="/"+m_token){socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");socket->disconnectFromHost();return;}
            client.head=first[0]=="HEAD";
            for(const auto &line:client.request.split('\n'))if(line.trimmed().toLower().startsWith("range: bytes=")){
                const auto bounds=line.trimmed().mid(13).split('-');
                if(bounds.size()!=2||bounds[0].isEmpty()){socket->disconnectFromHost();return;}
                bool ok=false;client.offset=bounds[0].toLongLong(&ok);
                if(!ok||client.offset<0){socket->disconnectFromHost();return;}
                if(!bounds[1].isEmpty()){client.end=bounds[1].toLongLong(&ok);if(!ok||client.end<client.offset){socket->disconnectFromHost();return;}}
            }
            client.parsed=true;pump(socket);
        });
    }});
}
void MediaPreloadWorker::closeReaders(){
    for(auto *socket:m_clients.keys()){socket->disconnect(this);socket->abort();socket->deleteLater();}
    m_clients.clear();
}
void MediaPreloadWorker::cancel(){
    // Qt can synchronously wait for its demuxer when replacing a source. Close
    // pending loopback reads first; they otherwise need this same event loop.
    m_failed=true;m_probeTimer.stop();m_probeTimeout.stop();m_server.close();
    if(m_probe.state()!=QProcess::NotRunning)m_probe.kill();
    if(m_reply){m_reply->disconnect(this);m_reply->abort();}
    for(auto *reply:m_rangeReplies.keys()){reply->disconnect(this);reply->abort();}
    closeReaders();
}
MediaPreloadWorker::~MediaPreloadWorker(){
    m_probeTimer.stop();m_probeTimeout.stop();
    if(m_reply){m_reply->disconnect(this);m_reply->abort();}
    for(auto *reply:m_rangeReplies.keys()){reply->disconnect(this);reply->abort();}
    closeReaders();
    m_probe.disconnect(this);if(m_probe.state()!=QProcess::NotRunning){m_probe.kill();m_probe.waitForFinished(1000);}
    for(auto *socket:m_clients.keys()){socket->disconnect(this);socket->abort();}
    DiskCache::releaseActive(m_file.fileName());
}
void MediaPreloadWorker::start(const QUrl &source,const QMap<QByteArray,QByteArray> &headers){
    if(!m_file.open()||!m_server.listen(QHostAddress::LocalHost)){fail();return;}
    DiskCache::markActive(m_file.fileName());
    emit stateUpdated(m_ranges,false,m_file.fileName());
    QNetworkRequest request(source);request.setTransferTimeout(20000);request.setAttribute(QNetworkRequest::Http2AllowedAttribute,false);
    request.setRawHeader("User-Agent","Mozilla/5.0");request.setRawHeader("Accept-Encoding","identity");
    for(auto it=headers.begin();it!=headers.end();++it)request.setRawHeader(it.key(),it.value());
    m_request=request;
    m_reply=m_network.get(request);
    connect(m_reply,&QNetworkReply::metaDataChanged,this,[this]{
        if(!m_reply)return;
        const int status=m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();if(status!=200&&status!=206)return;
        const auto length=m_reply->header(QNetworkRequest::ContentLengthHeader);
        if(length.isValid())m_total=length.toLongLong();
        const auto type=m_reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();if(!type.isEmpty()&&!type.contains('\r')&&!type.contains('\n'))m_type=type;
    });
    connect(m_reply,&QNetworkReply::readyRead,this,&MediaPreloadWorker::receive);
    connect(m_reply,&QNetworkReply::finished,this,[this]{
        receive();if(m_failed)return;
        if(m_reply->error()!=QNetworkReply::NoError){
            const auto status=m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto reason=status>=400?QString("服务器返回 HTTP %1").arg(status):m_reply->error()==QNetworkReply::TimeoutError?QStringLiteral("服务器连接超时"):m_reply->error()==QNetworkReply::HostNotFoundError?QStringLiteral("服务器地址无法解析"):QString("连接错误 %1").arg(int(m_reply->error()));
            fail(reason);return;
        }
        if(m_available<=0||(m_total>0&&m_available!=m_total)){fail(QStringLiteral("接收的数据不完整"));return;}
        m_complete=true;m_total=m_available;m_probeTimer.stop();
        m_ranges=QMediaTimeRange(0,m_duration);emit rangesChanged();announce();
        for(auto *socket:m_clients.keys())pump(socket);
    });
}
void MediaPreloadWorker::receive(){
    if(m_failed||!m_reply)return;
    const auto bytes=m_reply->readAll();if(bytes.isEmpty())return;
    const int status=m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();if(status!=200&&status!=206)return;
    if(!m_file.seek(m_available)||m_file.write(bytes)!=bytes.size()||!m_file.flush()){fail();return;}
    m_available+=bytes.size();announce();
    if(!m_probeTimer.isActive()&&!m_complete)m_probeTimer.start();
    for(auto *socket:m_clients.keys())pump(socket);
}
void MediaPreloadWorker::announce(){
    if(m_ready||m_failed||(!m_complete&&(m_total<=0||m_available<32768)))return;
    m_ready=true;emit ready(QUrl(QString("http://127.0.0.1:%1/%2").arg(m_server.serverPort()).arg(QString::fromLatin1(m_token))));
}
void MediaPreloadWorker::pump(QTcpSocket *socket){
    if(!m_clients.contains(socket)||m_failed||socket->state()!=QAbstractSocket::ConnectedState)return;
    auto &client=m_clients[socket];if(!client.parsed||m_total<=0)return;
    if(!client.headers){
        if(client.offset>=m_total){socket->write("HTTP/1.1 416 Range Not Satisfiable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");socket->disconnectFromHost();return;}
        client.end=client.end<0?m_total-1:qMin(client.end,m_total-1);client.headers=true;
        const bool ranged=client.request.toLower().contains("\r\nrange:");
        QByteArray response=ranged?"HTTP/1.1 206 Partial Content\r\n":"HTTP/1.1 200 OK\r\n";
        response+="Content-Type: "+m_type+"\r\nContent-Length: "+QByteArray::number(client.end-client.offset+1)+"\r\nAccept-Ranges: bytes\r\nConnection: close\r\n";
        if(ranged)response+="Content-Range: bytes "+QByteArray::number(client.offset)+"-"+QByteArray::number(client.end)+"/"+QByteArray::number(m_total)+"\r\n";
        socket->write(response+"\r\n");if(client.head){socket->disconnectFromHost();return;}
    }
    while(client.offset<=client.end&&socket->bytesToWrite()<262144){
        qint64 availableEnd=m_available-1;
        for(const auto &range:m_bytes.intervals())if(range.contains(client.offset)){availableEnd=qMax(availableEnd,range.end());break;}
        if(client.offset>availableEnd){fetchRange(client.offset);break;}
        if(!m_file.seek(client.offset)){fail();return;}
        const auto bytes=m_file.read(std::min<qint64>({65536,availableEnd-client.offset+1,client.end-client.offset+1}));
        if(bytes.isEmpty())break;
        const auto written=socket->write(bytes);if(written<0)return;client.offset+=written;
    }
    if(client.offset>client.end)socket->disconnectFromHost();
}
void MediaPreloadWorker::probe(){
    if(m_failed||m_complete||m_available==m_probed||m_probe.state()!=QProcess::NotRunning)return;
    // Re-probe only after useful growth, or a bounded freshness delay. Never
    // accumulate a full packet JSON document; parse bounded output chunks below.
    if(m_probed>0&&m_available-m_probed<qMax<qint64>(262144,m_probed/4)&&m_probeClock.elapsed()<4000)return;
    const auto executable=toolPath("ffprobe");if(executable.isEmpty())return;
    m_probeClock.restart();m_probeLines.clear();
    m_probed=m_available;m_probeSize=m_available;m_probeOffset=0;
    // Probe only the contiguous downloaded prefix, never sparse holes written
    // for a metadata/tail range requested by Qt.
    m_probe.start(executable,{"-v","error","-select_streams",m_probeStream,"-show_entries","packet=pts_time,duration_time,pos,size","-of","compact=p=0:nk=0","pipe:0"});
    m_probeTimeout.start();
}
void MediaPreloadWorker::readProbe(){
    if(m_failed||m_complete){m_probe.readAllStandardOutput();return;}
    m_probeLines+=m_probe.readAllStandardOutput();auto ranges=m_ranges;
    int newline=0;
    while((newline=m_probeLines.indexOf('\n'))>=0){
        const auto line=m_probeLines.left(newline);m_probeLines.remove(0,newline+1);
        double pts=0,duration=0;qint64 offset=-1,size=0;bool valid=false;
        for(const auto &field:line.split('|')){
            const auto p=field.indexOf('=');const auto name=field.left(p),value=field.mid(p+1);
            if(name=="pts_time")pts=value.toDouble(&valid);else if(name=="duration_time")duration=value.toDouble();else if(name=="pos")offset=value.toLongLong();else if(name=="size")size=value.toLongLong();
        }
        if(valid&&std::isfinite(pts)&&std::isfinite(duration)&&duration>0&&offset>=0&&size>0&&offset<=m_probeSize&&size<=m_probeSize-offset)ranges.addInterval(qMax<qint64>(0,qRound64(pts*1000)),qRound64((pts+duration)*1000));
    }
    if(m_probeLines.size()>16384)m_probeLines.clear();
    if(ranges!=m_ranges){m_ranges=ranges;emit rangesChanged();}
}
void MediaPreloadWorker::feedProbe(){
    if(m_probe.state()!=QProcess::Running)return;
    while(m_probeOffset<m_probeSize&&m_probe.bytesToWrite()<262144){
        if(!m_file.seek(m_probeOffset))return;
        const auto bytes=m_file.read(qMin<qint64>(65536,m_probeSize-m_probeOffset));if(bytes.isEmpty())return;
        const auto count=m_probe.write(bytes);if(count<0)return;m_probeOffset+=count;
    }
    if(m_probeOffset>=m_probeSize)m_probe.closeWriteChannel();
}
void MediaPreloadWorker::fetchRange(qint64 offset){
    if(m_rangeUnsupported||m_complete||m_failed)return;
    for(const auto &transfer:m_rangeReplies)if(offset>=transfer.start&&offset<=transfer.end)return;
    const auto end=qMin(m_total-1,offset+262143);
    auto request=m_request;request.setRawHeader("Range","bytes="+QByteArray::number(offset)+"-"+QByteArray::number(end));
    auto *reply=m_network.get(request);m_rangeReplies.insert(reply,{offset,end,0});
    connect(reply,&QNetworkReply::readyRead,this,[this,reply]{
        auto it=m_rangeReplies.find(reply);if(it==m_rangeReplies.end())return;
        const auto expected="bytes "+QByteArray::number(it->start)+"-";
        if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=206||!reply->rawHeader("Content-Range").startsWith(expected)){
            m_rangeUnsupported=true;reply->abort();return;
        }
        const auto bytes=reply->readAll();if(bytes.isEmpty())return;
        const auto start=it->start+it->received;
        if(start+bytes.size()>it->end+1){m_rangeUnsupported=true;reply->abort();return;}
        if(!m_file.seek(start)||m_file.write(bytes)!=bytes.size()||!m_file.flush()){fail();return;}
        it->received+=bytes.size();m_bytes.addInterval(start,start+bytes.size()-1);
        for(auto *socket:m_clients.keys())pump(socket);
    });
    connect(reply,&QNetworkReply::finished,this,[this,reply]{
        if(reply->error()!=QNetworkReply::NoError)m_rangeUnsupported=true;
        m_rangeReplies.remove(reply);reply->deleteLater();
        for(auto *socket:m_clients.keys())pump(socket);
    });
}
void MediaPreloadWorker::fail(const QString &reason){
    if(m_failed)return;m_failed=true;m_ranges.clear();m_probeTimer.stop();
    if(m_reply&&m_reply->isRunning())m_reply->abort();
    for(auto *socket:m_clients.keys())socket->disconnectFromHost();
    emit failed(QStringLiteral("网络音源加载中断")+(reason.isEmpty()?QString():QStringLiteral("：")+reason));
}
}
