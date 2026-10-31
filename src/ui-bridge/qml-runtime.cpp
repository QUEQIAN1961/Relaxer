#include "qml-runtime-api.h"
#include "qml-shell.h"
#include "phase-c-version.h"
#include "ui-run-diagnostics.h"
#include <QTextStream>
#include <QMessageBox>
#include <QThread>
#include <QQuickItem>
#include <QEventLoop>

namespace {
int runQml(QCoreApplication *app, const relaxer::QmlLaunchOptions *options,
           relaxer::UiRunDiagnostics *diagnostics) {
    if (!app || !options || !diagnostics || QThread::currentThread() != app->thread()) return 64;
    // main owns the one root lock. Only this mutually exclusive path opens the
    // Library; no business QObject, vtable or ownership crosses the DLL ABI.
    diagnostics->mark("databaseOpenStartMs");
    relaxer::Library library(options->dataRoot);
    QString error;
    if (!library.open(&error)) {
        diagnostics->result["startupError"] = error;
        diagnostics->result["exitCode"] = 2;
        if (!options->diagnostics.isEmpty()) diagnostics->write(options->diagnostics);
        QTextStream(stderr) << error << '\n';
        if (options->diagnostics.isEmpty() && !options->quitAfter && options->screenshot.isEmpty())
            QMessageBox::critical(nullptr, "Relaxer", "无法打开音乐资料库：\n" + error);
        return 2;
    }
    diagnostics->mark("databaseReadyMs");
    diagnostics->result["mode"] = "qml";
    diagnostics->result["version"] = QString::fromLatin1(relaxer::phaseC::Version);
    diagnostics->result["dataRoot"] = library.root();
    diagnostics->result["qtVersion"] = qVersion();
    diagnostics->mark("uiConstructStartMs");
    relaxer::QmlShell shell(&library);
    diagnostics->mark("qmlLoadStartMs");
    app->setProperty("relaxerNonInteractiveExit",options->quitAfter>0||!options->screenshot.isEmpty());
    app->setProperty("relaxerInteractiveExit",options->quitAfter==0&&options->screenshot.isEmpty());
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
    if (!options->diagnostics.isEmpty()) diagnostics->interactionProbe = [&shell, window, diagnostics] {
        const int original = shell.appBridge()->page();
        const int target = original == 1 ? 0 : 1;
        std::function<QQuickItem *(QQuickItem *)> find = [&](QQuickItem *parent) -> QQuickItem * {
            if (parent->objectName() == QString("navigation%1").arg(target)) return parent;
            for (auto *child : parent->childItems()) if (auto *match = find(child)) return match;
            return nullptr;
        };
        auto *nav = find(window->contentItem());
        if (!nav || !nav->isVisible() || !nav->isEnabled() || !window->isVisible()) return false;
        // The real navigation handler snapshots/animates asynchronously. Confirm
        // its pageChanged response, rather than confusing animation with failure.
        QEventLoop response;
        QTimer timeout; timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &response, &QEventLoop::quit);
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &response, &QEventLoop::quit);
        QObject::connect(shell.appBridge(), &relaxer::AppBridge::pageChanged, &response, [&] {
            if (shell.appBridge()->page() == target) response.quit();
        });
        diagnostics->mark("interactionRequestedMs");
        if (!QMetaObject::invokeMethod(nav, "click", Qt::DirectConnection)) return false;
        timeout.start(2000);
        if (shell.appBridge()->page() != target) response.exec();
        const bool ok = shell.appBridge()->page() == target;
        shell.appBridge()->navigate(original);
        return ok;
    };
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
