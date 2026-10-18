#include "qml-shell.h"
#include <QQmlContext>
#include <QQuickStyle>

static void initializeQmlResources() { Q_INIT_RESOURCE(qml); }
namespace relaxer {
QmlShell::QmlShell(Library *library, QObject *parent, bool decodeOnly)
    : QObject(parent), m_facade(library, this, decodeOnly), m_appBridge(library, this),
      m_playbackBridge(&m_facade, this) {
    initializeQmlResources();
    setObjectName("qmlShell");
    // A fixed official style makes custom control backgrounds portable on Windows.
    QQuickStyle::setStyle("Basic");
    QQmlEngine::setObjectOwnership(&m_appBridge, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&m_playbackBridge, QQmlEngine::CppOwnership);
    m_engine = std::make_unique<QQmlApplicationEngine>();
    m_engine->rootContext()->setContextProperty("appBridge", &m_appBridge);
    m_engine->rootContext()->setContextProperty("playbackBridge", &m_playbackBridge);
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
