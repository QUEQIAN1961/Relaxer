#pragma once
#include <QObject>
#include <QMediaTimeRange>
#include <QMap>
#include <QUrl>
#include <QPointer>
#include <QTimer>
namespace relaxer {
class MediaPreloadWorker;
// GUI-owned state projection. Network, file and ffprobe work lives on its own thread.
class MediaPreload final:public QObject {
    Q_OBJECT
public:
    MediaPreload(const QString &folder,qint64 duration,QObject *parent=nullptr);
    ~MediaPreload()override;
    void cancel();void closeReaders();
    void start(const QUrl &source,const QMap<QByteArray,QByteArray> &headers={});
    void setProbeStream(const QString &stream);
    QMediaTimeRange ranges()const{return m_ranges;}
    bool complete()const{return m_complete&&!m_cancelled;}
    QString cacheFile()const{return m_file;}
signals:
    void ready(const QUrl &source);void rangesChanged();void failed(const QString &message);
private:
    QPointer<MediaPreloadWorker> m_worker;
    QMediaTimeRange m_ranges;QString m_file;bool m_complete=false,m_cancelled=false;
};
}
