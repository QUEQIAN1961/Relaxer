// Explicit online smoke test, excluded from first-start/offline CTest.
#include "browser-translation.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>
using namespace relaxer;
int main(int argc,char **argv){
    qInstallMessageHandler([](QtMsgType,const QMessageLogContext &,const QString &message){QTextStream(stderr)<<message<<Qt::endl;});
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()<3){qCritical("usage: browser-smoke DATA_ROOT bing|baidu [visible]");return 2;}
    BrowserTranslation browser(args[1]);browser.setProperty("headlessTest",args.value(3)!="visible");browser.setProperty("traceTest",true);
    const QStringList lines{"The sun is shining.","We are walking home.","The sun is shining."};int current=0;QElapsedTimer clock;clock.start();
    auto start=[&]{browser.translate(quint64(current+1),args[2],lines[current],"en");};
    QObject::connect(&browser,&BrowserTranslation::needsAttention,&app,[&](const QString &reason){QTextStream(stderr)<<reason<<Qt::endl;app.exit(3);});
    QObject::connect(&browser,&BrowserTranslation::completed,&app,[&](quint64 token,const QString &text,const QString &error){
        const bool ok=token==quint64(current+1)&&error.isEmpty()&&text.contains(QRegularExpression("[\\x{3400}-\\x{9fff}]"))&&!text.contains(lines[current]);
        QTextStream(stdout)<<QJsonDocument(QJsonObject{{"provider",args[2]},{"line",current},{"source",lines[current]},{"text",text},{"error",error},{"ok",ok},{"elapsed_ms",double(clock.elapsed())}}).toJson(QJsonDocument::Compact)<<Qt::endl;
        if(!ok){app.exit(1);return;}++current;if(current==lines.size()){app.exit(0);return;}QTimer::singleShot(700,&app,start);
    });
    QTimer::singleShot(0,&app,start);QTimer::singleShot(150000,&app,[&]{app.exit(4);});return app.exec();
}
