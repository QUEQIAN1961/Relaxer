#include "qml-shell.h"
#include <QQmlContext>
#include <QQuickStyle>

static void initializeQmlResources() { Q_INIT_RESOURCE(qml); }
namespace relaxer {
QmlShell::QmlShell(Library *library, QObject *parent, bool decodeOnly)
    : QObject(parent), m_facade(library, this, decodeOnly), m_appBridge(library, this),
      m_playbackBridge(&m_facade, this), m_tracks(library, this), m_trackView(&m_tracks, this),
      m_playlists(library, this), m_libraryBridge(library, &m_facade, &m_tracks, &m_trackView, &m_playlists, this) {
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
    connect(m_engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError> &errors) {
        for (const auto &error : errors) m_warnings.append(error.toString());
    });
    m_facade.restore();
}
QmlShell::~QmlShell() { m_engine.reset(); shutdown(); }
bool QmlShell::load() {
    if (!m_engine->rootObjects().isEmpty()) return window() != nullptr;
    m_engine->load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    return window() != nullptr;
}
QQuickWindow *QmlShell::window() const {
    return m_engine && !m_engine->rootObjects().isEmpty()
        ? qobject_cast<QQuickWindow *>(m_engine->rootObjects().first()) : nullptr;
}
bool QmlShell::shutdown() { return m_facade.shutdown(); }
}
