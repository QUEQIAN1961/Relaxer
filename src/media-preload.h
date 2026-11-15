#pragma once
#include <QMediaTimeRange>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTimer>

namespace relaxer {
// One active media file. Qt reads an ordinary seekable loopback HTTP source;
// the original connection continues downloading independently of playback.
class MediaPreload final : public QObject {
    Q_OBJECT
public:
    MediaPreload(const QString &cacheFolder, qint64 duration, QObject *parent=nullptr);
    ~MediaPreload() override;
    void cancel();
    void closeReaders();
    void start(const QUrl &source, const QMap<QByteArray,QByteArray> &headers={});
    void setProbeStream(const QString &stream){m_probeStream=stream;}
    QMediaTimeRange ranges() const { return m_ranges; }
    bool complete() const { return m_complete&&!m_failed; }
    QString cacheFile() const { return m_file.fileName(); }
signals:
    void ready(const QUrl &source);
    void rangesChanged();
    void failed(const QString &message);
private:
    struct Client { QByteArray request; qint64 offset=0,end=-1; bool parsed=false,headers=false,head=false; };
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    struct Transfer { qint64 start=0,end=0,received=0; };
    QHash<QNetworkReply *,Transfer> m_rangeReplies;
    QNetworkRequest m_request;
    QMediaTimeRange m_bytes;
    QTcpServer m_server;
    QHash<QTcpSocket *,Client> m_clients;
    QTemporaryFile m_file;
    QTimer m_probeTimer,m_probeTimeout;
    QProcess m_probe;
    QMediaTimeRange m_ranges;
    QString m_probeStream="a:0";
    QByteArray m_token,m_type="application/octet-stream";
    qint64 m_duration=0,m_available=0,m_total=-1,m_probed=0,m_probeSize=0,m_probeOffset=0;
    bool m_ready=false,m_complete=false,m_failed=false,m_rangeUnsupported=false;
    void receive();
    void announce();
    void pump(QTcpSocket *socket);
    void probe();
    void feedProbe();
    void fetchRange(qint64 offset);
    void fail(const QString &reason={});
};
}
