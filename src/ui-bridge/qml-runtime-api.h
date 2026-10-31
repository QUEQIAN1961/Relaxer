#pragma once
#include <QCoreApplication>
#include <QString>

namespace relaxer {
class UiRunDiagnostics;
struct QmlLaunchOptions {
    QString dataRoot, screenshot, diagnostics;
    int page = 0, quitAfter = 0;
};
// Same-toolchain private ABI. Only Qt Core values cross the module boundary;
// business QObjects and every Quick type remain wholly inside the selected UI.
struct QmlRuntimeApi {
    unsigned version;
    int (*run)(QCoreApplication *, const QmlLaunchOptions *, UiRunDiagnostics *);
};
// Startup diagnostics now carries an interaction callback; reject old modules
// before passing this private C++ object across the DLL boundary.
inline constexpr unsigned QmlRuntimeAbi = 2;
using QmlRuntimeFactory = const QmlRuntimeApi *(*)();
}
