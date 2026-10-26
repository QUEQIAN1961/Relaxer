#include "qml-shell.h"
#include "ui-images.h"
#include <QQmlContext>
#include <QQuickStyle>
#include <QEventLoop>
#include <QCoreApplication>
#include <QEvent>
#include <QTimer>

static void initializeQmlResources() { Q_INIT_RESOURCE(qml); }
namespace relaxer {
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
      m_importBridge(library,&m_import,this),m_search(library,this,QUrl("https://lrclib.net/api"),{},searchTransport,std::move(searchSources)),m_searchBridge(library,&m_facade,&m_search,this) {
    initializeQmlResources();
    m_searchBridge.setTranslationCoordinator(&m_translation);
    setObjectName("qmlShell");
    // A fixed official style makes custom control backgrounds portable on Windows.
    static const bool styleConfigured = [] { QQuickStyle::setStyle("Basic"); return true; }();
    Q_UNUSED(styleConfigured)
    QQmlEngine::setObjectOwnership(&m_appBridge, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&m_playbackBridge, QQmlEngine::CppOwnership);
    m_engine = std::make_unique<QQmlApplicationEngine>();
    m_engine->addImageProvider("relaxer", new UiImages);
    m_engine->rootContext()->setContextProperty("appBridge", &m_appBridge);
    m_engine->rootContext()->setContextProperty("playbackBridge", &m_playbackBridge);
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
                    qMakePair("lyricsSearchBridge",static_cast<QObject *>(&m_searchBridge))}){
        QQmlEngine::setObjectOwnership(pair.second,QQmlEngine::CppOwnership);
        m_engine->rootContext()->setContextProperty(QString::fromLatin1(pair.first),pair.second);
    }
    connect(&m_queue, &QueueListModel::missingTracks, &m_libraryBridge, &LibraryBridge::forgetMissing, Qt::QueuedConnection);
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
    m_facade.restore();
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
bool QmlShell::load() {
    if (!m_engine->rootObjects().isEmpty()) return window() != nullptr;
    m_engine->load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (auto *loaded = window()) {
        m_uiIntegration.attach(loaded);
        if(!m_artworkRecoveryScheduled){m_artworkRecoveryScheduled=true;QTimer::singleShot(0,this,[this]{m_import.repairLegacyArtwork();});}
    }
    return window() != nullptr;
}
QQuickWindow *QmlShell::window() const {
    return m_engine && !m_engine->rootObjects().isEmpty()
        ? qobject_cast<QQuickWindow *>(m_engine->rootObjects().first()) : nullptr;
}
bool QmlShell::shutdown() {m_feedback.setClosing(true);m_searchBridge.cancelInstrumentalBatch();m_search.shutdown();m_translation.shutdown();m_import.shutdown();return m_facade.shutdown(true);}
bool QmlShell::eventFilter(QObject *object,QEvent *event){
    if(object==QCoreApplication::instance()&&event->type()==QEvent::Quit&&
       object->property("relaxerInteractiveExit").toBool()&&!m_facade.isShutdown()&&window()){
        QMetaObject::invokeMethod(window(),"requestQuit");event->ignore();return true;
    }
    return QObject::eventFilter(object,event);
}
}
