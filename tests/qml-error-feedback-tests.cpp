#include "ui-bridge/qml-shell.h"
#include "import-fixture.h"
#include "lyrics-document-reader.h"
#include <QApplication>
#include <QDataStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>
using namespace relaxer;
namespace {
QQuickItem *visual(QQuickItem *item,const QString &name) {
    if(item->objectName()==name)return item;
    for(auto *child:item->childItems())if(auto *found=visual(child,name))return found;
    return nullptr;
}
bool write(const QString &path,const QByteArray &bytes) {
    QFile f(path);return f.open(QIODevice::WriteOnly|QIODevice::Truncate)&&f.write(bytes)==bytes.size();
}
bool click(QQuickWindow *window,QQuickItem *item) {
    if(!item || !item->isVisible() || !item->isEnabled())return false;
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
                     item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());
    return true;
}
bool escapeEvidence(QQuickWindow *window,FeedbackBridge *feedback,const QString &stage) {
    const auto output=qEnvironmentVariable("RELAXER_D2_ESCAPE_EVIDENCE");
    if(output.isEmpty())return true;
    if(!QDir().mkpath(output))return false;
    const auto name=QString::fromLatin1(QTest::currentTestFunction())+"-"+stage;
    const QJsonObject state{{"stage",stage},{"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"input","QtTest window mouse/key events (not OS input)"},{"platform",QGuiApplication::platformName()},
        {"pid",double(QCoreApplication::applicationPid())},{"windowId",QString::number(quintptr(window->winId()))},
        {"title",window->title()},{"active",window->isActive()},{"visible",window->isVisible()},
        {"popupOpen",window->property("popupOpen").toBool()},{"lyricsOpen",window->property("lyricsOpen").toBool()},
        {"playerProgress",window->property("playerProgress").toDouble()},
        {"bulkActive",window->property("bulkActive").toBool()},{"fullScreen",window->visibility()==QWindow::FullScreen},
        {"feedbackVisible",feedback->visible()},{"historySize",feedback->history().size()},
        {"eventId",feedback->current()["eventId"].toString()}};
    return write(QDir(output).filePath(name+".json"),QJsonDocument(state).toJson())
        && window->grabWindow().save(QDir(output).filePath(name+".png"));
}
qint64 seed(Library &lib,const QString &name,bool playable=false) {
    Track t;t.title=name;t.hash=name;t.file="Music/"+name+".wav";t.duration=3000;
    if(playable){
        QByteArray bytes;QDataStream out(&bytes,QIODevice::WriteOnly);out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData("RIFF",4);out<<quint32(48036);out.writeRawData("WAVEfmt ",8);
        out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);
        out.writeRawData("data",4);out<<quint32(48000);bytes.append(QByteArray(48000,'\0'));
        if(!write(lib.absolute(t.file),bytes))return 0;
    }
    return lib.insert(t);
}
class Browser final:public TranslationBrowserBackend {
public:
    quint64 token=0;
    void translate(quint64 id,const QString &,const QString &,const QString &)override{token=id;}
    void cancel()override{} void resume()override{} void shutdown()override{}
    void answer(const QString &text,const QString &error={}){emit completed(token,text,error);}
};
class ScriptImporter final:public Importer {
public:
    ScriptImporter(const QString &root,std::shared_ptr<std::atomic_bool> token,bool cancel)
        :Importer(root,{"synthetic"}),token(std::move(token)),waitCancel(cancel){}
    void run()override{
        emit scanned(100,0,true);
        if(waitCancel){while(!token->load())QThread::msleep(2);emit finished(true);return;}
        for(int i=0;i<100;++i){ImportResult value;value.source=QString("synthetic-%1.wav").arg(i);value.error="controlled metadata failure";emit imported(value);}
        emit finished(false);
    }
    std::shared_ptr<std::atomic_bool> token;bool waitCancel;
};
class ScriptCoordinator final:public ImportCoordinator {
public:
    ScriptCoordinator(Library *l):ImportCoordinator(l),lib(l){} bool waitCancel=false;Library *lib;
    Importer *createImporter(const ImportRequest &,const std::shared_ptr<std::atomic_bool> &token)override{return new ScriptImporter(lib->root(),token,waitCancel);}
};
}
class ErrorFeedbackTests final:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){qputenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS","1");}
    void escapeWithoutNotification(){
        QTemporaryDir dir;QVERIFY(dir.isValid());Library lib(dir.filePath("data"));QVERIFY(lib.open());
        QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *f=shell.feedbackBridge();
        w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));
        auto *expand=visual(w->contentItem(),"playbackExpandButton");QVERIFY(click(w,expand));
        QTRY_VERIFY(w->property("lyricsOpen").toBool());QTRY_COMPARE(w->property("playerProgress").toDouble(),1.0);
        QVERIFY(escapeEvidence(w,f,"expanded"));
        QTest::keyClick(w,Qt::Key_Escape);
        QTRY_VERIFY_WITH_TIMEOUT(!w->property("lyricsOpen").toBool(),1500);
        QTRY_COMPARE(w->property("playerProgress").toDouble(),0.0);
        QVERIFY(!w->property("popupOpen").toBool());QVERIFY(!f->visible());QVERIFY(f->history().isEmpty());
        QVERIFY(escapeEvidence(w,f,"collapsed"));QVERIFY(shell.warnings().isEmpty());
    }
    void escapeDuringNotificationAndAfterExpiry(){
        QTemporaryDir dir;QVERIFY(dir.isValid());Library lib(dir.filePath("data"));QVERIFY(lib.open());
        const auto id=seed(lib,"escape-missing");QVERIFY(id);
        QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *f=shell.feedbackBridge();
        w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));
        auto *root=w->contentItem();auto *expand=visual(root,"playbackExpandButton");QVERIFY(click(w,expand));
        QTRY_COMPARE(w->property("playerProgress").toDouble(),1.0);
        // Trigger the real missing-file path, rather than a synthetic notification.
        shell.libraryBridge()->play(QString::number(id));QTRY_VERIFY(f->visible());
        auto *notice=visual(root,"feedbackNotice");QVERIFY(notice);QTRY_VERIFY(notice->isVisible());
        QCOMPARE(f->current()["source"].toString(),QString("Playback"));
        const auto event=f->current()["eventId"];const auto history=f->history().size();
        QVERIFY(escapeEvidence(w,f,"notice-expanded"));QTest::keyClick(w,Qt::Key_Escape);
        QTRY_VERIFY_WITH_TIMEOUT(!w->property("lyricsOpen").toBool(),1500);
        QTRY_COMPARE(w->property("playerProgress").toDouble(),0.0);QVERIFY(!w->property("popupOpen").toBool());
        QVERIFY(f->visible());QVERIFY(notice->isVisible());QCOMPARE(f->current()["eventId"],event);QCOMPARE(f->history().size(),history);
        QVERIFY(escapeEvidence(w,f,"notice-collapsed"));
        // Use the production expiry callback, with a shorter wait for the test.
        auto *expiry=w->findChild<QObject *>("feedbackExpiry");QVERIFY(expiry);QVERIFY(expiry->setProperty("interval",80));
        QVERIFY(QMetaObject::invokeMethod(expiry,"restart"));QTRY_VERIFY(!f->visible());
        QCOMPARE(f->history().size(),history);QVERIFY(!w->property("popupOpen").toBool());
        auto *historyButton=visual(root,"feedbackHistoryButton");QVERIFY(historyButton);QVERIFY(historyButton->isVisible());
        QVERIFY(click(w,expand));QTRY_COMPARE(w->property("playerProgress").toDouble(),1.0);QTest::keyClick(w,Qt::Key_Escape);
        QTRY_COMPARE(w->property("playerProgress").toDouble(),0.0);QVERIFY(!w->property("lyricsOpen").toBool());
        QVERIFY(escapeEvidence(w,f,"expired-collapsed"));
        QVERIFY(expiry->setProperty("interval",10000));
        shell.playbackBridge()->play();QTRY_VERIFY(f->visible());QVERIFY(f->current()["eventId"]!=event);
        QVERIFY(!w->property("popupOpen").toBool());QVERIFY(click(w,expand));QTRY_COMPARE(w->property("playerProgress").toDouble(),1.0);
        QTest::keyClick(w,Qt::Key_Escape);QTRY_COMPARE(w->property("playerProgress").toDouble(),0.0);
        QVERIFY(f->visible());QCOMPARE(f->history().size(),history+1);QVERIFY(shell.warnings().isEmpty());
    }
    void escapeDetailsAndModalPriority(){
        QTemporaryDir dir;QVERIFY(dir.isValid());Library lib(dir.filePath("data"));QVERIFY(lib.open());
        QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *f=shell.feedbackBridge();
        w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));auto *root=w->contentItem();
        auto *expand=visual(root,"playbackExpandButton");QVERIFY(click(w,expand));QTRY_COMPARE(w->property("playerProgress").toDouble(),1.0);
        f->post("Library","Error","Escape details","Preserve the detail and background state");
        QVERIFY(click(w,visual(root,"feedbackDetailsButton")));
        auto *details=w->findChild<QObject *>("feedbackDetailsDialog");QVERIFY(details);QTRY_VERIFY(details->property("opened").toBool());
        QTRY_VERIFY(w->property("popupOpen").toBool());QVERIFY(!details->property("modal").toBool());
        const auto event=f->current()["eventId"];QVERIFY(escapeEvidence(w,f,"details-open"));
        QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!details->property("visible").toBool());
        QVERIFY(w->property("lyricsOpen").toBool());QCOMPARE(w->property("playerProgress").toDouble(),1.0);
        QTRY_VERIFY(!w->property("popupOpen").toBool());QVERIFY(f->visible());QCOMPARE(f->current()["eventId"],event);
        auto *confirm=w->findChild<QObject *>("closeTasksConfirmation");QVERIFY(confirm);QVERIFY(confirm->property("modal").toBool());
        QSignalSpy rejected(confirm,SIGNAL(rejected()));QVERIFY(QMetaObject::invokeMethod(confirm,"open"));
        QTRY_VERIFY(confirm->property("opened").toBool());QTRY_VERIFY(w->property("popupOpen").toBool());
        QVERIFY(escapeEvidence(w,f,"modal-open"));QTest::keyClick(w,Qt::Key_Escape);
        QTRY_COMPARE(rejected.size(),1);QTRY_VERIFY(!confirm->property("visible").toBool());
        QVERIFY(w->property("lyricsOpen").toBool());QCOMPARE(w->property("playerProgress").toDouble(),1.0);
        QTRY_VERIFY(!w->property("popupOpen").toBool());QVERIFY(escapeEvidence(w,f,"modal-cancelled-background-expanded"));
        QTest::keyClick(w,Qt::Key_Escape);QTRY_COMPARE(w->property("playerProgress").toDouble(),0.0);
        QCOMPARE(f->current()["eventId"],event);QCOMPARE(f->history().size(),1);QVERIFY(shell.warnings().isEmpty());
    }
    void escapeBulkAndUpcoming_data(){
        QTest::addColumn<bool>("notification");QTest::newRow("quiet")<<false;QTest::newRow("visible-notice")<<true;
    }
    void escapeBulkAndUpcoming(){
        QFETCH(bool,notification);QTemporaryDir dir;QVERIFY(dir.isValid());Library lib(dir.filePath("data"));QVERIFY(lib.open());
        QVERIFY(seed(lib,"escape-bulk"));QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());
        auto *w=shell.window();auto *f=shell.feedbackBridge();w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));
        if(notification)f->post("Library","Error","Bulk Escape notification");
        auto *table=visual(w->contentItem(),"libraryListContainer");QVERIFY(table);
        QVERIFY(QMetaObject::invokeMethod(table,"beginBulk"));QTRY_VERIFY(w->property("bulkActive").toBool());
        QVERIFY(table->property("bulkMode").toBool());QVERIFY(!w->property("popupOpen").toBool());
        QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!w->property("bulkActive").toBool());QVERIFY(!table->property("bulkMode").toBool());
        auto *upcoming=visual(w->contentItem(),"nextQueuePanel");QVERIFY(upcoming);QVERIFY(upcoming->isVisible());
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,upcoming->mapToScene(QPointF(14,upcoming->property("headerHeight").toDouble()/2)).toPoint());
        QTRY_VERIFY(upcoming->property("panelExpanded").toBool());
        QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!upcoming->property("panelExpanded").toBool());
        QCOMPARE(f->visible(),notification);QCOMPARE(f->history().size(),notification?1:0);QVERIFY(shell.warnings().isEmpty());
    }
    void escapeFeedbackPanelLifetime(){
        QTemporaryDir dir;QVERIFY(dir.isValid());Library lib(dir.filePath("data"));QVERIFY(lib.open());
        auto shell=std::make_unique<QmlShell>(&lib,nullptr,true);QVERIFY(shell->load());auto *w=shell->window();
        QVERIFY(!w->property("popupOpen").toBool());auto *f=shell->feedbackBridge();f->post("Library","Error","closing with active feedback");
        QVERIFY(f->visible());QVERIFY(!w->property("popupOpen").toBool());
        QPointer<QQuickWindow> window=w;QPointer<QObject> panel=visual(w->contentItem(),"feedbackPanel");QVERIFY(panel);
        shell.reset();QVERIFY(!window);QVERIFY(!panel);QCoreApplication::processEvents();
        QmlShell restored(&lib,nullptr,true);QVERIFY(restored.load());w=restored.window();
        w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));QVERIFY(!w->property("popupOpen").toBool());
        QVERIFY(!restored.feedbackBridge()->visible());QVERIFY(restored.feedbackBridge()->history().isEmpty());
        QVERIFY(click(w,visual(w->contentItem(),"playbackExpandButton")));QTRY_COMPARE(w->property("playerProgress").toDouble(),1.0);
        QTest::keyClick(w,Qt::Key_Escape);QTRY_COMPARE(w->property("playerProgress").toDouble(),0.0);QVERIFY(restored.warnings().isEmpty());
    }
    void startupDatabaseFailureIsReported(){
        QTemporaryDir dir;QVERIFY(dir.isValid());
        QVERIFY(write(dir.filePath("relaxer.sqlite"),"not a sqlite database"));
        const auto report=dir.filePath("startup.json");QProcess process;
        QSignalSpy done(&process,&QProcess::finished);
        process.start(QString::fromUtf8(RELAXER_APP_PATH),{"--ui=qml","--data",dir.path(),"--diagnostics",report,"--quit-after=100"});
        QVERIFY(!done.isEmpty()||done.wait(15000));QCOMPARE(process.exitCode(),2);
        QFile f(report);QVERIFY(f.open(QIODevice::ReadOnly));const auto result=QJsonDocument::fromJson(f.readAll()).object();
        QVERIFY(!result["startupError"].toString().isEmpty());QVERIFY(!result.contains("databaseReadyMs"));
    }
    void missingRepeatedDuplicateAndRecovery(){
        QTemporaryDir dir;QVERIFY(dir.isValid());Library lib(dir.filePath("data"));QVERIFY2(lib.open(),qPrintable(lib.lastError()));
        const auto missing=seed(lib,"missing"),good=seed(lib,"good",true);QVERIFY(missing&&good);
        QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());auto *f=shell.feedbackBridge();auto *p=shell.playbackBridge();
        shell.libraryBridge()->play(QString::number(missing));
        QTRY_VERIFY(f->visible());QCOMPARE(f->current()["source"].toString(),QString("Playback"));
        QCOMPARE(f->current()["targetId"].toString(),QString::number(missing));
        QVERIFY(f->current()["details"].toString().contains(QStringLiteral("缺失")));
        auto *notice=visual(shell.window()->contentItem(),"feedbackNotice");QVERIFY(notice);QTRY_VERIFY(notice->isVisible());
        QVERIFY(notice->height()>20);QCOMPARE(shell.facade()->queue().items.size(),2);
        const auto first=f->current()["eventId"].toString();
        // The real failure above proves the full path. Only duplicate suppression uses signal injection.
        const auto reason=p->error();shell.facade()->backend()->error(reason);shell.facade()->backend()->error(reason);
        QCOMPARE(f->current()["eventId"].toString(),first);
        f->dismiss();QVERIFY(!f->visible());
        p->play();QTRY_VERIFY(f->visible());QVERIFY(f->current()["eventId"].toString()!=first);
        QCOMPARE(f->current()["targetId"].toString(),QString::number(missing));
        QVERIFY(p->canPlay());
        shell.libraryBridge()->play(QString::number(good));QTRY_VERIFY_WITH_TIMEOUT(p->playing(),10000);
        QCOMPARE(p->error(),QString());QCOMPARE(shell.facade()->queue().items.size(),2);
        QVERIFY(!f->visible());QVERIFY(shell.warnings().isEmpty());
    }
    void invalidMediaFullScreenAndDetails(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());const auto id=seed(lib,"invalid");QVERIFY(id);
        QVERIFY(write(lib.absolute(lib.track(id).file),"not a media file"));
        QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());
        QVERIFY(QMetaObject::invokeMethod(shell.window(),"animatePlayer",Q_ARG(QVariant,QVariant(true))));
        QTRY_VERIFY(shell.window()->property("lyricsOpen").toBool());shell.window()->showFullScreen();
        shell.libraryBridge()->play(QString::number(id));auto *f=shell.feedbackBridge();
        QTRY_VERIFY_WITH_TIMEOUT(f->visible(),15000);QCOMPARE(f->current()["category"].toString(),QString("Error"));
        QTRY_VERIFY(!shell.playbackBridge()->playing());
        auto *root=shell.window()->contentItem();auto *notice=visual(root,"feedbackNotice");QVERIFY(notice);QTRY_VERIFY(notice->isVisible());
        QVERIFY(f->current()["presented"].toBool());QVERIFY(!f->current()["details"].toString().isEmpty());
        auto *button=visual(root,"feedbackDetailsButton");QVERIFY(button);QVERIFY(QMetaObject::invokeMethod(button,"click"));
        auto *detail=visual(root,"feedbackDetailText");QVERIFY(detail);QTRY_VERIFY(detail->isVisible());
        QCOMPARE(detail->property("text").toString(),f->current()["details"].toString());
        if(qEnvironmentVariableIsSet("RELAXER_D2_EVIDENCE")){
            QTest::qWait(200);QVERIFY(shell.window()->grabWindow().save(qEnvironmentVariable("RELAXER_D2_EVIDENCE")+"/fullscreen-failure.png"));
        }
        QVERIFY(shell.warnings().isEmpty());
    }
    void libraryWriteFailureRepeatedAndReadFailure(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());const auto id=seed(lib,"library");QVERIFY(id);
        QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());QSqlQuery q(import_fixture::database(lib));
        QVERIFY(q.exec("CREATE TRIGGER deny_favorite BEFORE UPDATE OF favorite ON tracks BEGIN SELECT RAISE(ABORT,'controlled favorite failure'); END"));
        auto *f=shell.feedbackBridge();QVERIFY(!shell.libraryBridge()->toggleFavorite(QString::number(id)));
        QTRY_VERIFY(f->visible());QCOMPARE(f->current()["source"].toString(),QString("Library"));QVERIFY(!lib.track(id).favorite);
        const auto first=f->current()["eventId"];f->dismiss();QVERIFY(!shell.libraryBridge()->toggleFavorite(QString::number(id)));
        QVERIFY(f->visible());QVERIFY(f->current()["eventId"]!=first);QVERIFY(f->current()["details"].toString().contains("controlled favorite failure"));
        QVERIFY(!shell.libraryBridge()->applyFavorite({QString::number(id)},true));
        QVERIFY(f->current()["details"].toString().contains("controlled favorite failure"));
        QVERIFY(q.exec("DROP TRIGGER deny_favorite"));QVERIFY(shell.libraryBridge()->toggleFavorite(QString::number(id)));QVERIFY(lib.track(id).favorite);QVERIFY(!f->visible());
        QVERIFY(q.exec("CREATE TRIGGER deny_playlist BEFORE INSERT ON playlists BEGIN SELECT RAISE(ABORT,'controlled playlist failure'); END"));
        QCOMPARE(shell.libraryBridge()->createPlaylist("valid name"),QString("0"));
        QVERIFY(f->current()["details"].toString().contains("controlled playlist failure"));
        QVERIFY(q.exec("DROP TRIGGER deny_playlist"));const auto playlist=shell.libraryBridge()->createPlaylist("valid name");QVERIFY(playlist!="0");
        QVERIFY(q.exec("CREATE TRIGGER deny_rename BEFORE UPDATE OF name ON playlists BEGIN SELECT RAISE(ABORT,'controlled rename failure'); END"));
        QVERIFY(!shell.libraryBridge()->renamePlaylist(playlist,"another valid name"));
        QVERIFY(f->current()["details"].toString().contains("controlled rename failure"));QVERIFY(q.exec("DROP TRIGGER deny_rename"));
        QVERIFY(shell.libraryBridge()->renamePlaylist(playlist,"another valid name"));QVERIFY(!f->visible());
        f->dismiss();QVERIFY(q.exec("ALTER TABLE tracks RENAME TO hidden_tracks"));lib.tracks();
        QTRY_VERIFY(f->visible());QVERIFY(f->current()["details"].toString().contains("tracks"));
        QVERIFY(q.exec("ALTER TABLE hidden_tracks RENAME TO tracks"));
    }
    void realImportResultsAndVisibleDetails(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());
        const auto good=dir.filePath("good.mid"),bad=dir.filePath("bad.mp3");QVERIFY(import_fixture::midi(good));QVERIFY(write(bad,"invalid synthetic media"));
        auto *b=shell.importBridge();auto *f=shell.feedbackBridge();
        QVERIFY(b->start({QUrl::fromLocalFile(good)}));QTRY_VERIFY_WITH_TIMEOUT(!b->busy(),20000);
        QCOMPARE(b->result()["added"].toInt(),1);QCOMPARE(f->current()["category"].toString(),QString("Information"));
        QVERIFY(b->start({QUrl::fromLocalFile(good),QUrl::fromLocalFile(bad)}));QTRY_VERIFY_WITH_TIMEOUT(!b->busy(),20000);
        QCOMPARE(b->result()["duplicates"].toInt(),1);QCOMPARE(b->result()["failed"].toInt(),1);QCOMPARE(b->result()["processed"].toInt(),2);
        QCOMPARE(f->current()["category"].toString(),QString("Warning"));QVERIFY(f->current()["details"].toString().contains("bad.mp3"));
        auto *button=visual(shell.window()->contentItem(),"feedbackDetailsButton");QVERIFY(button);QVERIFY(QMetaObject::invokeMethod(button,"click"));
        auto *detail=visual(shell.window()->contentItem(),"feedbackDetailText");QVERIFY(detail);QTRY_VERIFY(detail->isVisible());
        QVERIFY(detail->property("text").toString().contains("bad.mp3"));
        QVERIFY(b->start({QUrl::fromLocalFile(bad)}));QTRY_VERIFY_WITH_TIMEOUT(!b->busy(),20000);
        QCOMPARE(f->current()["category"].toString(),QString("Error"));QCOMPARE(b->result()["failed"].toInt(),1);
        QVERIFY(b->start({QUrl::fromLocalFile(dir.filePath("absent.wav"))}));QTRY_VERIFY_WITH_TIMEOUT(!b->busy(),20000);
        QCOMPARE(b->result()["ignoredPaths"].toInt(),1);QCOMPARE(f->current()["category"].toString(),QString("Warning"));
        QVERIFY(shell.warnings().isEmpty());
    }
    void boundedImportCancellationAndStaleTask(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());ScriptCoordinator co(&lib);ImportBridge bridge(&lib,&co);
        QSignalSpy done(&bridge,&ImportBridge::completed);
        QVERIFY(bridge.start({QUrl::fromLocalFile(dir.filePath("synthetic"))}));QTRY_COMPARE(done.size(),1);
        QCOMPARE(bridge.result()["failed"].toInt(),100);QCOMPARE(bridge.result()["errors"].toStringList().size(),8);
        QCOMPARE(bridge.result()["omittedErrors"].toInt(),92);QCOMPARE(done.first()[0].toString(),QString("Error"));
        const auto old=bridge.result()["taskId"].toString().toULongLong();co.waitCancel=true;
        QVERIFY(bridge.start({QUrl::fromLocalFile(dir.filePath("next"))}));
        ImportSummary stale;stale.taskId=old;stale.failed=999;emit co.finished(stale);
        QCOMPARE(done.size(),1);QVERIFY(bridge.result().isEmpty());
        QVERIFY(bridge.cancel());QTRY_COMPARE(done.size(),2);QCOMPARE(done.last()[0].toString(),QString("Cancellation"));
        QVERIFY(bridge.result()["cancelled"].toBool());QVERIFY(co.isIdle());
    }
    void lyricsCancelAndSnapshotConflictPreserveDraft(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());const auto id=seed(lib,"lyrics");QVERIFY(id);
        QVERIFY(lib.saveLyrics(id,"[00:01.00]Old original\n"));QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());
        auto *b=shell.lyricsSearchBridge();QVERIFY(b->setTargetTrack(QString::number(id)));QSignalSpy request(b,&LyricsSearchBridge::replacementRequested),saved(b,&LyricsSearchBridge::saved);
        auto *f=shell.feedbackBridge();const auto before=f->history().size();
        QVERIFY(!b->saveText("[00:01.00]New original",true));QCOMPARE(request.size(),1);QCOMPARE(f->history().size(),before);
        QVERIFY(!b->resolveReplacement(request.last()[0].toString(),0));QCOMPARE(f->history().size(),before);QCOMPARE(saved.size(),0);
        QVERIFY(!b->resolveReplacement("expired-cancel-token",0));QCOMPARE(f->history().size(),before);
        QVERIFY(b->draftText().contains("New original"));
        QVERIFY(!b->saveText("[00:01.00]Another draft",true));QCOMPARE(request.size(),2);
        const QByteArray external="[00:01.00]Externally modified\n";QVERIFY(write(lib.absolute(lib.track(id).lyrics),external));
        QVERIFY(!b->resolveReplacement(request.last()[0].toString(),1));QTRY_VERIFY(f->visible());
        QCOMPARE(f->current()["source"].toString(),QString("Lyrics"));QCOMPARE(saved.size(),0);QVERIFY(b->draftText().contains("Another draft"));
        QCOMPARE(LyricsDocumentReader::readFile(lib.absolute(lib.track(id).lyrics)).bytes,external);
    }
    void translationFailureAndPendingSaveProtection(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());const auto id=seed(lib,"translation");QVERIFY(id);
        const QByteArray source="[00:01.00]Hello world\n[00:02.00]Walk home\n";QVERIFY(lib.saveLyrics(id,source));
        auto browser=std::make_unique<Browser>();auto *backend=browser.get();QmlShell shell(&lib,nullptr,true,nullptr,std::move(browser));QVERIFY(shell.load());
        auto *b=shell.translationBridge();QVERIFY(b->setTargetTrack(QString::number(id)));auto *f=shell.feedbackBridge();
        auto key=b->prepareBrowser("bing","en");QVERIFY(!key.isEmpty());QVERIFY(b->start(key));QTRY_VERIFY(backend->token);
        backend->answer({},"controlled service unavailable");QTRY_COMPARE(b->state(),QString("Failed"));QTRY_VERIFY(f->visible());
        QCOMPARE(f->current()["source"].toString(),QString("Translation"));
        key=b->prepareBrowser("bing","en");QVERIFY(!key.isEmpty());QVERIFY(b->start(key));
        QTRY_COMPARE(b->state(),QString("Running"));const auto firstToken=backend->token;
        backend->answer(QStringLiteral("你好世界"));QTRY_VERIFY(backend->token!=firstToken);backend->answer(QStringLiteral("走回家"));
        QTRY_COMPARE(b->state(),QString("CompletedPendingSave"));const auto preview=b->previewText();QVERIFY(!preview.isEmpty());
        QSqlQuery q(import_fixture::database(lib));QVERIFY(q.exec("CREATE TRIGGER deny_lyrics BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT RAISE(ABORT,'controlled save failure'); END"));
        f->dismiss();QVERIFY(!b->save());QTRY_VERIFY(f->visible());QCOMPARE(b->state(),QString("CompletedPendingSave"));QCOMPARE(b->previewText(),preview);
        QCOMPARE(LyricsDocumentReader::readFile(lib.absolute(lib.track(id).lyrics)).bytes,source);
        QVERIFY(q.exec("DROP TRIGGER deny_lyrics"));QVERIFY(b->save());QCOMPARE(b->state(),QString("Saved"));
    }
    void staleTranslationAndShutdown(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());const auto a=seed(lib,"old"),b=seed(lib,"new");
        QVERIFY(lib.saveLyrics(a,"[00:01.00]Hello world\n"));QVERIFY(lib.saveLyrics(b,"[00:01.00]New song\n"));
        auto browser=std::make_unique<Browser>();auto *backend=browser.get();QmlShell shell(&lib,nullptr,true,nullptr,std::move(browser));QVERIFY(shell.load());
        auto *translation=shell.translationBridge();QVERIFY(translation->setTargetTrack(QString::number(a)));
        const auto key=translation->prepareBrowser("bing","en");QVERIFY(!key.isEmpty());QVERIFY(translation->start(key));QTRY_VERIFY(backend->token);
        QVERIFY(translation->setTargetTrack(QString::number(b)));shell.appBridge()->navigate(2);
        const auto count=shell.feedbackBridge()->history().size();backend->answer({},"late failure");QTest::qWait(100);
        QCOMPARE(shell.feedbackBridge()->history().size(),count);
        QVERIFY(shell.shutdown());shell.libraryBridge()->renamePlaylist("99999","invalid");
        QCOMPARE(shell.feedbackBridge()->history().size(),count);QVERIFY(!shell.feedbackBridge()->visible());
        QVERIFY(shell.warnings().isEmpty());
    }
    void feedbackBoundsIdentityAndLifetime(){
        FeedbackBridge f;for(int i=0;i<1000;++i)f.post("Library","Error","repeated",QString(20000,'x'));
        QCOMPARE(f.history().size(),20);QCOMPARE(f.current()["details"].toString().size(),16384);QCOMPARE(f.current()["eventId"].toString(),QString("1000"));
        f.post("Import","Cancellation","cancelled");QVERIFY(f.visible());QCOMPARE(f.current()["eventId"].toString(),QString("1000"));
        f.dismiss();f.post("Import","Cancellation","cancelled");QVERIFY(!f.visible());
        f.post("Lyrics","ConfirmationRequired","decide",{}, {}, {},false,true);QVERIFY(!f.visible());
        f.setClosing(true);const auto id=f.current()["eventId"];f.post("Library","Error","late");QCOMPARE(f.current()["eventId"],id);
    }
    void queryRecoveryInvalidatesOnlyItsDeferredFailure(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());QVERIFY(seed(lib,"recovery"));
        QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());auto *f=shell.feedbackBridge();
        QSqlQuery q(import_fixture::database(lib));const auto before=f->history().size();
        QVERIFY(q.exec("ALTER TABLE tracks RENAME TO hidden_tracks"));lib.tracks();
        QVERIFY(q.exec("ALTER TABLE hidden_tracks RENAME TO tracks"));QCOMPARE(lib.tracks().size(),1);
        QCoreApplication::processEvents();QCOMPARE(f->history().size(),before);
        // The identical error after successful recovery is a fresh failure.
        QVERIFY(q.exec("ALTER TABLE tracks RENAME TO hidden_tracks"));lib.tracks();
        QTRY_COMPARE(f->history().size(),before+1);QVERIFY(f->visible());
        QVERIFY(q.exec("ALTER TABLE hidden_tracks RENAME TO tracks"));QCOMPARE(lib.tracks().size(),1);
    }
    void silentHistoryKeepsActiveIdentityDetailsAndExpiry(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());
        auto *f=shell.feedbackBridge();f->post("Playback","Error","original song","original reason","attempt","42");
        const auto id=f->current()["eventId"].toString();
        auto *root=shell.window()->contentItem();auto *button=visual(root,"feedbackDetailsButton");QVERIFY(button);
        QVERIFY(QMetaObject::invokeMethod(button,"click"));auto *detail=visual(root,"feedbackDetailText");QVERIFY(detail);
        QTRY_VERIFY(detail->isVisible());QCOMPARE(detail->property("text").toString(),QString("original reason"));
        auto *expiry=shell.window()->findChild<QObject *>("feedbackExpiry");QVERIFY(expiry);
        QVERIFY(expiry->setProperty("interval",200));QVERIFY(QMetaObject::invokeMethod(expiry,"restart"));QTest::qWait(120);
        f->post("Import","Cancellation","cancelled");
        QCOMPARE(f->current()["eventId"].toString(),id);QVERIFY(f->visible());
        QCOMPARE(detail->property("text").toString(),QString("original reason"));
        auto *choice=visual(root,"feedbackHistoryChoice");QVERIFY(choice);
        QCOMPARE(choice->property("displayText").toString(),QString("original song"));
        QTRY_VERIFY_WITH_TIMEOUT(!f->visible(),150);
        QVERIFY(f->history()[1].toMap()["presented"].toBool());QVERIFY(!f->history()[0].toMap()["presented"].toBool());
        FeedbackBridge unpresented;unpresented.post("Playback","Error","failed");const auto old=unpresented.current()["eventId"].toString();
        unpresented.post("Import","Cancellation","cancelled");unpresented.presented(old);
        QVERIFY(unpresented.history()[1].toMap()["presented"].toBool());QVERIFY(!unpresented.history()[0].toMap()["presented"].toBool());
    }
    void queuedQueryFailureCannotOutliveShell(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());QVERIFY(seed(lib,"lifetime"));
        auto shell=std::make_unique<QmlShell>(&lib,nullptr,true);QVERIFY(shell->load());
        QPointer<QQuickWindow> window=shell->window();QSqlQuery q(import_fixture::database(lib));
        QVERIFY(q.exec("ALTER TABLE tracks RENAME TO hidden_tracks"));lib.tracks();
        QVERIFY(q.exec("ALTER TABLE hidden_tracks RENAME TO tracks"));
        shell.reset();QVERIFY(window.isNull());QCoreApplication::processEvents();QCOMPARE(lib.tracks().size(),1);
    }
};
QTEST_MAIN(ErrorFeedbackTests)
#include "qml-error-feedback-tests.moc"
