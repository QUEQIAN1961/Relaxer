#include "library-bridge.h"
#include <QThread>

namespace relaxer {
LibraryBridge::LibraryBridge(Library *library, PlaybackFacade *facade, TrackListModel *source,
    TrackFilterModel *view, PlaylistListModel *playlists, QObject *parent)
    : QObject(parent), m_library(library), m_facade(facade), m_source(source), m_view(view), m_playlists(playlists) {
    Q_ASSERT(library->thread() == thread() && facade->thread() == thread());
    connect(playlists, &PlaylistListModel::countChanged, this, [this] {
        if (m_source->playlist() > 0 && !m_playlists->contains(m_source->playlist())) m_source->setPlaylist(0);
        emit playlistChanged();
    });
    connect(facade->backend(), &QObject::destroyed, this, [this] { m_facade = nullptr; });
}
bool LibraryBridge::result(bool ok, const QString &fallback) {
    Q_ASSERT(QThread::currentThread() == thread());
    const auto error = ok ? QString() : (!fallback.isEmpty() ? fallback : m_library ? m_library->lastError() : QString("资料库不可用"));
    if (m_error != error) { m_error = error; emit errorChanged(); }
    return ok;
}
QString LibraryBridge::playlistName() const {
    if (m_source->playlist() == 0) return "本地音乐";
    if (m_source->playlist() == -1) return "我喜欢";
    if (m_library) for (const auto &list : m_library->playlists()) if (list.id == m_source->playlist()) return list.name;
    return "歌单";
}
bool LibraryBridge::hasPlaylist(qint64 id) const {
    if (m_library) for (const auto &list : m_library->playlists()) if (list.id == id) return true;
    return false;
}
bool LibraryBridge::openPlaylist(const QString &id) {
    bool valid = false; const auto value = id.toLongLong(&valid);
    if (!m_library || !valid || value < -1 || (value > 0 && !hasPlaylist(value))) return result(false, "歌单不存在");
    m_source->setPlaylist(value); emit playlistChanged(); return result(true);
}
void LibraryBridge::play(const QString &id) {
    if (!m_facade || m_facade->isShutdown()) return;
    const auto ids = m_view->ids(); const auto track = id.toLongLong();
    if (!ids.contains(track)) { result(false, "歌曲不在当前列表中"); return; }
    result(true); m_facade->play(ids, track);
}
void LibraryBridge::playAll() {
    if (m_view->count()) play(m_view->idAt(0));
}
bool LibraryBridge::toggleFavorite(const QString &id) {
    const auto track = m_library ? m_library->track(id.toLongLong()) : Track{};
    return result(track.id && m_library->favorite(track.id, !track.favorite), track.id ? QString() : QString("歌曲不存在"));
}
bool LibraryBridge::removeTrack(const QString &id) {
    const auto track = m_library ? m_library->track(id.toLongLong()) : Track{};
    if (!track.id || !m_facade || m_facade->isShutdown()) return result(false, "歌曲或播放服务不可用");
    // Same order and retained-file removal contract as the Widgets context menu.
    m_facade->forget({track.id});
    return result(m_library->remove({track.id}));
}
QString LibraryBridge::createPlaylist(const QString &name) {
    const auto id = m_library && !name.trimmed().isEmpty() ? m_library->createPlaylist(name) : 0;
    result(id != 0, id ? QString() : QString("请输入歌单名称")); return QString::number(id);
}
bool LibraryBridge::renamePlaylist(const QString &id, const QString &name) {
    const auto value = id.toLongLong();
    return result(m_library && hasPlaylist(value) && !name.trimmed().isEmpty() && m_library->renamePlaylist(value, name),
                  "歌单不存在或名称为空");
}
bool LibraryBridge::deletePlaylist(const QString &id) {
    const auto value = id.toLongLong();
    return result(m_library && hasPlaylist(value) && m_library->deletePlaylist(value), "歌单不存在或删除失败");
}
bool LibraryBridge::addToPlaylist(const QString &playlist, const QString &track) {
    return result(m_library && hasPlaylist(playlist.toLongLong()) && m_library->track(track.toLongLong()).id &&
                  m_library->addToPlaylist(playlist.toLongLong(), {track.toLongLong()}), "请选择有效歌单和歌曲");
}
bool LibraryBridge::removeFromPlaylist(const QString &track) {
    return result(m_library && m_source->playlist() > 0 && m_library->removeFromPlaylist(m_source->playlist(), {track.toLongLong()}), "请选择歌单内歌曲");
}
bool LibraryBridge::moveInPlaylist(const QString &track, int direction) {
    if (!m_library || m_source->playlist() <= 0 || !m_view->filterText().isEmpty() ||
        m_view->sortKey() != 0 || m_view->descending() || (direction != -1 && direction != 1))
        return result(false, "请清空筛选并恢复原顺序后调整歌单");
    auto ids = m_source->ids(); const auto at = ids.indexOf(track.toLongLong());
    if (at < 0 || at + direction < 0 || at + direction >= ids.size()) return false;
    ids.swapItemsAt(at, at + direction); return result(m_library->reorder(m_source->playlist(), ids));
}
}
