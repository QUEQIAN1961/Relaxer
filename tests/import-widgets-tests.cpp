#include "window.h"
#include "import-fixture.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
class ImportWidgetsTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QApplication::setQuitOnLastWindowClosed(false);}
    void productionWindowKeepsConfirmedPlaylistAndAllowsRepeatedImports(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());
        const auto first=library.createPlaylist("Confirmed target"),second=library.createPlaylist("Changed selection");
        const auto a=directory.filePath("one.mid"),b=directory.filePath("second.mid");QVERIFY(import_fixture::midi(a,1));QVERIFY(import_fixture::midi(b,2));
        Window window(&library);window.playback()->media()->setAudioOutput(nullptr);window.show();
        auto *co=window.findChild<ImportCoordinator *>("importCoordinator");QVERIFY(co);QSignalSpy done(co,&ImportCoordinator::finished);
        auto *nav=window.findChild<QListWidget *>("playlistNavigation");QVERIFY(nav);
        auto select=[&](qint64 id){for(int i=0;i<nav->count();++i)if(nav->item(i)->data(Qt::UserRole).toLongLong()==id){nav->itemClicked(nav->item(i));return true;}return false;};
        QVERIFY(select(first));window.importSources({a,b});QVERIFY(co->isBusy());QVERIFY(select(second));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,20000);const auto report=qvariant_cast<ImportSummary>(done.first().first());
        QCOMPARE(report.targetPlaylist,first);QCOMPARE(report.added,2);QCOMPARE(library.tracks(first).size(),2);QCOMPARE(library.tracks(second).size(),0);
        QVERIFY(window.findChild<QWidget *>("importPanel")->isHidden());
        window.importSources({a});QTRY_COMPARE_WITH_TIMEOUT(done.size(),2,20000);const auto next=qvariant_cast<ImportSummary>(done.at(1).first());
        QVERIFY(next.taskId>report.taskId);QCOMPARE(next.duplicates,1);QCOMPARE(next.targetPlaylist,second);QCOMPARE(library.tracks(second).size(),1);
        QVERIFY(window.close());QVERIFY(!window.isVisible());QVERIFY(co->isIdle());
    }
    void closingActiveImportWaitsAsynchronouslyAndReopensDatabase(){
        QTemporaryDir directory;const auto root=directory.filePath("data");int committed=0;
        {
            Library library(root);QVERIFY(library.open());QStringList inputs;
            for(int i=0;i<256;++i){const auto path=directory.filePath(QString("fixture-%1.mid").arg(i));QVERIFY(import_fixture::midi(path,i));inputs<<path;}
            Window window(&library);window.playback()->media()->setAudioOutput(nullptr);window.show();
            auto *co=window.findChild<ImportCoordinator *>("importCoordinator");QVERIFY(co);QSignalSpy done(co,&ImportCoordinator::finished),ready(co,&ImportCoordinator::shutdownReady);
            window.importSources(inputs);QVERIFY(co->isBusy());QElapsedTimer timer;timer.start();QVERIFY(!window.close());QVERIFY(timer.elapsed()<1000);
            QVERIFY(window.isVisible());QVERIFY(co->isShuttingDown());
            QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,20000);QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(),20000);QCOMPARE(ready.size(),1);QVERIFY(co->isIdle());
            const auto report=qvariant_cast<ImportSummary>(done.first().first());committed=report.added;QCOMPARE(library.tracks().size(),committed);
            for(auto *widget:QApplication::topLevelWidgets())QVERIFY(!qobject_cast<QMessageBox *>(widget)||!widget->isVisible());
        }
        Library reopened(root);QVERIFY(reopened.open());QCOMPARE(reopened.tracks().size(),committed);
        Window restarted(&reopened);restarted.playback()->media()->setAudioOutput(nullptr);restarted.show();QVERIFY(restarted.close());
    }
    void directWindowDestructionDoesNotBlockWorkerCleanup(){
        QTemporaryDir directory;Library library(directory.filePath("data"));QVERIFY(library.open());const auto path=directory.filePath("fixture.mid");QVERIFY(import_fixture::midi(path));
        auto window=std::make_unique<Window>(&library);window->playback()->media()->setAudioOutput(nullptr);window->show();
        auto *co=window->findChild<ImportCoordinator *>("importCoordinator");QSignalSpy done(co,&ImportCoordinator::finished);
        window->importSources({path});QVERIFY(co->isBusy());QElapsedTimer timer;timer.start();window.reset();QVERIFY(timer.elapsed()<1000);
        // The separate service tests assert actual worker and thread deletion.
        QCoreApplication::processEvents();QCOMPARE(done.size(),0);
    }
};
QTEST_MAIN(ImportWidgetsTests)
#include "import-widgets-tests.moc"
