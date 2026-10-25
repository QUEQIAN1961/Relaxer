#include "lyrics-replacement-fixture.h"
#include "translation-fixture.h"
#include "import-fixture.h"
#include "lyricslookup.h"
#include "ui-bridge/lyrics-search-bridge.h"
#include "ui-bridge/qml-shell.h"
#include "lyrics-document-reader.h"
#include <QApplication>
#include <QMessageBox>
#include <QSignalSpy>
#include <QQuickWindow>
#include <QQuickItem>

using namespace relaxer;
using namespace replacement_fixture;
namespace relaxer {
class LyricsLookupDiagnostics {
public:
    static void choose(LyricsLookup &d,qint64 id){
        d.m_missingOnly->setChecked(false);d.m_missingTranslationOnly->setChecked(false);d.m_incomplete->setChecked(false);
        d.updateFilter();d.m_tracks->setCurrentRow(d.rowForTrack(id));d.m_tracks->currentItem()->setSelected(true);d.selectTrack();
    }
    static bool manual(LyricsLookup &d,const QByteArray &b){return d.saveManual(b,"已保存测试");}
    static qint64 target(LyricsLookup &d){return d.trackIdAt(d.m_tracks->currentRow());}
    static void cached(LyricsLookup &d,qint64 id){d.m_search->loadCached({id});d.rebuildCandidates();}
    static bool selected(LyricsLookup &d){return d.applySelected();}
    static QString identity(LyricsLookup &d){return d.selectedReplacementIdentity();}
    static TranslationCoordinator *translation(LyricsLookup &d){return d.m_translation;}
    static void attachTask(LyricsLookup &d,quint64 task){d.m_translationTaskId=task;}
    static void batchInstrumental(LyricsLookup &d){d.setMultiSelect(true);d.m_tracks->selectAll();d.markInstrumental();}
    static QByteArray draft(LyricsLookup &d,qint64 id){return d.m_manualDrafts.value(id);}
    static void paste(LyricsLookup &d){d.pasteLyrics();}
};
}
namespace {
QByteArray bytes(Library &l,qint64 id){return LyricsDocumentReader::readFile(l.absolute(l.track(id).lyrics)).bytes;}
bool write(const QString &path,const QByteArray &b){QFile f(path);return f.open(QIODevice::WriteOnly|QIODevice::Truncate)&&f.write(b)==b.size();}
QJsonObject record(const Track &t,const QByteArray &b){return {{"id",123},{"trackName",t.title},{"artistName",t.artist},{"source","Fixture"},{"syncedLyrics",QString::fromUtf8(b)}};}
struct ModalAnswer {
    QTimer timer;int count=0;int answer=QMessageBox::Yes;std::function<void()> during;
    ModalAnswer(){timer.setInterval(1);QObject::connect(&timer,&QTimer::timeout,[this]{
        if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())){
            if(box->objectName()!="lyricsReplaceConfirm"&&box->windowTitle()!=QStringLiteral("批量标记纯音乐"))return;
            ++count;if(during){auto f=std::move(during);during={};f();}
            box->button(static_cast<QMessageBox::StandardButton>(answer))->click();
        }
    });timer.start();}
};
QQuickItem *visual(QQuickItem *root,const QString &name){
    if(root->objectName()==name)return root;
    for(auto *child:root->childItems())if(auto *found=visual(child,name))return found;
    return nullptr;
}
class StaticReply final:public QNetworkReply {
public:
    StaticReply(const QNetworkRequest &r,QObject *parent,QByteArray body):QNetworkReply(parent),m_body(std::move(body)){
        setRequest(r);setUrl(r.url());open(QIODevice::ReadOnly);QTimer::singleShot(0,this,[this]{if(!isFinished()){setAttribute(QNetworkRequest::HttpStatusCodeAttribute,200);setFinished(true);emit readyRead();emit finished();}});
    }
    void abort()override{if(!isFinished()){setError(OperationCanceledError,"cancelled");setFinished(true);emit finished();}}
    qint64 bytesAvailable()const override{return m_body.size()-m_offset+QNetworkReply::bytesAvailable();}
protected:
    qint64 readData(char *out,qint64 size)override{auto n=qMin(size,m_body.size()-m_offset);if(n<=0)return -1;memcpy(out,m_body.constData()+m_offset,size_t(n));m_offset+=n;return n;}
private:QByteArray m_body;qint64 m_offset=0;
};
class StaticNetwork final:public QNetworkAccessManager {
public:QByteArray body;
protected:QNetworkReply *createRequest(Operation,const QNetworkRequest &r,QIODevice *)override{return new StaticReply(r,this,body);}
};
}
class LyricsReplacementIntegrationTests final:public QObject {
    Q_OBJECT
private slots:
    void frontendParity_data(){
        QTest::addColumn<int>("scenario");QTest::addColumn<bool>("qml");QTest::addColumn<QString>("entry");QTest::addColumn<bool>("accept");
        const auto fixtures=cases();
        for(int i=0;i<fixtures.size();++i)for(bool qml:{false,true})for(const auto &entry:{QString("text"),QString("import"),QString("selected")})
            for(bool accept:{false,true})QTest::newRow(qPrintable(QString("%1-%2-%3-%4").arg(fixtures[i].name).arg(qml?"qml":"widgets").arg(entry).arg(accept?"replace":"cancel")))<<i<<qml<<entry<<accept;
    }
    void frontendParity(){
        QFETCH(int,scenario);QFETCH(bool,qml);QFETCH(QString,entry);QFETCH(bool,accept);
        const auto fixture=cases()[scenario];QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());
        const auto a=seed(l,"First","parity-a"),b=seed(l,"Second","parity-b");
        if(!fixture.old.isEmpty())QVERIFY(l.saveLyricsPreservingText(a.id,fixture.old));
        const auto before=l.track(a.id);const auto initial=l.lyricsSaveSnapshot(a.id);
        QVERIFY(l.saveLookupCache(a.id,QJsonArray{record(a,fixture.candidate)},false));
        const auto file=dir.filePath("import.lrc");QVERIFY(write(file,fixture.candidate));
        const bool expectedSuccess=!fixture.confirmation||accept;
        if(qml){
            PlaybackFacade player(&l,nullptr,true);LyricsSearchCoordinator search(&l);LyricsSearchBridge bridge(&l,&player,&search);
            QVERIFY(bridge.setTargetTracks({QString::number(a.id),QString::number(b.id)},false,true));
            QSignalSpy request(&bridge,&LyricsSearchBridge::replacementRequested),saved(&bridge,&LyricsSearchBridge::saved);
            bool ok=entry=="selected"?bridge.saveSelected(bridge.selectedKey(),true):entry=="import"?bridge.importLocalLyrics(QUrl::fromLocalFile(file),true):bridge.saveText(QString::fromUtf8(fixture.candidate),true);
            QCOMPARE(request.size(),fixture.confirmation?1:0);
            if(fixture.confirmation){QVERIFY(!ok);QCOMPARE(bytes(l,a.id),fixture.old);QCOMPARE(l.track(a.id).lyrics,before.lyrics);QCOMPARE(saved.size(),0);
                ok=bridge.resolveReplacement(request.first()[0].toString(),accept?1:0);}
            QCOMPARE(ok,expectedSuccess);QCOMPARE(saved.size(),expectedSuccess?1:0);
            QCOMPARE(bridge.targetTrackKey(),QString::number(a.id)); // UI advances only on saved.
            if(expectedSuccess)QVERIFY(!bridge.saveSelected(bridge.selectedKey(),true));
            else QVERIFY(!bridge.previewText().isEmpty());
            search.shutdown();QVERIFY(player.shutdown());
        }else{
            LyricsLookup dialog(&l,{a.id,b.id},nullptr,QUrl("http://127.0.0.1:9/api"),{},true);
            LyricsLookupDiagnostics::choose(dialog,a.id);LyricsLookupDiagnostics::cached(dialog,a.id);
            ModalAnswer answer;answer.answer=accept?QMessageBox::Yes:QMessageBox::Cancel;
            const bool ok=entry=="selected"?LyricsLookupDiagnostics::selected(dialog):entry=="import"?dialog.importLyricsFile(file):LyricsLookupDiagnostics::manual(dialog,fixture.candidate);
            QCOMPARE(ok,expectedSuccess);QCOMPARE(answer.count,fixture.confirmation?1:0);
            QCOMPARE(LyricsLookupDiagnostics::target(dialog),expectedSuccess?b.id:a.id);
        }
        QCOMPARE(bytes(l,a.id),expectedSuccess?fixture.expected:fixture.old);
        QCOMPARE(l.track(a.id).lyrics,expectedSuccess?QString("Lyrics/track-%1.lrc").arg(a.id):before.lyrics);
        QCOMPARE(l.isInstrumental(a.id),expectedSuccess&&scenario==5);
        QVERIFY(l.track(b.id).lyrics.isEmpty());
        if(!expectedSuccess)QVERIFY(l.lyricsSnapshotMatches(initial));
    }
    void staleAndFailure_data(){
        QTest::addColumn<bool>("qml");QTest::addColumn<QString>("failure");
        for(bool qml:{false,true})for(const auto &failure:{QString("external"),QString("deleted"),QString("database"),QString("target"),QString("revision"),QString("hash"),QString("path")})
            QTest::newRow(qPrintable(QString("%1-%2").arg(qml?"qml":"widgets",failure)))<<qml<<failure;
    }
    void staleAndFailure(){
        QFETCH(bool,qml);QFETCH(QString,failure);QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());
        const auto a=seed(l,"A","stale-a"),b=seed(l,"B","stale-b");QVERIFY(l.saveLyricsPreservingText(a.id,bilingual()));
        auto candidate=mono();candidate.replace("Hello world","Changed original");
        QVERIFY(l.saveLookupCache(a.id,QJsonArray{record(a,candidate)},false));
        const auto path=l.absolute(l.track(a.id).lyrics);const auto before=l.track(a.id);
        if(failure=="database"){QSqlQuery query(import_fixture::database(l));QVERIFY(query.exec("CREATE TRIGGER replacement_fail BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT RAISE(FAIL,'D2-01 injected database failure'); END"));}
        auto mutate=[&]{if(failure=="external")QVERIFY(write(path,"External update"));else if(failure=="deleted")QVERIFY(QFile::remove(path));
            else if(failure=="hash"||failure=="path"){QSqlQuery q(import_fixture::database(l));QVERIFY(q.exec(QString("UPDATE tracks SET %1='Changed identity' WHERE id=%2").arg(failure=="hash"?"hash":"file").arg(a.id)));}};
        if(qml){
            PlaybackFacade player(&l,nullptr,true);LyricsSearchCoordinator search(&l);LyricsSearchBridge bridge(&l,&player,&search);
            QVERIFY(bridge.setTargetTrack(QString::number(a.id)));QSignalSpy request(&bridge,&LyricsSearchBridge::replacementRequested),saved(&bridge,&LyricsSearchBridge::saved);
            QVERIFY(!bridge.saveSelected(bridge.selectedKey(),true));QCOMPARE(request.size(),1);const auto token=request.first()[0].toString();mutate();
            if(failure=="target")QVERIFY(bridge.setTargetTrack(QString::number(b.id)));
            if(failure=="revision"){candidate.append("[00:03.00]New row");QVERIFY(l.saveLookupCache(a.id,QJsonArray{record(a,candidate)},false));search.loadCached({a.id});}
            QVERIFY(!bridge.resolveReplacement(token,1));QCOMPARE(saved.size(),0);QVERIFY(!bridge.resolveReplacement(token,1));
            if(failure!="target")QVERIFY(!bridge.previewText().isEmpty());
            search.shutdown();QVERIFY(player.shutdown());
        }else{
            LyricsLookup dialog(&l,{a.id,b.id},nullptr,QUrl("http://127.0.0.1:9/api"),{},true);
            LyricsLookupDiagnostics::choose(dialog,a.id);LyricsLookupDiagnostics::cached(dialog,a.id);
            ModalAnswer answer;answer.during=[&]{mutate();if(failure=="target")LyricsLookupDiagnostics::choose(dialog,b.id);
                if(failure=="revision"){candidate.append("[00:03.00]New row");QVERIFY(l.saveLookupCache(a.id,QJsonArray{record(a,candidate)},false));LyricsLookupDiagnostics::cached(dialog,a.id);}};
            QVERIFY(!LyricsLookupDiagnostics::selected(dialog));QCOMPARE(answer.count,1);
            QCOMPARE(LyricsLookupDiagnostics::target(dialog),failure=="target"?b.id:a.id);
        }
        QCOMPARE(l.track(a.id).lyrics,before.lyrics);QVERIFY(!l.isInstrumental(a.id));QVERIFY(l.track(b.id).lyrics.isEmpty());
        QCOMPARE(bytes(l,a.id),failure=="external"?QByteArray("External update"):failure=="deleted"?QByteArray():bilingual());
    }
    void planIdentityAndRepeatedSubmission(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"A","identity");
        auto plan=LyricsReplacementService::prepare(l,l.lyricsSaveSnapshot(a.id),mono(),"revision:1");QVERIFY(plan.decision.valid);
        QString error;QVERIFY(!LyricsReplacementService::commit(l,plan,LyricsReplacementChoice::SafeSave,a.id,"revision:2",nullptr,&error));QVERIFY(!error.isEmpty());
        QVERIFY(l.track(a.id).lyrics.isEmpty());QVERIFY(LyricsReplacementService::commit(l,plan,LyricsReplacementChoice::SafeSave,a.id,"revision:1",nullptr));
        QVERIFY(!LyricsReplacementService::commit(l,plan,LyricsReplacementChoice::Replace,a.id,"revision:1",nullptr));QCOMPARE(bytes(l,a.id),mono());
    }
    void localEditorSnapshotsAndLateCallbacks(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"A","edit-a"),b=seed(l,"B","edit-b");
        QVERIFY(l.saveLyricsPreservingText(a.id,bilingual()));QVERIFY(l.saveLookupCache(a.id,QJsonArray{record(a,mono())},false));
        PlaybackFacade player(&l,nullptr,true);LyricsSearchCoordinator search(&l);LyricsSearchBridge bridge(&l,&player,&search);
        QVERIFY(bridge.setTargetTrack(QString::number(a.id)));const auto edit=bridge.beginLocalEdit();QVERIFY(!edit.isEmpty());
        const auto old=search.candidates(a.id);auto newer=mono();newer.replace("Hello world","Newer original");QVERIFY(l.saveLookupCache(a.id,QJsonArray{record(a,newer)},false));search.loadCached({a.id});
        const auto latest=search.candidates(a.id);search.candidatesChanged(latest,search.summary().generation);const auto key=bridge.selectedKey();QVERIFY(!key.isEmpty());
        search.candidatesChanged(old,search.summary().generation);QCOMPARE(bridge.selectedKey(),key);
        search.candidatesChanged(old,search.summary().generation+1);QCOMPARE(bridge.selectedKey(),key);
        QVERIFY(write(l.absolute(l.track(a.id).lyrics),"External editor change"));QSignalSpy saved(&bridge,&LyricsSearchBridge::saved);
        QVERIFY(!bridge.saveText(QString::fromUtf8(mono()),true,edit));QCOMPARE(saved.size(),0);QCOMPARE(bytes(l,a.id),QByteArray("External editor change"));
        const auto next=bridge.beginLocalEdit();QVERIFY(bridge.setTargetTrack(QString::number(b.id)));QVERIFY(!bridge.saveText(QString::fromUtf8(mono()),true,next));QVERIFY(l.track(b.id).lyrics.isEmpty());
        search.shutdown();QVERIFY(player.shutdown());
    }
    void d1RegressionBeforeAndAfter(){
        QTemporaryDir dir;Library oldPath(dir.filePath("before")),newPath(dir.filePath("after"));QVERIFY(oldPath.open());QVERIFY(newPath.open());
        const auto before=seed(oldPath,"D1","d1-before"),after=seed(newPath,"D1","d1-after");
        QVERIFY(oldPath.saveLyricsPreservingText(before.id,bilingual()));QVERIFY(newPath.saveLyricsPreservingText(after.id,bilingual()));
        // Reproduce the former bridge's exact raw persistence call in isolation.
        QVERIFY(oldPath.saveLyricsChecked(before.id,mono(),oldPath.lyricsSaveSnapshot(before.id)));
        QVERIFY(!bytes(oldPath,before.id).contains("relaxer-bilingual"));QVERIFY(!bytes(oldPath,before.id).contains(QStringLiteral("世界你好").toUtf8()));
        auto plan=LyricsReplacementService::prepare(newPath,newPath.lyricsSaveSnapshot(after.id),mono(),"d1");QVERIFY(plan.decision.preservesTranslation);QVERIFY(!plan.decision.confirmationRequired);
        QVERIFY(LyricsReplacementService::commit(newPath,plan,LyricsReplacementChoice::SafeSave,after.id,"d1",nullptr));QCOMPARE(bytes(newPath,after.id),merged());
        qInfo()<<"D1 equivalent replacement synthetic evidence: saved="<<bilingual().size()<<"former raw path="<<bytes(oldPath,before.id).size()<<"shared policy="<<bytes(newPath,after.id).size();
    }
    void manualDraftsRemainRecoverable(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"A","draft-a"),b=seed(l,"B","draft-b");
        QVERIFY(l.saveLyricsPreservingText(a.id,bilingual()));QVERIFY(l.saveLookupCache(a.id,QJsonArray{record(a,mono())},false));
        const QByteArray draft="[00:01.00]Unsubmitted original\n[00:02.00]Draft second line";
        {
            PlaybackFacade player(&l,nullptr,true);LyricsSearchCoordinator search(&l);LyricsSearchBridge bridge(&l,&player,&search);
            QVERIFY(bridge.setTargetTrack(QString::number(a.id)));QSignalSpy request(&bridge,&LyricsSearchBridge::replacementRequested);
            QVERIFY(!bridge.saveText(QString::fromUtf8(draft),true));bridge.cancelReplacement(request.last()[0].toString());
            QCOMPARE(bridge.draftText(),QString::fromUtf8(draft));QCOMPARE(bridge.previewText(),QString::fromUtf8(draft));QCOMPARE(bytes(l,a.id),bilingual());
            QVERIFY(bridge.setTargetTrack(QString::number(b.id)));QVERIFY(bridge.setTargetTrack(QString::number(a.id)));QCOMPARE(bridge.previewText(),QString::fromUtf8(draft));
            // Saving a chosen search record must never accidentally save the
            // retained manual preview under that record's identity.
            QVERIFY(bridge.saveSelected(bridge.selectedKey(),true));QCOMPARE(bytes(l,a.id),merged());QVERIFY(bridge.draftText().isEmpty());search.shutdown();QVERIFY(player.shutdown());
        }
        {
            LyricsLookup dialog(&l,{a.id},nullptr,QUrl("http://127.0.0.1:9/api"),{},true);LyricsLookupDiagnostics::choose(dialog,a.id);
            ModalAnswer answer;answer.answer=QMessageBox::Cancel;QVERIFY(!LyricsLookupDiagnostics::manual(dialog,draft));QCOMPARE(LyricsLookupDiagnostics::draft(dialog,a.id),draft);
            QTimer editor;editor.setInterval(1);bool restored=false;connect(&editor,&QTimer::timeout,[&]{auto *box=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!box||box->objectName()!="lyricsPasteDialog")return;
                restored=box->findChild<QPlainTextEdit *>("lyricsPasteText")->toPlainText()==QString::fromUtf8(draft);box->reject();});editor.start();
            LyricsLookupDiagnostics::paste(dialog);QVERIFY(restored);QCOMPARE(bytes(l,a.id),merged());QCOMPARE(LyricsLookupDiagnostics::target(dialog),a.id);
        }
    }
    void repeatedTextSaveCannotRepeatApproval(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"A","repeat-text");
        PlaybackFacade player(&l,nullptr,true);LyricsSearchCoordinator search(&l);LyricsSearchBridge bridge(&l,&player,&search);
        QVERIFY(bridge.setTargetTrack(QString::number(a.id)));QSignalSpy saved(&bridge,&LyricsSearchBridge::saved),request(&bridge,&LyricsSearchBridge::replacementRequested);
        QVERIFY(bridge.saveText(QString::fromUtf8(mono()),true));QVERIFY(!bridge.saveText(QString::fromUtf8(mono()),true));QCOMPARE(saved.size(),1);QVERIFY(bridge.draftText().isEmpty());
        QVERIFY(write(l.absolute(l.track(a.id).lyrics),"External repeat update"));QVERIFY(!bridge.saveText(QString::fromUtf8(mono()),true));QCOMPARE(request.size(),1);QCOMPARE(saved.size(),1);
        bridge.cancelReplacement(request.first()[0].toString());QCOMPARE(bytes(l,a.id),QByteArray("External repeat update"));QCOMPARE(bridge.draftText(),QString::fromUtf8(mono()));search.shutdown();QVERIFY(player.shutdown());
    }
    void writeFailure_data(){QTest::addColumn<bool>("qml");QTest::newRow("qml")<<true;QTest::newRow("widgets")<<false;}
    void writeFailure(){
        QFETCH(bool,qml);QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"A","write-failure");
        QVERIFY(QDir().rmdir(l.absolute("Lyrics")));QVERIFY(write(l.absolute("Lyrics"),"Blocked parent"));
        if(qml){PlaybackFacade player(&l,nullptr,true);LyricsSearchCoordinator search(&l);LyricsSearchBridge bridge(&l,&player,&search);
            QVERIFY(bridge.setTargetTrack(QString::number(a.id)));QSignalSpy saved(&bridge,&LyricsSearchBridge::saved);
            QVERIFY(!bridge.saveText(QString::fromUtf8(mono()),true));QVERIFY(!bridge.status().isEmpty());QCOMPARE(bridge.draftText(),QString::fromUtf8(mono()));QCOMPARE(saved.size(),0);QCOMPARE(bridge.targetTrackKey(),QString::number(a.id));search.shutdown();QVERIFY(player.shutdown());
        }else{LyricsLookup dialog(&l,{a.id},nullptr,QUrl("http://127.0.0.1:9/api"),{},true);LyricsLookupDiagnostics::choose(dialog,a.id);
            QVERIFY(!LyricsLookupDiagnostics::manual(dialog,mono()));QCOMPARE(LyricsLookupDiagnostics::draft(dialog,a.id),mono());QCOMPARE(LyricsLookupDiagnostics::target(dialog),a.id);}
        QVERIFY(l.track(a.id).lyrics.isEmpty());QVERIFY(!l.isInstrumental(a.id));QVERIFY(QFileInfo(l.absolute("Lyrics")).isFile());
    }
    void existingBatchAndOrphanDestinationStayProtected(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"A","auto-a"),b=seed(l,"B","auto-b");
        QVERIFY(l.saveLyricsPreservingText(a.id,bilingual()));const auto orphan=l.absolute(QString("Lyrics/track-%1.lrc").arg(b.id));QVERIFY(write(orphan,bilingual()));
        auto candidate=mono();candidate.replace("Hello world","Different words");StaticNetwork network;
        network.body=QJsonDocument(QJsonArray{record(a,candidate),record(b,candidate)}).toJson(QJsonDocument::Compact);
        LyricsSearchCoordinator search(&l,nullptr,QUrl("https://mock.invalid/api"),{},&network);
        LyricsSearchRequest one;one.original=l.track(a.id);one.match=one.original;one.replace=true;
        LyricsSearchRequest two;two.original=l.track(b.id);two.match=two.original;two.replace=true;
        QVERIFY(search.start({one,two}));QTRY_VERIFY_WITH_TIMEOUT(!search.summary().running,5000);
        QCOMPARE(search.summary().saved,0);QCOMPARE(bytes(l,a.id),bilingual());QCOMPARE(LyricsDocumentReader::readFile(orphan).bytes,bilingual());QVERIFY(l.track(b.id).lyrics.isEmpty());search.shutdown();
    }
    void batchInstrumentalCapturesSnapshotsBeforeApproval(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"A","batch-a"),b=seed(l,"B","batch-b");
        QVERIFY(l.saveLyricsPreservingText(a.id,bilingual()));QVERIFY(l.saveLyricsPreservingText(b.id,bilingual()));
        LyricsLookup dialog(&l,{a.id,b.id},nullptr,QUrl("http://127.0.0.1:9/api"),{},true);LyricsLookupDiagnostics::choose(dialog,a.id);
        ModalAnswer answer;answer.answer=QMessageBox::No;LyricsLookupDiagnostics::batchInstrumental(dialog);QCOMPARE(bytes(l,a.id),bilingual());QCOMPARE(bytes(l,b.id),bilingual());
        answer.answer=QMessageBox::Yes;answer.during=[&]{QVERIFY(write(l.absolute(l.track(a.id).lyrics),"External batch update"));};
        LyricsLookupDiagnostics::batchInstrumental(dialog);QCOMPARE(bytes(l,a.id),QByteArray("External batch update"));QVERIFY(!l.isInstrumental(a.id));QVERIFY(l.isInstrumental(b.id));QCOMPARE(bytes(l,b.id),QStringLiteral("纯音乐，请欣赏").toUtf8());
    }
    void pendingTranslation_data(){QTest::addColumn<bool>("qml");QTest::newRow("qml")<<true;QTest::newRow("widgets")<<false;}
    void pendingTranslation(){
        QFETCH(bool,qml);QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"A","preview");
        QVERIFY(l.saveLyrics(a.id,mono()));QVERIFY(l.setTranslationJudgment(a.id,TranslationJudgment::NeedsTranslation));
        TranslationHttpMock http;QVERIFY(http.listen());AiRuntime runtime(&l,DeepSeekProvider::MockEndpoint{http.baseUrl()},std::make_unique<FakeCredentialStore>());QVERIFY(configureRuntime(runtime));
        std::unique_ptr<LyricsLookup> dialog;if(!qml){dialog=std::make_unique<LyricsLookup>(&l,QVector<qint64>{a.id},nullptr,QUrl("http://127.0.0.1:9/api"),QUrl(),true,&runtime);LyricsLookupDiagnostics::choose(*dialog,a.id);}
        TranslationCoordinator qmlTranslation(&l,&runtime);auto *translation=qml?&qmlTranslation:LyricsLookupDiagnostics::translation(*dialog);
        http.enqueue(completion(QString::fromUtf8(translationJson({{0,QStringLiteral("世界你好")},{1,QStringLiteral("走回家")}}))));
        http.setHold(true);const auto task=translation->prepareAi(a.id);QVERIFY(task);QVERIFY(translation->start(task));QTRY_COMPARE(http.heldCount(),1);
        if(!qml)LyricsLookupDiagnostics::attachTask(*dialog,task);
        // Running task requires explicit choice. Completing while a decision is
        // open invalidates that decision; preview remains recoverable.
        auto plan=LyricsReplacementService::prepare(l,l.lyricsSaveSnapshot(a.id),"Changed source","pending",translation);
        QVERIFY(plan.decision.confirmationRequired);QVERIFY(http.releaseHeld());QTRY_COMPARE(translation->summary(task).state,TranslationState::CompletedPendingSave);
        const auto preview=translation->summary(task).translations;QString error;
        QVERIFY(!LyricsReplacementService::commit(l,plan,LyricsReplacementChoice::Replace,a.id,"pending",translation,&error));
        QCOMPARE(translation->summary(task).translations,preview);QCOMPARE(bytes(l,a.id),mono());
        if(qml){
            PlaybackFacade player(&l,nullptr,true);LyricsSearchCoordinator search(&l);LyricsSearchBridge bridge(&l,&player,&search);bridge.setTranslationCoordinator(translation);
            QVERIFY(bridge.setTargetTrack(QString::number(a.id)));QSignalSpy request(&bridge,&LyricsSearchBridge::replacementRequested),saved(&bridge,&LyricsSearchBridge::saved);
            QVERIFY(!bridge.saveText("Changed source",true));bridge.cancelReplacement(request.last()[0].toString());
            QVERIFY(translation->summary(task).hasPreview());QCOMPARE(translation->summary(task).translations,preview);QCOMPARE(saved.size(),0);
            QSqlQuery failQuery(import_fixture::database(l));QVERIFY(failQuery.exec("CREATE TRIGGER preview_fail BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT RAISE(FAIL,'preview failure'); END"));
            QVERIFY(!bridge.markInstrumental(true));QVERIFY(!bridge.resolveReplacement(request.last()[0].toString(),1));
            QVERIFY(translation->summary(task).hasPreview());QCOMPARE(translation->summary(task).translations,preview);QCOMPARE(bytes(l,a.id),mono());QCOMPARE(saved.size(),0);
            QVERIFY(failQuery.exec("DROP TRIGGER preview_fail"));
            QVERIFY(!bridge.markInstrumental(true));QVERIFY(bridge.resolveReplacement(request.last()[0].toString(),1));QCOMPARE(saved.size(),1);QVERIFY(l.isInstrumental(a.id));
            search.shutdown();QVERIFY(player.shutdown());
        }else{
            ModalAnswer answer;answer.answer=QMessageBox::Cancel;QVERIFY(!LyricsLookupDiagnostics::manual(*dialog,"Changed source"));
            QVERIFY(translation->summary(task).hasPreview());QCOMPARE(translation->summary(task).translations,preview);
            QSqlQuery failQuery(import_fixture::database(l));QVERIFY(failQuery.exec("CREATE TRIGGER preview_fail BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT RAISE(FAIL,'preview failure'); END"));
            answer.answer=QMessageBox::Yes;QVERIFY(!LyricsLookupDiagnostics::manual(*dialog,QStringLiteral("纯音乐，请欣赏").toUtf8()));
            QVERIFY(translation->summary(task).hasPreview());QCOMPARE(translation->summary(task).translations,preview);QCOMPARE(bytes(l,a.id),mono());QVERIFY(failQuery.exec("DROP TRIGGER preview_fail"));
            answer.answer=QMessageBox::Yes;QVERIFY(LyricsLookupDiagnostics::manual(*dialog,QStringLiteral("纯音乐，请欣赏").toUtf8()));QVERIFY(l.isInstrumental(a.id));
        }
        QCOMPARE(translation->summary(task).state,TranslationState::Cancelled);
    }
    void qmlWorkspaceContinuousConfirmation(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"A","gui-a"),b=seed(l,"B","gui-b"),c=seed(l,"C","gui-c");
        QVERIFY(l.saveLyricsPreservingText(c.id,bilingual()));auto different=mono();different.replace("Hello world","Different original");
        QVERIFY(l.saveLookupCache(a.id,QJsonArray{record(a,mono())},false));QVERIFY(l.saveLookupCache(b.id,QJsonArray{record(b,mono())},false));QVERIFY(l.saveLookupCache(c.id,QJsonArray{record(c,different)},false));
        QVERIFY(l.setSetting(QString("qml.lyricsReviewV1.%1").arg(c.id),QJsonObject{{"pending",true},{"trackHash",c.hash},{"sourceDigest",QString::fromLatin1(QCryptographicHash::hash(bilingual(),QCryptographicHash::Sha256).toHex())}}));
        QmlShell shell(&l,nullptr,true);QVERIFY(shell.load());auto *window=shell.window();auto *bridge=shell.lyricsSearchBridge();
        QSignalSpy saved(bridge,&LyricsSearchBridge::saved);const QStringList keys{QString::number(a.id),QString::number(b.id),QString::number(c.id)};
        const auto queue=shell.facade()->queue().items;const auto playing=shell.facade()->current().id;
        QVERIFY(QMetaObject::invokeMethod(window,"openLyricsTools",Q_ARG(QVariant,QVariant(keys)),Q_ARG(QVariant,QVariant(false))));
        auto *workspace=visual(window->contentItem(),"lyricsWorkspace");QVERIFY(workspace);
        auto *conflict=window->findChild<QObject *>("lyricsTranslationConflict");QVERIFY(conflict);
        QVERIFY(QMetaObject::invokeMethod(workspace,"applySelection"));QCOMPARE(saved.size(),1);QCOMPARE(bridge->targetTrackKey(),keys[1]);QVERIFY(!conflict->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(workspace,"applySelection"));QCOMPARE(saved.size(),2);QCOMPARE(bridge->targetTrackKey(),keys[2]);
        QVERIFY(QMetaObject::invokeMethod(workspace,"applySelection"));QTRY_VERIFY(conflict->property("visible").toBool());QCOMPARE(saved.size(),2);QCOMPARE(bytes(l,c.id),bilingual());
        QVERIFY(QMetaObject::invokeMethod(conflict,"reject"));QTRY_VERIFY(!conflict->property("visible").toBool());QCOMPARE(bridge->targetTrackKey(),keys[2]);QVERIFY(!bridge->selectedKey().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(workspace,"applySelection"));QTRY_VERIFY(conflict->property("visible").toBool());
        auto *keep=visual(window->contentItem(),"lyricsKeepExisting");QVERIFY(keep);QVERIFY(QMetaObject::invokeMethod(keep,"click"));QTRY_VERIFY(!conflict->property("visible").toBool());QCOMPARE(bytes(l,c.id),bilingual());QCOMPARE(saved.size(),2);
        QVERIFY(QMetaObject::invokeMethod(workspace,"applySelection"));QTRY_VERIFY(conflict->property("visible").toBool());
        auto *replace=visual(window->contentItem(),"lyricsReplaceExplicit");QVERIFY(replace);QVERIFY(QMetaObject::invokeMethod(replace,"click"));QTRY_COMPARE(saved.size(),3);QCOMPARE(bytes(l,c.id),different);
        QVERIFY(QMetaObject::invokeMethod(workspace,"applySelection"));QCOMPARE(saved.size(),3);
        QCOMPARE(bytes(l,a.id),mono());QCOMPARE(bytes(l,b.id),mono());QCOMPARE(shell.facade()->current().id,playing);QCOMPARE(shell.facade()->queue().items,queue);
        QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));shell.shutdown();
    }
};
QTEST_MAIN(LyricsReplacementIntegrationTests)
#include "lyrics-replacement-integration-tests.moc"
