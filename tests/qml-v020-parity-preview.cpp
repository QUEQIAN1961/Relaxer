// Explicit native visual probe. Every database, song and image belongs to the
// new --output directory; no user library, credential or paid endpoint is used.
#include "window.h"
#include "ui-bridge/qml-shell.h"
#include "ai/ai-settings-page.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QScreen>
#include <QSGRendererInterface>
#include <QSqlDatabase>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QtTest>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <stdexcept>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifndef RELAXER_BUILD_SOURCE_SHA256
#define RELAXER_BUILD_SOURCE_SHA256 "unavailable"
#endif

using namespace relaxer;
namespace {
QMutex messageMutex;
QJsonArray messages;
QString phase = "startup";
QElapsedTimer lifetime;
void setPhase(const QString &value) { QMutexLocker lock(&messageMutex); phase = value; }
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    const QString level = type == QtWarningMsg ? "warning" : type == QtCriticalMsg ? "critical" :
        type == QtFatalMsg ? "fatal" : type == QtInfoMsg ? "info" : "debug";
    {
        QMutexLocker lock(&messageMutex);
        messages.append(QJsonObject{{"phase", phase}, {"elapsedMs", double(lifetime.elapsed())}, {"level", level},
            {"category", QString::fromUtf8(context.category ? context.category : "")}, {"message", message},
            {"source", QString::fromUtf8(context.file ? context.file : "")}, {"line", context.line}});
    }
    const auto bytes = (level + ": " + message + '\n').toUtf8();
    std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stderr); std::fflush(stderr);
    if (type == QtFatalMsg) std::abort();
}
bool saveJson(const QString &path, const QJsonObject &object) {
    QSaveFile file(path); const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
QString sha256(const QString &path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash digest(QCryptographicHash::Sha256);
    return digest.addData(&file) ? QString::fromLatin1(digest.result().toHex()) : QString();
}
void require(bool condition, const QString &detail) { if (!condition) throw std::runtime_error(detail.toUtf8().constData()); }
QJsonArray idsJson(const QVector<qint64> &ids) {
    QJsonArray result; for (auto id : ids) result.append(double(id)); return result;
}
QQuickItem *visual(QQuickItem *item, const QString &name) {
    if (!item) return nullptr;
    if (item->objectName() == name) return item;
    for (auto *child : item->childItems()) if (auto *found = visual(child, name)) return found;
    return nullptr;
}
QJsonObject rectangle(const QRectF &rect, const QSize &bounds, bool visible, double opacity = 1) {
    return {{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()},
        {"visible", visible}, {"opacity", opacity}, {"insideWindow", rect.left() >= -1 && rect.top() >= -1 &&
            rect.right() <= bounds.width() + 1 && rect.bottom() <= bounds.height() + 1}};
}
QJsonObject widgetLayout(Window &window) {
    QJsonObject result;
    for (const auto *name : {"sidebar", "contentHost", "bottomStack", "playerBar", "playerMetadata", "playerCenter",
         "bottomCover", "bottomSongTitle", "nowCover", "nowTitle", "lyricView", "nextQueueCompact", "nextQueueLarge", "lyricToolsQuestion"}) {
        if (auto *item = window.findChild<QWidget *>(name))
            result[name] = rectangle(QRectF(item->mapTo(&window, QPoint()), item->size()), window.size(), item->isVisible());
    }
    return result;
}
QJsonObject qmlLayout(QQuickWindow &window) {
    QJsonObject result;
    for (const auto *name : {"sidebar", "contentHost", "playerBar", "playerMetadata", "playerCenter", "bottomCover",
         "currentTitle", "currentArtist", "currentArtwork", "lyricsPanel", "nextQueuePanel", "lyricToolsQuestion", "sharedArtworkFlight"}) {
        if (auto *item = visual(window.contentItem(), name))
            result[name] = rectangle(item->mapRectToScene(QRectF(0, 0, item->width(), item->height())),
                window.size(), item->isVisible(), item->opacity());
    }
    result["lyricsOpen"] = window.property("lyricsOpen").toBool();
    result["playerProgress"] = window.property("playerProgress").toDouble();
    result["coverTravelling"] = window.property("coverTravelling").toBool();
    return result;
}
class CountingCredentials final : public ai::CredentialStore {
  public:
    int reads = 0, writes = 0, removes = 0;
    bool store(const QString &, const QString &, QString *error) override { ++writes; if (error) error->clear(); return true; }
    ai::CredentialReadResult read(const QString &) override { ++reads; ai::CredentialReadResult value; value.ok = true; return value; }
    bool remove(const QString &, QString *error) override { ++removes; if (error) error->clear(); return true; }
    bool supported() const override { return true; }
    bool allowsMockRequests() const override { return true; }
};
class LoopbackOnly final : public QObject {
  public:
    QTcpServer server;
    int connections = 0;
    LoopbackOnly() {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (server.hasPendingConnections()) {
                auto *socket = server.nextPendingConnection(); ++connections;
                connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                    socket->readAll();
                    socket->write("HTTP/1.1 503 Probe forbids network actions\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}");
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        require(server.listen(QHostAddress::LocalHost), "Cannot open loopback-only guard");
    }
    QUrl endpoint() const { return QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort())); }
};
struct Fixture {
    QVector<qint64> ids;
    qint64 playlistA = 0, playlistB = 0;
    QString signature;
};
bool wave(const QString &path) {
    QFile file(path); if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian);
    constexpr int samples = 8000 * 20;
    stream.writeRawData("RIFF", 4); stream << quint32(samples * 2 + 36); stream.writeRawData("WAVEfmt ", 8);
    stream << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
    stream.writeRawData("data", 4); stream << quint32(samples * 2);
    for (int i = 0; i < samples; ++i) stream << qint16(700 * (std::sin(i * 6.283185307179586 * 220 / 8000) + std::sin(i * 6.283185307179586 * 330 / 8000)));
    return stream.status() == QDataStream::Ok;
}
Fixture seed(Library &library) {
    require(wave(library.absolute("Music/preview-synthetic.wav")), "Cannot create synthetic PCM");
    Fixture fixture;
    const QStringList titles{"把时间留给音乐", "First Light", "海辺の記憶", "午后慢调", "城市的另一面", "Blue Hour", "風を待つ", "最后一班电车"};
    const QStringList artists{"Relaxer Ensemble", "Mira & The Waves", "星野 そら", "林间来信", "夜航电台", "The Quiet Hours", "青い森", "南方车站"};
    const QStringList albums{"慢慢听", "Scenes From A Window", "風のある場所"};
    QJsonArray catalog;
    for (int i = 0; i < titles.size(); ++i) {
        const auto relativeCover = QString("Artwork/preview-%1.png").arg(i + 1);
        QImage image(512, 512, QImage::Format_RGB32); QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
        QLinearGradient gradient(0, 0, 512, 512); gradient.setColorAt(0, QColor::fromHsl((i * 43 + 8) % 360, 150, 154));
        gradient.setColorAt(1, QColor::fromHsl((i * 43 + 58) % 360, 145, 55)); painter.fillRect(image.rect(), gradient);
        painter.setPen(Qt::NoPen); painter.setBrush(QColor(255, 255, 255, 30)); painter.drawEllipse(QRectF(120, -90, 460, 460));
        painter.setBrush(QColor(20, 20, 40, 35)); painter.drawEllipse(QRectF(-100, 260, 440, 440));
        QFont coverFont("Segoe UI"); coverFont.setPixelSize(140); coverFont.setWeight(QFont::Light); painter.setFont(coverFont);
        painter.setPen(QColor(255, 255, 255, 220)); painter.drawText(image.rect(), Qt::AlignCenter, QString(QChar('A' + i)));
        coverFont.setPixelSize(17); coverFont.setLetterSpacing(QFont::AbsoluteSpacing, 4); painter.setFont(coverFont);
        painter.drawText(QRect(30, 440, 452, 40), Qt::AlignCenter, "RELAXER / SYNTHETIC"); painter.end();
        require(image.save(library.absolute(relativeCover)), "Cannot create cover fixture");
        Track track; track.hash = QString("v020-parity-%1").arg(i); track.title = titles[i]; track.artist = artists[i];
        track.album = albums[i % albums.size()]; track.file = "Music/preview-synthetic.wav"; track.cover = relativeCover;
        track.coverOrigin = "manual"; track.duration = 20000; track.importedAt = "2026-10-19T00:00:00Z";
        track.id = library.insert(track); require(track.id > 0, "Cannot insert fixture track"); fixture.ids.append(track.id);
        QByteArray lyrics;
        for (int line = 0; line < 80; ++line) {
            const int time = line * 200;
            lyrics += QString("[%1:%2.%3]We keep a little light — verse %4\n[%1:%2.%3]让音乐留一点空白 · 日本語の記憶 %4\n")
                .arg(time / 60000, 2, 10, QChar('0')).arg(time / 1000 % 60, 2, 10, QChar('0')).arg(time % 1000 / 10, 2, 10, QChar('0')).arg(line + 1).toUtf8();
        }
        require(library.saveLyrics(track.id, lyrics), "Cannot save fixture lyrics");
        if (i % 3 == 0) require(library.favorite(track.id, true), "Cannot set fixture favorite");
        for (int play = 0; play < 1 + i % 3; ++play) {
            auto session = library.startSession(track.id, 20000); require(session.id > 0, "Cannot create fixture statistics");
            require(library.saveSession(session, 18000, 80, true), "Cannot save fixture statistics");
        }
        const auto current = library.track(track.id);
        catalog.append(QJsonObject{{"id", QString::number(track.id)}, {"title", track.title}, {"artist", track.artist}, {"album", track.album},
            {"coverSha256", sha256(library.absolute(relativeCover))}, {"lyricsSha256", sha256(library.absolute(current.lyrics))},
            {"favorite", current.favorite}, {"plays", double(current.plays)}, {"heard", double(current.heard)}});
    }
    fixture.playlistA = library.createPlaylist("给午后的风"); fixture.playlistB = library.createPlaylist("夜色与星光");
    require(fixture.playlistA && fixture.playlistB, "Cannot create fixture playlists");
    require(library.addToPlaylist(fixture.playlistA, {fixture.ids[0], fixture.ids[2], fixture.ids[4], fixture.ids[6]}), "Cannot fill first playlist");
    require(library.addToPlaylist(fixture.playlistB, {fixture.ids[1], fixture.ids[3], fixture.ids[5], fixture.ids[7]}), "Cannot fill second playlist");
    require(library.playlistCover(fixture.playlistA, library.absolute("Artwork/preview-1.png")), "Cannot set playlist artwork");
    require(library.playlistCover(fixture.playlistB, library.absolute("Artwork/preview-6.png")), "Cannot set playlist artwork");
    require(library.playlistDescription(fixture.playlistA, "把喜欢的歌，留给一个慢下来的午后。"), "Cannot save playlist description");
    require(library.setSettings({{"nickname", "Relaxer Listener"}, {"profileDescription", "收藏、回忆，还有反复听过的那些歌。"},
        {"queue", idsJson(fixture.ids)}, {"current", double(fixture.ids.first())}, {"position", 3400}, {"mode", int(Mode::ListLoop)},
        {"volume", 0}, {"theme", 0}, {"closeToTray", false}, {"playerCoverBackground", true}, {"showLyricTranslation", true},
        {"windowWidth", 1240}, {"windowHeight", 790}, {"qml.reduceMotion", false}}), "Cannot save fixture settings");
    fixture.signature = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(QJsonObject{{"catalog", catalog},
        {"pcm", sha256(library.absolute("Music/preview-synthetic.wav"))}, {"playlistA", idsJson({fixture.ids[0], fixture.ids[2], fixture.ids[4], fixture.ids[6]})},
        {"playlistB", idsJson({fixture.ids[1], fixture.ids[3], fixture.ids[5], fixture.ids[7]})}}).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    return fixture;
}
void configureFake(ai::AiRuntime &runtime) {
    ai::ModelInfo model; model.providerId = "deepseek"; model.id = "deepseek-flash"; model.displayName = "DeepSeek Flash";
    model.availability = ai::ModelAvailability::Available; model.capabilities.inputModalities = QStringList{"text"};
    model.capabilities.outputModalities = QStringList{"text"}; model.capabilities.reasoningMetadataKnown = true;
    model.capabilities.reasoningEfforts = QStringList{"low", "high", "max"}; model.capabilities.contextWindow = 32768;
    model.capabilities.maxOutputTokens = 8192; model.protocol = {true, true, true, true};
    require(runtime.acceptModelCatalog("deepseek", {model}), "Cannot seed fake model catalog");
    auto configuration = runtime.configuration(); configuration.defaults = {"deepseek", "deepseek-flash", false, {}};
    configuration.targetLanguage = "zh-CN"; configuration.translationUsesDefaults = true;
    require(runtime.saveConfiguration(configuration), "Cannot seed fake AI configuration");
}
struct Configuration { QString name; QSize size; int theme; };
const QList<Configuration> configurations{{"normal-light", {1240, 790}, 0}, {"compact-light", {1000, 660}, 0}, {"normal-dark", {1240, 790}, 1}};
struct Recorder {
    QString output;
    QJsonArray captures, checks;
    int failures = 0;
    void check(const QString &name, bool ok, const QString &detail = {}) {
        checks.append(QJsonObject{{"name", name}, {"passed", ok}, {"detail", detail}}); if (!ok) ++failures;
    }
    QString directory(const QString &frontend, const Configuration &configuration) {
        const auto path = QDir(output).filePath(frontend + '/' + configuration.name); require(QDir().mkpath(path), "Cannot create screenshot directory"); return path;
    }
    void record(const QString &frontend, const Configuration &configuration, const QString &scene, const QImage &image,
                const QJsonObject &layout, int facadeCount, int backendCount, int requestedMs = -1, double actualMs = -1) {
        const auto path = QDir(directory(frontend, configuration)).filePath(scene + ".png");
        const auto connections = QSqlDatabase::connectionNames();
        check(frontend + '/' + configuration.name + '/' + scene + "/one-core-connection", connections.size() == 1 && facadeCount == 1 && backendCount == 1,
            QString("database=%1 facade=%2 backend=%3").arg(connections.size()).arg(facadeCount).arg(backendCount));
        const bool saved = !image.isNull() && image.save(path); check(frontend + '/' + scene + "/png", saved);
        captures.append(QJsonObject{{"frontend", frontend}, {"configuration", configuration.name}, {"scene", scene},
            {"path", QDir(output).relativeFilePath(path)}, {"sha256", saved ? sha256(path) : QString()},
            {"pixelWidth", image.width()}, {"pixelHeight", image.height()}, {"dpr", image.devicePixelRatio()},
            {"requestedMs", requestedMs}, {"actualCaptureMs", actualMs}, {"databaseConnections", connections.size()},
            {"facades", facadeCount}, {"playbackBackends", backendCount}, {"layout", layout}});
    }
};
template <typename Capture> void timeline(const std::function<void()> &action, const QList<int> &times, const QString &name, Capture capture) {
    QElapsedTimer elapsed; elapsed.start(); action();
    for (const int target : times) {
        while (elapsed.elapsed() < target) QTest::qWait(qMax(1, qMin(5, target - int(elapsed.elapsed()))));
        capture(QString("%1-%2ms").arg(name).arg(target, 3, 10, QChar('0')), target, elapsed.nsecsElapsed() / 1000000.);
    }
    QTest::qWait(150);
}
bool qmlCall(QObject *object, const char *method) { return QMetaObject::invokeMethod(object, method, Qt::DirectConnection); }
bool qmlCall(QObject *object, const char *method, const QVariant &argument) {
    return QMetaObject::invokeMethod(object, method, Qt::DirectConnection, Q_ARG(QVariant, argument));
}
void widgets(Recorder &recorder, QString &signature, QJsonObject &safety) {
    setPhase("widgets-fixture");
    Library library(QDir(recorder.output).filePath("synthetic-widgets")); require(library.open(), library.lastError());
    const auto fixture = seed(library); signature = fixture.signature;
    LoopbackOnly loopback; auto credentials = std::make_unique<CountingCredentials>(); auto *counts = credentials.get();
    ai::AiRuntime runtime(&library, ai::DeepSeekProvider::MockEndpoint{loopback.endpoint()}, std::move(credentials)); configureFake(runtime);
    {
        Window window(&library); window.playbackFacade()->setVolume(0); window.show();
        require(QTest::qWaitForWindowExposed(&window, 5000), "Widgets window was not exposed"); QTest::qWait(300);
        auto *pages = window.findChild<QStackedWidget *>("pages"); require(pages, "Missing original Widgets page stack");
        auto *aiPage = new ai::AiSettingsPage(&runtime, pages); pages->addWidget(aiPage);
        for (const auto &configuration : configurations) {
            setPhase("widgets/" + configuration.name);
            window.resize(configuration.size);
            auto *theme = window.findChild<QComboBox *>("themeSetting"); require(theme, "Missing original theme selector"); theme->setCurrentIndex(configuration.theme);
            window.showLibrary(); QTest::qWait(350);
            const auto capture = [&](const QString &scene, int target = -1, double elapsed = -1, QWidget *surface = nullptr) {
                recorder.record("widgets", configuration, scene, (surface ? surface : &window)->grab().toImage(), widgetLayout(window),
                    window.findChildren<PlaybackFacade *>().size(), window.findChildren<Playback *>().size(), target, elapsed);
            };
            capture("01-local");
            timeline([&] { window.showPlaylists(); }, {0, 90, 220}, "02-page-switch", capture);
            capture("03-playlist-grid"); window.showProfile(); QTest::qWait(300); capture("04-profile");
            window.showSettings(); QTest::qWait(300); capture("05-settings");
            window.showLibrary(); QTest::qWait(300);
            timeline([&] { window.showNowPlaying(); }, {0, 100, 220, 420}, "06-expand", capture); capture("07-expanded");
            auto *queue = window.findChild<NextQueuePanel *>("nextQueueLarge"); require(queue, "Missing original next panel");
            queue->setExpanded(true); QTest::qWait(350); capture("08-next-expanded"); queue->setExpanded(false); QTest::qWait(320);
            auto *lyrics = window.findChild<LyricView *>(); auto *question = window.findChild<QPushButton *>("lyricToolsQuestion");
            require(lyrics && question, "Missing original lyric tool"); QTest::mouseMove(lyrics, lyrics->rect().center()); QTest::qWait(170);
            QTest::mouseClick(question, Qt::LeftButton); QTest::qWait(100); capture("09-lyric-tools");
            if (auto *delay = window.findChild<QPushButton *>("lyricDelayEntry")) { QTest::mouseClick(delay, Qt::LeftButton); QTest::qWait(80); capture("10-lyric-delay"); }
            timeline([&] { window.showLibrary(); }, {0, 100, 220, 420}, "11-collapse", capture); capture("12-collapsed");
            {
                LyricsLookup dialog(&library, fixture.ids, &window, loopback.endpoint().resolved(QUrl("/api")), {}, true, &runtime);
                dialog.resize(configuration.size - QSize(60, 60)); dialog.show(); QTest::qWait(220); capture("13-lyrics-workspace", -1, -1, &dialog); dialog.close();
            }
            window.showSettings(); QTest::qWait(280); pages->setCurrentWidget(aiPage); QTest::qWait(180); capture("14-ai-settings");
            recorder.check("widgets/" + configuration.name + "/unchanged-queue", window.playbackFacade()->queue().items == fixture.ids);
        }
        require(window.playbackFacade()->shutdown(), "Widgets shutdown failed"); window.close(); QTest::qWait(50);
    }
    safety = {{"fakeCredentialReads", counts->reads}, {"fakeCredentialWrites", counts->writes}, {"fakeCredentialRemoves", counts->removes},
        {"loopbackConnections", loopback.connections}, {"realPaidRequests", 0},
        {"aiPageInjection", "Original AiSettingsPage uses the explicitly injected fake runtime inside the original page stack. The Window-owned production runtime is never used for an AI action."}};
    recorder.check("widgets/no-network-action", loopback.connections == 0);
}
void qml(Recorder &recorder, QString &signature, QJsonObject &safety) {
    setPhase("qml-fixture");
    Library library(QDir(recorder.output).filePath("synthetic-qml")); require(library.open(), library.lastError());
    const auto fixture = seed(library); signature = fixture.signature;
    LoopbackOnly loopback; auto credentials = std::make_unique<CountingCredentials>(); auto *counts = credentials.get();
    ai::AiRuntime runtime(&library, ai::DeepSeekProvider::MockEndpoint{loopback.endpoint()}, std::move(credentials)); configureFake(runtime);
    {
        QmlShell shell(&library, nullptr, true, &runtime); require(shell.load(), "QML shell did not load; inspect messages and shellWarnings");
        auto *window = shell.window(); require(window, "Missing QML window");
        require(QTest::qWaitForWindowExposed(window, 5000), "QML window was not exposed"); QTest::qWait(350);
        for (const auto &configuration : configurations) {
            setPhase("qml/" + configuration.name); window->resize(configuration.size); require(shell.settingsBridge()->setTheme(configuration.theme), "Cannot switch QML theme");
            require(qmlCall(window, "navigateOriginal", 0), "Missing navigateOriginal"); QTest::qWait(350);
            const auto capture = [&](const QString &scene, int target = -1, double elapsed = -1) {
                recorder.record("qml", configuration, scene, window->grabWindow(), qmlLayout(*window),
                    shell.findChildren<PlaybackFacade *>().size(), shell.findChildren<Playback *>().size(), target, elapsed);
            };
            capture("01-local");
            timeline([&] { require(qmlCall(window, "navigateOriginal", 1), "Cannot navigate playlists"); }, {0, 90, 220}, "02-page-switch", capture);
            capture("03-playlist-grid"); require(qmlCall(window, "navigateOriginal", 2), "Cannot navigate profile"); QTest::qWait(300); capture("04-profile");
            require(qmlCall(window, "navigateOriginal", 3), "Cannot navigate settings"); QTest::qWait(300); capture("05-settings");
            require(qmlCall(window, "navigateOriginal", 0), "Cannot return to library"); QTest::qWait(300);
            timeline([&] { require(qmlCall(window, "showLyrics"), "Cannot expand QML player"); }, {0, 100, 220, 420}, "06-expand", capture); capture("07-expanded");
            auto *queue = visual(window->contentItem(), "nextQueuePanel"); require(queue, "Missing QML next panel");
            queue->setProperty("panelExpanded", true); QTest::qWait(350); capture("08-next-expanded"); queue->setProperty("panelExpanded", false); QTest::qWait(320);
            auto *question = visual(window->contentItem(), "lyricToolsQuestion"); require(question, "Missing QML lyric tool button");
            const auto point = question->mapToScene(QPointF(question->width() / 2, question->height() / 2)).toPoint();
            QTest::mouseMove(window, point - QPoint(30, 80)); QTest::qWait(170); QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point); QTest::qWait(100); capture("09-lyric-tools");
            if (auto *delay = visual(window->contentItem(), "lyricDelayEntry")) {
                const auto position = delay->mapToScene(QPointF(delay->width() / 2, delay->height() / 2)).toPoint();
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, position); QTest::qWait(80); capture("10-lyric-delay");
            }
            if (auto *tools = window->findChild<QObject *>("lyricToolsMenu")) qmlCall(tools, "close");
            timeline([&] { require(qmlCall(window, "closeLyrics"), "Cannot collapse QML player"); }, {0, 100, 220, 420}, "11-collapse", capture); capture("12-collapsed");
            QStringList keys; for (auto id : fixture.ids) keys.append(QString::number(id));
            require(QMetaObject::invokeMethod(window, "openLyricsTools", Qt::DirectConnection,
                Q_ARG(QVariant, QVariant(keys)), Q_ARG(QVariant, QVariant(false))), "Cannot open QML lyrics workspace");
            QTest::qWait(220); capture("13-lyrics-workspace");
            if (auto *workspace = window->findChild<QObject *>("lyricsWorkspace")) qmlCall(workspace, "requestClose");
            QTest::qWait(100); require(qmlCall(window, "navigateOriginal", 3), "Cannot return to settings"); QTest::qWait(280);
            require(window->setProperty("settingsSection", "ai"), "Missing QML AI section"); QTest::qWait(180); capture("14-ai-settings");
            recorder.check("qml/" + configuration.name + "/unchanged-queue", shell.facade()->queue().items == fixture.ids);
        }
        safety["shellWarnings"] = QJsonArray::fromStringList(shell.warnings());
        require(shell.shutdown(), "QML shutdown failed"); window->setProperty("allowClose", true); window->close(); QTest::qWait(50);
    }
    safety["fakeCredentialReads"] = counts->reads; safety["fakeCredentialWrites"] = counts->writes; safety["fakeCredentialRemoves"] = counts->removes;
    safety["loopbackConnections"] = loopback.connections; safety["realPaidRequests"] = 0;
    recorder.check("qml/no-network-action", loopback.connections == 0);
}
}
int main(int argc, char **argv) {
#ifdef Q_OS_WIN
    SetErrorMode(0x8003);
#endif
    lifetime.start(); qInstallMessageHandler(messageHandler);
    qputenv("QT_QUICK_BACKEND", "software"); qputenv("QML_DISABLE_DISK_CACHE", "1"); qputenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS", "1");
    QApplication app(argc, argv); app.setApplicationName("Relaxer Synthetic UI Preview"); app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false);
    QCommandLineParser parser; parser.addHelpOption(); parser.addOption({"output", "New owned output directory (must not exist)", "directory"}); parser.process(app);
    const auto requested = parser.value("output");
    if (requested.isEmpty() || QFileInfo::exists(requested)) { QTextStream(stderr) << "--output must name a new directory\n"; return 64; }
    Recorder recorder{QFileInfo(requested).absoluteFilePath()};
    if (!QDir().mkpath(recorder.output)) return 65;
    QJsonObject result{{"probe", "native-v020-widgets-and-qml-visual-parity"}, {"qtVersion", QT_VERSION_STR},
        {"sourceFingerprint", RELAXER_BUILD_SOURCE_SHA256}, {"qpa", QGuiApplication::platformName()}, {"quickBackend", "software"},
        {"referenceCommit", "43203314012f509a2f83e4558b9206001b2e5f75"},
        {"referenceNote", "Widgets renders the existing original Window, PageMotion, NextQueuePanel and lyric tools. Compare the recorded source hashes to the stable reference; this probe does not replace independent visual acceptance."},
        {"captureNote", "Actual QWidget grab / QQuickWindow grabWindow frames. Requested animation deadlines and actual capture times are both recorded. PNG encoding is not excluded from subsequent deadlines. No static UI mockups or synthetic animation frames."},
        {"fixture", "Eight synthetic twenty-second PCM tracks share one generated file; eight distinct covers; eighty bilingual timestamp groups each; two playlists, favorites and saved statistics. Frontends own separate, semantically identical fixture directories."}};
    QJsonObject sourceHashes;
    const auto sourceRoot = QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().absoluteFilePath("..");
    for (const auto *path : {"src/window.cpp", "src/motion.cpp", "src/nextqueue.cpp", "src/widgets.cpp", "src/lyric-tools.cpp"})
        sourceHashes[path] = sha256(QDir(sourceRoot).filePath(path));
    result["referenceSourceSha256"] = sourceHashes;
    QString widgetsSignature, qmlSignature; QJsonObject widgetsSafety, qmlSafety;
    recorder.check("initial/no-library-connection", QSqlDatabase::connectionNames().isEmpty());
    try { widgets(recorder, widgetsSignature, widgetsSafety); }
    catch (const std::exception &error) { recorder.check("widgets/probe-completed", false, QString::fromUtf8(error.what())); }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); QTest::qWait(30);
    recorder.check("between-frontends/no-library-connection", QSqlDatabase::connectionNames().isEmpty());
    try { qml(recorder, qmlSignature, qmlSafety); }
    catch (const std::exception &error) { recorder.check("qml/probe-completed", false, QString::fromUtf8(error.what())); }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); QTest::qWait(30);
    recorder.check("final/no-library-connection", QSqlDatabase::connectionNames().isEmpty());
    recorder.check("fixtures/identical-semantic-signature", !widgetsSignature.isEmpty() && widgetsSignature == qmlSignature);
    result["widgetsFixtureSignature"] = widgetsSignature; result["qmlFixtureSignature"] = qmlSignature;
    result["widgetsSafety"] = widgetsSafety; result["qmlSafety"] = qmlSafety;
    result["captures"] = recorder.captures; result["checks"] = recorder.checks; result["failures"] = recorder.failures;
    { QMutexLocker lock(&messageMutex); result["messages"] = messages; }
    result["completedUtc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const bool written = saveJson(QDir(recorder.output).filePath("parity-preview.json"), result);
    QTextStream(stdout) << "Native visual preview: " << recorder.captures.size() << " frames, " << recorder.failures << " failed checks; "
        << QDir(recorder.output).filePath("parity-preview.json") << '\n';
    qInstallMessageHandler(nullptr);
    return written ? (recorder.failures ? 1 : 0) : 66;
}
