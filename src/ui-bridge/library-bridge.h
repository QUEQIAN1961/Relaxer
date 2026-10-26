#pragma once
#include "library-model.h"
#include "playback-facade.h"
#include <QVariantMap>

namespace relaxer {
class LibraryBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString playlistId READ playlistId NOTIFY playlistChanged)
    Q_PROPERTY(QString playlistName READ playlistName NOTIFY playlistChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantMap statistics READ statistics NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY profileChanged)
    Q_PROPERTY(QVariantMap playlistDetails READ playlistDetails NOTIFY playlistChanged)
    Q_PROPERTY(QStringList playlistTags READ playlistTags CONSTANT)
  public:
    LibraryBridge(Library *library, PlaybackFacade *facade, TrackListModel *source,
                  TrackFilterModel *view, PlaylistListModel *playlists, QObject *parent = nullptr);
    QString playlistId() const { return QString::number(m_source->playlist()); }
    QString playlistName() const;
    QString error() const { return m_error; }
    QVariantMap statistics() const { return m_statistics; }
    QVariantMap profile() const;
    QVariantMap playlistDetails() const;
    QStringList playlistTags() const { return playlistTagOptions(); }
    Q_INVOKABLE bool openPlaylist(const QString &id);
    Q_INVOKABLE void play(const QString &id);
    Q_INVOKABLE void playAll();
    Q_INVOKABLE void shuffleAll();
    Q_INVOKABLE QStringList visibleTrackKeys() const;
    Q_INVOKABLE QVariantMap trackDetails(const QString &id) const;
    Q_INVOKABLE bool saveProfile(const QString &nickname, const QString &description);
    Q_INVOKABLE bool setProfileAvatar(const QUrl &source);
    Q_INVOKABLE bool setTrackCover(const QString &id, const QUrl &source);
    Q_INVOKABLE bool openTrackFolder(const QString &id);
    Q_INVOKABLE bool toggleFavorite(const QString &id);
    Q_INVOKABLE bool removeTrack(const QString &id);
    Q_INVOKABLE QString createPlaylist(const QString &name);
    Q_INVOKABLE bool renamePlaylist(const QString &id, const QString &name);
    Q_INVOKABLE bool deletePlaylist(const QString &id);
    Q_INVOKABLE bool addToPlaylist(const QString &playlist, const QString &track);
    Q_INVOKABLE bool removeFromPlaylist(const QString &track);
    Q_INVOKABLE bool moveInPlaylist(const QString &track, int direction);
    Q_INVOKABLE bool applyFavorite(const QStringList &tracks, bool favorite);
    Q_INVOKABLE bool addTracksToPlaylist(const QString &playlist, const QStringList &tracks);
    Q_INVOKABLE bool removeTracksFromPlaylist(const QStringList &tracks);
    Q_INVOKABLE bool removeTracks(const QStringList &tracks, bool managed = false);
    Q_INVOKABLE bool moveTracksInPlaylist(const QStringList &tracks, int direction);
    Q_INVOKABLE bool setPlaylistCover(const QString &id, const QUrl &source);
    Q_INVOKABLE bool setPlaylistTags(const QString &id, const QStringList &tags);
    Q_INVOKABLE bool setPlaylistDescription(const QString &id, const QString &description);
    void refreshStatistics();
    void forgetMissing(const QVector<qint64> &ids);
  signals:
    void playlistChanged();
    void errorChanged();
    void operationFailed(const QString &message);
    void operationSucceeded();
    void statisticsChanged();
    void profileChanged();
  private:
    QPointer<Library> m_library;
    QPointer<PlaybackFacade> m_facade;
    TrackListModel *m_source;
    TrackFilterModel *m_view;
    PlaylistListModel *m_playlists;
    QString m_error;
    QHash<QString, quint64> m_queryRevisions;
    QHash<QString, QString> m_queryErrors;
    quint64 m_queryRevision = 0;
    QVariantMap m_statistics;
    QTimer m_statisticsTimer;
    bool result(bool ok, const QString &fallback = {});
    bool hasPlaylist(qint64 id) const;
    QVector<qint64> checkedIds(const QStringList &keys) const;
};
}
