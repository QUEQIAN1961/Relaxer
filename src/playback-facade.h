#pragma once
#include "playback.h"
#include <QSet>

namespace relaxer {
// A value read from the authoritative engine. Reading never forecasts shuffle bags.
struct PlaybackSnapshot {
    Track track;
    qint64 position = 0, duration = 0;
    bool playing = false, seekable = false, shutdown = false;
    int volume = 0;
    Mode mode = Mode::ListLoop;
    QMediaPlayer::PlaybackState transport = QMediaPlayer::StoppedState;
    QMediaPlayer::MediaStatus mediaStatus = QMediaPlayer::NoMedia;
    QVector<qint64> queue;
};
class PlaybackFacade final : public QObject {
    Q_OBJECT
  public:
    explicit PlaybackFacade(Library *library, QObject *parent = nullptr, bool decodeOnly = false);
    ~PlaybackFacade() override;
    PlaybackSnapshot snapshot() const;
    // Compatibility seam for existing consumers; production UI uses the facade.
    Playback *backend() const { return m_playback; }
    Track current() const { return m_playback->current(); }
    qint64 position() const { return m_playback->position(); }
    qint64 duration() const { return m_playback->duration(); }
    bool playing() const { return m_playback->playing(); }
    bool seekable() const { return m_playback->seekable(); }
    int volume() const { return m_playback->volume(); }
    Mode mode() const { return m_playback->mode(); }
    const Queue &queue() const { return m_playback->queue(); }
    QString roundStatus() const { return m_playback->roundStatus(); }
    QVector<qint64> upcoming();
    void play(const QVector<qint64> &ids, qint64 start = 0);
    void toggle();
    void next();
    void previous();
    void playUpcoming(int index);
    void seek(qint64 position);
    void setMode(Mode mode);
    void setVolume(int volume);
    void forget(const QVector<qint64> &ids);
    void restore();
    bool save();
    bool shutdown(bool force=false);
    enum class ExitDecision { Retry=0, ExitAnyway=1, Cancel=2 };
    Q_ENUM(ExitDecision)
    // Shared interactive decision boundary for both frontends.
    bool requestExit(ExitDecision decision=ExitDecision::Retry);
    QString exitError() const { return m_playback->lastSaveError(); }
    bool isShutdown() const { return m_playback->isShutdown(); }
  signals:
    void snapshotChanged(const relaxer::PlaybackSnapshot &snapshot);
    void trackChanged();
    void stateChanged();
    void positionChanged(qint64 position, qint64 duration);
    void seekableChanged(bool seekable);
    void error(const QString &message);
    void saveFailed(const QString &message);
    void saved(bool success);
    void playbackAttemptStarted(quint64 operation);
    void playbackFailure(quint64 operation, qint64 trackId, const QString &title,
                         const QString &message, int code);
    void playbackRecovered();

  private:
    Playback *m_playback;
    Library *m_library;
    quint64 m_operation = 0;
    qint64 m_requestedTrack = 0, m_failedTrack = 0;
    QSet<QString> m_reportedFailures;
    void beginAttempt(qint64 requested = 0);
    QTimer m_saveTimer;
    enum class Notice { Track, State, Position, Seekable, Error, Snapshot };
    bool m_shuttingDown = false;
    void notify(Notice notice, qint64 position = 0, qint64 duration = 0, const QString &error = {});
    void publish();
};
} // namespace relaxer
Q_DECLARE_METATYPE(relaxer::PlaybackSnapshot)
