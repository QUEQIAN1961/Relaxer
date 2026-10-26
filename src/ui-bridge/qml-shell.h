#pragma once
#include "app-bridge.h"
#include "feedback-bridge.h"
#include "playback-bridge.h"
#include "library-bridge.h"
#include "queue-model.h"
#include "upcoming-model.h"
#include "ui-integration.h"
#include "lyrics-model.h"
#include "settings-bridge.h"
#include "import-bridge.h"
#include "ai-bridge.h"
#include "translation-bridge.h"
#include "lyrics-search-bridge.h"
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <memory>

namespace relaxer {
// Library outlives this owner. Engine is explicitly destroyed before its borrowed bridges.
class QmlShell final : public QObject {
    Q_OBJECT
  public:
    explicit QmlShell(Library *library, QObject *parent = nullptr, bool decodeOnly = false,
                      ai::AiRuntime *runtime = nullptr, std::unique_ptr<TranslationBrowserBackend> browser = {},
                      SettingsBridge::SystemThemeReader systemTheme = {},
                      QNetworkAccessManager *searchTransport = nullptr, QVector<LyricsSearchSource> searchSources = {});
    ~QmlShell() override;
    bool load();
    bool shutdown();
    QQuickWindow *window() const;
    AppBridge *appBridge() { return &m_appBridge; }
    PlaybackBridge *playbackBridge() { return &m_playbackBridge; }
    FeedbackBridge *feedbackBridge() { return &m_feedback; }
    PlaybackFacade *facade() { return &m_facade; }
    TrackFilterModel *libraryModel() { return &m_trackView; }
    LibraryBridge *libraryBridge() { return &m_libraryBridge; }
    PlaylistListModel *playlistModel() { return &m_playlists; }
    QueueListModel *queueModel() { return &m_queue; }
    UpcomingModel *upcomingModel() { return &m_upcoming; }
    LyricsModel *lyricsModel() { return &m_lyrics; }
    SettingsBridge *settingsBridge() { return &m_settings; }
    ImportBridge *importBridge() { return &m_importBridge; }
    ImportCoordinator *importCoordinator() { return &m_import; }
    AiBridge *aiBridge() { return &m_aiBridge; }
    TranslationBridge *translationBridge() { return &m_translationBridge; }
    TranslationCoordinator *translationCoordinator() { return &m_translation; }
    LyricsSearchBridge *lyricsSearchBridge() { return &m_searchBridge; }
    QStringList warnings() const { return m_warnings; }
  protected:
    bool eventFilter(QObject *object,QEvent *event) override;
  private:
    FeedbackBridge m_feedback;
    PlaybackFacade m_facade;
    AppBridge m_appBridge;
    PlaybackBridge m_playbackBridge;
    TrackListModel m_tracks;
    TrackFilterModel m_trackView;
    PlaylistListModel m_playlists;
    LibraryBridge m_libraryBridge;
    QueueListModel m_queue;
    UpcomingModel m_upcoming;
    UiIntegration m_uiIntegration;
    std::unique_ptr<ai::AiRuntime> m_ownedAi;
    ai::AiRuntime *m_ai;
    ImportCoordinator m_import;
    LyricsModel m_lyrics;
    SettingsBridge m_settings;
    AiBridge m_aiBridge;
    TranslationCoordinator m_translation;
    TranslationBridge m_translationBridge;
    ImportBridge m_importBridge;
    LyricsSearchCoordinator m_search;
    LyricsSearchBridge m_searchBridge;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QStringList m_warnings;
    bool m_artworkRecoveryScheduled=false;
};
}
