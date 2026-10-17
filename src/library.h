#pragma once
#include "model.h"
#include <QJsonValue>
#include <QJsonArray>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
namespace relaxer {
struct LookupCache { bool fresh=false, noResult=false; QJsonArray records; };
// Snapshot only the lyric files about to be replaced, never the music library.
struct LyricFileSnapshot {
    QString path;
    bool exists=false;
    QByteArray digest;
    bool operator==(const LyricFileSnapshot &other) const {
        return path==other.path&&exists==other.exists&&digest==other.digest;
    }
};
struct LyricsSaveSnapshot {
    qint64 id=0;
    QString hash,lyrics,error;
    LyricFileSnapshot source,target;
    bool valid=false;
};
class Library : public QObject {
    Q_OBJECT
  public:
    explicit Library(QString root, QObject *parent = nullptr);
    ~Library() override;
    QString root() const { return m_root; }
    QString absolute(const QString &relative) const;
    bool open(QString *error = nullptr);
    QVector<Track> tracks(qint64 playlist = 0, const QString &filter = {}) const;
    QVector<Track> recent() const;
    Track track(qint64 id) const;
    qint64 insert(const Track &t);
    QVector<Playlist> playlists() const;
    qint64 createPlaylist(const QString &name);
    bool renamePlaylist(qint64 id, const QString &name);
    bool deletePlaylist(qint64 id);
    bool playlistCover(qint64 id, const QString &source);
    bool playlistTags(qint64 id, const QStringList &tags);
    bool playlistDescription(qint64 id, const QString &description);
    bool addToPlaylist(qint64 playlist, const QVector<qint64> &ids);
    bool removeFromPlaylist(qint64 playlist, const QVector<qint64> &ids);
    bool reorder(qint64 playlist, const QVector<qint64> &ids);
    bool favorite(qint64 id, bool value);
    bool remove(const QVector<qint64> &ids); // Remove references; retain managed files.
    bool removeManaged(const QVector<qint64> &ids);
    bool favorites(const QVector<qint64> &ids,bool value);
    bool saveLyrics(qint64 id,const QByteArray &data);
    bool saveLyricsPreservingText(qint64 id,const QByteArray &data);
    LyricsSaveSnapshot lyricsSaveSnapshot(qint64 id) const;
    bool lyricsSnapshotMatches(const LyricsSaveSnapshot &snapshot,QString *error=nullptr) const;
    bool saveLyricsChecked(qint64 id,const QByteArray &data,const LyricsSaveSnapshot &snapshot,bool preserveText=false);
    bool updateArtwork(qint64 id, const QString &source);
    bool applyArtworkRepair(const ArtworkResult &result);
    bool updateLyrics(qint64 id, const QString &source);
    LookupCache lookupCache(qint64 id) const;
    bool saveLookupCache(qint64 id,const QJsonArray &records,bool noResult);
    bool clearLookupCache(qint64 id);
    int lyricOffset(qint64 id) const;
    bool setLyricOffset(qint64 id,int milliseconds);
    bool isInstrumental(qint64 id) const;
    bool markInstrumental(qint64 id);
    QJsonValue setting(const QString &key, const QJsonValue &fallback = {}) const;
    bool setSetting(const QString &key, const QJsonValue &value);
    bool setSettings(const QJsonObject &values);
    Session startSession(qint64 track, qint64 duration);
    Session session(qint64 id) const;
    bool saveSession(Session &s, qint64 delta, int threshold, bool finished = false);
    qint64 totalHeard() const;
    qint64 totalPlays() const;
    QString lastError() const { return m_error; }
    void deferChanges(bool on);
  signals:
    void changed();
    void statsChanged();

  private:
    QString m_root, m_connection;
    bool writeLyrics(qint64 id,const QByteArray &data,const LyricsSaveSnapshot *snapshot=nullptr);
    mutable QString m_error;
    int m_deferChanges = 0;
    bool m_changePending = false;
    QSqlDatabase m_db;
    bool exec(const QString &sql, const QVariantList &args = {}) const;
    void notifyChanged();
    Track fromQuery(const QSqlQuery &q) const;
    bool copyImage(const QString &source, const QString &relative);
    bool playlistExists(qint64 id) const;
};
} // namespace relaxer
