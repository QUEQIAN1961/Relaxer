#include "qml-shell.h"
#include <QQmlContext>
#include <QQuickStyle>
#include <QEventLoop>

static void initializeQmlResources() { Q_INIT_RESOURCE(qml); }
namespace relaxer {
QmlShell::QmlShell(Library *library, QObject *parent, bool decodeOnly,ai::AiRuntime *runtime,
                   std::unique_ptr<TranslationBrowserBackend> browser)
    : QObject(parent), m_facade(library, this, decodeOnly), m_appBridge(library, this),
      m_playbackBridge(&m_facade, this, library), m_tracks(library, this), m_trackView(&m_tracks, this),
      m_playlists(library, this), m_libraryBridge(library, &m_facade, &m_tracks, &m_trackView, &m_playlists, this),
      m_queue(library, &m_facade, this),
      m_ownedAi(runtime?nullptr:std::make_unique<ai::AiRuntime>(library)),m_ai(runtime?runtime:m_ownedAi.get()),
      m_import(library,this),m_lyrics(library,&m_facade,this),m_settings(library,this),m_aiBridge(m_ai,this),
      m_translation(library,m_ai,this,std::move(browser)),m_translationBridge(library,&m_facade,&m_translation,&m_lyrics,this),
      m_importBridge(library,&m_import,this),m_search(library,this),m_searchBridge(library,&m_facade,&m_search,this) {
    initializeQmlResources();
    setObjectName("qmlShell");
    // A fixed official style makes custom control backgrounds portable on Windows.
    static const bool styleConfigured = [] { QQuickStyle::setStyle("Basic"); return true; }();
    Q_UNUSED(styleConfigured)
    QQmlEngine::setObjectOwnership(&m_appBridge, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&m_playbackBridge, QQmlEngine::CppOwnership);
    m_engine = std::make_unique<QQmlApplicationEngine>();
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
    connect(m_engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError> &errors) {
        for (const auto &error : errors) m_warnings.append(error.toString());
    });
    m_facade.restore();
}
QmlShell::~QmlShell() {
    m_engine.reset();shutdown();
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
    return window() != nullptr;
}
QQuickWindow *QmlShell::window() const {
    return m_engine && !m_engine->rootObjects().isEmpty()
        ? qobject_cast<QQuickWindow *>(m_engine->rootObjects().first()) : nullptr;
}
bool QmlShell::shutdown() {m_search.shutdown();m_translation.shutdown();m_import.shutdown();return m_facade.shutdown();}
}
