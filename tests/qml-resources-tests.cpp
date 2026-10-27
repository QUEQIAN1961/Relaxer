#include "ui-bridge/qml-shell.h"
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace relaxer;

class QmlResourcesTests : public QObject {
    Q_OBJECT
private slots:
    void qmlResourcesAndShellAreAvailable() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Library library(directory.filePath("data"));
        QVERIFY(library.open());
        QmlShell shell(&library, nullptr, true);
        for (const auto &path : {":/qml/Main.qml", ":/qml/theme/Theme.qml",
                                 ":/qml/pages/ShortcutsPage.qml", ":/icon.svg", ":/changelog.json"}) {
            QFile resource(QString::fromLatin1(path));
            QVERIFY2(resource.open(QIODevice::ReadOnly), path);
            QVERIFY2(!resource.readAll().isEmpty(), path);
        }
        QFile log(":/changelog.json");
        QVERIFY(log.open(QIODevice::ReadOnly));
        QVERIFY(QJsonDocument::fromJson(log.readAll()).isArray());
        QVERIFY(QImageReader(":/icon.svg").canRead());
        QVERIFY2(shell.load(), qPrintable(shell.warnings().join('\n')));
        QVERIFY(shell.window());
    }
    void missingQmlResourceFailsClearly() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Library library(directory.filePath("data"));
        QVERIFY(library.open());
        QmlShell shell(&library, nullptr, true);
        // Only this test process unregisters its compiled-in resource. Restore
        // it on exit even if an assertion fails; no file or deployment is changed.
        struct Restore { ~Restore() { Q_INIT_RESOURCE(qml); } } restore;
        for (int i = 0; i < 16 && QFile::exists(":/qml/Main.qml"); ++i)
            Q_CLEANUP_RESOURCE(qml);
        QVERIFY(!QFile::exists(":/qml/Main.qml"));
        QVERIFY(!shell.load());
        QVERIFY(!shell.warnings().isEmpty());
        QVERIFY(shell.warnings().join('\n').contains("Main.qml"));
    }
};

QTEST_MAIN(QmlResourcesTests)
#include "qml-resources-tests.moc"
