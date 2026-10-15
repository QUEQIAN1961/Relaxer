#pragma once
#include "library.h"
#include <QAudioOutput>
#include <QElapsedTimer>
#include <QMediaPlayer>
#include <QPointer>
#include <QProcess>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>
namespace relaxer {
enum class Mode { Sequential, ListLoop, SingleLoop, Shuffle };
// A shuffle bag visits every available track once before opening a new round.
class Queue {
  public:
    QVector<qint64> items;
    int index = -1;
    Mode mode = Mode::ListLoop;
    void set(QVector<qint64> ids, qint64 current = 0);
    qint64 current() const { return index >= 0 && index < items.size() ? items[index] : 0; }
    qint64 next(bool manual = false);
    qint64 previous();
    void setMode(Mode m);
    QVector<qint64> upcoming(int count=-1);

  private:
    QVector<qint64> bag, history;
    int historyIndex = -1;
    QVector<QVector<qint64>> m_plannedBags,m_generated;
    bool m_previewing=false;
    void refill(bool full = false);
};
class Playback : public QObject {
    Q_OBJECT
  public:
    explicit Playback(Library *library, QObject *parent = nullptr, bool decodeOnly = false);
    ~Playback() override;
    void play(const QVector<qint64> &ids, qint64 start = 0);
    void toggle();
    void next();
    QVector<qint64> upcoming() {return m_queue.upcoming();}
    void playUpcoming(int index);
    void previous();
    void seek(qint64 position);
    void setMode(Mode mode);
    void setVolume(int value);
    void restore();
    void save();
    void forget(const QVector<qint64> &ids);
    Track current() const { return m_library->track(m_queue.current()); }
    qint64 position() const;
    qint64 duration() const;
    bool playing() const;
    bool seekable() const;
    Mode mode() const { return m_queue.mode; }
    const Queue &queue() const { return m_queue; }
    QMediaPlayer *media() { return &m_player; }
    int volume() const { return qRound(m_output.volume() * 100); }
    QString roundStatus() const;
  signals:
    void trackChanged();
    void stateChanged();
    void positionChanged(qint64, qint64);
    void error(const QString &message);

  private:
    Library *m_library;
    QAudioOutput m_output;
    QMediaPlayer m_player;
    Queue m_queue;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    Session m_session;
    qint64 m_lastPosition = 0, m_lastWall = 0, m_restorePosition = -1;
    int m_threshold = 80;
    bool m_loading = false, m_autoStart = false, m_wasPlaying = false, m_decodeOnly = false;
    QSet<qint64> m_failed;
    bool m_midi = false, m_midiPlaying = false;
    qint64 m_midiDuration = 0;
    int m_playGeneration = 0;
    QPointer<QProcess> m_transcode;
    void load(qint64 id, bool autoplay, bool resume = false);
    void sample(bool finishing = false, bool flush = false);
    void finish();
    void saveQueue();
    void onError(const QString &);
    void haltTransport();
    void stopMidi();
    bool startMidi(const QString &path, bool autoplay);
    bool ensurePlayable(const Track &track, bool autoplay);
};
} // namespace relaxer
