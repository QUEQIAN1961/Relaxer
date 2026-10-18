#include "playback-bridge.h"
#include <QThread>

namespace relaxer {
PlaybackBridge::PlaybackBridge(PlaybackFacade *facade, QObject *parent) : QObject(parent), m_facade(facade) {
    Q_ASSERT(facade && facade->thread() == thread());
    connect(facade, &PlaybackFacade::snapshotChanged, this, [this] { refresh(); });
    connect(facade, &PlaybackFacade::saved, this, [this] { refresh(); });
    connect(facade, &PlaybackFacade::error, this, &PlaybackBridge::setError);
    connect(facade, &PlaybackFacade::saveFailed, this, &PlaybackBridge::setError);
    const auto invalidate = [this] { m_facade = nullptr; refresh(); };
    // The legacy facade disconnects every outgoing connection at the start of
    // its destructor. Its owned backend still emits destroyed, before facade
    // QObject destruction, so use that lifetime notification as well.
    connect(facade->backend(), &QObject::destroyed, this, invalidate);
    refresh();
}
void PlaybackBridge::refresh() {
    Q_ASSERT(QThread::currentThread() == thread());
    const auto next = m_facade ? m_facade->snapshot() : PlaybackSnapshot{};
    const auto previous = m_state;
    const bool wasAvailable = m_available;
    m_state = next;
    m_available = available();
    if (previous.track.id != next.track.id || previous.track.title != next.track.title ||
        previous.track.artist != next.track.artist) emit trackChanged();
    if (previous.playing != next.playing) emit playingChanged();
    if (previous.position != next.position || previous.duration != next.duration) emit progressChanged();
    if (previous.volume != next.volume) emit volumeChanged();
    if (wasAvailable != m_available) emit availableChanged();
}
void PlaybackBridge::toggle() {
    Q_ASSERT(QThread::currentThread() == thread());
    if (available() && m_facade->current().id) m_facade->toggle();
}
void PlaybackBridge::setError(const QString &error) {
    if (m_error != error) { m_error = error; emit errorChanged(); }
}
}
