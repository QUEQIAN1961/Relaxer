#pragma once
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QPluginLoader>
#include <QtTest>
#include <cstdio>

// Run before QApplication: Qt platform loading can abort before initTestCase.
// The test-only override restricts lookup to a disposable plugin fixture.
template<class Test> int relaxerGuiTestMain(int argc,char **argv) {
    QString platform=qEnvironmentVariable("QT_QPA_PLATFORM");
    for(int i=1;i+1<argc;++i)if(QByteArray(argv[i])=="-platform")platform=QString::fromLocal8Bit(argv[i+1]);
#ifdef Q_OS_WIN
    if(platform.isEmpty())platform="windows";
#endif
    const auto isolated=qEnvironmentVariable("RELAXER_TEST_PLUGIN_ROOT");
    if(!isolated.isEmpty()){
        QCoreApplication::setLibraryPaths({isolated});
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH",QFile::encodeName(isolated+"/platforms"));
    }
    const auto diagnostic=QString("GUI-TEST-START platform=%1 executable=%2 pluginRoot=%3 qpaPath=%4 compiledPlugins=%5\n")
        .arg(platform,QString::fromLocal8Bit(argv[0]),isolated,qEnvironmentVariable("QT_QPA_PLATFORM_PLUGIN_PATH"),QLibraryInfo::path(QLibraryInfo::PluginsPath)).toUtf8();
    std::fwrite(diagnostic.constData(),1,size_t(diagnostic.size()),stderr);std::fflush(stderr);
    // Preload for readable failures before Qt's Windows fatal-dialog path.
    // Success here is diagnostic only: QApplication must still initialize the
    // selected integration below. This does not turn a missing plugin to SKIP.
#ifdef Q_OS_WIN
    QStringList paths;
    if(!isolated.isEmpty())paths<<isolated+"/platforms";
    else {
        paths+=qEnvironmentVariable("QT_QPA_PLATFORM_PLUGIN_PATH").split(';',Qt::SkipEmptyParts);
        for(const auto &root:QCoreApplication::libraryPaths())paths<<root+"/platforms";
        paths<<QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath()+"/platforms";
    }
    QStringList errors;bool loaded=false;
    for(const auto &path:paths){
        QPluginLoader plugin(QDir(path).filePath("q"+platform.section(':',0,0)+".dll"));
        if(plugin.load()){loaded=true;break;}
        errors<<plugin.fileName()+": "+plugin.errorString();
    }
    if(!loaded){
        const auto error=("GUI-TEST-PLUGIN-ERROR: Qt platform plugin could not load; "+errors.join("; ")+"\n").toUtf8();
        std::fwrite(error.constData(),1,size_t(error.size()),stderr);std::fflush(stderr);return 78;
    }
#endif
    QApplication app(argc,argv);
    qInfo().noquote()<<"GUI-TEST-LOADED platform="+QGuiApplication::platformName();
    Test test;return QTest::qExec(&test,argc,argv);
}
#define RELAXER_GUI_TEST_MAIN(Test) int main(int argc,char **argv){return relaxerGuiTestMain<Test>(argc,argv);}
