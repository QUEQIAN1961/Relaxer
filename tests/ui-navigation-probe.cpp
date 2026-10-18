#include "window.h"
#include "ui-bridge/qml-shell.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <functional>

class PaintObserver : public QObject {
  public:
    std::function<void()> painted;
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::Paint && painted) painted();
        return false;
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addOptions({{"ui", "widgets/qml", "mode"}, {"data", "isolated fixture root", "directory"}, {"output", "JSON", "file"}});
    parser.process(app);
    if (!parser.isSet("data") || !parser.isSet("output") ||
        (parser.value("ui") != "qml" && parser.value("ui") != "widgets")) return 64;
    QDir().mkpath(parser.value("data"));
    QLockFile lock(QDir(parser.value("data")).filePath("relaxer.lock"));
    if (!lock.tryLock()) return 3;
    relaxer::Library library(parser.value("data"));
    if (!library.open()) return 2;
    std::unique_ptr<relaxer::Window> widgets;
    std::unique_ptr<relaxer::QmlShell> quick;
    const bool qml = parser.value("ui") == "qml";
    if (qml) { quick = std::make_unique<relaxer::QmlShell>(&library); if (!quick->load()) return 10; }
    else { widgets = std::make_unique<relaxer::Window>(&library); widgets->show(); }
    QElapsedTimer timer;
    QJsonArray samples;
    bool awaiting = false;
    double callUs = 0;
    PaintObserver observer;
    std::function<void()> navigate;
    const auto painted = [&] {
        if (!awaiting) return;
        awaiting = false;
        samples.append(QJsonObject{{"callUs", callUs}, {"nextPaintMs", double(timer.nsecsElapsed()) / 1000000.}});
        QTimer::singleShot(40, &app, [&] { navigate(); });
    };
    if (qml) QObject::connect(quick->window(), &QQuickWindow::frameSwapped, &app, painted, Qt::QueuedConnection);
    else { observer.painted = painted; widgets->installEventFilter(&observer); }
    navigate = [&] {
        if (samples.size() == 20) {
            QSaveFile file(parser.value("output"));
            const auto bytes = QJsonDocument(QJsonObject{{"mode", parser.value("ui")}, {"samples", samples},
                {"paintBoundary", qml ? "frameSwapped queued to GUI thread" : "QWidget Paint event dispatched"},
                {"note", "callUs measures synchronous bindings/layout only; paint metrics have different boundaries and exclude compositor"}}).toJson();
            const bool saved = file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
            app.exit(saved ? 0 : 9); return;
        }
        timer.start(); awaiting = true;
        if (qml) { quick->appBridge()->navigate(samples.size() % 2 == 0 ? 1 : 3); quick->window()->requestUpdate(); }
        else { if (samples.size() % 2 == 0) widgets->showPlaylists(); else widgets->showSettings(); widgets->update(); }
        callUs = double(timer.nsecsElapsed()) / 1000.;
    };
    QTimer::singleShot(1000, &app, [&] { navigate(); });
    QTimer::singleShot(20000, &app, [&] { app.exit(12); });
    const int code = app.exec();
    if (quick && !quick->shutdown()) return 8;
    if (widgets && !widgets->playbackFacade()->shutdown()) return 8;
    return code;
}
