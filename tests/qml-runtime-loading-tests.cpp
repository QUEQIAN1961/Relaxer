#include "ui-bridge/optional-qml-runtime.h"
#include "ui-run-diagnostics.h"
#include <QDir>
#include <QFile>
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
class QmlRuntimeLoadingTests : public QObject {
    Q_OBJECT
  private slots:
    void loadDoesNotOpenDatabase() {
        const auto before = QSqlDatabase::connectionNames();
        OptionalQmlRuntime runtime(QString::fromUtf8(RELAXER_QML_MODULE_PATH));
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
    void incompatibleInterfaceIsDiagnosable() {
        OptionalQmlRuntime runtime(QString::fromUtf8(RELAXER_QT_CORE_PATH));
        QVERIFY(!runtime.load()); QVERIFY(runtime.error().contains("incompatible"));
    }
    void repeatedSessionsReleaseEveryConnection() {
        const auto before = QSqlDatabase::connectionNames();
        QTemporaryDir dir;
        for (int i = 0; i < 3; ++i) {
            OptionalQmlRuntime runtime(QString::fromUtf8(RELAXER_QML_MODULE_PATH));
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
        QVERIFY(QFile::copy(QString::fromUtf8(RELAXER_APP_PATH), app));
        const auto root = dir.filePath("must-not-create");
        QProcess process;
        process.start(app, {"--ui=qml", "--data", root});
        QVERIFY(process.waitForFinished(15000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 11);
        QVERIFY(process.readAllStandardError().contains("Optional QML module"));
        QVERIFY(!QFile::exists(root));
    }
    void hostPeImportsExcludeOptionalQuick() {
        QProcess process;
        process.start(QString::fromUtf8(RELAXER_OBJDUMP), {"-p", QString::fromUtf8(RELAXER_APP_PATH)});
        QVERIFY(process.waitForFinished()); QCOMPARE(process.exitCode(), 0);
        const auto imports = process.readAllStandardOutput();
        QVERIFY(!imports.contains("Qt6Quick")); QVERIFY(!imports.contains("Qt6Qml"));
        process.start(QString::fromUtf8(RELAXER_OBJDUMP), {"-p", QString::fromUtf8(RELAXER_QML_MODULE_PATH)});
        QVERIFY(process.waitForFinished()); QCOMPARE(process.exitCode(), 0);
        const auto optional = process.readAllStandardOutput();
        QVERIFY(optional.contains("Qt6Quick.dll")); QVERIFY(optional.contains("Qt6Qml.dll"));
        QVERIFY(optional.contains("Qt6QuickControls2.dll"));
    }
    void moduleCodeRemainsMappedAfterObjectsAreDestroyed() {
        {
            OptionalQmlRuntime runtime(QString::fromUtf8(RELAXER_QML_MODULE_PATH));
            QVERIFY(runtime.load());
        }
#ifdef Q_OS_WIN
        // Explicit process-lifetime pin policy; no unsupported hard-unload can
        // invalidate Quick's process-global type/metaobject caches.
        const auto module = QString::fromUtf8(RELAXER_QML_MODULE_PATH).toStdWString();
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
