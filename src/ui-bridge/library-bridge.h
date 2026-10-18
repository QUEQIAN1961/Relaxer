#pragma once
#include "library-model.h"
#include "playback-facade.h"

namespace relaxer {
class LibraryBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString playlistId READ playlistId NOTIFY playlistChanged)
    Q_PROPERTY(QString playlistName READ playlistName NOTIFY playlistChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  public:
    LibraryBridge(Library *library, PlaybackFacade *facade, TrackListModel *source,
                  TrackFilterModel *view, PlaylistListModel *playlists, QObject *parent = nullptr);
    QString playlistId() const { return QString::number(m_source->playlist()); }
    QString playlistName() const;
    QString error() const { return m_error; }
    Q_INVOKABLE bool openPlaylist(const QString &id);
    Q_INVOKABLE void play(const QString &id);
    Q_INVOKABLE void playAll();
    Q_INVOKABLE bool toggleFavorite(const QString &id);
    Q_INVOKABLE bool removeTrack(const QString &id);
    Q_INVOKABLE QString createPlaylist(const QString &name);
    Q_INVOKABLE bool renamePlaylist(const QString &id, const QString &name);
    Q_INVOKABLE bool deletePlaylist(const QString &id);
    Q_INVOKABLE bool addToPlaylist(const QString &playlist, const QString &track);
    Q_INVOKABLE bool removeFromPlaylist(const QString &track);
    Q_INVOKABLE bool moveInPlaylist(const QString &track, int direction);
  signals:
    void playlistChanged();
    void errorChanged();
  private:
    QPointer<Library> m_library;
    QPointer<PlaybackFacade> m_facade;
    TrackListModel *m_source;
    TrackFilterModel *m_view;
    PlaylistListModel *m_playlists;
    QString m_error;
    bool result(bool ok, const QString &fallback = {});
    bool hasPlaylist(qint64 id) const;
};
}
