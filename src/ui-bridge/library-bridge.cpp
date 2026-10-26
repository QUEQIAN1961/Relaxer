#include "library-bridge.h"
#include <QThread>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QImage>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>

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
    m_statisticsTimer.setSingleShot(true); m_statisticsTimer.setInterval(1200);
    connect(&m_statisticsTimer, &QTimer::timeout, this, &LibraryBridge::refreshStatistics);
    connect(library, &Library::statsChanged, this, [this] {
        if (!m_statisticsTimer.isActive()) m_statisticsTimer.start();
    });
    connect(library, &Library::changed, this, [this] {
        refreshStatistics(); emit profileChanged(); emit playlistChanged();
    });
    connect(library, &QObject::destroyed, this, [this] {
        m_library = nullptr; m_statisticsTimer.stop(); refreshStatistics(); emit profileChanged(); emit playlistChanged();
    });
    connect(library,&Library::queryFailed,this,[this](const QString &query,const QString &message){
        if(m_queryErrors.value(query)==message)return;
        m_queryErrors[query]=message;
        const auto revision=++m_queryRevision;
        m_queryRevisions[query]=revision;
        // Context ownership prevents use after destruction; a successful retry
        // or a later operation invalidates this exact deferred failure.
        QTimer::singleShot(0,this,[this,query,message,revision]{
            if(m_queryRevisions.value(query)==revision)emit operationFailed(message);
        });
    });
    connect(library,&Library::querySucceeded,this,[this](const QString &query){
        m_queryErrors.remove(query);m_queryRevisions.remove(query);
    });
    refreshStatistics();
}
bool LibraryBridge::result(bool ok, const QString &fallback) {
    Q_ASSERT(QThread::currentThread() == thread());
    if(ok){m_queryErrors.clear();m_queryRevisions.clear();}
    const auto error = ok ? QString() : (!fallback.isEmpty() ? fallback : m_library ? m_library->lastError() : QString("资料库不可用"));
    if (m_error != error) { m_error = error; emit errorChanged(); }
    if (!ok) emit operationFailed(error.isEmpty() ? QStringLiteral("资料库操作失败") : error);
    else emit operationSucceeded();
    return ok;
}
QString LibraryBridge::playlistName() const {
    if (m_source->playlist() == 0) return "本地音乐";
    if (m_source->playlist() == -1) return "我喜欢";
    if (m_source->playlist() == -2) return "最近播放";
    if (m_source->playlist() == -3) return "最常播放";
    if (m_library) for (const auto &list : m_library->playlists()) if (list.id == m_source->playlist()) return list.name;
    return "歌单";
}
bool LibraryBridge::hasPlaylist(qint64 id) const {
    if (m_library) for (const auto &list : m_library->playlists()) if (list.id == id) return true;
    return false;
}
bool LibraryBridge::openPlaylist(const QString &id) {
    bool valid = false; const auto value = id.toLongLong(&valid);
    if (!m_library || !valid || value < -3 || (value > 0 && !hasPlaylist(value))) return result(false, "歌单不存在");
    if (m_source->playlist() != value) m_view->clearFilters();
    if (value == -2 || value == -3) { m_view->setSortKey(0); m_view->setDescending(false); }
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
void LibraryBridge::shuffleAll() {
    if (!m_facade || m_facade->isShutdown()) return;
    const auto ids = m_view->ids(); if (ids.isEmpty()) return;
    m_facade->setMode(Mode::Shuffle);
    m_facade->play(ids, ids[QRandomGenerator::global()->bounded(ids.size())]); result(true);
}
QStringList LibraryBridge::visibleTrackKeys() const {
    QStringList keys;
    for (auto id : m_view->ids()) keys.append(QString::number(id));
    return keys;
}
QVariantMap LibraryBridge::trackDetails(const QString &id) const {
    const auto track = m_library ? m_library->track(id.toLongLong()) : Track{};
    if (!track.id) return {};
    const QFileInfo file(m_library->absolute(track.file));
    return {{"trackKey", QString::number(track.id)}, {"title", track.title}, {"artist", track.artist},
            {"album", track.album}, {"duration", track.duration}, {"plays", track.plays}, {"heard", track.heard},
            {"favorite", track.favorite}, {"importedAt", track.importedAt}, {"lyricsAvailable", !track.lyrics.isEmpty()},
            {"fileSize", file.isFile() ? file.size() : qint64(-1)},
            {"cover", track.cover.isEmpty() ? QUrl{} : QUrl::fromLocalFile(m_library->absolute(track.cover))}};
}
void LibraryBridge::refreshStatistics() {
    if (m_facade) m_source->refreshStatistics(m_facade->current().id);
    const QVariantMap next{{"trackCount", m_library ? m_library->trackCount() : 0},
                           {"totalPlays", m_library ? m_library->totalPlays() : 0},
                           {"totalHeard", m_library ? m_library->totalHeard() : 0}};
    if (next != m_statistics) { m_statistics = next; emit statisticsChanged(); }
}
QVariantMap LibraryBridge::profile() const {
    if (!m_library) return {};
    const auto avatar = m_library->setting("avatar").toString();
    return {{"nickname", m_library->setting("nickname", "Relaxer Listener").toString()},
            {"description", m_library->setting("profileDescription", "收藏、回忆，还有反复听过的那些歌。").toString()},
            {"avatar", avatar.isEmpty() ? QUrl{} : QUrl::fromLocalFile(m_library->absolute(avatar))}};
}
QVariantMap LibraryBridge::playlistDetails() const {
    if (!m_library || m_source->playlist() <= 0) return {};
    for (const auto &playlist : m_library->playlists()) if (playlist.id == m_source->playlist()) {
        auto cover = playlist.cover;
        if (cover.isEmpty() && m_source->rowCount())
            cover = m_library->track(m_source->idAt(0)).cover;
        return {{"playlistKey", QString::number(playlist.id)}, {"name", playlist.name}, {"trackCount", playlist.count},
                {"plays", playlist.plays}, {"tags", playlist.tags}, {"description", playlist.description},
                {"cover", cover.isEmpty() ? QUrl{} : QUrl::fromLocalFile(m_library->absolute(cover))}};
    }
    return {};
}
bool LibraryBridge::saveProfile(const QString &nickname, const QString &description) {
    if (!m_library || nickname.trimmed().isEmpty() || nickname.size() > 60 || characterCount(description) > 1000)
        return result(false, "昵称不能为空且不能超过60字，简介不能超过1000字");
    const bool ok = m_library->setSettings({{"nickname", nickname.trimmed()}, {"profileDescription", description}});
    if (ok) emit profileChanged();
    return result(ok);
}
bool LibraryBridge::setProfileAvatar(const QUrl &source) {
    if (!m_library || !source.isLocalFile()) return result(false, "请选择本地图片");
    QImageReader reader(source.toLocalFile()); const auto size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 120000000)
        return result(false, "图片尺寸过大或无法读取");
    reader.setAutoTransform(true); reader.setScaledSize(size.scaled(512, 512, Qt::KeepAspectRatio));
    const auto image = reader.read(); QSaveFile file(m_library->absolute("Profile/avatar.png"));
    if (image.isNull() || !file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit())
        return result(false, "头像保存失败");
    const bool ok = m_library->setSetting("avatar", "Profile/avatar.png");
    if (ok) emit profileChanged();
    return result(ok);
}
bool LibraryBridge::setTrackCover(const QString &id, const QUrl &source) {
    return result(m_library && source.isLocalFile() && m_library->track(id.toLongLong()).id &&
                  m_library->updateArtwork(id.toLongLong(), source.toLocalFile()), "歌曲不存在或封面无法保存");
}
bool LibraryBridge::openTrackFolder(const QString &id) {
    const auto track = m_library ? m_library->track(id.toLongLong()) : Track{};
    return result(track.id && QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_library->absolute(track.file)).absolutePath())),
                  "无法打开歌曲所在文件夹");
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
    if (!m_library || name.trimmed().isEmpty()) { result(false,"请输入歌单名称");return "0"; }
    const auto id = m_library->createPlaylist(name);
    result(id != 0); return QString::number(id);
}
bool LibraryBridge::renamePlaylist(const QString &id, const QString &name) {
    const auto value = id.toLongLong();
    if (!m_library || !hasPlaylist(value) || name.trimmed().isEmpty()) return result(false,"歌单不存在或名称为空");
    return result(m_library->renamePlaylist(value,name));
}
bool LibraryBridge::deletePlaylist(const QString &id) {
    const auto value = id.toLongLong();
    if (!m_library || !hasPlaylist(value)) return result(false,"歌单不存在");
    return result(m_library->deletePlaylist(value));
}
bool LibraryBridge::addToPlaylist(const QString &playlist, const QString &track) {
    if (!m_library || !hasPlaylist(playlist.toLongLong()) || !m_library->track(track.toLongLong()).id) return result(false,"请选择有效歌单和歌曲");
    return result(m_library->addToPlaylist(playlist.toLongLong(), {track.toLongLong()}));
}
bool LibraryBridge::removeFromPlaylist(const QString &track) {
    if (!m_library || m_source->playlist() <= 0) return result(false,"请选择歌单内歌曲");
    return result(m_library->removeFromPlaylist(m_source->playlist(), {track.toLongLong()}));
}
bool LibraryBridge::moveInPlaylist(const QString &track, int direction) {
    return moveTracksInPlaylist({track}, direction);
}
QVector<qint64> LibraryBridge::checkedIds(const QStringList &keys) const {
    if (!m_library || keys.isEmpty()) return {};
    QVector<qint64> ids; QSet<qint64> seen;
    for (const auto &key : keys) {
        bool valid = false; const auto id = key.toLongLong(&valid);
        if (!valid || id <= 0) return {};
        if (!seen.contains(id)) { seen.insert(id); ids.append(id); }
    }
    if (m_library->tracksByIds(ids).size() != ids.size()) return {};
    return ids;
}
bool LibraryBridge::applyFavorite(const QStringList &tracks, bool favorite) {
    const auto ids = checkedIds(tracks);
    if (ids.isEmpty()) return result(false,"请选择有效歌曲");
    return result(m_library->favorites(ids, favorite));
}
bool LibraryBridge::addTracksToPlaylist(const QString &playlist, const QStringList &tracks) {
    const auto ids = checkedIds(tracks);
    if (ids.isEmpty() || !hasPlaylist(playlist.toLongLong())) return result(false,"请选择有效歌单和歌曲");
    return result(m_library->addToPlaylist(playlist.toLongLong(), ids));
}
bool LibraryBridge::removeTracksFromPlaylist(const QStringList &tracks) {
    const auto ids = checkedIds(tracks); const auto members = m_source->ids();
    bool valid = m_source->playlist() > 0 && !ids.isEmpty();
    for (auto id : ids) valid = valid && members.contains(id);
    if (!valid) return result(false,"请选择当前歌单内歌曲");
    return result(m_library->removeFromPlaylist(m_source->playlist(), ids));
}
bool LibraryBridge::removeTracks(const QStringList &tracks, bool managed) {
    const auto ids = checkedIds(tracks);
    if (ids.isEmpty() || !m_facade || m_facade->isShutdown()) return result(false, "请选择有效歌曲");
    m_facade->forget(ids);
    return result(managed ? m_library->removeManaged(ids) : m_library->remove(ids));
}
bool LibraryBridge::moveTracksInPlaylist(const QStringList &tracks, int direction) {
    if (!m_library || m_source->playlist() <= 0 || !m_view->filterText().isEmpty() ||
        !m_view->artistFilter().isEmpty() || !m_view->albumFilter().isEmpty() ||
        m_view->sortKey() != 0 || m_view->descending() || (direction != -1 && direction != 1))
        return result(false, "请清空筛选并恢复原顺序后调整歌单");
    const auto selected = checkedIds(tracks); auto ids = m_source->ids();
    if (selected.isEmpty()) return result(false, "请选择当前歌单内歌曲");
    for (auto id : selected) if (!ids.contains(id)) return result(false, "请选择当前歌单内歌曲");
    if (direction < 0) {
        for (int i = 1; i < ids.size(); ++i)
            if (selected.contains(ids[i]) && !selected.contains(ids[i - 1])) ids.swapItemsAt(i, i - 1);
    } else {
        for (int i = int(ids.size()) - 2; i >= 0; --i)
            if (selected.contains(ids[i]) && !selected.contains(ids[i + 1])) ids.swapItemsAt(i, i + 1);
    }
    return result(m_library->reorder(m_source->playlist(), ids));
}
bool LibraryBridge::setPlaylistCover(const QString &id, const QUrl &source) {
    return result(m_library && hasPlaylist(id.toLongLong()) && source.isLocalFile() &&
                  m_library->playlistCover(id.toLongLong(), source.toLocalFile()), "请选择有效歌单和本地图片");
}
bool LibraryBridge::setPlaylistTags(const QString &id, const QStringList &tags) {
    if (!m_library || !hasPlaylist(id.toLongLong())) return result(false,"歌单不存在");
    return result(m_library->playlistTags(id.toLongLong(), tags));
}
bool LibraryBridge::setPlaylistDescription(const QString &id, const QString &description) {
    if (!m_library || !hasPlaylist(id.toLongLong())) return result(false,"歌单不存在");
    return result(m_library->playlistDescription(id.toLongLong(), description));
}
void LibraryBridge::forgetMissing(const QVector<qint64> &ids) {
    if (!m_library || !m_facade || m_facade->isShutdown()) return;
    QVector<qint64> missing;
    for (auto id : ids) if (!m_library->track(id).id) missing.append(id);
    if (!missing.isEmpty()) m_facade->forget(missing);
}
}
