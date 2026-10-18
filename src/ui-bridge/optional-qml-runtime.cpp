#include "optional-qml-runtime.h"
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace relaxer {
OptionalQmlRuntime::OptionalQmlRuntime(const QString &file) : m_library(file) {
    m_library.setLoadHints(QLibrary::ResolveAllSymbolsHint | QLibrary::PreventUnloadHint);
}
bool OptionalQmlRuntime::load() {
    if (m_api) return true;
#ifdef Q_OS_WIN
    DWORD oldMode = 0;
    const bool scoped = SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &oldMode);
#endif
    const bool loaded = m_library.load();
#ifdef Q_OS_WIN
    if (scoped) SetThreadErrorMode(oldMode, nullptr);
#endif
    if (!loaded) {
        m_error = QStringLiteral("Optional QML module or its Qt6Qml/Qt6Quick/Qt6QuickControls2 dependencies are unavailable: ")
            + m_library.fileName() + QStringLiteral("\n") + m_library.errorString();
        return false;
    }
    const auto factory = reinterpret_cast<QmlRuntimeFactory>(m_library.resolve("relaxerQmlRuntimeApi"));
    if (!factory || !(m_api = factory()) || m_api->version != QmlRuntimeAbi || !m_api->run) {
        m_api = nullptr;
        m_error = QStringLiteral("Optional QML module has an incompatible or missing runtime interface: ") + m_library.fileName();
        return false;
    }
    return true;
}
int OptionalQmlRuntime::run(QCoreApplication *app, const QmlLaunchOptions &options, UiRunDiagnostics &diagnostics) {
    return m_api ? m_api->run(app, &options, &diagnostics) : 11;
}
}
