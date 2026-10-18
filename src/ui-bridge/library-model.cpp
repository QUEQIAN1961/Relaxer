#include "library-model.h"
#include <QThread>

namespace relaxer {
TrackListModel::TrackListModel(Library *library, QObject *parent) : QAbstractListModel(parent), m_library(library) {
    Q_ASSERT(library && library->thread() == thread());
    m_refresh.setSingleShot(true);
    connect(&m_refresh, &QTimer::timeout, this, &TrackListModel::refresh);
    connect(library, &Library::changed, this, [this] { if (!m_refresh.isActive()) m_refresh.start(0); });
    connect(library, &QObject::destroyed, this, [this] { m_library = nullptr; m_refresh.stop(); refresh(); });
    refresh();
}
int TrackListModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_tracks.size(); }
QHash<int, QByteArray> TrackListModel::roleNames() const {
    return {{TrackId, "trackId"}, {Title, "title"}, {Artist, "artist"}, {Album, "album"},
            {Duration, "duration"}, {Favorite, "favorite"}, {File, "file"}, {TrackKey, "trackKey"}};
}
QVariant TrackListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= m_tracks.size()) return {};
    const auto &t = m_tracks[index.row()];
    switch (role) {
    case TrackId: return t.id;
    case TrackKey: return QString::number(t.id); // Lossless command key across QML/JS.
    case Title: case Qt::DisplayRole: return t.title;
    case Artist: return t.artist;
    case Album: return t.album;
    case Duration: return t.duration;
    case Favorite: return t.favorite;
    case File: return t.file;
    default: return {};
    }
}
qint64 TrackListModel::idAt(int row) const { return row >= 0 && row < m_tracks.size() ? m_tracks[row].id : 0; }
int TrackListModel::rowForId(qint64 id) const { return m_rows.value(id, -1); }
QVector<qint64> TrackListModel::ids() const {
    QVector<qint64> result; result.reserve(m_tracks.size());
    for (const auto &track : m_tracks) result.append(track.id);
    return result;
}
bool TrackListModel::matches(int row, const QString &text) const {
    if (row < 0 || row >= m_tracks.size()) return false;
    const auto &track = m_tracks[row];
    return track.title.contains(text,Qt::CaseInsensitive) || track.artist.contains(text,Qt::CaseInsensitive) ||
           track.album.contains(text,Qt::CaseInsensitive);
}
void TrackListModel::setPlaylist(qint64 playlist) {
    if (m_playlist == playlist) return;
    m_playlist = playlist; m_refresh.stop(); refresh();
}
void TrackListModel::refresh() {
    Q_ASSERT(QThread::currentThread() == thread());
    const auto next = m_library ? m_library->tracks(m_playlist) : QVector<Track>{};
    beginResetModel();
    m_tracks = next; m_rows.clear(); m_rows.reserve(m_tracks.size());
    for (int row = 0; row < m_tracks.size(); ++row) m_rows.insert(m_tracks[row].id, row);
    endResetModel();
}

TrackFilterModel::TrackFilterModel(TrackListModel *source, QObject *parent)
    : QSortFilterProxyModel(parent), m_source(source) {
    setSourceModel(source); setDynamicSortFilter(true); sort(0);
    m_notifications.setSingleShot(true);
    connect(&m_notifications,&QTimer::timeout,this,&TrackFilterModel::changed);
    connect(this, &QAbstractItemModel::modelReset, this, &TrackFilterModel::changed);
    connect(this, &QAbstractItemModel::layoutChanged, this, &TrackFilterModel::changed);
    const auto notify = [this] { if (!m_notifications.isActive()) m_notifications.start(0); };
    connect(this, &QAbstractItemModel::rowsInserted, this, notify);
    connect(this, &QAbstractItemModel::rowsRemoved, this, notify);
}
void TrackFilterModel::changed() {
    m_notifications.stop();
    if (m_selected && (!m_source || m_source->rowForId(m_selected) < 0)) m_selected = 0;
    emit countChanged(); emit selectionChanged();
}
void TrackFilterModel::setFilterText(const QString &text) {
    if (m_filter == text) return;
    m_filter = text; invalidateFilter(); emit filterTextChanged(); changed();
}
void TrackFilterModel::setSortKey(int key) {
    if (key < 0 || key > 4 || m_sortKey == key) return;
    m_sortKey = key; invalidate(); sort(0, m_descending ? Qt::DescendingOrder : Qt::AscendingOrder);
    emit sortChanged(); emit selectionChanged();
}
void TrackFilterModel::setDescending(bool value) {
    if (m_descending == value) return;
    m_descending = value; sort(0, value ? Qt::DescendingOrder : Qt::AscendingOrder);
    emit sortChanged(); emit selectionChanged();
}
void TrackFilterModel::setSelectedId(const QString &id) {
    const auto value = id.toLongLong();
    if (value && (!m_source || m_source->rowForId(value) < 0)) return;
    if (m_selected != value) { m_selected = value; emit selectionChanged(); }
}
int TrackFilterModel::selectedRow() const {
    if (!m_source) return -1;
    const auto source = m_source->index(m_source->rowForId(m_selected), 0);
    const auto mapped = mapFromSource(source);
    return mapped.isValid() ? mapped.row() : -1;
}
QVector<qint64> TrackFilterModel::ids() const {
    QVector<qint64> result; result.reserve(rowCount());
    for (int row = 0; row < rowCount(); ++row) result.append(idAt(row).toLongLong());
    return result;
}
QString TrackFilterModel::idAt(int row) const { return data(index(row, 0), TrackListModel::TrackId).toString(); }
bool TrackFilterModel::filterAcceptsRow(int row, const QModelIndex &parent) const {
    Q_UNUSED(parent)
    if (m_filter.isEmpty()) return true;
    return m_source && m_source->matches(row,m_filter);
}
bool TrackFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    if (m_sortKey == 0) return left.row() < right.row();
    const int role = m_sortKey == 1 ? TrackListModel::Title : m_sortKey == 2 ? TrackListModel::Artist :
                     m_sortKey == 3 ? TrackListModel::Album : TrackListModel::Duration;
    const auto a = m_source->data(left, role), b = m_source->data(right, role);
    const int comparison = role == TrackListModel::Duration ? (a.toLongLong() < b.toLongLong() ? -1 : a == b ? 0 : 1)
        : QString::localeAwareCompare(a.toString(), b.toString());
    return comparison == 0 ? m_source->idAt(left.row()) < m_source->idAt(right.row()) : comparison < 0;
}

PlaylistListModel::PlaylistListModel(Library *library, QObject *parent) : QAbstractListModel(parent), m_library(library) {
    Q_ASSERT(library && library->thread() == thread());
    m_refresh.setSingleShot(true);
    const auto schedule = [this] { if (!m_refresh.isActive()) m_refresh.start(0); };
    connect(library, &Library::changed, this, schedule);
    // Only name/member count roles are exposed. Playback statistics must not
    // turn progress sampling into repeated full playlist aggregate queries.
    connect(&m_refresh, &QTimer::timeout, this, &PlaylistListModel::refresh);
    connect(library, &QObject::destroyed, this, [this] { m_library = nullptr; m_refresh.stop(); refresh(); });
    refresh();
}
int PlaylistListModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_playlists.size(); }
QHash<int, QByteArray> PlaylistListModel::roleNames() const {
    return {{Qt::UserRole + 1, "playlistId"}, {Qt::UserRole + 2, "name"}, {Qt::UserRole + 3, "trackCount"}, {Qt::UserRole + 4, "playlistKey"}};
}
QVariant PlaylistListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_playlists.size()) return {};
    const auto &p = m_playlists[index.row()];
    switch (role) {
    case Qt::UserRole + 1: return p.id;
    case Qt::UserRole + 4: return QString::number(p.id);
    case Qt::UserRole + 2: case Qt::DisplayRole: return p.name;
    case Qt::UserRole + 3: return p.count;
    default: return {};
    }
}
bool PlaylistListModel::contains(qint64 id) const {
    for (const auto &p : m_playlists) if (p.id == id) return true;
    return false;
}
void PlaylistListModel::refresh() {
    beginResetModel(); m_playlists = m_library ? m_library->playlists() : QVector<Playlist>{}; endResetModel();
    emit countChanged();
}
}
