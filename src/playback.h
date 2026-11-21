#pragma once
#include "domain/queue.h"
#include "library.h"
#include "midi-transport.h"
#include "audio-fader.h"
#include <QAudioOutput>
#include <QAudioDevice>
#include <QElapsedTimer>
#include <QMediaPlayer>
#include <QPointer>
#include <QProcess>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
namespace relaxer {
class Playback : public QObject {
    Q_OBJECT
  public:
    explicit Playback(Library *library, QObject *parent = nullptr, bool decodeOnly = false);
    ~Playback() override;
    void play(const QVector<qint64> &ids, qint64 start = 0);
    void toggle();
    void pauseForQueue();
    void stopForQueue();
    void restoreQueued(qint64 id,qint64 position);
    void setExternalQueue(bool enabled){m_externalQueue=enabled;}
    void next();
    QVector<qint64> upcoming() {return m_queue.upcoming();}
    void playUpcoming(int index);
    void previous();
    void seek(qint64 position);
    void setMode(Mode mode);
    void setVolume(int value);
    void restore();
    bool save();
    bool shutdown(bool force=false);
    bool lastSaveWrote() const { return m_lastSaveWrote; }
    QString lastSaveError() const { return m_saveError; }
    bool isShutdown() const { return m_shutdown; }
    void forget(const QVector<qint64> &ids);
    Track current() const;
    qint64 position() const;
    qint64 duration() const;
    bool playing() const;
    QMediaPlayer::PlaybackState transportState() const;
    bool seekable() const;
    Mode mode() const { return m_queue.mode; }
    const Queue &queue() const { return m_queue; }
    QMediaPlayer *media() { return &m_player; }
    int volume() const { return m_volume; }
    QAudioDevice outputDevice() const;
    bool applyOutputDevice(const QAudioDevice &device);
    QString roundStatus() const;
  signals:
    void trackChanged();
    void stateChanged();
    void positionChanged(qint64, qint64);
    void error(const QString &message);
    void saveFailed(const QString &message);
    void advanceRequested(bool failed);

  private:
    friend class PlaybackDiagnostics; // Controlled clock samples in contract tests.
    Library *m_library;
    std::unique_ptr<QAudioOutput> m_output;
    int m_volume = 65;
    AudioFader m_fader;
    QMediaPlayer m_player;
    Queue m_queue;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    Session m_session;
    qint64 m_historyHeard=0;
    void finishHistory();
    qint64 m_lastPosition = 0, m_lastWall = 0, m_restorePosition = -1;
    int m_threshold = 80;
    bool m_loading = false, m_autoStart = false, m_wasPlaying = false, m_decodeOnly = false;
    bool m_externalQueue=false;
    QSet<qint64> m_failed;
    bool m_midi = false, m_midiPlaying = false;
    std::unique_ptr<MidiTransport> m_midiBackend;
    qint64 m_midiDuration = 0;
    int m_playGeneration = 0;
    QPointer<QProcess> m_transcode;
    void applyLocalVideoMode();
    void load(qint64 id, bool autoplay, bool resume = false);
    bool m_shutdown = false, m_shutdownInProgress = false, m_shutdownSaved = true;
    mutable Track m_currentTrack;
    mutable bool m_currentTrackValid=false;
    QVector<qint64> m_savedQueueIds;
    QJsonArray m_savedQueueArray;
    qint64 m_emittedPosition=-1,m_emittedDuration=-1;
    QJsonObject m_lastSavedQueue;
    bool m_haveSavedQueue=false,m_lastSaveWrote=false;
    QString m_saveError;
    bool sample(bool finishing = false, bool flush = false);
    bool sampleAt(qint64 now, qint64 position, bool isPlaying, bool finishing, bool flush);
    void finish();
    bool saveQueue();
    void onError(const QString &);
    void haltTransport();
    void stopMidi();
    bool startMidi(const QString &path, bool autoplay);
    bool ensurePlayable(const Track &track, bool autoplay);
    QAudioOutput *audioOutput();
};
} // namespace relaxer
