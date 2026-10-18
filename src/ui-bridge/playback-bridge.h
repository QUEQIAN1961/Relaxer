#pragma once
#include "playback-facade.h"
#include <QPointer>

namespace relaxer {
class PlaybackBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY trackChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY progressChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY progressChanged)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  public:
    explicit PlaybackBridge(PlaybackFacade *facade, QObject *parent = nullptr);
    QString title() const { return m_state.track.title; }
    QString artist() const { return m_state.track.artist; }
    bool hasTrack() const { return m_state.track.id != 0; }
    bool playing() const { return m_state.playing; }
    qint64 position() const { return m_state.position; }
    qint64 duration() const { return m_state.duration; }
    int volume() const { return m_state.volume; }
    bool available() const { return m_facade && !m_facade->isShutdown(); }
    QString error() const { return m_error; }
    Q_INVOKABLE void toggle();
  signals:
    void trackChanged();
    void playingChanged();
    void progressChanged();
    void volumeChanged();
    void availableChanged();
    void errorChanged();
  private:
    QPointer<PlaybackFacade> m_facade;
    PlaybackSnapshot m_state;
    QString m_error;
    bool m_available = false;
    void refresh();
    void setError(const QString &error);
};
}
