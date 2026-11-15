#include "qml-shell.h"
#include "ui-images.h"
#include <QQmlContext>
#include <QQuickStyle>
#include <QEventLoop>
#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QQmlNetworkAccessManagerFactory>

static void initializeQmlResources() { Q_INIT_RESOURCE(qml); }
namespace relaxer {
namespace {
class QmlHttpNetwork final:public QNetworkAccessManager {
public:using QNetworkAccessManager::QNetworkAccessManager;
protected:QNetworkReply *createRequest(Operation operation,const QNetworkRequest &original,QIODevice *data)override{
    QNetworkRequest request(original);request.setAttribute(QNetworkRequest::Http2AllowedAttribute,false);
    return QNetworkAccessManager::createRequest(operation,request,data);
}
};
class QmlHttpFactory final:public QQmlNetworkAccessManagerFactory {
public:QNetworkAccessManager *create(QObject *parent)override{return new QmlHttpNetwork(parent);}
};
}

QmlShell::QmlShell(Library *library, QObject *parent, bool decodeOnly,ai::AiRuntime *runtime,
                   std::unique_ptr<TranslationBrowserBackend> browser, SettingsBridge::SystemThemeReader systemTheme,
                   QNetworkAccessManager *searchTransport, QVector<LyricsSearchSource> searchSources)
    : QObject(parent), m_feedback(this), m_facade(library, this, decodeOnly), m_appBridge(library, this),
      m_playbackBridge(&m_facade, this, library), m_tracks(library, this), m_trackView(&m_tracks, this),
      m_playlists(library, this), m_libraryBridge(library, &m_facade, &m_tracks, &m_trackView, &m_playlists, this),
      m_queue(library, &m_facade, this),
      m_upcoming(library, &m_facade, this), m_uiIntegration(library, &m_playbackBridge, this),
      m_ownedAi(runtime?nullptr:std::make_unique<ai::AiRuntime>(library)),m_ai(runtime?runtime:m_ownedAi.get()),
      m_import(library,this),m_lyrics(library,&m_facade,this),m_settings(library,this,std::move(systemTheme)),m_aiBridge(m_ai,this),
      m_translation(library,m_ai,this,std::move(browser)),m_translationBridge(library,&m_facade,&m_translation,&m_lyrics,this),
      m_importBridge(library,&m_import,this),m_search(library,this,QUrl("https://lrclib.net/api"),{},searchTransport,std::move(searchSources)),m_searchBridge(library,&m_facade,&m_search,this),
      m_onlineArtwork(library,&m_facade,this),m_onlineDownloads(library,this),m_onlineMusic(library,&m_facade,&m_lyrics,m_ai,&m_onlineArtwork,this),m_qqMusic(library,&m_facade,&m_lyrics,m_ai,&m_onlineArtwork,this),
      m_kugouMusic(AdditionalMusicBridge::Kugou,library,&m_facade,&m_lyrics,&m_onlineArtwork,this),
      m_sodaMusic(AdditionalMusicBridge::Qishui,library,&m_facade,&m_lyrics,&m_onlineArtwork,this),
      m_bilibili(library,&m_facade,&m_onlineArtwork,this) {
    m_navigationLibrary=library;
    initializeQmlResources();
    // Embed the standard controls' Chinese strings so portable builds do not
    // depend on a Qt translation package installed on the user's computer.
    if(m_controlsTranslator.load(":/qml/translations/controls_zh_CN.qm"))
        QCoreApplication::installTranslator(&m_controlsTranslator);
    m_searchBridge.setTranslationCoordinator(&m_translation);
    setObjectName("qmlShell");
    // A fixed official style makes custom control backgrounds portable on Windows.
    static const bool styleConfigured = [] {
        QQuickStyle::setStyle("Basic");
#ifdef Q_OS_WIN
        // D3D11 uses DirectComposition + stretch scaling for an alpha surface.
        // The scene itself stays opaque. During native growth the compositor
        // can extend the last painted frame instead of exposing a black strip.
        QQuickWindow::setDefaultAlphaBuffer(true);
#endif
        return true;
    }();
    Q_UNUSED(styleConfigured)
    QQmlEngine::setObjectOwnership(&m_appBridge, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&m_playbackBridge, QQmlEngine::CppOwnership);
    m_engine = std::make_unique<QQmlApplicationEngine>();
    static QmlHttpFactory httpFactory;m_engine->setNetworkAccessManagerFactory(&httpFactory);
    m_engine->addImageProvider("relaxer", new UiImages);
    m_engine->rootContext()->setContextProperty("appBridge", &m_appBridge);
    m_engine->rootContext()->setContextProperty("playbackBridge", &m_playbackBridge);
    m_engine->rootContext()->setContextProperty("onlineArtwork", &m_onlineArtwork);
    m_engine->rootContext()->setContextProperty("kugouMusic", &m_kugouMusic);
    m_engine->rootContext()->setContextProperty("sodaMusic", &m_sodaMusic);
    m_engine->rootContext()->setContextProperty("bilibili", &m_bilibili);
    m_engine->rootContext()->setContextProperty("onlineDownloads",&m_onlineDownloads);QQmlEngine::setObjectOwnership(&m_onlineDownloads,QQmlEngine::CppOwnership);
    for (auto *object : {static_cast<QObject *>(&m_trackView), static_cast<QObject *>(&m_playlists), static_cast<QObject *>(&m_libraryBridge)})
        QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
    m_engine->rootContext()->setContextProperty("libraryModel", &m_trackView);
    m_engine->rootContext()->setContextProperty("playlistModel", &m_playlists);
    m_engine->rootContext()->setContextProperty("libraryBridge", &m_libraryBridge);
    QQmlEngine::setObjectOwnership(&m_queue, QQmlEngine::CppOwnership);
    m_engine->rootContext()->setContextProperty("queueModel", &m_queue);
    for(auto pair : {qMakePair("lyricsModel",static_cast<QObject *>(&m_lyrics)),
                    qMakePair("upcomingModel",static_cast<QObject *>(&m_upcoming)),
                    qMakePair("uiIntegration",static_cast<QObject *>(&m_uiIntegration)),
                    qMakePair("settingsBridge",static_cast<QObject *>(&m_settings)),
                    qMakePair("importBridge",static_cast<QObject *>(&m_importBridge)),
                    qMakePair("aiBridge",static_cast<QObject *>(&m_aiBridge)),
                    qMakePair("translationBridge",static_cast<QObject *>(&m_translationBridge)),
                    qMakePair("lyricsSearchBridge",static_cast<QObject *>(&m_searchBridge)),
                    qMakePair("onlineMusic",static_cast<QObject *>(&m_onlineMusic)),
                    qMakePair("qqMusic",static_cast<QObject *>(&m_qqMusic))}){
        QQmlEngine::setObjectOwnership(pair.second,QQmlEngine::CppOwnership);
        m_engine->rootContext()->setContextProperty(QString::fromLatin1(pair.first),pair.second);
    }
    connect(&m_queue, &QueueListModel::missingTracks, &m_libraryBridge, &LibraryBridge::forgetMissing, Qt::QueuedConnection);
    connect(&m_playbackBridge,&PlaybackBridge::onlineFavoriteRequested,&m_onlineMusic,&OnlineMusicBridge::toggleFavorite);
    connect(&m_playbackBridge,&PlaybackBridge::onlineFavoriteRequested,&m_qqMusic,&QqMusicBridge::toggleFavorite);
    connect(&m_playbackBridge,&PlaybackBridge::onlineArtworkDetailRequested,&m_onlineArtwork,&OnlineArtworkCache::setExpanded);
    connect(&m_onlineMusic,&OnlineMusicBridge::downloadReady,&m_onlineDownloads,&OnlineDownloadBridge::chooseDestination);
    connect(&m_qqMusic,&QqMusicBridge::downloadReady,&m_onlineDownloads,&OnlineDownloadBridge::chooseDestination);
    connect(&m_onlineDownloads,&OnlineDownloadBridge::resolveRequested,this,[this](const QVariantMap &song,const QString &quality){const auto provider=song.value("provider").toString();if(provider=="qq")m_qqMusic.resolveDownload(song,quality);else if(provider=="kugou")m_kugouMusic.resolveDownload(song,quality);else if(provider=="qishui")m_sodaMusic.resolveDownload(song,quality);else m_onlineMusic.resolveDownload(song,quality);});
    connect(&m_onlineMusic,&OnlineMusicBridge::downloadFailed,&m_onlineDownloads,&OnlineDownloadBridge::resolutionFailed);
    connect(&m_qqMusic,&QqMusicBridge::downloadFailed,&m_onlineDownloads,&OnlineDownloadBridge::resolutionFailed);
    connect(&m_libraryBridge,&LibraryBridge::cloudPlaylistRequested,this,[this](const QString &provider,const QString &id){if(provider=="qq"){m_qqMusic.activate();m_qqMusic.openPlaylist(id);}else if(provider=="kugou"){m_kugouMusic.activate();m_kugouMusic.openPlaylist(id);}else if(provider=="qishui"){m_sodaMusic.activate();m_sodaMusic.openPlaylist(id);}else if(provider=="bilibili"){m_bilibili.activate();m_bilibili.openPlaylist(id);}else{m_onlineMusic.activate();m_onlineMusic.openPlaylist(id);}});
    connect(&m_libraryBridge,&LibraryBridge::cloudPlaylistPinned,this,[this](const QString &provider,const QString &id){if(provider=="qq")m_qqMusic.savePinnedPlaylist(id);else if(provider=="kugou")m_kugouMusic.savePinnedPlaylist(id);else if(provider=="qishui")m_sodaMusic.savePinnedPlaylist(id);else if(provider=="bilibili")m_bilibili.savePinnedPlaylist(id);else m_onlineMusic.savePinnedPlaylist(id);});
    connect(&m_onlineMusic,&OnlineMusicBridge::playlistMissing,this,[this](const QString &id){fallbackMissingPlaylist("netease",id);},Qt::QueuedConnection);
    connect(&m_qqMusic,&QqMusicBridge::playlistMissing,this,[this](const QString &id){fallbackMissingPlaylist("qq",id);},Qt::QueuedConnection);
    connect(&m_bilibili,&BilibiliBridge::playlistMissing,this,[this](const QString &id){fallbackMissingPlaylist("bilibili",id);},Qt::QueuedConnection);
    connect(&m_libraryBridge,&LibraryBridge::playlistChanged,this,[this]{if(m_libraryBridge.playlistId()!=m_restoredCloudKey)m_restoredCloudKey.clear();});
    connect(&m_appBridge,&AppBridge::pageChanged,this,[this]{if(m_appBridge.page()!=0)m_restoredCloudKey.clear();});
    connect(&m_libraryBridge,&LibraryBridge::cloudPlayRequested,this,[this](const QString &provider,const QStringList &ids,const QString &start){if(provider=="qq")m_qqMusic.playSongs(ids,start);else if(provider=="kugou")m_kugouMusic.playSongs(ids,start);else if(provider=="qishui")m_sodaMusic.playSongs(ids,start);else if(provider=="bilibili")m_bilibili.playSongs(ids,start);else m_onlineMusic.playSongs(ids,start);});
    connect(&m_bilibili,&BilibiliBridge::songsChanged,this,[this]{m_libraryBridge.setCloudSongs("bilibili",m_bilibili.currentPlaylist(),m_bilibili.songs());});
    connect(&m_onlineMusic,&OnlineMusicBridge::songsChanged,this,[this]{m_libraryBridge.setCloudSongs("netease",m_onlineMusic.currentPlaylist(),m_onlineMusic.songs());});
    connect(&m_qqMusic,&QqMusicBridge::songsChanged,this,[this]{m_libraryBridge.setCloudSongs("qq",m_qqMusic.currentPlaylist(),m_qqMusic.songs());});
    for(auto *platform:{&m_kugouMusic,&m_sodaMusic}){
        const auto provider=platform==&m_kugouMusic?QString("kugou"):QString("qishui");
        connect(platform,&AdditionalMusicBridge::playlistMissing,this,[this,provider](const QString &id){fallbackMissingPlaylist(provider,id);},Qt::QueuedConnection);
        connect(platform,&AdditionalMusicBridge::downloadReady,&m_onlineDownloads,&OnlineDownloadBridge::chooseDestination);
        connect(platform,&AdditionalMusicBridge::downloadFailed,&m_onlineDownloads,&OnlineDownloadBridge::resolutionFailed);
        connect(platform,&AdditionalMusicBridge::songsChanged,this,[this,platform,provider]{m_libraryBridge.setCloudSongs(provider,platform->currentPlaylist(),platform->songs());});
    }
    connect(&m_facade,&PlaybackFacade::onlineTrackUpdated,this,[this](qint64 id){m_tracks.updateCloudTrack(m_facade.track(id));});
    connect(&m_importBridge,&ImportBridge::contentsChanged,this,[this]{m_tracks.refresh();m_playlists.refresh();m_appBridge.refresh();});
    connect(&m_import,&ImportCoordinator::artworkUpdated,this,[this](qint64 id){
        m_playbackBridge.refreshArtwork(id);
        if(m_facade.current().id==id)m_lyrics.reload();
    });
    connect(&m_import,&ImportCoordinator::artworkFinished,this,[this](int recovered){if(recovered)m_upcoming.refresh();});
    connect(m_engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError> &errors) {
        for (const auto &error : errors) m_warnings.append(error.toString());
    });
    m_engine->rootContext()->setContextProperty("feedbackBridge", &m_feedback);
    QQmlEngine::setObjectOwnership(&m_feedback, QQmlEngine::CppOwnership);
    connect(m_ai,&ai::AiRuntime::startupConfigurationFailed,this,[this](const QString &message){
        m_feedback.post("AIStartup","Warning","AI 配置自动加载失败："+message,message,{}, {},true);
    });
    connect(&m_playbackBridge,&PlaybackBridge::failure,this,[this](quint64 operation,const QString &target,const QString &title,const QString &message,int code){
        m_feedback.post("Playback","Error",title.isEmpty()?QStringLiteral("播放失败"):QStringLiteral("无法播放：")+title,message,QString::number(operation),target,true,false,code);
    });
    connect(&m_playbackBridge,&PlaybackBridge::recovered,this,[this]{m_feedback.clearSource("Playback");});
    connect(&m_playbackBridge,&PlaybackBridge::persistenceFailure,this,[this](const QString &message){m_feedback.post("Library","Error","播放状态或资料修改未能保存",message,{}, {},true);});
    connect(&m_libraryBridge,&LibraryBridge::operationFailed,this,[this](const QString &message){m_feedback.post("Library","Error","资料库操作失败",message,{}, {},true);});
    connect(&m_libraryBridge,&LibraryBridge::operationSucceeded,this,[this]{m_feedback.clearSource("Library");});
    connect(&m_importBridge,&ImportBridge::started,this,[this]{m_feedback.clearSource("Import");});
    connect(&m_importBridge,&ImportBridge::completed,this,[this](const QString &category,const QString &task,const QString &message,const QString &details){m_feedback.post("Import",category,message,details,task,{},category=="Error");});
    connect(&m_importBridge,&ImportBridge::artworkCompleted,this,[this](const QString &category,const QString &task,const QString &message,const QString &details){
        // Background migration records one bounded history entry, without
        // replacing a playback error or interrupting normal placeholder use.
        m_feedback.post("Artwork",category,message,details,task,{},false,false,0,true);
    });
    connect(&m_importBridge,&ImportBridge::rejected,this,[this](const QString &message){m_feedback.post("Import","Error","无法开始导入",message,{}, {},true);});
    connect(&m_searchBridge,&LyricsSearchBridge::operationFailed,this,[this](const QString &target,const QString &message){m_feedback.post("Lyrics","Error","歌词未能保存："+m_searchBridge.targetTitle(),message,{},target,true);});
    connect(&m_translationBridge,&TranslationBridge::feedback,this,[this](const QString &category,const QString &task,const QString &target,const QString &message,const QString &details){m_feedback.post("Translation",category,message+" · "+m_translationBridge.targetTitle(),details,task,target,category=="Error",category=="ConfirmationRequired");});
    connect(&m_translationBridge,&TranslationBridge::feedbackCleared,this,[this]{m_feedback.clearSource("Translation");});
    connect(&m_searchBridge,&LyricsSearchBridge::saved,this,[this]{m_feedback.clearSource("Lyrics");});
    connect(&m_searchBridge,&LyricsSearchBridge::searchFeedback,this,[this](const QString &category,const QString &operation,const QString &target,const QString &message,const QString &details){m_feedback.post("LyricsSearch",category,message,details,operation,target,category=="Error");});
    connect(&m_searchBridge,&LyricsSearchBridge::editCompleted,this,[this](const QString &category,const QString &operation,const QString &message,const QString &details){m_feedback.post("LyricsEdit",category,message,details,operation,{},category=="Error");});
    m_facade.restore(true);
    QCoreApplication::instance()->installEventFilter(this);
}
QmlShell::~QmlShell() {
    m_feedback.setClosing(true);m_engine.reset();shutdown();
    // Direct owner destruction also waits for ImportCoordinator's existing
    // cancellation/result/thread-exit barrier before the Library can disappear.
    if(!m_import.isIdle()){
        QEventLoop barrier;connect(&m_import,&ImportCoordinator::shutdownReady,&barrier,&QEventLoop::quit);
        if(!m_import.isIdle())barrier.exec();
    }
}
bool QmlShell::load(int initialPage) {
    if (!m_engine->rootObjects().isEmpty()) return window() != nullptr;
    // Explicit --page remains an isolated screenshot/diagnostic override.
    // Normal launches restore only stable library IDs, never row positions.
    const auto saved=m_navigationLibrary->setting("qml.lastNavigation").toObject();
    int page=initialPage>=0?initialPage:saved.value("page").toInt(0);
    if(page<0||page>5)page=0;
    if(page==0){
        bool valid=false;
        const auto key=initialPage>=0?QStringLiteral("0"):saved.value("playlistId").toString("0");
        const auto id=key.toLongLong(&valid);
        const bool exists=valid&&id>=-3&&(id<=0||m_playlists.contains(id));
        m_libraryBridge.openPlaylist(exists?key:QStringLiteral("0"));
        if(initialPage<0&&exists&&!m_libraryBridge.cloudPlaylist(key).isEmpty())m_restoredCloudKey=key;
    }else if(page==2)m_libraryBridge.openPlaylist("-2");
    m_appBridge.navigate(page);
    m_engine->load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (auto *loaded = window()) {
        m_rememberNavigation=initialPage<0;
        // Release render resources on unexpose, preserving the QML state,
        // playback, tasks and native shortcut registrations.
        loaded->setPersistentGraphics(false);
        loaded->setPersistentSceneGraph(false);
        m_backgroundRelease.setSingleShot(true);m_backgroundRelease.setInterval(300);
        connect(&m_backgroundRelease,&QTimer::timeout,this,[this]{
                auto *view = window();
                if (!view || (view->isVisible() && view->visibility() != QWindow::Minimized)) return;
                view->releaseResources();
        });
        connect(loaded, &QWindow::visibilityChanged, this, [this](QWindow::Visibility visibility) {
            const bool backgrounded=visibility==QWindow::Hidden||visibility==QWindow::Minimized;
            if(auto *images=dynamic_cast<UiImages *>(m_engine->imageProvider("relaxer")))images->setBackgrounded(backgrounded);
            m_onlineArtwork.setPresentationActive(!backgrounded);
            if(backgrounded)m_backgroundRelease.start();else m_backgroundRelease.stop();
        });
        loaded->create();
        m_uiIntegration.attach(loaded);
        if(m_settings.windowBounds().value("maximized").toBool())loaded->showMaximized();else loaded->show();
        if (m_ownedAi) QTimer::singleShot(0,m_ai,&ai::AiRuntime::restoreSavedConfiguration);
        if(!m_artworkRecoveryScheduled){m_artworkRecoveryScheduled=true;QTimer::singleShot(0,this,[this]{m_import.repairLegacyArtwork();});}
    }
    return window() != nullptr;
}
QQuickWindow *QmlShell::window() const {
    return m_engine && !m_engine->rootObjects().isEmpty()
        ? qobject_cast<QQuickWindow *>(m_engine->rootObjects().first()) : nullptr;
}
void QmlShell::fallbackMissingPlaylist(const QString &provider,const QString &id){
    if(m_restoredCloudKey.isEmpty()||m_appBridge.page()!=0||m_libraryBridge.playlistId()!=m_restoredCloudKey)return;
    const auto cloud=m_libraryBridge.cloudPlaylist(m_restoredCloudKey);
    if(cloud.value("provider").toString()!=provider||cloud.value("id").toString()!=id)return;
    m_restoredCloudKey.clear();
    m_libraryBridge.openPlaylist("0");
    m_appBridge.navigate(0);
}
bool QmlShell::saveNavigation(){
    if(!m_rememberNavigation||m_navigationSaved)return m_navigationSaveOk;
    m_navigationSaved=true;
    m_navigationSaveOk=m_navigationLibrary&&m_navigationLibrary->setSetting("qml.lastNavigation",
        QJsonObject{{"page",m_appBridge.page()},{"playlistId",m_libraryBridge.playlistId()}});
    return m_navigationSaveOk;
}
bool QmlShell::shutdown() {const bool navigationSaved=saveNavigation();m_feedback.setClosing(true);m_searchBridge.cancelInstrumentalBatch();m_search.shutdown();m_translation.shutdown();m_import.shutdown();const bool playbackSaved=m_facade.shutdown(true);return navigationSaved&&playbackSaved;}
bool QmlShell::eventFilter(QObject *object,QEvent *event){
    if(event->type()==QEvent::Show||event->type()==QEvent::Hide){
        if(auto *view=qobject_cast<QQuickWindow *>(object);view&&view!=window()){
            auto *owner=view->transientParent();
            while(owner&&owner!=window())owner=owner->transientParent();
            if(owner&&owner==window()){
                // Queue, volume and tooltip windows must release their own
                // scene graphs too; releasing only the main window misses them.
                view->setPersistentGraphics(false);view->setPersistentSceneGraph(false);
                if(event->type()==QEvent::Hide)QTimer::singleShot(300,view,[view]{if(!view->isExposed())view->releaseResources();});
            }
        }
    }
    if(event->type()==QEvent::MouseButtonPress && object==window()) {
        const auto point=static_cast<QMouseEvent *>(event)->position();
        QMetaObject::invokeMethod(window(),"feedbackPointerPressed",Q_ARG(QVariant,point.x()),Q_ARG(QVariant,point.y()));
    }
    if(object==QCoreApplication::instance()&&event->type()==QEvent::Quit&&
       object->property("relaxerInteractiveExit").toBool()&&!m_facade.isShutdown()&&window()){
        QMetaObject::invokeMethod(window(),"requestQuit");event->ignore();return true;
    }
    return QObject::eventFilter(object,event);
}
}
