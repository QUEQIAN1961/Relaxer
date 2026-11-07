#pragma once
#include "playback-facade.h"
#include <QNetworkAccessManager>
#include <QSet>
#include <QTimer>
#include <QUrl>

namespace relaxer {
// Online queues carry an immediately usable thumbnail. Only the visible large
// player asks for the original; current/next thumbnails are cached ahead of time.
class OnlineArtworkCache final:public QObject {
    Q_OBJECT
public:
    OnlineArtworkCache(Library *library,PlaybackFacade *facade,QObject *parent=nullptr);
    ~OnlineArtworkCache() override;
    void prepare(Track &track,const QUrl &thumbnail,const QUrl &original,const QString &originalPath);
    void setExpanded(bool expanded);
private:
    struct Entry {
        QUrl thumbnail,original;QString thumbnailPath,originalPath;
        bool thumbnailReady=false,originalReady=false,thumbnailPending=false,originalPending=false;
    };
    Library *m_library;PlaybackFacade *m_facade;QNetworkAccessManager m_network;
    QHash<qint64,Entry> m_entries;QSet<QNetworkReply *> m_requests;QTimer m_prefetch;
    bool m_expanded=false;
    QString bestCover(const Entry &entry) const;
    void prefetch();
    void fetch(qint64 id,bool original);
    void publish(qint64 id);
};
}
