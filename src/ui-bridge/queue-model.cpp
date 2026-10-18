#include "queue-model.h"
#include <QThread>

namespace relaxer {
QueueListModel::QueueListModel(Library *library, PlaybackFacade *facade, QObject *parent)
    : QAbstractListModel(parent), m_library(library), m_facade(facade) {
    Q_ASSERT(library->thread() == thread() && facade->thread() == thread());
    m_refresh.setSingleShot(true);
    connect(&m_refresh, &QTimer::timeout, this, &QueueListModel::refresh);
    connect(library, &Library::changed, this, &QueueListModel::schedule);
    connect(library, &QObject::destroyed, this, [this] { m_library = nullptr; m_refresh.stop(); refresh(); });
    connect(facade, &PlaybackFacade::snapshotChanged, this, [this] {
        if (!m_facade) return;
        const auto &ids = m_facade->queue().items;
        // Progress snapshots share the same QVector backing store. No library
        // scan, model reset, or row-by-row work is needed for these events.
        if (!ids.isSharedWith(m_ids) && ids != m_ids) schedule();
    });
    connect(facade->backend(), &QObject::destroyed, this, [this] { m_facade = nullptr; m_refresh.stop(); refresh(); });
    refresh();
}
void QueueListModel::schedule() { if (!m_refresh.isActive()) m_refresh.start(0); }
int QueueListModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_tracks.size(); }
QHash<int, QByteArray> QueueListModel::roleNames() const {
    return {{TrackListModel::TrackId,"trackId"},{TrackListModel::TrackKey,"trackKey"},
            {TrackListModel::Title,"title"},{TrackListModel::Artist,"artist"},{TrackListModel::Album,"album"},
            {TrackListModel::Duration,"duration"},{TrackListModel::Favorite,"favorite"}};
}
QVariant QueueListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size()) return {};
    const auto &t = m_tracks[index.row()];
    switch (role) {
    case TrackListModel::TrackId: return t.id;
    case TrackListModel::TrackKey: return QString::number(t.id);
    case TrackListModel::Title: case Qt::DisplayRole: return t.title;
    case TrackListModel::Artist: return t.artist;
    case TrackListModel::Album: return t.album;
    case TrackListModel::Duration: return t.duration;
    case TrackListModel::Favorite: return t.favorite;
    default: return {};
    }
}
int QueueListModel::indexForKey(const QString &id) const { return m_ids.indexOf(id.toLongLong()); }
void QueueListModel::refresh() {
    Q_ASSERT(QThread::currentThread() == thread());
    const auto ids = m_facade && m_library ? m_facade->queue().items : QVector<qint64>{};
    QVector<Track> rows; QVector<qint64> missing;
    if (!ids.isEmpty()) {
        QHash<qint64,Track> catalog;
        const auto snapshot = m_library->tracks(); catalog.reserve(snapshot.size()); rows.reserve(ids.size());
        for (const auto &track : snapshot) catalog.insert(track.id,track);
        for (auto id : ids) {
            auto t = catalog.value(id);
            if (!t.id) { missing.append(id); t.id = id; t.title = "歌曲已从曲库移除"; }
            rows.append(t);
        }
    }
    beginResetModel(); m_ids = ids; m_tracks = std::move(rows); endResetModel(); emit countChanged();
    // Detection is a notification only. A command bridge revalidates and asks
    // the existing facade to forget missing IDs after this reset completes.
    if (!missing.isEmpty()) emit missingTracks(missing);
}
}
