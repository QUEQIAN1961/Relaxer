#pragma once
#include "qml-runtime-api.h"
#include <QLibrary>

namespace relaxer {
// No unload API: Quick registers process-global type caches. Code stays mapped
// until process termination; each run destroys all its objects before returning.
class OptionalQmlRuntime final {
  public:
    explicit OptionalQmlRuntime(const QString &file);
    bool load();
    QString error() const { return m_error; }
    int run(QCoreApplication *, const QmlLaunchOptions &, UiRunDiagnostics &);
  private:
    QLibrary m_library;
    const QmlRuntimeApi *m_api = nullptr;
    QString m_error;
};
}
