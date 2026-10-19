#include "library-model.h"
#include <QThread>
#include <QFileInfo>
#include <QDateTime>
#include <QUrlQuery>
#include <algorithm>

namespace relaxer {
namespace {
QUrl artworkUrl(const QString &path) {
    if(path.isEmpty())return {};
    const QFileInfo info(path);auto url=QUrl::fromLocalFile(path);QUrlQuery query;
    query.addQueryItem("revision",QString::number(info.lastModified().toMSecsSinceEpoch())+":"+QString::number(info.size()));
    url.setQuery(query);return url;
}
}

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
            {Duration, "duration"}, {Favorite, "favorite"}, {File, "file"}, {TrackKey, "trackKey"},
            {Cover, "cover"}, {Plays, "plays"}, {Heard, "heard"}, {ImportedAt, "importedAt"},
            {LyricsAvailable, "lyricsAvailable"}, {FileSize, "fileSize"}};
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
    case Cover: return m_library && !t.cover.isEmpty() ? artworkUrl(m_library->absolute(t.cover)) : QUrl{};
    case Plays: return t.plays;
    case Heard: return t.heard;
    case ImportedAt: return t.importedAt;
    case LyricsAvailable: return !t.lyrics.isEmpty();
    case FileSize: {
        if (!m_library) return qint64(-1);
        const auto path = m_library->absolute(t.file);
        if (!m_fileSizes.contains(path)) {
            const QFileInfo file(path);
            m_fileSizes.insert(path, file.isFile() ? file.size() : qint64(-1));
        }
        return m_fileSizes.value(path);
    }
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
    auto next = !m_library ? QVector<Track>{} : m_playlist == -2 ? m_library->recent() :
                m_library->tracks(m_playlist == -3 ? 0 : m_playlist);
    if (m_playlist == -3)
        std::stable_sort(next.begin(), next.end(), [](const Track &a, const Track &b) { return a.plays > b.plays; });
    beginResetModel();
    m_tracks = next; m_rows.clear(); m_rows.reserve(m_tracks.size()); m_fileSizes.clear();
    for (int row = 0; row < m_tracks.size(); ++row) m_rows.insert(m_tracks[row].id, row);
    endResetModel();
}
void TrackListModel::refreshStatistics(qint64 trackId) {
    if (!m_library || trackId <= 0) return;
    const auto row = rowForId(trackId); if (row < 0) return;
    // The facade identifies the one track that accumulated listening time;
    // keep ordinary progress independent of total library size.
    auto &track = m_tracks[row]; const auto updated = m_library->track(trackId);
    if (!updated.id || (track.plays == updated.plays && track.heard == updated.heard)) return;
    track.plays = updated.plays; track.heard = updated.heard;
    emit dataChanged(index(row), index(row), {Plays, Heard});
}

namespace {
QStringList artistNames(const QString &artist) {
    if (artist.contains("AC/DC", Qt::CaseInsensitive)) return {artist};
    QStringList names;
    for (const auto &part : artist.split('/', Qt::SkipEmptyParts))
        if (!part.trimmed().isEmpty()) names.append(part.trimmed());
    return names;
}
bool matchesArtist(const QString &value, const QString &filter) {
    if (filter.isEmpty() || value.compare(filter, Qt::CaseInsensitive) == 0) return true;
    for (const auto &name : artistNames(value)) if (name.compare(filter, Qt::CaseInsensitive) == 0) return true;
    return false;
}
void normalizeOptions(QStringList &options) {
    options.removeAll(QString()); options.removeDuplicates(); options.sort(Qt::CaseInsensitive);
}
}

TrackFilterModel::TrackFilterModel(TrackListModel *source, QObject *parent)
    : QSortFilterProxyModel(parent), m_source(source) {
    setSourceModel(source); setDynamicSortFilter(true); sort(0);
    m_notifications.setSingleShot(true);
    connect(&m_notifications,&QTimer::timeout,this,&TrackFilterModel::changed);
    connect(this, &QAbstractItemModel::modelReset, this, &TrackFilterModel::changed);
    connect(source, &QAbstractItemModel::modelReset, this, [this] { emit optionsChanged(); });
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
    if (key < 0 || key > 8 || m_sortKey == key) return;
    static const int roles[]{Qt::DisplayRole, TrackListModel::Title, TrackListModel::Artist, TrackListModel::Album,
        TrackListModel::Duration, TrackListModel::Plays, TrackListModel::ImportedAt, TrackListModel::Heard, TrackListModel::FileSize};
    m_sortKey = key; setSortRole(roles[key]); invalidate(); sort(0, m_descending ? Qt::DescendingOrder : Qt::AscendingOrder);
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
void TrackFilterModel::setArtistFilter(const QString &artist) {
    if (m_artist == artist) return;
    m_artist = artist;
    // Album choices follow the selected artist, as in the stable filter menu.
    if (!m_album.isEmpty() && !albums().contains(m_album, Qt::CaseInsensitive)) m_album.clear();
    invalidateFilter(); emit filtersChanged(); emit optionsChanged(); changed();
}
void TrackFilterModel::setAlbumFilter(const QString &album) {
    if (m_album == album) return;
    m_album = album; invalidateFilter(); emit filtersChanged(); changed();
}
QStringList TrackFilterModel::artists() const {
    QStringList options;
    if (m_source) for (int row = 0; row < m_source->rowCount(); ++row) {
        const auto artist = m_source->data(m_source->index(row), TrackListModel::Artist).toString();
        options.append(artist); options.append(artistNames(artist));
    }
    normalizeOptions(options); return options;
}
QStringList TrackFilterModel::albums() const {
    QStringList options;
    if (m_source) for (int row = 0; row < m_source->rowCount(); ++row) {
        const auto index = m_source->index(row);
        if (matchesArtist(m_source->data(index, TrackListModel::Artist).toString(), m_artist))
            options.append(m_source->data(index, TrackListModel::Album).toString());
    }
    normalizeOptions(options); return options;
}
qint64 TrackFilterModel::totalDuration() const {
    qint64 total = 0;
    for (int row = 0; row < rowCount(); ++row) total += data(index(row, 0), TrackListModel::Duration).toLongLong();
    return total;
}
qint64 TrackFilterModel::totalFileSize() const {
    qint64 total = 0;
    for (int row = 0; row < rowCount(); ++row)
        total += qMax<qint64>(0, data(index(row, 0), TrackListModel::FileSize).toLongLong());
    return total;
}
void TrackFilterModel::clearFilters() {
    const bool textChanged = !m_filter.isEmpty(), selectorsChanged = !m_artist.isEmpty() || !m_album.isEmpty();
    m_filter.clear(); m_artist.clear(); m_album.clear(); invalidateFilter();
    if (textChanged) emit filterTextChanged();
    if (selectorsChanged) { emit filtersChanged(); emit optionsChanged(); }
    changed();
}
bool TrackFilterModel::filterAcceptsRow(int row, const QModelIndex &parent) const {
    Q_UNUSED(parent)
    if (!m_source || (!m_filter.isEmpty() && !m_source->matches(row,m_filter))) return false;
    if (m_artist.isEmpty() && m_album.isEmpty()) return true;
    const auto index = m_source->index(row);
    return matchesArtist(m_source->data(index, TrackListModel::Artist).toString(), m_artist) &&
           (m_album.isEmpty() || m_source->data(index, TrackListModel::Album).toString().compare(m_album, Qt::CaseInsensitive) == 0);
}
bool TrackFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    if (m_sortKey == 0) return left.row() < right.row();
    const int role = m_sortKey == 1 ? TrackListModel::Title : m_sortKey == 2 ? TrackListModel::Artist :
                     m_sortKey == 3 ? TrackListModel::Album : m_sortKey == 4 ? TrackListModel::Duration :
                     m_sortKey == 5 ? TrackListModel::Plays : m_sortKey == 6 ? TrackListModel::ImportedAt :
                     m_sortKey == 7 ? TrackListModel::Heard : TrackListModel::FileSize;
    const auto a = m_source->data(left, role), b = m_source->data(right, role);
    const bool numeric = role == TrackListModel::Duration || role == TrackListModel::Plays ||
                         role == TrackListModel::Heard || role == TrackListModel::FileSize;
    const int comparison = numeric ? (a.toLongLong() < b.toLongLong() ? -1 : a == b ? 0 : 1)
        : QString::localeAwareCompare(a.toString(), b.toString());
    return comparison == 0 ? m_source->idAt(left.row()) < m_source->idAt(right.row()) : comparison < 0;
}

PlaylistListModel::PlaylistListModel(Library *library, QObject *parent) : QAbstractListModel(parent), m_library(library) {
    Q_ASSERT(library && library->thread() == thread());
    m_refresh.setSingleShot(true);
    const auto schedule = [this] { if (!m_refresh.isActive()) m_refresh.start(0); };
    connect(library, &Library::changed, this, schedule);
    // Playlist metadata is refreshed only on content changes. Progress samples
    // must not trigger full playlist aggregate queries.
    connect(&m_refresh, &QTimer::timeout, this, &PlaylistListModel::refresh);
    connect(library, &QObject::destroyed, this, [this] { m_library = nullptr; m_refresh.stop(); refresh(); });
    refresh();
}
int PlaylistListModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_playlists.size(); }
QHash<int, QByteArray> PlaylistListModel::roleNames() const {
    return {{Qt::UserRole + 1, "playlistId"}, {Qt::UserRole + 2, "name"}, {Qt::UserRole + 3, "trackCount"},
            {Qt::UserRole + 4, "playlistKey"}, {Qt::UserRole + 5, "cover"}, {Qt::UserRole + 6, "tags"},
            {Qt::UserRole + 7, "description"}, {Qt::UserRole + 8, "plays"}};
}
QVariant PlaylistListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_playlists.size()) return {};
    const auto &p = m_playlists[index.row()];
    switch (role) {
    case Qt::UserRole + 1: return p.id;
    case Qt::UserRole + 4: return QString::number(p.id);
    case Qt::UserRole + 2: case Qt::DisplayRole: return p.name;
    case Qt::UserRole + 3: return p.count;
    case Qt::UserRole + 5: return m_library && !p.cover.isEmpty() ? artworkUrl(m_library->absolute(p.cover)) : QUrl{};
    case Qt::UserRole + 6: return p.tags;
    case Qt::UserRole + 7: return p.description;
    case Qt::UserRole + 8: return p.plays;
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
