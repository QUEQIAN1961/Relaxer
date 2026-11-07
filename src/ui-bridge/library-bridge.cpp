#include "library-bridge.h"
#include "online-music-bridge.h"
#include "qq-music-bridge.h"
#include "online-track-matcher.h"
#include <QThread>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QImage>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDateTime>

namespace relaxer {
LibraryBridge::LibraryBridge(Library *library, PlaybackFacade *facade, TrackListModel *source,
    TrackFilterModel *view, PlaylistListModel *playlists, QObject *parent)
    : QObject(parent), m_library(library), m_facade(facade), m_source(source), m_view(view), m_playlists(playlists) {
    Q_ASSERT(library->thread() == thread() && facade->thread() == thread());
    connect(playlists, &PlaylistListModel::countChanged, this, [this] {
        if (m_source->playlist() > 0 && !m_playlists->contains(m_source->playlist())) {m_source->setPlaylist(0);restoreSorting();}
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
    connect(library, &Library::settingsUpdated, this, [this](const QStringList &keys) {
        if (keys.contains("qml.playlistMarkers")) emit playlistMarkersChanged();
        if(keys.contains("qml.cloudPlaylistRemarks"))emit playlistChanged();
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
    restoreSorting();
    connect(view,&TrackFilterModel::sortChanged,this,&LibraryBridge::saveSorting);
    refreshStatistics();
}
void LibraryBridge::restoreSorting() {
    if (!m_library) return;
    const auto state=m_library->setting("qml.playlistSorting").toObject().value(playlistId()).toObject();
    m_restoringSort=true;
    m_view->setSorting(state.value("key").toInt(),state.value("descending").toBool());
    m_restoringSort=false;
}
void LibraryBridge::saveSorting() {
    if (!m_library || m_restoringSort) return;
    auto states=m_library->setting("qml.playlistSorting").toObject();
    const QJsonObject state{{"key",m_view->sortKey()},{"descending",m_view->descending()}};
    if (states.value(playlistId()).toObject()==state) return;
    states.insert(playlistId(),state);
    result(m_library->setSetting("qml.playlistSorting",states));
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
    if (m_source->playlist() == -3) return "最长播放";
    if (m_library) for (const auto &list : m_library->playlists()) if (list.id == m_source->playlist()){const auto remark=!list.provider.isEmpty()?m_library->setting("qml.cloudPlaylistRemarks").toObject().value(QString::number(list.id)).toString():QString();return remark.isEmpty()?list.name:remark;}
    return "歌单";
}
bool LibraryBridge::hasPlaylist(qint64 id) const {
    if (m_library) for (const auto &list : m_library->playlists()) if (list.id == id) return true;
    return false;
}
QVariantMap LibraryBridge::playlistMarkers() const {
    return m_library ? m_library->setting("qml.playlistMarkers").toObject().toVariantMap() : QVariantMap{};
}
QStringList LibraryBridge::playlistMarkerIcons() const {
    return {"music", "headphones", "disc-3", "radio", "mic-vocal", "star", "heart", "sun",
            "moon", "cloud-rain", "coffee", "book-open", "leaf", "flame", "mountain", "gamepad-2",
            "bell", "bookmark", "briefcase", "camera", "clock", "compass", "cube", "diamond",
            "flower", "globe", "graduation-cap", "home", "key", "laptop", "lightbulb", "mail",
            "map-pin", "palette", "paw-print", "plane", "rocket", "shield", "smile", "sparkles",
            "trophy", "umbrella", "video", "waves", "wine", "wrench", "zap", "balloon"};
}
bool LibraryBridge::setPlaylistOrder(const QStringList &ids) {
    if(!m_library)return result(false,"资料库不可用");
    const auto lists=m_library->playlists();QSet<QString> keys;
    for(const auto &list:lists)keys.insert(QString::number(list.id));
    const QSet<QString> requested(ids.begin(),ids.end());
    if(ids.size()!=lists.size()||requested!=keys)return result(false,"歌单列表已改变，请重新调整排序");
    return result(m_library->setSetting("qml.playlistOrder",QJsonArray::fromStringList(ids)));
}
bool LibraryBridge::setPlaylistMarker(const QString &id, const QString &marker) {
    const auto key = id.toLongLong();
    if (!m_library || !hasPlaylist(key)) return result(false, "歌单不存在");
    if (!(marker.startsWith("icon:") && playlistMarkerIcons().contains(marker.mid(5))) &&
        !(marker.startsWith("text:") && characterCount(marker.mid(5).trimmed()) == 1 && marker.mid(5) == marker.mid(5).trimmed()))
        return result(false, "请选择图标或输入一个文字标记");
    auto markers = m_library->setting("qml.playlistMarkers").toObject();
    markers.insert(QString::number(key), marker);
    return result(m_library->setSetting("qml.playlistMarkers", markers));
}
bool LibraryBridge::openPlaylist(const QString &id) {
    bool valid = false; const auto value = id.toLongLong(&valid);
    if (!m_library || !valid || value < -3 || (value > 0 && !hasPlaylist(value))) return result(false, "歌单不存在");
    if (m_source->playlist() != value) {m_view->clearFilters();m_source->setPlaylist(value);restoreSorting();}
    emit playlistChanged();const auto cloud=cloudPlaylist(id);if(!cloud.isEmpty())emit cloudPlaylistRequested(cloud.value("provider").toString(),cloud.value("id").toString());return result(true);
}
QString LibraryBridge::pinCloudPlaylist(const QVariantMap &playlist){
    if(!m_library)return "0";const auto id=m_library->pinCloudPlaylist(playlist.value("provider").toString(),playlist.value("id").toString(),playlist.value("name").toString(),playlist.value("cover").toString(),playlist.value("count").toInt());
    result(id>0,"云端歌单未能加入侧栏");if(id>0)emit cloudPlaylistPinned(playlist.value("provider").toString(),playlist.value("id").toString());return QString::number(id);
}
QVariantMap LibraryBridge::cloudPlaylist(const QString &key) const {
    if(m_library)for(const auto &p:m_library->playlists())if(p.id==key.toLongLong()&&!p.provider.isEmpty())return {{"key",key},{"provider",p.provider},{"id",p.remoteId},{"name",p.name},{"remark",m_library->setting("qml.cloudPlaylistRemarks").toObject().value(key).toString()},{"cover",p.cover},{"count",p.count}};
    return {};
}
bool LibraryBridge::setCloudPlaylistRemark(const QString &key,const QString &remark){
    if(cloudPlaylist(key).isEmpty()||characterCount(remark)>120)return result(false,"请选择云端歌单，备注不能超过120字");auto remarks=m_library->setting("qml.cloudPlaylistRemarks").toObject();if(remark.trimmed().isEmpty())remarks.remove(key);else remarks.insert(key,remark.trimmed());return result(m_library->setSetting("qml.cloudPlaylistRemarks",remarks));
}
void LibraryBridge::setCloudSongs(const QString &provider,const QVariantMap &playlist,const QVariantList &songs){
    if(playlist.value("kind")=="artist"||playlist.value("kind")=="album")return;
    const auto cloud=cloudPlaylist(playlistId());if(cloud.value("provider").toString()!=provider||cloud.value("id")!=playlist.value("id"))return;
    const auto base=provider=="qq"?QqMusicBridge::TrackBase:OnlineMusicBridge::TrackBase;QVector<Track> tracks;
    const auto local=m_library->tracks();QHash<qint64,Track> localStats;for(const auto &t:local)localStats.insert(t.id,t);QHash<QString,Track> onlineStats;for(const auto &t:m_library->onlineTracks())onlineStats.insert(t.hash,t);
    for(const auto &value:songs){const auto row=value.toMap();Track t;t.id=base+row.value("id").toLongLong();t.title=row.value("title").toString();t.artist=row.value("artist").toString();t.album=row.value("album").toString();t.duration=row.value("duration").toLongLong();t.cover=row.value("thumbnail",row.value("cover")).toString();t.hash=provider=="qq"?"qqmusic:"+row.value("mid").toString():"netease:"+row.value("id").toString();t.matchedLocalId=matchOnlineTrack(t,local);const auto stats=t.matchedLocalId?localStats.value(t.matchedLocalId):onlineStats.value(t.hash);t.plays=stats.plays;t.heard=stats.heard;tracks.append(t);}
    m_source->setCloudTracks(m_source->playlist(),tracks);
    const auto name=playlist.value("name").toString(),cover=playlist.value("cover").toString();const auto count=playlist.value("count").toInt();
    if(!name.isEmpty()&&(name!=cloud.value("name").toString()||cover!=cloud.value("cover").toString()||count!=cloud.value("count").toInt()))m_library->pinCloudPlaylist(provider,cloud.value("id").toString(),name,cover,count);
}
void LibraryBridge::play(const QString &id) {
    if (!m_facade || m_facade->isShutdown()) return;
    const auto ids = m_view->ids(); const auto track = id.toLongLong();
    if (!ids.contains(track)) { result(false, "歌曲不在当前列表中"); return; }
    const auto cloud=cloudPlaylist(playlistId());if(!cloud.isEmpty()){const auto base=cloud.value("provider").toString()=="qq"?QqMusicBridge::TrackBase:OnlineMusicBridge::TrackBase;QStringList remote;for(auto id:ids)remote.append(QString::number(id-base));result(true);emit cloudPlayRequested(cloud.value("provider").toString(),remote,QString::number(track-base));return;}
    result(true); m_facade->play(ids, track);
}
void LibraryBridge::playAll() {
    if (m_view->count()) play(m_view->idAt(0));
}
void LibraryBridge::shuffleAll() {
    if (!m_facade || m_facade->isShutdown()) return;
    const auto ids = m_view->ids(); if (ids.isEmpty()) return;
    m_facade->setMode(Mode::Shuffle);
    play(QString::number(ids[QRandomGenerator::global()->bounded(ids.size())]));
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
    return playlistDetailsFor(playlistId());
}
QVariantMap LibraryBridge::playlistDetailsFor(const QString &id) const {
    const auto key = id.toLongLong();
    if (!m_library || key <= 0) return {};
    for (const auto &playlist : m_library->playlists()) if (playlist.id == key) {
        auto cover = playlist.cover;
        if (cover.isEmpty() && key == m_source->playlist() && m_source->rowCount())
            cover = m_library->track(m_source->idAt(0)).cover;
        auto coverUrl = !playlist.provider.isEmpty()?QUrl(cover):cover.isEmpty() ? QUrl{} : QUrl::fromLocalFile(m_library->absolute(cover));
        if (!coverUrl.isEmpty()&&coverUrl.isLocalFile()) {
            const QFileInfo info(coverUrl.toLocalFile());QUrlQuery query;
            query.addQueryItem("revision", QString::number(info.lastModified().toMSecsSinceEpoch()) + ":" + QString::number(info.size()));
            coverUrl.setQuery(query);
        }
        return {{"playlistKey", QString::number(playlist.id)}, {"name", playlist.name}, {"trackCount", playlist.count},
                {"plays", playlist.plays}, {"tags", playlist.tags}, {"description", playlist.description},
                {"cover", coverUrl},{"cloud",!playlist.provider.isEmpty()},{"provider",playlist.provider},{"remoteId",playlist.remoteId},{"remark",m_library->setting("qml.cloudPlaylistRemarks").toObject().value(id).toString()}};
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
    if (id) {
        auto markers = m_library->setting("qml.playlistMarkers").toObject();
        if (markers.contains(QString::number(id))) {
            markers.remove(QString::number(id));m_library->setSetting("qml.playlistMarkers", markers);
        }
    }
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
    if (!m_library->deletePlaylist(value)) return result(false);
    auto markers = m_library->setting("qml.playlistMarkers").toObject();
    markers.remove(QString::number(value));auto remarks=m_library->setting("qml.cloudPlaylistRemarks").toObject();remarks.remove(id);
    auto sorts=m_library->setting("qml.playlistSorting").toObject();sorts.remove(id);
    return result(m_library->setSettings({{"qml.playlistMarkers",markers},{"qml.cloudPlaylistRemarks",remarks},{"qml.playlistSorting",sorts}}));
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
