#pragma once
#include "model.h"
#include <memory>
#include <QHash>
namespace relaxer {
class Library;
class OnlineTrackIndex {
public:
    explicit OnlineTrackIndex(const QVector<Track> &tracks,const QString &root={});
    QString path(qint64 id)const{return m_paths.value(id);}
    qint64 match(const Track &online,bool requireUnique=true,bool requireExisting=false)const;
private:
    struct Row{qint64 id=0,duration=0;QString title,artist,kind,path;};
    QHash<qint64,QString> m_paths;
    QVector<Row> m_rows;QHash<QString,QVector<int>> m_kinds;
};
// One immutable, normalized index shared by the player and download detection.
std::shared_ptr<const OnlineTrackIndex> sharedOnlineTrackIndex(Library *library);
qint64 matchOnlineTrack(const Track &online,const QVector<Track> &local,bool requireUnique=true);
}
