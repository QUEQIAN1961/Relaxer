#include "window.h"
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
    QApplication app(argc, argv);
    app.setApplicationName("Relaxer");
    app.setApplicationVersion("1.10.0");
    app.setOrganizationName("Relaxer");
    app.setStyle("Fusion");
    app.setWindowIcon(QIcon(":/icon.svg"));
    QCommandLineParser parser;
    parser.setApplicationDescription("Relaxer · 本地音乐播放器");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({{"data", "资料目录", "directory"},
                       {"screenshot", "保存窗口截图并退出", "file"},
                       {"page", "截图页面 local/playlists/profile/settings/playing", "name", "local"},
                       {"self-test", "检查导入、数据库和播放统计"},
                       {"media-smoke", "检验真实Qt音频解码，不依赖声卡", "file"},
                       {"media-smoke-worker", "内部音频解码验证子进程", "file"},
                       {"audio-device-smoke", "单独检验实际声卡播放与进度", "file"},
                       {"launch", "启动播放器"}});
    parser.process(app);
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
        if (parser.isSet("screenshot") || parser.isSet("self-test") || parser.isSet("audio-device-smoke"))
            return 3;
        QMessageBox::information(nullptr, "Relaxer",
                                 "此音乐资料库已在另一个 Relaxer 窗口中打开。请切换到已有窗口。");
        return 3;
    }
    Library lib(root);
    QString error;
    if (!lib.open(&error)) {
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
    Window window(&lib);
    window.show();
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
    return app.exec();
}
