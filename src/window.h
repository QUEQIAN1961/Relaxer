#pragma once
#include "ai/ai-settings-page.h"
#include "ai/ai-runtime.h"
#include "appearance.h"
#include "motion.h"
#include "nextqueue.h"
#include "lyric-tools.h"
#include "v14widgets.h"
#include "lyricslookup.h"
#include <QTimer>
#include "chrome.h"
#include "import-coordinator.h"
#include "playback-facade.h"
#include "shortcuts.h"
#include "widgets.h"
#include "playlist-ui.h"
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QProgressBar>
#include <QStackedWidget>
#include <QSystemTrayIcon>
namespace relaxer {
class Window : public QMainWindow {
    Q_OBJECT
  public:
    explicit Window(Library *library, QWidget *p = nullptr);
    ~Window() override;
    Playback *playback() { return m_player->backend(); }
    PlaybackFacade *playbackFacade() { return m_player; }
    void showNowPlaying();
    void showLibrary();
    void showPlaylists();
    void showProfile();
    void showSettings();
    void showVersionLog();
    void importSources(const QStringList &paths);

  protected:
    void closeEvent(QCloseEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void showEvent(QShowEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    bool eventFilter(QObject *,QEvent *) override;
    void changeEvent(QEvent *) override;
    bool nativeEvent(const QByteArray &,void *,qintptr *) override;

  private:
    QStackedWidget *m_bottomStack=nullptr;
    QPointer<SongTable> m_bulkTable;
    qint64 m_bulkScope=0;
    QLabel *m_bulkCount=nullptr;
    QPushButton *m_bulkDelete=nullptr,*m_bulkRemove=nullptr;
    QList<QPushButton *> m_bulkActions;
    FilterButton *m_artistFilter=nullptr,*m_albumFilter=nullptr;
    QWidget *m_filterHost=nullptr;
    bool m_filterRefreshing=false,m_profileScrolled=false,m_playlistScrolled=false,m_restoringWindow=true;
    ProfileHeader *m_profileHeader=nullptr;
    QTimer *m_sizeSaveTimer=nullptr;
    QSize m_normalSize;
    QWidget *buildBulkBar();
    void enterBulk(SongTable *table);
    void exitBulk();
    void updateBulk();
    void batchDelete();
    void removeFromPlaylist(const QVector<qint64> &ids,qint64 playlist);
    void newPlaylistWith(const QVector<qint64> &ids);
    void refreshFilters();
    void lookupLyrics(const QVector<qint64> &ids,bool pinExplicit=false);
    void lookupMissingLyrics();
    void saveWindowSize();
    Library *m_library;
    PlaybackFacade *m_player;
    QListWidget *m_nav, *m_lists;
    QStackedWidget *m_pages;
    SongTable *m_songs, *m_recent;
    QLineEdit *m_search;
    QLabel *m_title, *m_subtitle, *m_bottomTitle, *m_bottomArtist, *m_time, *m_duration, *m_nowTitle,
        *m_nowArtist;
    QLabel *m_statSongs, *m_statPlays, *m_statTime, *m_profileName;
    QStackedWidget *m_songStack;
    QListWidget *m_cards;
    Cover *m_bottomCover, *m_nowCover, *m_avatar, *m_listCover;
    Backdrop *m_nowPage=nullptr;
    Surface *m_sideSurface, *m_paperSurface, *m_footerSurface=nullptr;
    QWidget *m_contentHost=nullptr;
    bool m_playerExpanded=false;
    int m_returnPage=0;
    QWidget *m_volumeHost=nullptr;
    QPropertyAnimation *m_volumeMotion=nullptr;
    bool m_volumeTransition=false;
    IconButton *m_playbackToggle=nullptr;
    void updateVolumePosition(bool animate=false);
    void updatePlaybackToggle();
    void openAiSettings();
    void closeNowPlaying();
    void setPlayerPage(bool expanded,int page);
    PageMotion *m_motion=nullptr;
    NextQueuePanel *m_nextCompact=nullptr,*m_nextLarge=nullptr;
    ArtworkFlight *m_queueFlight=nullptr;
    QTimer *m_queueBackgroundTimer=nullptr;
    QLabel *m_toastNotice=nullptr;
    QTimer m_toastTimer;
    void positionToast();
    Mode m_lastQueueMode=static_cast<Mode>(-1);
    void refreshQueue();void updateQueueGeometry();void activateUpcoming(int index,bool large);
    MovingArtwork capturePlayingArtwork() const;
    void switchTrack(int direction);
    void startTrackAnimation(MovingArtwork art,int direction=0);
    quint64 m_motionRevision=0;
    qint64 m_presentedScope=0;
    void settleMotion();
    PlayerShortcuts *m_shortcuts;
    QColor m_accent = QColor("#e5455f"), m_ink = QColor("#25252b"), m_sideInk = QColor("#25252b");
    QJsonObject m_appearancePreview,m_currentAppearance;
    QPointer<AppearancePage> m_appearancePage;
    QPointer<ai::AiSettingsPage> m_aiPage;
    ai::AiRuntime *m_aiRuntime = nullptr;
    TitleBar *m_topBar=nullptr;
    bool m_frameInitialized=false,m_maximumPressed=false;
    QWidget *m_libraryActions;
    PlaylistHeader *m_playlistHeader;
    int m_settingsScrollPosition=0;
    LyricView *m_lyrics;
    QDoubleSpinBox *m_lyricDelay=nullptr;
    QWidget *m_delayControls=nullptr;
    LyricTools *m_lyricTools=nullptr;
    qint64 m_loadedLyricId=-1;
    QByteArray m_loadedLyricData;
    Slider *m_seek, *m_volume;
    CircleButton *m_play;
    IconButton *m_mode,*m_like;
    QPushButton *m_rename, *m_reorderUp, *m_reorderDown;
    QProgressBar *m_importProgress;
    QLabel *m_importLabel;
    QPushButton *m_cancel;
    QSystemTrayIcon *m_tray = nullptr;
    ImportCoordinator *m_importCoordinator=nullptr;
    bool m_closePending=false;
    qint64 m_scope = 0;
    QString m_view = "local";
    bool m_dark = false, m_refreshing = false, m_quitting = false, m_statsPending = false;
    QVector<Track> visibleTracks() const;
    void buildSidebar(QWidget *);
    QWidget *buildLibrary();
    QWidget *buildPlaylists();
    QWidget *buildProfile();
    QWidget *buildSettings();
    QWidget *buildVersionLog();
    QWidget *buildNowPlaying();
    QWidget *buildPlayerBar();
    void refresh();
    void updateLibrarySummary(const QVector<Track> &tracks);
    void refreshStats();
    void updateProfileTabs();
    void refreshTrack();
    void repairLegacyArtwork();
    void refreshPlaylists();
    void applyTheme();
    void updatePlayerBackground();
    void editAppearance();
    void selectScope(qint64 id);
    void createPlaylist();
    void playlistMenu(qint64 id, const QPoint &point);
    void songMenu(SongTable *, const QPoint &);
    void chooseFiles();
    void chooseFolder();
    void reorder(int direction);
    void setPage(int page);
    void updateNavigation();
    void editPlaylistTags();
    void editPlaylistDescription();
    bool isDragPosition(const QPoint &) const;
    void updateNativeFrameTheme();
    bool check(bool ok);
    void toast(const QString &message);
};
} // namespace relaxer
