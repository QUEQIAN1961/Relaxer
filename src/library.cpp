#include "library.h"
#include "lyric-text.h"
#include "lyrics.h"
#include "lyrics.h"
#include "lyrics-document-reader.h"
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
namespace relaxer {
Library::Library(QString root, QObject *p)
    : QObject(p), m_root(QDir(root).absolutePath()), m_connection(QUuid::createUuid().toString()) {}
Library::~Library() {
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}
void Library::deferChanges(bool on) {
    if (on) {
        ++m_deferChanges;
        return;
    }
    if (m_deferChanges <= 0)
        return;
    --m_deferChanges;
    if (m_deferChanges == 0 && m_changePending) {
        m_changePending = false;
        emit changed();
    }
}
void Library::notifyChanged() {
    if (m_deferChanges > 0) {
        m_changePending = true;
        return;
    }
    emit changed();
}
QString Library::absolute(const QString &r) const {
    return r.isEmpty() ? QString() : QDir(m_root).filePath(r);
}
bool Library::exec(const QString &s, const QVariantList &a) const {
    QSqlQuery q(m_db);
    q.prepare(s);
    for (const auto &v : a)
        q.addBindValue(v);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}
bool Library::open(QString *error) {
    for (const auto &d : QStringList{"Music", "Artwork", "Lyrics", "Profile"})
        if (!QDir().mkpath(absolute(d))) {
            if (error)
                *error = "无法创建音乐资料目录：" + m_root;
            return false;
        }
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connection);
    m_db.setDatabaseName(absolute("relaxer.sqlite"));
    if (!m_db.open()) {
        m_error = m_db.lastError().text();
        if (error)
            *error = m_error;
        return false;
    }
    QStringList sql = {"PRAGMA foreign_keys=ON",
                       "PRAGMA journal_mode=WAL",
                       "PRAGMA busy_timeout=5000",
                       "CREATE TABLE IF NOT EXISTS tracks(id INTEGER PRIMARY KEY,hash TEXT NOT NULL "
                       "UNIQUE,file TEXT NOT NULL,title TEXT,artist TEXT,album TEXT,cover TEXT,lyrics "
                       "TEXT,duration INTEGER DEFAULT 0,plays INTEGER DEFAULT 0,heard INTEGER DEFAULT "
                       "0,favorite INTEGER DEFAULT 0,added TEXT DEFAULT CURRENT_TIMESTAMP)",
                       "CREATE TABLE IF NOT EXISTS playlists(id INTEGER PRIMARY KEY,name TEXT NOT NULL,cover "
                       "TEXT DEFAULT '')",
                       "CREATE TABLE IF NOT EXISTS playlist_tracks(playlist INTEGER REFERENCES playlists(id) "
                       "ON DELETE CASCADE,track INTEGER REFERENCES tracks(id) ON DELETE CASCADE,position "
                       "INTEGER NOT NULL,PRIMARY KEY(playlist,track))",
                       "CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT NOT NULL)",
                       "INSERT OR IGNORE INTO settings(key,value) VALUES('theme','[2]')",
                       "CREATE TABLE IF NOT EXISTS lyrics_lookup(track INTEGER PRIMARY KEY REFERENCES tracks(id) ON DELETE CASCADE,payload TEXT NOT NULL,fetched TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,no_result INTEGER DEFAULT 0)",
                       "CREATE TABLE IF NOT EXISTS lyrics_playback(track INTEGER PRIMARY KEY REFERENCES tracks(id) ON DELETE CASCADE,offset_ms INTEGER NOT NULL DEFAULT 0,instrumental INTEGER NOT NULL DEFAULT 0)",
                       "CREATE TABLE IF NOT EXISTS sessions(id INTEGER PRIMARY KEY,track INTEGER REFERENCES "
                       "tracks(id) ON DELETE CASCADE,heard INTEGER DEFAULT 0,duration INTEGER DEFAULT "
                       "0,sought INTEGER DEFAULT 0,counted INTEGER DEFAULT 0,finished INTEGER DEFAULT "
                       "0,started TEXT DEFAULT CURRENT_TIMESTAMP,updated TEXT DEFAULT CURRENT_TIMESTAMP)",
                       "CREATE INDEX IF NOT EXISTS session_track ON sessions(track)"};
    for (const auto &s : sql)
        if (!exec(s)) {
            if (error)
                *error = m_error;
            return false;
        }
    QSqlQuery columns(m_db);
    columns.exec("PRAGMA table_info(tracks)");
    bool hasOrigin = false;
    while (columns.next())
        hasOrigin |= columns.value(1).toString() == "cover_origin";
    columns.finish();
    if ((!hasOrigin && !exec("ALTER TABLE tracks ADD COLUMN cover_origin TEXT NOT NULL DEFAULT ''")) ||
        !exec("UPDATE tracks SET cover_origin='manual' WHERE cover_origin='' AND "
              "COALESCE(cover,'')<>'' AND cover<>'Artwork/'||hash||'.png'") ||
        !exec("UPDATE tracks SET cover_origin='none' WHERE cover_origin='' AND COALESCE(cover,'')=''")) {
        if (error)
            *error = m_error;
        return false;
    }
    columns.exec("PRAGMA table_info(playlists)");
    QSet<QString> playlistColumns;
    while (columns.next()) playlistColumns.insert(columns.value(1).toString());
    columns.finish();
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        if (error) *error = m_error;
        return false;
    }
    if ((!playlistColumns.contains("tags") &&
         !exec("ALTER TABLE playlists ADD COLUMN tags TEXT NOT NULL DEFAULT '[]'")) ||
        (!playlistColumns.contains("description") &&
         !exec("ALTER TABLE playlists ADD COLUMN description TEXT NOT NULL DEFAULT ''")) ||
        !m_db.commit()) {
        if (m_error.isEmpty()) m_error = m_db.lastError().text();
        m_db.rollback();
        if (error) *error = m_error;
        return false;
    }
    return true;
}
Track Library::fromQuery(const QSqlQuery &q) const {
    Track t;
    t.id = q.value(0).toLongLong();
    t.hash = q.value(1).toString();
    t.file = q.value(2).toString();
    t.title = q.value(3).toString();
    t.artist = q.value(4).toString();
    t.album = q.value(5).toString();
    t.cover = q.value(6).toString();
    t.lyrics = q.value(7).toString();
    t.duration = q.value(8).toLongLong();
    t.plays = q.value(9).toLongLong();
    t.heard = q.value(10).toLongLong();
    t.favorite = q.value(11).toBool();
    t.coverOrigin = q.value(12).toString();
    t.importedAt = q.value(13).toString();
    return t;
}
QVector<Track> Library::tracks(qint64 list, const QString &f) const {
    QString sql = "SELECT "
                  "t.id,t.hash,t.file,t.title,t.artist,t.album,t.cover,t.lyrics,t.duration,t.plays,t.heard,t."
                  "favorite,t.cover_origin,t.added FROM tracks t ";
    if (list > 0)
        sql += "JOIN playlist_tracks p ON p.track=t.id AND p.playlist=? ";
    sql += "WHERE 1=1 ";
    if (list == -1)
        sql += "AND t.favorite=1 ";
    if (!f.isEmpty())
        sql +=
            "AND (t.title LIKE ? ESCAPE '\\' OR t.artist LIKE ? ESCAPE '\\' OR t.album LIKE ? ESCAPE '\\') ";
    sql += list > 0 ? "ORDER BY p.position,t.id" : "ORDER BY t.added DESC,t.id DESC";
    QSqlQuery q(m_db);
    q.prepare(sql);
    if (list > 0)
        q.addBindValue(list);
    if (!f.isEmpty()) {
        QString escaped = f;
        escaped.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_");
        for (int i = 0; i < 3; ++i)
            q.addBindValue("%" + escaped + "%");
    }
    QVector<Track> r;
    if (q.exec())
        while (q.next())
            r.push_back(fromQuery(q));
    else
        m_error = q.lastError().text();
    return r;
}
Track Library::track(qint64 id) const {
    QSqlQuery q(m_db);
    q.prepare(
        "SELECT id,hash,file,title,artist,album,cover,lyrics,duration,plays,heard,favorite,cover_origin,added FROM "
        "tracks "
        "WHERE id=?");
    q.addBindValue(id);
    if (q.exec() && q.next())
        return fromQuery(q);
    return {};
}
QVector<Track> Library::recent() const {
    QSqlQuery q(m_db);
    q.exec("SELECT "
           "t.id,t.hash,t.file,t.title,t.artist,t.album,t.cover,t.lyrics,t.duration,t.plays,t.heard,t."
           "favorite,t.cover_origin,t.added FROM tracks t JOIN (SELECT track,MAX(id) latest FROM sessions GROUP BY "
           "track) s ON "
           "s.track=t.id ORDER BY s.latest DESC LIMIT 200");
    QVector<Track> r;
    while (q.next())
        r.push_back(fromQuery(q));
    return r;
}
qint64 Library::insert(const Track &t) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM tracks WHERE hash=?");
    q.addBindValue(t.hash);
    if (q.exec() && q.next()) {
        const auto id = q.value(0).toLongLong();
        const auto existing = track(id);
        // Re-import can correct automatic artwork while retaining all user edits and statistics.
        if (!t.coverOrigin.isEmpty() && existing.coverOrigin != "manual" &&
            !(t.coverOrigin == "none" &&
              (existing.coverOrigin == "embedded" || existing.coverOrigin == "sidecar")) &&
            (existing.cover != t.cover || existing.coverOrigin != t.coverOrigin)) {
            if (!exec("UPDATE tracks SET cover=?,cover_origin=? WHERE id=?", {t.cover, t.coverOrigin, id}))
                return 0;
            notifyChanged();
        }
        if (existing.file != t.file && QFileInfo(t.file).suffix().compare("flac", Qt::CaseInsensitive) == 0 &&
            needsTranscodedPlayback(QFileInfo(existing.file).suffix()) &&
            QFileInfo::exists(absolute(t.file)) && QFileInfo(absolute(t.file)).size() > 0) {
            const auto previous = existing.file;
            if (!exec("UPDATE tracks SET file=? WHERE id=?", {t.file, id}))
                return 0;
            if (previous.startsWith("Music/") && previous != t.file)
                QFile::remove(absolute(previous));
            notifyChanged();
        }
        return id;
    }
    if (!exec("INSERT INTO tracks(hash,file,title,artist,album,cover,lyrics,duration,cover_origin) "
              "VALUES(?,?,?,?,?,?,?,?,?)",
              {t.hash, t.file, t.title, t.artist, t.album, t.cover, t.lyrics, t.duration,
               t.coverOrigin.isEmpty() ? (t.cover.isEmpty() ? QString("none") : QStringLiteral(""))
                                       : t.coverOrigin}))
        return 0;
    QSqlQuery last(m_db);
    last.exec("SELECT last_insert_rowid()");
    last.next();
    auto id = last.value(0).toLongLong();
    notifyChanged();
    return id;
}
QVector<Playlist> Library::playlists() const {
    QSqlQuery q(m_db);
    q.exec("SELECT p.id,p.name,p.cover,COUNT(pt.track),COALESCE(SUM(t.plays),0),p.tags,p.description "
           "FROM playlists p LEFT JOIN playlist_tracks pt ON pt.playlist=p.id "
           "LEFT JOIN tracks t ON t.id=pt.track GROUP BY p.id ORDER BY p.id");
    QVector<Playlist> r;
    while (q.next()) {
        Playlist p;
        p.id=q.value(0).toLongLong();p.name=q.value(1).toString();p.cover=q.value(2).toString();
        p.count=q.value(3).toInt();p.plays=q.value(4).toLongLong();p.description=q.value(6).toString();
        const auto saved=QJsonDocument::fromJson(q.value(5).toByteArray()).array();
        for (const auto &tag : playlistTagOptions())
            if (saved.contains(tag)) p.tags << tag;
        r.push_back(p);
    }
    return r;
}
bool Library::playlistExists(qint64 id) const {
    QSqlQuery q(m_db);q.prepare("SELECT 1 FROM playlists WHERE id=?");q.addBindValue(id);
    if (q.exec() && q.next()) return true;
    m_error="歌单不存在";
    return false;
}
bool Library::playlistTags(qint64 id, const QStringList &tags) {
    const auto options=playlistTagOptions();
    if (tags.isEmpty()) { m_error="请至少选择一个标签";return false; }
    for (const auto &tag : tags)
        if (!options.contains(tag)) { m_error="标签不在可选范围内";return false; }
    if (!playlistExists(id)) return false;
    QJsonArray saved;
    for (const auto &tag : options) if (tags.contains(tag)) saved.append(tag);
    if (!exec("UPDATE playlists SET tags=? WHERE id=?",
              {QString::fromUtf8(QJsonDocument(saved).toJson(QJsonDocument::Compact)),id})) return false;
    notifyChanged();return true;
}
bool Library::playlistDescription(qint64 id, const QString &description) {
    if (characterCount(description)>1000) { m_error="简介不能超过 1000 字";return false; }
    const QString stored=description.isNull()?QStringLiteral(""):description;
    if (!playlistExists(id) || !exec("UPDATE playlists SET description=? WHERE id=?",{stored,id}))
        return false;
    notifyChanged();return true;
}
qint64 Library::createPlaylist(const QString &name) {
    if (name.trimmed().isEmpty() || !exec("INSERT INTO playlists(name) VALUES(?)", {name.trimmed()}))
        return 0;
    QSqlQuery q(m_db);
    q.exec("SELECT last_insert_rowid()");
    q.next();
    auto id = q.value(0).toLongLong();
    notifyChanged();
    return id;
}
bool Library::renamePlaylist(qint64 id, const QString &n) {
    if (n.trimmed().isEmpty())
        return false;
    bool ok = exec("UPDATE playlists SET name=? WHERE id=?", {n.trimmed(), id});
    if (ok)
        notifyChanged();
    return ok;
}
bool Library::deletePlaylist(qint64 id) {
    bool ok = exec("DELETE FROM playlists WHERE id=?", {id});
    if (ok)
        notifyChanged();
    return ok;
}
bool Library::copyImage(const QString &source, const QString &relative) {
    QImageReader r(source);
    r.setAutoTransform(true);
    auto sz = r.size();
    if (sz.width() > 20000 || sz.height() > 20000 || qint64(sz.width()) * sz.height() > 120000000) {
        m_error = "图片尺寸过大";
        return false;
    }
    if (sz.width() > 1600 || sz.height() > 1600)
        r.setScaledSize(sz.scaled(1600, 1600, Qt::KeepAspectRatio));
    QImage img = r.read();
    if (img.isNull()) {
        m_error = "无法读取图片";
        return false;
    }
    QSaveFile f(absolute(relative));
    if (!f.open(QIODevice::WriteOnly) || !img.save(&f, "PNG") || !f.commit()) {
        m_error = "无法保存图片";
        return false;
    }
    return true;
}
bool Library::playlistCover(qint64 id, const QString &path) {
    auto r = QString("Artwork/playlist-%1.png").arg(id);
    if (!copyImage(path, r))
        return false;
    bool ok = exec("UPDATE playlists SET cover=? WHERE id=?", {r, id});
    if (ok)
        notifyChanged();
    return ok;
}
bool Library::addToPlaylist(qint64 p, const QVector<qint64> &ids) {
    if (!m_db.transaction())
        return false;
    QSqlQuery max(m_db);
    max.prepare("SELECT COALESCE(MAX(position),-1)+1 FROM playlist_tracks WHERE playlist=?");
    max.addBindValue(p);
    max.exec();
    max.next();
    int pos = max.value(0).toInt();
    for (auto id : ids)
        if (!exec("INSERT OR IGNORE INTO playlist_tracks(playlist,track,position) VALUES(?,?,?)",
                  {p, id, pos++})) {
            m_db.rollback();
            return false;
        }
    if (!m_db.commit())
        return false;
    notifyChanged();
    return true;
}
bool Library::removeFromPlaylist(qint64 p, const QVector<qint64> &ids) {
    if (!m_db.transaction())
        return false;
    for (auto id : ids)
        if (!exec("DELETE FROM playlist_tracks WHERE playlist=? AND track=?", {p, id})) {
            m_db.rollback();
            return false;
        }
    bool ok = m_db.commit();
    if (ok)
        notifyChanged();
    return ok;
}
bool Library::reorder(qint64 p, const QVector<qint64> &ids) {
    auto old = tracks(p);
    QSet<qint64> expected, got;
    for (const auto &t : old)
        expected.insert(t.id);
    for (auto id : ids)
        got.insert(id);
    if (expected != got || ids.size() != got.size())
        return false;
    if (!m_db.transaction())
        return false;
    for (int i = 0; i < ids.size(); ++i)
        if (!exec("UPDATE playlist_tracks SET position=? WHERE playlist=? AND track=?", {i, p, ids[i]})) {
            m_db.rollback();
            return false;
        }
    bool ok = m_db.commit();
    if (ok)
        notifyChanged();
    return ok;
}
bool Library::favorite(qint64 id, bool v) {
    bool ok = exec("UPDATE tracks SET favorite=? WHERE id=?", {v, id});
    if (ok)
        notifyChanged();
    return ok;
}
bool Library::remove(const QVector<qint64> &ids) {
    if (!m_db.transaction())
        return false;
    for (auto id : ids)
        if (!exec("DELETE FROM tracks WHERE id=?", {id})) {
            m_db.rollback();
            return false;
        }
    bool ok = m_db.commit();
    if (ok)
        notifyChanged();
    return ok;
}
bool Library::favorites(const QVector<qint64> &ids,bool value){
    if(!m_db.transaction())return false;
    for(auto id:ids)if(!track(id).id||!exec("UPDATE tracks SET favorite=? WHERE id=?",{value,id})){m_db.rollback();return false;}
    const bool ok=m_db.commit();if(ok)notifyChanged();return ok;
}
bool Library::removeManaged(const QVector<qint64> &ids){
    m_error.clear();if(ids.isEmpty())return true;
    const QSet<qint64> selected(ids.begin(),ids.end());
    const QString root=QFileInfo(m_root).canonicalFilePath();const QString music=QFileInfo(absolute("Music")).canonicalFilePath();
#ifdef Q_OS_WIN
    const auto sensitivity=Qt::CaseInsensitive;
#else
    const auto sensitivity=Qt::CaseSensitive;
#endif
    if(root.isEmpty()||music.isEmpty()||!music.startsWith(root+"/",sensitivity)){m_error="音乐管理目录无效，未删除文件";return false;}
    QStringList files;
    for(auto id:selected){const auto t=track(id);if(!t.id){m_error="歌曲记录不存在";return false;}
        QString relative=t.file;relative.replace(QChar(92),QChar('/'));
        if(QDir::isAbsolutePath(relative)||!relative.startsWith("Music/")||QDir::cleanPath(relative)!=relative){m_error="歌曲不在软件音乐管理目录中，未删除文件";return false;}
        const QFileInfo info(absolute(relative));
        if(info.exists()){
            if(info.isSymLink()||!info.isFile()||!info.canonicalFilePath().startsWith(music+"/",sensitivity)){m_error="歌曲文件不在软件音乐管理目录中，未删除文件";return false;}
            QSqlQuery refs(m_db);refs.prepare("SELECT id FROM tracks WHERE file=?");refs.addBindValue(t.file);if(!refs.exec()){m_error=refs.lastError().text();return false;}bool shared=false;while(refs.next())if(!selected.contains(refs.value(0).toLongLong()))shared=true;
            if(!shared&&!files.contains(info.absoluteFilePath()))files<<info.absoluteFilePath();
        }
    }
    if(!m_db.transaction())return false;
    QVector<QPair<QString,QString>> staged;
    auto undo=[&]{for(auto it=staged.rbegin();it!=staged.rend();++it)QFile::rename(it->second,it->first);m_db.rollback();};
    for(const auto &file:files){const QString staging=QFileInfo(file).absolutePath()+"/.relaxer-delete-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-"+QFileInfo(file).fileName();
        if(!QFile::rename(file,staging)){m_error="无法删除音乐副本，文件可能正在被其他程序使用";undo();return false;}staged<<qMakePair(file,staging);
    }
    for(auto id:selected)if(!exec("DELETE FROM tracks WHERE id=?",{id})){undo();return false;}
    if(!m_db.commit()){m_error=m_db.lastError().text();undo();return false;}
    for(const auto &pair:staged)if(!QFile::remove(pair.second))m_error="歌曲已从资料库删除，但部分副本暂时未能清理";
    notifyChanged();return true;
}
bool Library::saveLyrics(qint64 id,const QByteArray &data){
    const auto normalized=simplifyTraditionalLyrics(data);
    if(!track(id).id||normalized.isEmpty()||normalized.size()>4*1024*1024||Lyrics::parse(normalized).lines.isEmpty()){m_error="歌词内容无效或歌曲已不存在";return false;}
    const auto relative=QString("Lyrics/track-%1.lrc").arg(id);QSaveFile out(absolute(relative));
    if(!out.open(QIODevice::WriteOnly)||out.write(normalized)!=normalized.size()||!out.commit()){m_error="无法保存歌词文件";return false;}
    const bool ok=exec("UPDATE tracks SET lyrics=? WHERE id=?",{relative,id})&&exec("INSERT INTO lyrics_playback(track,instrumental) VALUES(?,?) ON CONFLICT(track) DO UPDATE SET instrumental=excluded.instrumental",{id,Lyrics::parse(normalized).instrumental?1:0});if(ok)notifyChanged();return ok;
}
bool Library::saveLyricsPreservingText(qint64 id,const QByteArray &data){
    if(!track(id).id||data.isEmpty()||data.size()>4*1024*1024||Lyrics::parse(data).lines.isEmpty()){
        m_error="歌词内容无效或歌曲已不存在";return false;
    }
    const auto relative=QString("Lyrics/track-%1.lrc").arg(id);
    const auto path=absolute(relative);
    QByteArray previous;
    bool hadPrevious=false;
    QFile existing(path);
    if(QFileInfo::exists(path)){
        if(!existing.open(QIODevice::ReadOnly)){m_error="无法读取待替换的歌词文件";return false;}
        previous=existing.readAll();
        if(existing.error()!=QFileDevice::NoError){m_error="无法备份待替换的歌词文件";return false;}
        hadPrevious=true;
    }
    existing.close();
    if(!m_db.transaction()){m_error=m_db.lastError().text();return false;}
    auto restoreFile=[&]{
        if(hadPrevious){QSaveFile restore(path);if(!restore.open(QIODevice::WriteOnly)||restore.write(previous)!=previous.size()||!restore.commit())return false;}
        else if(QFileInfo::exists(path)&&!QFile::remove(path))return false;
        return true;
    };
    QSaveFile out(path);
    if(!out.open(QIODevice::WriteOnly)||out.write(data)!=data.size()||!out.commit()){
        m_db.rollback();m_error="无法保存歌词文件";return false;
    }
    const bool updated=exec("UPDATE tracks SET lyrics=? WHERE id=?",{relative,id})&&
        exec("INSERT INTO lyrics_playback(track,instrumental) VALUES(?,?) ON CONFLICT(track) DO UPDATE SET instrumental=excluded.instrumental",{id,Lyrics::parse(data).instrumental?1:0});
    if(!updated||!m_db.commit()){
        if(m_db.isOpen())m_db.rollback();
        const auto databaseError=m_error.isEmpty()?m_db.lastError().text():m_error;
        if(!restoreFile())m_error="歌词保存失败，且无法恢复原歌词文件";
        else m_error=databaseError.isEmpty()?QStringLiteral("无法提交歌词保存"):databaseError;
        return false;
    }
    notifyChanged();
    return true;
}
bool Library::updateArtwork(qint64 id, const QString &source) {
    auto r = QString("Artwork/track-%1.png").arg(id);
    if (!copyImage(source, r))
        return false;
    bool ok = exec("UPDATE tracks SET cover=?,cover_origin='manual' WHERE id=?", {r, id});
    if (ok)
        notifyChanged();
    return ok;
}
bool Library::applyArtworkRepair(const ArtworkResult &result) {
    const auto current = track(result.id);
    // A manual replacement or re-import performed while the worker runs takes precedence.
    if (!result.error.isEmpty() || result.origin.isEmpty() || !current.id || current.hash != result.hash ||
        current.cover != result.previousCover || !current.coverOrigin.isEmpty())
        return false;
    const bool ok =
        exec("UPDATE tracks SET cover=?,cover_origin=? WHERE id=?", {result.cover, result.origin, result.id});
    if (ok)
        notifyChanged();
    return ok;
}
bool Library::updateLyrics(qint64 id, const QString &source) {
    const auto sourceDocument = LyricsDocumentReader::readFile(source);
    if (!sourceDocument.opened || sourceDocument.tooLarge) {
        m_error = "歌词文件无法读取或超过 4 MB";
        return false;
    }
    auto r = QString("Lyrics/track-%1.lrc").arg(id);
    QSaveFile out(absolute(r));
    const auto &b = sourceDocument.bytes;
    if (!out.open(QIODevice::WriteOnly) || out.write(b) != b.size() || !out.commit())
        return false;
    bool ok = exec("UPDATE tracks SET lyrics=? WHERE id=?", {r, id})&&exec("INSERT INTO lyrics_playback(track,instrumental) VALUES(?,?) ON CONFLICT(track) DO UPDATE SET instrumental=excluded.instrumental",{id,Lyrics::parse(b).instrumental?1:0});
    if (ok)
        notifyChanged();
    return ok;
}
LookupCache Library::lookupCache(qint64 id) const {
    LookupCache out;QSqlQuery q(m_db);
    q.prepare("SELECT payload,no_result FROM lyrics_lookup WHERE track=? AND julianday('now')-julianday(fetched) BETWEEN 0 AND CASE WHEN no_result=1 THEN 7 ELSE 30 END");q.addBindValue(id);
    if(!q.exec()||!q.next())return out;
    const auto doc=QJsonDocument::fromJson(q.value(0).toByteArray());if(!doc.isArray())return out;
    out.fresh=true;out.noResult=q.value(1).toBool();out.records=doc.array();return out;
}
bool Library::saveLookupCache(qint64 id,const QJsonArray &records,bool noResult){
    const auto bytes=QJsonDocument(noResult?QJsonArray():records).toJson(QJsonDocument::Compact);
    if(bytes.size()>4*1024*1024||!track(id).id)return false;
    return exec("INSERT INTO lyrics_lookup(track,payload,no_result) VALUES(?,?,?) ON CONFLICT(track) DO UPDATE SET payload=excluded.payload,no_result=excluded.no_result,fetched=CURRENT_TIMESTAMP",{id,QString::fromUtf8(bytes),noResult?1:0});
}
bool Library::clearLookupCache(qint64 id){return exec("DELETE FROM lyrics_lookup WHERE track=?",{id});}
int Library::lyricOffset(qint64 id) const {QSqlQuery q(m_db);q.prepare("SELECT offset_ms FROM lyrics_playback WHERE track=?");q.addBindValue(id);return q.exec()&&q.next()?qBound(-5000,q.value(0).toInt(),5000):0;}
bool Library::setLyricOffset(qint64 id,int ms){if(!track(id).id)return false;const bool ok=exec("INSERT INTO lyrics_playback(track,offset_ms) VALUES(?,?) ON CONFLICT(track) DO UPDATE SET offset_ms=excluded.offset_ms",{id,qBound(-5000,ms,5000)});if(ok)notifyChanged();return ok;}
bool Library::isInstrumental(qint64 id) const {QSqlQuery q(m_db);q.prepare("SELECT instrumental FROM lyrics_playback WHERE track=?");q.addBindValue(id);return q.exec()&&q.next()&&q.value(0).toBool();}
bool Library::markInstrumental(qint64 id){return saveLyrics(id,QString("纯音乐，请欣赏").toUtf8());}
QJsonValue Library::setting(const QString &k, const QJsonValue &def) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM settings WHERE key=?");
    q.addBindValue(k);
    if (!q.exec() || !q.next())
        return def;
    auto doc = QJsonDocument::fromJson(q.value(0).toByteArray());
    return doc.isArray() && !doc.array().isEmpty() ? doc.array()[0] : def;
}
bool Library::setSetting(const QString &k, const QJsonValue &v) {
    return exec(
        "INSERT INTO settings(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value",
        {k, QString::fromUtf8(QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact))});
}
bool Library::setSettings(const QJsonObject &values) {
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }
    for (auto it = values.begin(); it != values.end(); ++it)
        if (!setSetting(it.key(), it.value())) {
            m_db.rollback();
            return false;
        }
    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}
Session Library::startSession(qint64 id, qint64 duration) {
    Session s;
    if (!exec("INSERT INTO sessions(track,duration) VALUES(?,?)", {id, duration}))
        return s;
    QSqlQuery q(m_db);
    q.exec("SELECT last_insert_rowid()");
    q.next();
    s.id = q.value(0).toLongLong();
    s.track = id;
    s.duration = duration;
    return s;
}
Session Library::session(qint64 id) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT id,track,heard,duration,sought,counted FROM sessions WHERE id=?");
    q.addBindValue(id);
    if (q.exec() && q.next())
        return {q.value(0).toLongLong(), q.value(1).toLongLong(), q.value(2).toLongLong(),
                q.value(3).toLongLong(), q.value(4).toBool(),     q.value(5).toBool()};
    return {};
}
bool Library::saveSession(Session &s, qint64 delta, int threshold, bool finished) {
    if (!s.id || !s.track)
        return false;
    threshold = qBound(70, threshold, 100);
    delta = qMax<qint64>(0, delta);
    Session stored = session(s.id);
    if (!stored.id)
        return false;
    s.heard = stored.heard + delta;
    s.sought = s.sought || stored.sought;
    // Media clocks can round the last sample. Full completion admits 100% only
    // for a continuous, unseeked round that actually reached end-of-media.
    bool counted = !s.sought && s.duration > 0 &&
                   (s.heard >= ((s.duration * threshold + 99) / 100) ||
                    (finished && threshold == 100 && s.heard >= s.duration - 250));
    qint64 change = int(counted) - int(stored.counted);
    if (!m_db.transaction())
        return false;
    if (!exec("UPDATE sessions SET "
              "heard=?,duration=?,sought=?,counted=?,finished=?,updated=CURRENT_TIMESTAMP WHERE id=?",
              {s.heard, s.duration, s.sought, counted, finished, s.id}) ||
        !exec("UPDATE tracks SET heard=heard+?,plays=MAX(0,plays+?) WHERE id=?", {delta, change, s.track})) {
        m_db.rollback();
        return false;
    }
    if (!m_db.commit())
        return false;
    s.counted = counted;
    if (delta || change)
        emit statsChanged();
    return true;
}
qint64 Library::totalHeard() const {
    QSqlQuery q(m_db);
    q.exec("SELECT COALESCE(SUM(heard),0) FROM tracks");
    q.next();
    return q.value(0).toLongLong();
}
qint64 Library::totalPlays() const {
    QSqlQuery q(m_db);
    q.exec("SELECT COALESCE(SUM(plays),0) FROM tracks");
    q.next();
    return q.value(0).toLongLong();
}
} // namespace relaxer
