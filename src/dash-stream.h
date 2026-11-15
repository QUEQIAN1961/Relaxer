#pragma once
#include "media-preload.h"
#include <QJsonObject>
#include <memory>
namespace relaxer {
// Keep DASH track downloads independent. Audio is the playback clock; the
// optional picture reader can come and go without replacing the audio source.
class DashStream final:public QObject {
    Q_OBJECT
public:
    DashStream(const QString &folder,qint64 duration,const QJsonObject &video,const QJsonObject &audio,
               const QMap<QByteArray,QByteArray> &headers,QObject *parent=nullptr);
    void start();
    void cancel();
    void closeReaders();
    void closeVideoReaders();
    void setVideoEnabled(bool enabled);
    bool videoEnabled()const{return m_videoEnabled;}
    bool hasVideo()const{return !m_urls[0].isEmpty();}
    QMediaTimeRange ranges()const;
signals:
    void ready(const QUrl &source);
    void videoReady(const QUrl &source);
    void videoFailed(const QString &message);
    void rangesChanged();
    void failed(const QString &message);
private:
    QString m_folder; qint64 m_duration;
    QList<QUrl> m_urls[2];int m_index[2]={0,0};
    std::unique_ptr<MediaPreload> m_cache[2];QUrl m_local[2];
    QMap<QByteArray,QByteArray> m_headers;
    bool m_videoEnabled=true,m_started=false,m_cancelled=false;
    void begin(int index);
};
}
