#include "translation-coordinator.h"
#include "translation-fixture.h"
#include "import-fixture.h"
#include <QPointer>
#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>

using State=TranslationState;
namespace {
class BrowserMock final : public TranslationBrowserBackend {
public:
    struct Call {quint64 token;QString text;};
    QList<Call> calls;
    int cancels=0,resumes=0,closes=0;
    void translate(quint64 token,const QString &,const QString &text,const QString &) override {calls.append({token,text});}
    void cancel() override {++cancels;}
    void resume() override {++resumes;}
    void shutdown() override {++closes;}
    void answer(const QString &text=QStringLiteral("译文"),const QString &error={}){emit completed(calls.last().token,text,error);}
    void wait(){emit attention(calls.last().token,QStringLiteral("请人工验证"));}
};
struct Fixture {
    QTemporaryDir dir;
    Library library{dir.filePath("data")};
    TranslationHttpMock http;
    std::unique_ptr<AiRuntime> runtime;
    std::unique_ptr<TranslationCoordinator> coordinator;
    BrowserMock *browser=nullptr;
    Track a,b;
    const QByteArray source="[00:01.00]Hello world\n[00:02.00]Walk home\n";
    bool ok=false;
    Fixture(){
        if(!dir.isValid()||!library.open()||!http.listen())return;
        a=seed(library,"Alpha","b7-alpha");b=seed(library,"Beta","b7-beta");
        if(!a.id||!b.id||!library.saveLyrics(a.id,source)||!library.saveLyrics(b.id,source))return;
        a=library.track(a.id);b=library.track(b.id);
        if(!library.setTranslationJudgment(a.id,TranslationJudgment::NeedsTranslation)||!library.setTranslationJudgment(b.id,TranslationJudgment::NeedsTranslation))return;
        runtime=std::make_unique<AiRuntime>(&library,DeepSeekProvider::MockEndpoint{http.baseUrl()},std::make_unique<FakeCredentialStore>());
        if(!configureRuntime(*runtime))return;
        auto backend=std::make_unique<BrowserMock>();browser=backend.get();
        coordinator=std::make_unique<TranslationCoordinator>(&library,runtime.get(),nullptr,std::move(backend));ok=true;
    }
    void response(){http.enqueue(completion(QString::fromUtf8(translationJson({{0,QStringLiteral("你好世界")},{1,QStringLiteral("走回家")}}))));}
    quint64 ai(qint64 track=0){return coordinator->prepareAi(track?track:a.id);}
    quint64 web(qint64 track=0){return coordinator->prepareBrowser(track?track:a.id,"bing","en");}
    State state(quint64 id){return coordinator->summary(id).state;}
    QByteArray bytes(qint64 id=0){QByteArray result;readFile(library.absolute(library.track(id?id:a.id).lyrics),&result);return result;}
};
}
class TranslationCoordinatorTests final : public QObject {
    Q_OBJECT
private slots:
    void preparationIsPureAndIdentityUnique(){
        Fixture f;QVERIFY(f.ok);const auto a=f.ai(),b=f.web();QVERIFY(a);QVERIFY(b>a);
        QCOMPARE(f.http.requestCount(),0);QVERIFY(f.browser->calls.isEmpty());
        const auto v=f.coordinator->summary(a);QCOMPARE(v.state,State::WaitingForConsent);QCOMPARE(v.trackId,f.a.id);
        QVERIFY(!v.sourceDigest.isEmpty());QVERIFY(!v.configurationDigest.isEmpty());QCOMPARE(v.totalLines,2);
        QVERIFY(f.coordinator->cancel(a));QVERIFY(f.coordinator->forget(a));QVERIFY(!f.coordinator->start(a));
    }
    void aiPreviewSaveIsOneRequestAndOneTerminal(){
        Fixture f;QVERIFY(f.ok);QSignalSpy finished(f.coordinator.get(),&TranslationCoordinator::finished);f.response();
        const auto id=f.ai();QVERIFY(f.coordinator->start(id));QVERIFY(!f.coordinator->start(id));
        QTRY_COMPARE(f.state(id),State::CompletedPendingSave);QCOMPARE(f.http.requestCount(),1);QCOMPARE(f.bytes(),f.source);
        QCOMPARE(finished.size(),0);QVERIFY(!f.coordinator->cancel(id));QVERIFY(f.coordinator->summary(id).hasPreview());
        QVERIFY(f.coordinator->save(id));QVERIFY(!f.coordinator->save(id));QTRY_COMPARE(finished.size(),1);
        QCOMPARE(f.state(id),State::Saved);QVERIFY(f.bytes().contains("relaxer-bilingual"));QCOMPARE(f.http.requestCount(),1);
        qInfo()<<"B7 AI dispatch count"<<f.http.requestCount();
    }
    void queueAndCancelBeforeDispatch(){
        Fixture f;QVERIFY(f.ok);const auto a=f.web(),b=f.web(f.b.id);QVERIFY(f.coordinator->start(a));QVERIFY(f.coordinator->start(b));
        QTRY_COMPARE(f.browser->calls.size(),1);QCOMPARE(f.state(b),State::Queued);
        QVERIFY(f.coordinator->cancel(b));f.browser->answer();QTRY_COMPARE(f.browser->calls.size(),2);
        f.browser->answer();QTRY_COMPARE(f.state(a),State::CompletedPendingSave);
        QCOMPARE(f.state(b),State::Cancelled);QCOMPARE(f.browser->calls.size(),2);
    }
    void browserQueueRunsInOrder(){
        Fixture f;QVERIFY(f.ok);const auto a=f.web(),b=f.web(f.b.id);f.coordinator->start(a);f.coordinator->start(b);
        QTRY_COMPARE(f.browser->calls.size(),1);f.browser->answer();QTRY_COMPARE(f.browser->calls.size(),2);
        f.browser->answer();QTRY_COMPARE(f.browser->calls.size(),3);QCOMPARE(f.state(a),State::CompletedPendingSave);
        QCOMPARE(f.state(b),State::Running);f.browser->answer();QTRY_COMPARE(f.browser->calls.size(),4);
        f.browser->answer();QTRY_COMPARE(f.state(b),State::CompletedPendingSave);
    }
    void browserAttentionResumeAndRepeat(){
        Fixture f;QVERIFY(f.ok);const auto id=f.web();f.coordinator->start(id);QTRY_COMPARE(f.browser->calls.size(),1);
        f.browser->wait();QTRY_COMPARE(f.state(id),State::NeedsAttention);QVERIFY(f.coordinator->resume(id));
        QVERIFY(!f.coordinator->resume(id));QCOMPARE(f.browser->resumes,1);f.browser->wait();QTRY_COMPARE(f.state(id),State::NeedsAttention);
        QVERIFY(f.coordinator->resume(id));QCOMPARE(f.browser->resumes,2);f.browser->answer();QTRY_COMPARE(f.browser->calls.size(),2);
        f.browser->answer();QTRY_COMPARE(f.state(id),State::CompletedPendingSave);QVERIFY(f.coordinator->save(id));
    }
    void browserCancelAndLateResume(){
        Fixture f;QVERIFY(f.ok);QSignalSpy done(f.coordinator.get(),&TranslationCoordinator::finished);
        const auto id=f.web();f.coordinator->start(id);QTRY_COMPARE(f.browser->calls.size(),1);
        const auto token=f.browser->calls.last().token;f.browser->wait();QTRY_COMPARE(f.state(id),State::NeedsAttention);
        QVERIFY(f.coordinator->cancel(id));emit f.browser->resumed(token);f.browser->answer();
        QTRY_COMPARE(done.size(),1);QCOMPARE(f.state(id),State::Cancelled);QVERIFY(!f.coordinator->resume(id));QCOMPARE(f.bytes(),f.source);
    }
    void cancellationOrder_data(){QTest::addColumn<bool>("completionFirst");QTest::newRow("complete-first")<<true;QTest::newRow("cancel-first")<<false;}
    void cancellationOrder(){
        QFETCH(bool,completionFirst);Fixture f;QVERIFY(f.ok);QSignalSpy done(f.coordinator.get(),&TranslationCoordinator::finished);
        f.http.setHold(true);f.response();const auto id=f.ai();f.coordinator->start(id);QTRY_COMPARE(f.http.requestCount(),1);
        if(completionFirst){QVERIFY(f.http.releaseHeld());QTRY_COMPARE(f.state(id),State::CompletedPendingSave);QVERIFY(!f.coordinator->cancel(id));QVERIFY(f.coordinator->discard(id));}
        else {QVERIFY(f.coordinator->cancel(id));f.http.releaseHeld();}
        QTRY_COMPARE(done.size(),1);QVERIFY(!f.coordinator->cancel(id));QCOMPARE(f.state(id),State::Cancelled);
        QCOMPARE(f.bytes(),f.source);QCOMPARE(f.http.requestCount(),1);
    }
    void oldBrowserMessagesCannotCompleteNewTask(){
        Fixture f;QVERIFY(f.ok);const auto a=f.web();f.coordinator->start(a);QTRY_COMPARE(f.browser->calls.size(),1);
        const auto token=f.browser->calls.last().token;f.coordinator->cancel(a);const auto b=f.web(f.b.id);f.coordinator->start(b);
        QTRY_COMPARE(f.browser->calls.size(),2);emit f.browser->completed(token,"wrong",{});emit f.browser->attention(token,"wrong");
        f.browser->answer();QTRY_COMPARE(f.browser->calls.size(),3);QCOMPARE(f.coordinator->summary(b).completedLines,1);
        f.browser->answer();QTRY_COMPARE(f.state(b),State::CompletedPendingSave);QCOMPARE(f.coordinator->summary(b).trackId,f.b.id);
    }
    void browserFailures_data(){
        QTest::addColumn<QString>("text");QTest::addColumn<QString>("error");
        QTest::newRow("closed")<<QString()<<QString("browser closed");QTest::newRow("disconnect")<<QString()<<QString("transport disconnected");
        QTest::newRow("timeout")<<QString()<<QString("transport timed out");QTest::newRow("partial-empty")<<QString()<<QString();
        QTest::newRow("timestamp")<<QString("[00:01.00]bad")<<QString();QTest::newRow("multiline")<<QString("bad\nline")<<QString();
    }
    void browserFailures(){
        QFETCH(QString,text);QFETCH(QString,error);Fixture f;QVERIFY(f.ok);const auto id=f.web();f.coordinator->start(id);
        QTRY_COMPARE(f.browser->calls.size(),1);f.browser->answer();QTRY_COMPARE(f.browser->calls.size(),2);f.browser->answer(text,error);
        QTRY_COMPARE(f.state(id),State::Failed);QVERIFY(!f.coordinator->save(id));QCOMPARE(f.bytes(),f.source);
    }
    void duplicateBrowserCompletionAndFloodAreBounded(){
        Fixture f;QVERIFY(f.ok);QSignalSpy changed(f.coordinator.get(),&TranslationCoordinator::taskChanged);
        const auto id=f.web();f.coordinator->start(id);QTRY_COMPARE(f.browser->calls.size(),1);
        const auto token=f.browser->calls.last().token;
        for(int i=0;i<10000;++i)emit f.browser->attention(token,"attention");
        QTRY_COMPARE(f.state(id),State::NeedsAttention);f.coordinator->resume(id);
        for(int i=0;i<10000;++i)emit f.browser->completed(token,QStringLiteral("译文"),{});
        QTRY_COMPARE(f.browser->calls.size(),2);QCOMPARE(f.coordinator->summary(id).completedLines,1);
        f.browser->answer();QTRY_COMPARE(f.state(id),State::CompletedPendingSave);QVERIFY(changed.size()<15);
        qInfo()<<"B7 20000 transport events, business notifications"<<changed.size();
    }
    void snapshotProtection_data(){
        QTest::addColumn<QString>("change");QTest::addColumn<bool>("afterResult");
        for(const auto &change:{"bytes","deleted","hash","title","artist","path","configuration"})
            for(bool after:{false,true})QTest::newRow((QByteArray(change)+(after?"-save":"-response")).constData())<<QString(change)<<after;
    }
    void snapshotProtection(){
        QFETCH(QString,change);QFETCH(bool,afterResult);Fixture f;QVERIFY(f.ok);f.http.setHold(true);f.response();
        const auto id=f.ai();f.coordinator->start(id);QTRY_COMPARE(f.http.requestCount(),1);
        if(afterResult){QVERIFY(f.http.releaseHeld());QTRY_COMPARE(f.state(id),State::CompletedPendingSave);}
        const auto path=f.library.absolute(f.a.lyrics);
        if(change=="bytes"){QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write("[00:01.00]OTHER world\n[00:02.00]Walk home\n"),qint64(f.source.size()));}
        else if(change=="deleted")QVERIFY(QFile::remove(path));
        else if(change=="configuration"){auto config=f.runtime->configuration();config.translationStylePrompt+=" changed";QVERIFY(f.runtime->saveConfiguration(config));}
        else {QSqlQuery query(import_fixture::database(f.library));QVERIFY(query.exec("UPDATE tracks SET "+(change=="path"?QString("lyrics"):change)+"='changed' WHERE id="+QString::number(f.a.id)));}
        QByteArray before;readFile(path,&before);
        if(afterResult)QVERIFY(!f.coordinator->save(id));else QVERIFY(f.http.releaseHeld());
        QTRY_COMPARE(f.state(id),State::Invalidated);QVERIFY(!f.coordinator->save(id));
        QByteArray after;readFile(path,&after);QCOMPARE(after,before);QCOMPARE(f.http.requestCount(),1);
    }
    void consentSnapshotChangesNeverDispatch(){
        Fixture f;QVERIFY(f.ok);const auto id=f.ai();QVERIFY(f.library.saveLyrics(f.a.id,"new source"));
        QVERIFY(!f.coordinator->start(id));QCOMPARE(f.http.requestCount(),0);QCOMPARE(f.state(id),State::Invalidated);
    }
    void saveSqlFailureRetainsPaidPreviewAndRetry(){
        Fixture f;QVERIFY(f.ok);f.response();const auto id=f.ai();f.coordinator->start(id);QTRY_COMPARE(f.state(id),State::CompletedPendingSave);
        QSqlQuery query(import_fixture::database(f.library));QVERIFY(query.exec("CREATE TRIGGER b7_fail BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT RAISE(ABORT,'B7 SQL failure'); END"));
        QString error;QVERIFY(!f.coordinator->save(id,&error));QVERIFY(error.contains("B7 SQL failure"));QCOMPARE(f.bytes(),f.source);
        QCOMPARE(f.state(id),State::CompletedPendingSave);QVERIFY(query.exec("DROP TRIGGER b7_fail"));QVERIFY(f.coordinator->save(id));
        QCOMPARE(f.http.requestCount(),1);QVERIFY(f.bytes().contains("relaxer-bilingual"));
    }
    void requestTimeoutAndLateSuccessAreSingleTerminal(){
        Fixture f;QVERIFY(f.ok);f.runtime->requests()->setTimeoutMs(15);f.http.setHold(true);f.response();
        QSignalSpy done(f.coordinator.get(),&TranslationCoordinator::finished);const auto id=f.ai();f.coordinator->start(id);
        QTRY_COMPARE(f.state(id),State::Failed);QTRY_COMPARE(done.size(),1);f.http.releaseHeld();
        QCoreApplication::sendPostedEvents();QCOMPARE(done.size(),1);QVERIFY(!f.coordinator->save(id));QCOMPARE(f.bytes(),f.source);
    }
    void duplicateAiNotificationDoesNotCreateAnotherPreview(){
        Fixture f;QVERIFY(f.ok);f.response();QSignalSpy requests(f.runtime->requests(),&AiRequestCoordinator::completionFinished);
        const auto id=f.ai();f.coordinator->start(id);QTRY_COMPARE(f.state(id),State::CompletedPendingSave);QCOMPARE(requests.size(),1);
        const auto request=requests.first().first().toULongLong();auto result=qvariant_cast<ChatCompletionResult>(requests.first().at(2));
        emit f.runtime->requests()->completionFinished(request,"deepseek",result);QVERIFY(f.coordinator->save(id));
        emit f.runtime->requests()->completionFinished(request,"deepseek",result);QCOMPARE(f.state(id),State::Saved);QCOMPARE(f.http.requestCount(),1);
    }
    void runtimeDestructionInvalidatesWithoutSaving(){
        Fixture f;QVERIFY(f.ok);f.http.setHold(true);const auto id=f.ai();f.coordinator->start(id);QTRY_COMPARE(f.http.requestCount(),1);
        f.runtime.reset();QTRY_COMPARE(f.state(id),State::Invalidated);QVERIFY(!f.coordinator->save(id));QCOMPARE(f.bytes(),f.source);
    }
    void shutdownAndDestructionWithPendingCallbacks(){
        Fixture f;QVERIFY(f.ok);const auto id=f.web();f.coordinator->start(id);QTRY_COMPARE(f.browser->calls.size(),1);
        f.browser->wait();QPointer<TranslationCoordinator> guard=f.coordinator.get();f.coordinator->shutdown();f.coordinator->shutdown();
        QCOMPARE(f.browser->closes,1);QCOMPARE(f.state(id),State::Cancelled);f.browser->answer();f.coordinator.reset();QVERIFY(!guard);
        QCoreApplication::sendPostedEvents();QCOMPARE(f.bytes(),f.source);
    }
    void ownerThreadRejectsBeforeDatabaseAccess(){
        Fixture f;QVERIFY(f.ok);quint64 id=1;QString error;QSemaphore done;
        auto *worker=QThread::create([&]{id=f.coordinator->prepareAi(f.a.id,&error);done.release();});worker->start();
        QVERIFY(done.tryAcquire(1,3000));QVERIFY(worker->wait(3000));delete worker;QCOMPARE(id,quint64(0));QVERIFY(!error.isEmpty());
    }
    void boundedStorageAndResourceRelease(){
        Fixture f;QVERIFY(f.ok);QList<quint64> ids;
        for(int i=0;i<32;++i){const auto id=f.web();QVERIFY(id);ids<<id;}
        QVERIFY(!f.web());for(const auto id:ids){QVERIFY(f.coordinator->cancel(id));QVERIFY(f.coordinator->forget(id));}
        QVERIFY(f.web());QVERIFY(f.browser->calls.isEmpty());
    }
    void twoWindowsReverseAiCompletionStayWithTheirSongs_data(){
        QTest::addColumn<bool>("secondArrivesFirst");
        QTest::newRow("first-window-arrives-first")<<false;
        QTest::newRow("second-window-arrives-first")<<true;
    }
    void twoWindowsReverseAiCompletionStayWithTheirSongs(){
        QFETCH(bool,secondArrivesFirst);
        Fixture f;QVERIFY(f.ok);f.http.setHold(true);f.response();f.response();
        TranslationCoordinator second(&f.library,f.runtime.get());
        const auto a=f.ai(),b=second.prepareAi(f.b.id);QVERIFY(a!=b);
        if(secondArrivesFirst){
            QVERIFY(second.start(b));QTRY_COMPARE(f.http.heldCount(),1);
            QVERIFY(f.coordinator->start(a));
        }else{
            QVERIFY(f.coordinator->start(a));QTRY_COMPARE(f.http.heldCount(),1);
            QVERIFY(second.start(b));
        }
        QTRY_COMPARE(f.http.heldCount(),2);
        // Independent HTTP connections may arrive in either order. Release B's
        // held response, not an assumed second arrival. The barriers above make
        // both arrival orders deterministic without changing completion order.
        QVERIFY(f.http.releaseHeldAt(secondArrivesFirst?0:1));QTRY_COMPARE(second.summary(b).state,State::CompletedPendingSave);
        QCOMPARE(f.state(a),State::Running);QCOMPARE(second.summary(b).trackId,f.b.id);
        QVERIFY(second.save(b));QCOMPARE(f.bytes(f.a.id),f.source);QVERIFY(f.bytes(f.b.id).contains("relaxer-bilingual"));
        QVERIFY(f.http.releaseHeld());QTRY_COMPARE(f.state(a),State::CompletedPendingSave);
        QVERIFY(f.coordinator->save(a));QCOMPARE(f.http.requestCount(),2);
    }
    void requestDisconnectedDoesNotRetry(){
        Fixture f;QVERIFY(f.ok);f.http.setHold(true);const auto id=f.ai();f.coordinator->start(id);
        QTRY_COMPARE(f.http.heldCount(),1);f.http.disconnectHeld();QTRY_COMPARE(f.state(id),State::Failed);
        QCOMPARE(f.http.requestCount(),1);QCOMPARE(f.bytes(),f.source);QVERIFY(!f.coordinator->save(id));
    }
    void browserSourceReplacedWhileWaitingCannotResume(){
        Fixture f;QVERIFY(f.ok);const auto id=f.web();f.coordinator->start(id);QTRY_COMPARE(f.browser->calls.size(),1);
        f.browser->wait();QTRY_COMPARE(f.state(id),State::NeedsAttention);QVERIFY(f.library.saveLyrics(f.a.id,"Changed source"));
        QVERIFY(!f.coordinator->resume(id));QCOMPARE(f.state(id),State::Invalidated);QCOMPARE(f.browser->resumes,0);
    }
    void browserSourceReplacedBeforeCompletionCannotSave(){
        Fixture f;QVERIFY(f.ok);const auto id=f.web();f.coordinator->start(id);QTRY_COMPARE(f.browser->calls.size(),1);
        f.browser->answer();QTRY_COMPARE(f.browser->calls.size(),2);QVERIFY(f.library.saveLyrics(f.a.id,"Changed source"));
        f.browser->answer();QTRY_COMPARE(f.state(id),State::Invalidated);QVERIFY(!f.coordinator->save(id));QCOMPARE(f.bytes(),QByteArray("Changed source"));
    }
    void destructionCancelsAiAndDropsQueuedDelivery(){
        Fixture f;QVERIFY(f.ok);f.http.setHold(true);f.response();const auto id=f.ai();f.coordinator->start(id);
        QTRY_COMPARE(f.http.heldCount(),1);QSignalSpy requestDone(f.runtime->requests(),&AiRequestCoordinator::completionFinished);
        QPointer<TranslationCoordinator> guard=f.coordinator.get();f.coordinator.reset();QVERIFY(!guard);
        QTRY_COMPARE(requestDone.size(),1);const auto result=qvariant_cast<ChatCompletionResult>(requestDone.first().at(2));
        QVERIFY(result.error.cancelled);f.http.releaseHeld();QCoreApplication::sendPostedEvents();QCOMPARE(f.bytes(),f.source);
    }
    void completedPreviewShutdownIsExplicitDiscard(){
        Fixture f;QVERIFY(f.ok);f.response();const auto id=f.ai();f.coordinator->start(id);QTRY_COMPARE(f.state(id),State::CompletedPendingSave);
        f.coordinator->shutdown();QCOMPARE(f.state(id),State::Cancelled);QCOMPARE(f.bytes(),f.source);QCOMPARE(f.http.requestCount(),1);
    }
};
QTEST_GUILESS_MAIN(TranslationCoordinatorTests)
#include "translation-coordinator-tests.moc"
