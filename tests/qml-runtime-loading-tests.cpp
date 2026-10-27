#include "ui-bridge/optional-qml-runtime.h"
#include "ui-run-diagnostics.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace relaxer;
static QString hostPath() {
    return qEnvironmentVariable("RELAXER_TEST_APP", QString::fromUtf8(RELAXER_APP_PATH));
}
static QString modulePath() {
    return qEnvironmentVariableIsSet("RELAXER_TEST_APP")
        ? QFileInfo(hostPath()).dir().filePath("RelaxerQmlUi.dll") : QString::fromUtf8(RELAXER_QML_MODULE_PATH);
}
class QmlRuntimeLoadingTests : public QObject {
    Q_OBJECT
  private slots:
    void loadDoesNotOpenDatabase() {
        const auto before = QSqlDatabase::connectionNames();
        OptionalQmlRuntime runtime(modulePath());
        QVERIFY2(runtime.load(), qPrintable(runtime.error()));
        QVERIFY(runtime.load());
        QCOMPARE(QSqlDatabase::connectionNames(), before);
    }
    void missingModuleIsDiagnosable() {
        QTemporaryDir dir;
        OptionalQmlRuntime runtime(dir.filePath("absent.dll"));
        QVERIFY(!runtime.load()); QVERIFY(!runtime.error().isEmpty());
        QVERIFY(runtime.error().contains("Optional QML module"));
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }
    void corruptModuleIsDiagnosable() {
        QTemporaryDir dir;
        QFile file(dir.filePath("invalid.dll"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("not a shared library"), qint64(20));
        file.close();
        OptionalQmlRuntime runtime(file.fileName());
        QVERIFY(!runtime.load());
        QVERIFY(runtime.error().contains("Optional QML module"));
    }
    void incompatibleInterfaceIsDiagnosable() {
        const auto deployedCore = QFileInfo(hostPath()).dir().filePath("Qt6Core.dll");
        OptionalQmlRuntime runtime(QFile::exists(deployedCore) ? deployedCore : QString::fromUtf8(RELAXER_QT_CORE_PATH));
        QVERIFY(!runtime.load()); QVERIFY(runtime.error().contains("incompatible"));
    }
    void repeatedSessionsReleaseEveryConnection() {
        const auto before = QSqlDatabase::connectionNames();
        QTemporaryDir dir;
        for (int i = 0; i < 3; ++i) {
            OptionalQmlRuntime runtime(modulePath());
            QVERIFY2(runtime.load(), qPrintable(runtime.error()));
            UiRunDiagnostics diagnostics;
            const auto report = dir.filePath(QString("session-%1.json").arg(i));
            const auto root = dir.filePath(QString("data-%1").arg(i));
            QmlLaunchOptions options{root, {}, report, i, 40};
            QCOMPARE(runtime.run(qApp, options, diagnostics), 0);
            QCOMPARE(diagnostics.result["databaseConnections"].toInt(), 1);
            QCOMPARE(diagnostics.result["playbackInstances"].toInt(), 1);
            QVERIFY(diagnostics.result["saveSucceeded"].toBool());
            QCOMPARE(QSqlDatabase::connectionNames(), before);
            QVERIFY(!QFile::exists(root + "/data"));
        }
    }
    void hostRejectsMissingModuleBeforeTouchingData() {
        QTemporaryDir dir;
        const auto bin = dir.filePath(QString::fromUtf8("隔离 bin"));
        QVERIFY(QDir().mkpath(bin));
        const auto app = bin + "/Relaxer.exe";
        QVERIFY(QFile::copy(hostPath(), app));
        // Standalone deployment tests have system-only PATH. Copy only the
        // host's colocated base runtime, excluding the optional UI and Quick.
        const auto origin = QFileInfo(hostPath()).dir();
        for (const auto &name : origin.entryList({"*.dll"}, QDir::Files)) {
            if (name == "RelaxerQmlUi.dll" || name.startsWith("Qt6Quick") || name.startsWith("Qt6Qml")) continue;
            QVERIFY(QFile::copy(origin.filePath(name), bin + "/" + name));
        }
        if (QFile::exists(origin.filePath("qt.conf"))) {
            QVERIFY(QFile::copy(origin.filePath("qt.conf"), bin + "/qt.conf"));
            QVERIFY(QDir().mkpath(bin + "/platforms"));
            const auto platforms = QDir(origin.filePath("platforms"));
            for (const auto &name : platforms.entryList({"*.dll"}, QDir::Files))
                QVERIFY(QFile::copy(platforms.filePath(name), bin + "/platforms/" + name));
        }
        const auto root = dir.filePath("must-not-create");
        QProcess process;
        process.start(app, {"--ui=qml", "--data", root});
        QVERIFY(process.waitForFinished(15000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 11);
        QVERIFY(process.readAllStandardError().contains("Optional QML module"));
        QVERIFY(!QFile::exists(root));
    }
    void widgetsHostRunsWithoutOptionalModule() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto bin = dir.filePath("bin");
        QVERIFY(QDir().mkpath(bin));
        const auto app = bin + "/Relaxer.exe";
        QVERIFY(QFile::copy(hostPath(), app));
        const auto origin = QFileInfo(hostPath()).dir();
        for (const auto &name : origin.entryList({"*.dll"}, QDir::Files)) {
            if (name == "RelaxerQmlUi.dll" || name.startsWith("Qt6Quick") || name.startsWith("Qt6Qml")) continue;
            QVERIFY(QFile::copy(origin.filePath(name), bin + "/" + name));
        }
        if (QFile::exists(origin.filePath("qt.conf"))) {
            QVERIFY(QFile::copy(origin.filePath("qt.conf"), bin + "/qt.conf"));
            QVERIFY(QDir().mkpath(bin + "/platforms"));
            const auto platforms = QDir(origin.filePath("platforms"));
            for (const auto &name : platforms.entryList({"*.dll"}, QDir::Files))
                QVERIFY(QFile::copy(platforms.filePath(name), bin + "/platforms/" + name));
        }
        QVERIFY(!QFile::exists(bin + "/RelaxerQmlUi.dll"));
        const auto report = dir.filePath("widgets-run.json");
        QProcess process;
        process.start(app, {"--ui=widgets", "--data", dir.filePath("data"),
                            "--diagnostics", report, "--quit-after", "200"});
        QVERIFY(process.waitForFinished(15000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
        QFile result(report);
        QVERIFY(result.open(QIODevice::ReadOnly));
        const auto diagnostics = QJsonDocument::fromJson(result.readAll()).object();
        QCOMPARE(diagnostics["mode"].toString(), QString("widgets"));
        QVERIFY(diagnostics["saveSucceeded"].toBool());
    }
    void hostPeImportsExcludeOptionalQuick() {
        // GNU objdump's narrow Windows filename handling cannot open a
        // non-ASCII absolute path. Qt copies the actual inputs losslessly;
        // inspect ASCII basenames relative to the temporary working directory.
        QTemporaryDir inspection;
        QVERIFY(inspection.isValid());
        QVERIFY(QFile::copy(hostPath(), inspection.filePath("host.exe")));
        QVERIFY(QFile::copy(modulePath(), inspection.filePath("optional.dll")));
        QProcess process;
        process.setWorkingDirectory(inspection.path());
        process.start(QString::fromUtf8(RELAXER_OBJDUMP), {"-p", "host.exe"});
        QVERIFY(process.waitForFinished()); QCOMPARE(process.exitCode(), 0);
        const auto imports = process.readAllStandardOutput();
        QVERIFY(!imports.contains("Qt6Quick")); QVERIFY(!imports.contains("Qt6Qml"));
        process.start(QString::fromUtf8(RELAXER_OBJDUMP), {"-p", "optional.dll"});
        QVERIFY(process.waitForFinished()); QCOMPARE(process.exitCode(), 0);
        const auto optional = process.readAllStandardOutput();
        QVERIFY(optional.contains("Qt6Quick.dll")); QVERIFY(optional.contains("Qt6Qml.dll"));
        QVERIFY(optional.contains("Qt6QuickControls2.dll"));
    }
    void moduleCodeRemainsMappedAfterObjectsAreDestroyed() {
        {
            OptionalQmlRuntime runtime(modulePath());
            QVERIFY(runtime.load());
        }
#ifdef Q_OS_WIN
        // Explicit process-lifetime pin policy; no unsupported hard-unload can
        // invalidate Quick's process-global type/metaobject caches.
        const auto module = modulePath().toStdWString();
        QVERIFY(GetModuleHandleW(module.c_str()) != nullptr);
#endif
        QCoreApplication::processEvents();
        QVERIFY(QSqlDatabase::connectionNames().isEmpty());
    }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QmlRuntimeLoadingTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "qml-runtime-loading-tests.moc"
