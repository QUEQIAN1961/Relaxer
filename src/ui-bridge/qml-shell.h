#pragma once
#include "app-bridge.h"
#include "playback-bridge.h"
#include "library-bridge.h"
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <memory>

namespace relaxer {
// Library outlives this owner. Engine is explicitly destroyed before its borrowed bridges.
class QmlShell final : public QObject {
    Q_OBJECT
  public:
    explicit QmlShell(Library *library, QObject *parent = nullptr, bool decodeOnly = false);
    ~QmlShell() override;
    bool load();
    bool shutdown();
    QQuickWindow *window() const;
    AppBridge *appBridge() { return &m_appBridge; }
    PlaybackBridge *playbackBridge() { return &m_playbackBridge; }
    PlaybackFacade *facade() { return &m_facade; }
    TrackFilterModel *libraryModel() { return &m_trackView; }
    LibraryBridge *libraryBridge() { return &m_libraryBridge; }
    PlaylistListModel *playlistModel() { return &m_playlists; }
    QStringList warnings() const { return m_warnings; }
  private:
    PlaybackFacade m_facade;
    AppBridge m_appBridge;
    PlaybackBridge m_playbackBridge;
    TrackListModel m_tracks;
    TrackFilterModel m_trackView;
    PlaylistListModel m_playlists;
    LibraryBridge m_libraryBridge;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QStringList m_warnings;
};
}
