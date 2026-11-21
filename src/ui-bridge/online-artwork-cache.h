#pragma once
#include "playback-facade.h"
#include <QNetworkAccessManager>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QThreadPool>
#include <atomic>
#include <memory>

namespace relaxer {
// Online queues carry an immediately usable thumbnail. Only the visible large
// player asks for the original; current/next thumbnails are cached ahead of time.
class OnlineArtworkCache final:public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl thumbnailCover READ thumbnailCover NOTIFY thumbnailChanged)
public:
    OnlineArtworkCache(Library *library,PlaybackFacade *facade,QObject *parent=nullptr);
    ~OnlineArtworkCache() override;
    void prepare(Track &track,const QUrl &thumbnail,const QUrl &original,const QString &originalPath);
    void setExpanded(bool expanded);
    void setPresentationActive(bool active);
    QUrl thumbnailCover() const;
signals:
    void thumbnailChanged();
private:
    struct Entry {
        QUrl thumbnail,original;QString thumbnailPath,originalPath;
        bool thumbnailReady=false,originalReady=false,thumbnailPending=false,originalPending=false;
        qint64 thumbnailRetryAt=0,originalRetryAt=0;quint64 generation=0;
        std::shared_ptr<std::atomic_bool> valid=std::make_shared<std::atomic_bool>(true);
    };
    Library *m_library;PlaybackFacade *m_facade;QNetworkAccessManager m_network;
    QHash<qint64,Entry> m_entries;QSet<QNetworkReply *> m_requests;QTimer m_prefetch,m_trim;
    QThreadPool m_decodePool;int m_decoding=0;quint64 m_generation=0;
    std::shared_ptr<std::atomic_bool> m_decodeTicket=std::make_shared<std::atomic_bool>(true);
    bool m_expanded=false,m_presentationActive=true;
    QString bestCover(const Entry &entry) const;
    void prefetch();
    void fetch(qint64 id,bool original);
    void publish(qint64 id);
};
}
