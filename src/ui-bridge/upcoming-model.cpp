#include "upcoming-model.h"
#include <QThread>
#include <QDateTime>
#include <QFileInfo>

namespace relaxer {
namespace {
bool sameMetadata(const Track &left, const Track &right) {
    return left.id == right.id && left.title == right.title && left.artist == right.artist &&
        left.album == right.album && left.duration == right.duration && left.favorite == right.favorite &&
        left.cover == right.cover && isVideoTrack(left)==isVideoTrack(right);
}
}
UpcomingModel::UpcomingModel(Library *library, PlaybackFacade *facade, QObject *parent)
    : QAbstractListModel(parent), m_library(library), m_facade(facade) {
    Q_ASSERT(library && facade && library->thread() == thread() && facade->thread() == thread());
    m_refresh.setSingleShot(true);
    connect(&m_refresh, &QTimer::timeout, this, &UpcomingModel::refresh);
    connect(library, &Library::changed, this, [this] { schedule(false, true); });
    connect(facade, &PlaybackFacade::snapshotChanged, this, [this] { observe(); });
    connect(facade, &PlaybackFacade::trackChanged, this, [this] { schedule(true); });
    connect(facade, &PlaybackFacade::onlineTrackUpdated, this, [this] { schedule(false,true); });
    // A mode command can rebuild the existing shuffle plan even when its enum
    // value is unchanged. State notifications cover that command; the ordinary
    // position notifications and progress snapshots never forecast or query.
    connect(facade, &PlaybackFacade::stateChanged, this, [this] { schedule(true); });
    connect(facade, &PlaybackFacade::saved, this, [this] { observe(); });
    connect(library, &QObject::destroyed, this, [this] {
        m_library = nullptr; m_refresh.stop(); refresh();
    });
    // PlaybackFacade disconnects its own outgoing signals before destruction.
    connect(facade->backend(), &QObject::destroyed, this, [this] {
        m_facade = nullptr; m_refresh.stop(); refresh();
    });
    refresh();
}
int UpcomingModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : count(); }
QHash<int, QByteArray> UpcomingModel::roleNames() const {
    return {{TrackId, "trackId"}, {TrackKey, "trackKey"}, {Title, "title"}, {Artist, "artist"},
        {Album, "album"}, {Duration, "duration"}, {Favorite, "favorite"}, {CoverUrl, "coverUrl"},
        {CoverRevision, "coverRevision"}, {UpcomingIndex, "upcomingIndex"}, {VideoTrack,"videoTrack"}};
}
QVariant UpcomingModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= count()) return {};
    const auto &track = m_tracks[index.row()];
    switch (role) {
    case TrackId: return track.id;
    case TrackKey: return QString::number(track.id);
    case Title: case Qt::DisplayRole: return track.title;
    case Artist: return track.artist;
    case Album: return track.album;
    case Duration: return track.duration;
    case Favorite: return track.favorite;
    case CoverUrl: return artwork(track);
    case CoverRevision: return m_artworkRevisions.value(index.row());
    case UpcomingIndex: return index.row();
    case VideoTrack: return isVideoTrack(track);
    default: return {};
    }
}
QUrl UpcomingModel::artwork(const Track &track) const {
    const QUrl source(track.cover);if(source.scheme()=="https"||source.scheme()=="http")return source;
    return m_library && !track.cover.isEmpty() ? QUrl::fromLocalFile(m_library->absolute(track.cover)) : QUrl();
}
QString UpcomingModel::artworkRevision(const Track &track) const {
    const auto url = artwork(track);
    if (url.isEmpty()) return {};
    if(!url.isLocalFile())return url.toString();
    const QFileInfo image(url.toLocalFile());
    return QString::number(image.lastModified().toMSecsSinceEpoch()) + ':' + QString::number(image.size());
}
QString UpcomingModel::idAt(int row) const { return row >= 0 && row < count() ? QString::number(m_ids[row]) : QString("0"); }
void UpcomingModel::schedule(bool forecast, bool metadata) {
    m_forecastDirty |= forecast;
    m_metadataDirty |= metadata;
    if (!m_refresh.isActive()) m_refresh.start(0);
}
void UpcomingModel::observe() {
    if (!m_facade || m_facade->isShutdown()) { schedule(true); return; }
    const auto &source = m_facade->queue();
    // QVector copies share their backing storage. This is constant work on
    // progress; neither comparing every queue row nor forecasting is needed.
    const bool storageChanged = (!source.items.isEmpty() || !m_sourceIds.isEmpty()) && !source.items.isSharedWith(m_sourceIds);
    if (storageChanged || source.index != m_sourceIndex ||
        source.current() != m_current || source.mode != m_mode) schedule(true);
}
void UpcomingModel::refresh() {
    Q_ASSERT(QThread::currentThread() == thread());
    m_refresh.stop();
    const bool alive = m_library && m_facade && !m_facade->isShutdown();
    const Track previousFirst = m_tracks.isEmpty() ? Track{} : m_tracks.first();
    const QUrl previousArtwork = firstCoverUrl();
    const auto previousRevision = firstCoverRevision();
    const int previousCount = count();
    QVector<qint64> ids;
    if (alive) {
        const auto &source = m_facade->queue();
        m_sourceIds = source.items; m_sourceIndex = source.index; m_current = source.current(); m_mode = source.mode;
        if (m_forecastDirty) { ++m_forecastReads; ids = m_facade->upcoming(); }
        else ids = m_ids;
    } else { m_sourceIds.clear(); m_sourceIndex = -1; m_current = 0; }
    const bool orderChanged = ids != m_ids;
    QVector<Track> tracks;
    QVector<QString> revisions;
    if (alive && !ids.isEmpty() && (orderChanged || m_metadataDirty)) {
        ++m_metadataReads;
        const auto selected = m_library->tracksByIds(ids);
        tracks.reserve(ids.size());
        for (const auto id : ids) {
            auto track = selected.value(id);if(!track.id)track=m_facade->track(id);
            if (!track.id) { track.id = id; track.title = "歌曲已从曲库移除"; }
            revisions.append(artworkRevision(track));
            tracks.append(std::move(track));
        }
    } else if (alive && !orderChanged) { tracks = m_tracks; revisions = m_artworkRevisions; }
    m_forecastDirty = false; m_metadataDirty = false;
    if (orderChanged) {
        beginResetModel(); m_ids = std::move(ids); m_tracks = std::move(tracks); m_artworkRevisions = std::move(revisions); endResetModel();
    } else if (tracks.size() == m_tracks.size()) {
        int first = -1, last = -1;
        for (int row = 0; row < tracks.size(); ++row) {
            if (sameMetadata(m_tracks[row], tracks[row]) && m_artworkRevisions.value(row) == revisions.value(row)) continue;
            if (first < 0) first = row;
            last = row;
        }
        m_tracks = std::move(tracks);
        m_artworkRevisions = std::move(revisions);
        if (first >= 0) emit dataChanged(index(first), index(last), {Title, Artist, Album, Duration, Favorite, CoverUrl, CoverRevision, VideoTrack});
    }
    if (previousCount != count()) emit countChanged();
    const Track first = m_tracks.isEmpty() ? Track{} : m_tracks.first();
    if (!sameMetadata(previousFirst, first) || previousArtwork != firstCoverUrl() || previousRevision != firstCoverRevision()) emit firstChanged();
}
QStringList UpcomingModel::selectableKeys() const {
    QStringList keys;if(!m_facade)return keys;
    for(auto id:m_ids)if(id!=m_facade->current().id&&!keys.contains(QString::number(id)))keys.append(QString::number(id));return keys;
}
bool UpcomingModel::canMove(const QString &key,int direction) const {
    if(!m_facade||m_facade->isShutdown()||(direction!=-1&&direction!=1)||!m_ids.contains(key.toLongLong()))return false;
    const auto order=m_facade->editableUpcoming();const int at=order.indexOf(key.toLongLong());return at>=0&&at+direction>=0&&at+direction<order.size();
}
bool UpcomingModel::moveTrack(const QString &key,int direction){
    if(!m_facade||m_facade->upcoming()!=m_ids){schedule(true);emit operationFailed("待播列表已变化，请重新选择");return false;}
    if(!canMove(key,direction))return false;
    auto order=m_facade->editableUpcoming();const int at=order.indexOf(key.toLongLong());order.swapItemsAt(at,at+direction);
    const bool ok=m_facade->editUpcoming(order);if(ok){schedule(true);refresh();}return ok;
}
bool UpcomingModel::removeTracks(const QStringList &keys){
    if(!m_facade||m_facade->isShutdown()||keys.isEmpty())return false;
    if(m_facade->upcoming()!=m_ids){schedule(true);emit operationFailed("待播列表已变化，请重新选择");return false;}
    auto order=m_facade->editableUpcoming();QSet<qint64> selected;
    for(const auto &key:keys){const auto id=key.toLongLong();if(!m_ids.contains(id)||id==m_facade->current().id)return false;selected.insert(id);}
    for(auto id:selected)order.removeAll(id);
    const bool ok=m_facade->editUpcoming(order);if(ok){schedule(true);refresh();}return ok;
}
bool UpcomingModel::clearPending(){
    if(!m_facade||m_facade->isShutdown()||!count())return false;
    if (!m_facade->editUpcoming({})) return false;
    m_facade->pause();
    schedule(true);
    refresh();
    return true;
}
bool UpcomingModel::locateTrack(const QString &key){
    if(!m_facade||!m_ids.contains(key.toLongLong())||!m_facade->queue().items.contains(key.toLongLong()))return false;
    emit locateRequested(key);return true;
}
bool UpcomingModel::activate(int row) {
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_library || !m_facade || m_facade->isShutdown() || row < 0 || row >= count()) return false;
    const auto expected = m_ids[row];
    const auto current = m_facade->upcoming();
    if (row >= current.size() || current[row] != expected || !m_facade->track(expected).id) {
        schedule(true, true); return false;
    }
    emit aboutToActivate(row, QString::number(expected), artwork(m_tracks[row]));
    m_facade->playUpcoming(row);
    schedule(true);
    return true;
}
}
