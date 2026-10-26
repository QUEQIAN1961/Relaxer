#pragma once
#include "playback-facade.h"
#include <QPointer>
#include <QUrl>

namespace relaxer {
class PlaybackBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString trackId READ trackId NOTIFY trackChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY trackChanged)
    Q_PROPERTY(QUrl coverUrl READ coverUrl NOTIFY artworkChanged)
    Q_PROPERTY(QString artworkRevision READ artworkRevision NOTIFY artworkChanged)
    Q_PROPERTY(bool currentFavorite READ currentFavorite NOTIFY currentFavoriteChanged)
    Q_PROPERTY(int transitionDirection READ transitionDirection NOTIFY transitionDirectionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY progressChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY progressChanged)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY volumeChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canPlay READ canPlay NOTIFY capabilitiesChanged)
    Q_PROPERTY(int mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString exitError READ exitError NOTIFY errorChanged)
  public:
    explicit PlaybackBridge(PlaybackFacade *facade, QObject *parent = nullptr, Library *library = nullptr);
    QString title() const { return m_metadata.id == m_state.track.id ? m_metadata.title : m_state.track.title; }
    QString artist() const { return m_metadata.id == m_state.track.id ? m_metadata.artist : m_state.track.artist; }
    QString trackId() const { return QString::number(m_state.track.id); }
    bool hasTrack() const { return m_state.track.id != 0; }
    QUrl coverUrl() const { return m_coverUrl; }
    QString artworkRevision() const { return m_artworkRevision; }
    bool currentFavorite() const { return m_metadata.id == m_state.track.id && m_metadata.favorite; }
    int transitionDirection() const { return m_transitionDirection; }
    bool playing() const { return m_state.playing; }
    qint64 position() const { return m_state.position; }
    qint64 duration() const { return m_state.duration; }
    int volume() const { return m_state.volume; }
    bool muted() const { return volume() == 0; }
    bool seekable() const { return m_state.seekable && m_state.duration > 0 && available(); }
    bool canPlay() const { return m_canPlay; }
    int mode() const { return int(m_state.mode); }
    bool available() const { return m_facade && !m_facade->isShutdown(); }
    QString error() const { return m_error; }
    QString exitError() const { return m_facade?m_facade->exitError():QString(); }
    Q_INVOKABLE bool requestExit(int decision=0);
    Q_INVOKABLE bool nonInteractiveExit() const;
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void playUpcoming(int index);
    Q_INVOKABLE void toggleFavorite();
    Q_INVOKABLE void seek(qint64 position);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void setMode(int mode);
    Q_INVOKABLE void playQueued(const QString &id);
    Q_INVOKABLE void removeFromQueue(const QString &id);
  signals:
    void trackChanged();
    void artworkChanged();
    void currentFavoriteChanged();
    void transitionDirectionChanged();
    // Emitted before a manual command so the frontend can capture the old
    // cover, including repeated presses in the same direction.
    void transitionStarted(int direction);
    void playingChanged();
    void progressChanged();
    void volumeChanged();
    void capabilitiesChanged();
    void modeChanged();
    void availableChanged();
    void errorChanged();
    void failure(quint64 operation, const QString &target, const QString &title, const QString &message, int code);
    void persistenceFailure(const QString &message);
    void recovered();
  public:
    void refreshArtwork(qint64 trackId) {if(m_state.track.id==trackId)refreshMetadata();}
  private:
    QPointer<PlaybackFacade> m_facade;
    QPointer<Library> m_library;
    PlaybackSnapshot m_state;
    Track m_metadata;
    QUrl m_coverUrl;
    QString m_artworkRevision;
    QString m_error, m_saveError;
    bool m_available = false;
    bool m_canPlay = false;
    int m_lastVolume = 65;
    int m_transitionDirection = 0;
    void refresh();
    void refreshMetadata();
    void readMetadata();
    void beginTransition(int direction);
    void setError(const QString &error);
};
}
