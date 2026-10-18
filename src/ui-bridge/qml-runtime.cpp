#include "qml-runtime-api.h"
#include "qml-shell.h"
#include "phase-c-version.h"
#include "ui-run-diagnostics.h"
#include <QTextStream>
#include <QThread>

namespace {
int runQml(QCoreApplication *app, const relaxer::QmlLaunchOptions *options,
           relaxer::UiRunDiagnostics *diagnostics) {
    if (!app || !options || !diagnostics || QThread::currentThread() != app->thread()) return 64;
    // main owns the one root lock. Only this mutually exclusive path opens the
    // Library; no business QObject, vtable or ownership crosses the DLL ABI.
    relaxer::Library library(options->dataRoot);
    QString error;
    if (!library.open(&error)) { QTextStream(stderr) << error << '\n'; return 2; }
    diagnostics->mark("databaseReadyMs");
    diagnostics->result["mode"] = "qml";
    diagnostics->result["version"] = QString::fromLatin1(relaxer::phaseC::Version);
    diagnostics->result["dataRoot"] = library.root();
    diagnostics->result["qtVersion"] = qVersion();
    relaxer::QmlShell shell(&library);
    QObject::connect(app, &QCoreApplication::aboutToQuit, &shell, &relaxer::QmlShell::shutdown);
    if (!shell.load()) {
        QTextStream(stderr) << "QML window creation failed: " << shell.warnings().join('\n') << '\n';
        return 10;
    }
    diagnostics->mark("uiLoadedMs");
    shell.appBridge()->navigate(options->page);
    auto *window = shell.window();
    diagnostics->result["windowObject"] = window->objectName();
    diagnostics->result["title"] = window->title();
    diagnostics->result["databaseConnections"] = QSqlDatabase::connectionNames().size();
    diagnostics->result["playbackInstances"] = shell.findChildren<relaxer::Playback *>().size();
    QObject::connect(window, &QQuickWindow::frameSwapped, &shell,
                     [diagnostics] { diagnostics->firstFrame(); }, Qt::QueuedConnection);
    if (!options->screenshot.isEmpty()) QTimer::singleShot(800, &shell, [app, window, options] {
        app->exit(window->grabWindow().save(options->screenshot) ? 0 : 7);
    });
    if (options->quitAfter) QTimer::singleShot(options->quitAfter, &shell, [diagnostics, window] {
        diagnostics->mark("closeRequestedMs"); window->close();
    });
    diagnostics->mark("windowShownMs");
    if (!options->diagnostics.isEmpty()) diagnostics->write(options->diagnostics);
    const int code = app->exec();
    diagnostics->mark("eventLoopReturnedMs");
    const bool saved = shell.shutdown();
    diagnostics->mark("shutdownCompleteMs");
    diagnostics->result["saveSucceeded"] = saved;
    diagnostics->result["exitCode"] = saved ? code : 8;
    if (!options->diagnostics.isEmpty() && !diagnostics->write(options->diagnostics)) return 9;
    // shell destroys engine, bridges and facade before library; callbacks have
    // shell contexts and are cancelled before this function returns to the host.
    return saved ? code : 8;
}
}
extern "C" Q_DECL_EXPORT const relaxer::QmlRuntimeApi *relaxerQmlRuntimeApi() {
    static const relaxer::QmlRuntimeApi api{relaxer::QmlRuntimeAbi, &runQml};
    return &api;
}
