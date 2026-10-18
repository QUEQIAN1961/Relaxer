#include "window.h"
#include "release-version.h"
#include "phase-c-version.h"
#include "ui-run-diagnostics.h"
#ifdef RELAXER_ENABLE_QML
#include "ui-bridge/qml-shell.h"
#endif
#include "media-smoke.h"
#include <QApplication>
#include <QAudioDevice>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QMediaDevices>
#include <QMessageBox>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
using namespace relaxer;
static void initializeAppResources() { Q_INIT_RESOURCE(resources); }
static QString dataRoot() {
    auto specified = qEnvironmentVariable("RELAXER_DATA");
    if (!specified.isEmpty())
        return specified;
    QDir dir(QCoreApplication::applicationDirPath());
    if (dir.dirName() == "runtime" || dir.dirName().startsWith("build"))
        dir.cdUp();
    return dir.filePath("data");
}
static bool selfTest(Library &lib) {
    QTemporaryDir fixtures;
    QString f = fixtures.filePath("check.wav");
    QProcess p;
    p.start(toolPath("ffmpeg"), {"-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:duration=1.2",
                                 "-metadata", "title=Relaxer runtime check", "-y", f});
    if (!p.waitForFinished(30000) || p.exitCode() != 0)
        return false;
    auto r = Importer::importOne(lib.root(), f);
    if (!r.error.isEmpty())
        return false;
    auto id = lib.insert(r.track);
    auto list = lib.createPlaylist("Runtime check");
    if (!id || !list || !lib.addToPlaylist(list, {id}))
        return false;
    auto s = lib.startSession(id, 1000);
    if (!lib.saveSession(s, 800, 80) || lib.track(id).plays != 1)
        return false;
    s.sought = true;
    if (!lib.saveSession(s, 0, 80) || lib.track(id).plays != 0)
        return false;
    return lib.deletePlaylist(list) && lib.remove({id});
}
int main(int argc, char **argv) {
    UiRunDiagnostics diagnostics;
    QApplication app(argc, argv);
    app.setApplicationName("Relaxer");
    app.setApplicationVersion(QString::fromLatin1(phaseC::Version));
    app.setProperty("relaxerDevelopmentTitle", QString::fromLatin1(phaseC::WindowTitle));
    app.setOrganizationName("Relaxer");
    app.setStyle("Fusion");
    initializeAppResources();
    app.setWindowIcon(QIcon(":/icon.svg"));
    QCommandLineParser parser;
    parser.setApplicationDescription("Relaxer · 本地音乐播放器");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({{"ui", "界面模式 widgets（默认）/qml（C1 开发预览）", "mode", "widgets"},
                       {"data", "资料目录", "directory"},
                       {"diagnostics", "写出 UI 启动/关闭测量 JSON（必须显式 --data）", "file"},
                       {"quit-after", "测量时自动正常关闭的毫秒数（需 --diagnostics 和 --data）", "milliseconds"},
                       {"screenshot", "保存窗口截图并退出", "file"},
                       {"page", "截图页面 local/playlists/profile/settings/playing", "name", "local"},
                       {"self-test", "检查导入、数据库和播放统计"},
                       {"media-smoke", "检验真实Qt音频解码，不依赖声卡", "file"},
                       {"media-smoke-worker", "内部音频解码验证子进程", "file"},
                       {"audio-device-smoke", "单独检验实际声卡播放与进度", "file"},
                       {"launch", "启动播放器"}});
    parser.process(app);
    const auto ui = parser.value("ui");
    auto usageError = [](const QString &message) {
        QTextStream(stderr) << "Relaxer: " << message << '\n';
        return 64;
    };
    if (ui != "widgets" && ui != "qml") return usageError("--ui must be widgets or qml");
#ifndef RELAXER_ENABLE_QML
    if (ui == "qml") return usageError("This build has no QML support; rebuild with RELAXER_ENABLE_QML=ON");
#endif
    if (parser.isSet("data") && parser.value("data").trimmed().isEmpty())
        return usageError("--data cannot be empty");
    if (parser.isSet("diagnostics") && (!parser.isSet("data") || parser.value("diagnostics").isEmpty()))
        return usageError("--diagnostics requires an output file and explicit --data");
    bool quitValid = false;
    const int quitAfter = parser.value("quit-after").toInt(&quitValid);
    if (parser.isSet("quit-after") && (!quitValid || quitAfter < 1 || quitAfter > 60000 ||
        !parser.isSet("diagnostics") || !parser.isSet("data") || parser.isSet("screenshot")))
        return usageError("--quit-after requires 1..60000 ms, --diagnostics and --data, without --screenshot");
    const QStringList qmlPages{"local", "playlists", "playing", "settings"};
    const QStringList widgetsPages{"local", "playlists", "profile", "settings", "playing"};
    if (!(ui == "qml" ? qmlPages : widgetsPages).contains(parser.value("page")))
        return usageError("Unsupported --page for the selected UI");
    if (ui == "qml" && (parser.isSet("self-test") || parser.isSet("media-smoke") ||
        parser.isSet("media-smoke-worker") || parser.isSet("audio-device-smoke")))
        return usageError("Audio/database diagnostics cannot be combined with --ui=qml");
    // These isolated diagnostics must not open the user's data or construct a player window.
    if (parser.isSet("media-smoke-worker")) mediaSmokeWorker(parser.value("media-smoke-worker"));
    if (parser.isSet("media-smoke")) {
        const auto result = runMediaSmokeProcess(QCoreApplication::applicationFilePath(),
            {"--media-smoke-worker", QFileInfo(parser.value("media-smoke")).absoluteFilePath()});
        QTextStream(stdout) << QString::fromUtf8(result.output) << Qt::flush;
        QTextStream(stderr) << QString::fromUtf8(result.error) << Qt::flush;
        if (mediaSmokeSucceeded(result)) return 0;
        if (result.exitCode == 0) QTextStream(stderr) << "Audio worker returned no valid PCM verification proof.\n";
        return result.timedOut ? 6 : (result.exitCode ? result.exitCode : 5);
    }
    QString root = parser.isSet("data") ? parser.value("data") : dataRoot();
    QDir().mkpath(root);
    QLockFile lock(QDir(root).filePath("relaxer.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        if (parser.isSet("screenshot") || parser.isSet("self-test") || parser.isSet("audio-device-smoke") || parser.isSet("diagnostics"))
            return 3;
        QMessageBox::information(nullptr, "Relaxer",
                                 "此音乐资料库已在另一个 Relaxer 窗口中打开。请切换到已有窗口。");
        return 3;
    }
    Library lib(root);
    QString error;
    if (!lib.open(&error)) {
        if (parser.isSet("diagnostics")) {
            QTextStream(stderr) << error << '\n';
            return 2;
        }
        QMessageBox::critical(nullptr, "Relaxer", "无法打开音乐资料库：\n" + error);
        return 2;
    }
    if (parser.isSet("self-test")) {
        bool ok = selfTest(lib);
        QTextStream(stdout) << (ok ? "Relaxer import/database/stats OK\n" : "Relaxer self-test FAILED\n");
        return ok ? 0 : 4;
    }
    if (parser.isSet("audio-device-smoke")) {
        if (QMediaDevices::defaultAudioOutput().isNull()) {
            QTextStream(stderr) << "No audio output is available for playback verification.\n";
            return 6;
        }
        QMediaPlayer player;
        QAudioOutput output;
        output.setVolume(0);
        player.setAudioOutput(&output);
        bool progressed = false;
        QObject::connect(&player, &QMediaPlayer::errorOccurred, &app, [&](auto, const QString &text) {
            QTextStream(stderr) << text << '\n';
            app.exit(5);
        });
        QObject::connect(&player, &QMediaPlayer::positionChanged, &app, [&](qint64 ms) {
            if (ms >= 500 && player.duration() > 0 && player.hasAudio()) {
                progressed = true;
                QTextStream(stdout) << "Audio decode and playback position OK\n";
                app.quit();
            }
        });
        QTimer::singleShot(15000, &app, [&] { app.exit(6); });
        player.setSource(QUrl::fromLocalFile(QFileInfo(parser.value("audio-device-smoke")).absoluteFilePath()));
        player.play();
        int code = app.exec();
        return progressed ? 0 : (code ? code : 6);
    }
    diagnostics.mark("databaseReadyMs");
    diagnostics.result["mode"] = ui;
    diagnostics.result["version"] = QString::fromLatin1(phaseC::Version);
    diagnostics.result["dataRoot"] = lib.root();
    diagnostics.result["qtVersion"] = qVersion();
    if (parser.isSet("diagnostics")) diagnostics.write(parser.value("diagnostics"));
    auto finish = [&](int code, bool saved) {
        diagnostics.mark("shutdownCompleteMs");
        diagnostics.result["saveSucceeded"] = saved;
        diagnostics.result["exitCode"] = saved ? code : 8;
        if (parser.isSet("diagnostics") && !diagnostics.write(parser.value("diagnostics"))) return 9;
        return saved ? code : 8;
    };
#ifdef RELAXER_ENABLE_QML
    if (ui == "qml") {
        QmlShell shell(&lib);
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &shell, &QmlShell::shutdown);
        if (!shell.load()) {
            QTextStream(stderr) << "QML window creation failed: " << shell.warnings().join('\n') << '\n';
            return 10;
        }
        diagnostics.mark("uiLoadedMs");
        const int page = qmlPages.indexOf(parser.value("page"));
        shell.appBridge()->navigate(page);
        auto *window = shell.window();
        diagnostics.result["windowObject"] = window->objectName();
        diagnostics.result["title"] = window->title();
        diagnostics.result["databaseConnections"] = QSqlDatabase::connectionNames().size();
        diagnostics.result["playbackInstances"] = shell.findChildren<Playback *>().size();
        QObject::connect(window, &QQuickWindow::frameSwapped, &app, [&] { diagnostics.firstFrame(); }, Qt::QueuedConnection);
        if (parser.isSet("screenshot")) QTimer::singleShot(800, &app, [&] {
            app.exit(window->grabWindow().save(parser.value("screenshot")) ? 0 : 7);
        });
        if (parser.isSet("quit-after")) QTimer::singleShot(quitAfter, &app, [&, window] {
            diagnostics.mark("closeRequestedMs"); window->close();
        });
        diagnostics.mark("windowShownMs");
        if (parser.isSet("diagnostics")) diagnostics.write(parser.value("diagnostics"));
        const int code = app.exec();
        diagnostics.mark("eventLoopReturnedMs");
        return finish(code, shell.shutdown());
    }
#endif
    Window window(&lib);
    window.installEventFilter(&diagnostics);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &window, [&] { window.playbackFacade()->shutdown(); });
    window.show();
    diagnostics.mark("uiLoadedMs");
    diagnostics.mark("windowShownMs");
    diagnostics.result["windowObject"] = "widgetsWindow";
    diagnostics.result["title"] = window.windowTitle();
    diagnostics.result["databaseConnections"] = QSqlDatabase::connectionNames().size();
    diagnostics.result["playbackInstances"] = window.findChildren<Playback *>().size();
    if (parser.isSet("diagnostics")) diagnostics.write(parser.value("diagnostics"));
    if (parser.isSet("quit-after")) QTimer::singleShot(quitAfter, &app, [&] {
        diagnostics.mark("closeRequestedMs");
        // Explicit measurements must close even if the fixture requests close-to-tray.
        app.quit();
    });
    if (parser.isSet("screenshot")) {
        auto page = parser.value("page");
        if (page == "playlists")
            window.showPlaylists();
        else if (page == "profile")
            window.showProfile();
        else if (page == "settings")
            window.showSettings();
        else if (page == "playing")
            window.showNowPlaying();
        QTimer::singleShot(800, &app, [&] {
            if (!window.grab().save(parser.value("screenshot")))
                app.exit(7);
            else
                app.quit();
        });
    }
    const int code = app.exec();
    diagnostics.mark("eventLoopReturnedMs");
    return finish(code, window.playbackFacade()->shutdown());
}
