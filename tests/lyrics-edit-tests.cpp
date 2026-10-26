#include "lyrics-edit-service.h"
#include "lyricslookup.h"
#include "ui-bridge/qml-shell.h"
#include "lyrics-document-reader.h"
#include "lyrics-replacement-fixture.h"
#include "import-fixture.h"
#include <QApplication>
#include <QMessageBox>
#include <QQuickItem>
#include <QSignalSpy>
#include <QStringConverter>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
#include <limits>
using namespace relaxer;
using namespace replacement_fixture;
namespace relaxer {
class LyricsLookupDiagnostics {
public:
    static void choose(LyricsLookup &d,qint64 id) {d.m_missingOnly->setChecked(false);d.m_missingTranslationOnly->setChecked(false);d.m_incomplete->setChecked(false);d.updateFilter();d.m_tracks->setCurrentRow(d.rowForTrack(id));d.m_tracks->currentItem()->setSelected(true);d.selectTrack();}
    static void offset(LyricsLookup &d,int value){d.m_offset->setValue(value);d.saveOffsets();}
    static void instrumental(LyricsLookup &d){d.setMultiSelect(true);d.m_tracks->selectAll();d.markInstrumental();}
};
}
namespace {
Track seed(Library &l,const QString &name){Track t;t.hash=name;t.title=name;t.file="Music/"+name+".wav";t.duration=60000;t.id=l.insert(t);return t;}
QByteArray read(Library &l,qint64 id){return LyricsDocumentReader::readFile(l.absolute(l.track(id).lyrics)).bytes;}
bool write(const QString &path,const QByteArray &data){QFile f(path);return f.open(QIODevice::WriteOnly|QIODevice::Truncate)&&f.write(data)==data.size();}
QByteArray utf16(const QString &text,bool little){QStringEncoder e(little?QStringConverter::Utf16LE:QStringConverter::Utf16BE);return QByteArray(little?"\xff\xfe":"\xfe\xff",2)+e(text);}
QQuickItem *visual(QQuickItem *item,const QString &name){if(item->objectName()==name)return item;for(auto *child:item->childItems())if(auto *found=visual(child,name))return found;return nullptr;}
bool click(QQuickWindow *w,QQuickItem *item){if(!item||!item->isVisible()||!item->isEnabled())return false;QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());return true;}
bool shot(QmlShell &shell,const QString &name){
    const auto out=qEnvironmentVariable("RELAXER_D203_EVIDENCE");if(out.isEmpty())return true;QDir().mkpath(out);
    const auto state=QJsonObject{{"input","QtTest window mouse/key events; not OS foreground input"},{"platform",QGuiApplication::platformName()},
        {"pid",double(QCoreApplication::applicationPid())},{"windowId",QString::number(quintptr(shell.window()->winId()))},
        {"title",shell.window()->title()},{"popupOpen",shell.window()->property("popupOpen").toBool()},
        {"lyricsOpen",shell.window()->property("lyricsOpen").toBool()},{"fileOffset",QJsonObject::fromVariantMap(shell.lyricsSearchBridge()->fileOffset())},
        {"batch",QJsonObject::fromVariantMap(shell.lyricsSearchBridge()->batchResult())}};
    const auto tag=QString::fromLatin1(QTest::currentDataTag());const auto file=name+(tag.isEmpty()?QString():"-"+tag);
    return write(out+"/"+file+".json",QJsonDocument(state).toJson())&&shell.window()->grabWindow().save(out+"/"+file+".png");
}
class Browser final:public TranslationBrowserBackend {
public:quint64 token=0;void translate(quint64 id,const QString &,const QString &,const QString &)override{token=id;}
    void cancel()override{}void resume()override{}void shutdown()override{}void answer(const QString &text){emit completed(token,text,{});}
};
}
class LyricsEditTests final:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){qputenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS","1");}
    void offsetBytesAndTimeline_data(){
        QTest::addColumn<QByteArray>("source");QTest::addColumn<int>("value");QTest::addColumn<QByteArray>("expected");QTest::addColumn<qint64>("first");
        const auto plain=QByteArray("[ti:Synthetic]\n[ar:Artist]\n[al:Album]\n[by:Human]\n[x-custom:Keep]\n[00:01.00]Hello\n[00:02.00]World\n");
        QTest::newRow("O01-positive")<<plain<<500<<QByteArray("[offset:500]\n")+plain<<qint64(1500);
        QTest::newRow("O02-negative")<<plain<<-500<<QByteArray("[offset:-500]\n")+plain<<qint64(500);
        QTest::newRow("O02-clamp")<<plain<<-2000<<QByteArray("[offset:-2000]\n")+plain<<qint64(0);
        QTest::newRow("O03-zero-preserves-existing")<<QByteArray("[offset:300]\n")+plain<<0<<QByteArray("[offset:300]\n")+plain<<qint64(1300);
        QTest::newRow("O04-absolute-not-added")<<QByteArray("[offset:300]\n")+plain<<700<<QByteArray("[offset:700]\n")+plain<<qint64(1700);
        QTest::newRow("O05-idempotent")<<QByteArray("[offset:500]\n")+plain<<500<<QByteArray("[offset:500]\n")+plain<<qint64(1500);
        QTest::newRow("O06-marked-bilingual")<<bilingual()<<200<<QByteArray("[offset:200]\n")+bilingual()<<qint64(1200);
        auto legacy=bilingual();legacy.replace("[re:relaxer-bilingual]\n","");
        QTest::newRow("O07-human-bilingual")<<legacy<<-200<<QByteArray("[offset:-200]\n")+legacy<<qint64(800);
        auto crlf=plain;crlf.replace("\n","\r\n");
        QTest::newRow("O08-CRLF-and-tags")<<crlf<<100<<QByteArray("[offset:100]\r\n")+crlf<<qint64(1100);
        QTest::newRow("O08-UTF8-BOM")<<QByteArray("\xef\xbb\xbf")+plain<<100<<QByteArray("\xef\xbb\xbf[offset:100]\n")+plain<<qint64(1100);
        const auto legacyBytes=QByteArray("[00:01.00]")+QByteArray::fromHex("b2e2cad4");
        QTest::newRow("O08-legacy-byte-preservation")<<legacyBytes<<100<<QByteArray("[offset:100]\n")+legacyBytes<<qint64(1100);
        for(bool le:{false,true})QTest::newRow(le?"O08-UTF16-LE":"O08-UTF16-BE")<<utf16(QString::fromUtf8(bilingual()),le)<<100<<utf16("[offset:100]\n"+QString::fromUtf8(bilingual()),le)<<qint64(1100);
        const auto multi=QByteArray("[00:59.999][01:00.001]Two timestamps\n");
        QTest::newRow("O09-carry-and-multiple")<<multi<<2<<QByteArray("[offset:2]\n")+multi<<qint64(60001);
    }
    void offsetBytesAndTimeline(){
        QFETCH(QByteArray,source);QFETCH(int,value);QFETCH(QByteArray,expected);QFETCH(qint64,first);
        QTemporaryDir dir;QVERIFY(dir.isValid());Library l(dir.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"offset");QVERIFY(t.id);
        QVERIFY(l.saveLyricsPreservingText(t.id,source));QVERIFY(l.setLyricOffset(t.id,450));
        auto plan=LyricsEditService::prepareOffset(l,l.lyricsSaveSnapshot(t.id),value);QVERIFY2(plan.valid(),qPrintable(plan.error()));
        QCOMPARE(plan.content(),expected);QCOMPARE(LyricsLookup::withOffset(source,value),expected);
        const auto parsed=Lyrics::parse(plan.content());QVERIFY(parsed.timed);QCOMPARE(parsed.lines.first().time,first);
        if(parsed.bilingual)for(int i=0;i<parsed.lines.size();i+=2)QCOMPARE(parsed.lines[i].time,parsed.lines[i+1].time);
        QString error;QVERIFY2(LyricsEditService::commitOffset(l,plan,t.id,false,nullptr,&error),qPrintable(error));
        QCOMPARE(read(l,t.id),expected);QCOMPARE(l.lyricOffset(t.id),450);QVERIFY(!LyricsEditService::commitOffset(l,plan,t.id,false,nullptr,&error));
        auto repeat=LyricsEditService::prepareOffset(l,l.lyricsSaveSnapshot(t.id),value);QVERIFY(repeat.valid());QVERIFY(!repeat.changed());
        QSignalSpy changed(&l,&Library::changed);QVERIFY(LyricsEditService::commitOffset(l,repeat,t.id,false,nullptr,&error));QCOMPARE(changed.size(),0);QCOMPARE(read(l,t.id),expected);
    }
    void unsafeOffset_data(){
        QTest::addColumn<QByteArray>("source");QTest::addColumn<int>("value");
        for(const auto &pair:QList<QPair<const char *,QByteArray>>{{"plain","No timestamps"},{"instrumental",QStringLiteral("纯音乐，请欣赏").toUtf8()},
            {"duplicate","[offset:1]\n[offset:2]\n[00:01]Hello"},{"invalid-tag","[offset:x]\n[00:01]Hello"},
            {"inline-offset","[offset:1][00:01]Hello"},{"bad-seconds","[00:99.00]Hello"},{"bad-fraction","[00:01.0001]Hello"},
            {"overflow","[9999999999999999999999:01]Hello"},{"offset-overflow","[offset:999999999999999999999]\n[00:01]Hello"},
            {"unpaired-bilingual","[re:relaxer-bilingual]\n[00:01]A\n[00:02]Translation"},
            {"untimed-bilingual-row","[re:relaxer-bilingual]\n[00:01]A\nTranslation"},
            {"extra-bilingual-row","[re:relaxer-bilingual]\n[00:01]A\n[00:01]Translation\nUnpaired"}})
            QTest::newRow(pair.first)<<pair.second<<100;
        QTest::newRow("O10-high")<<mono()<<std::numeric_limits<int>::max();QTest::newRow("O10-low")<<mono()<<std::numeric_limits<int>::min();
        QTest::newRow("O10-range")<<mono()<<5001;
    }
    void unsafeOffset(){
        QFETCH(QByteArray,source);QFETCH(int,value);QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=seed(l,"invalid");
        QVERIFY(l.saveLyricsPreservingText(t.id,mono()));QVERIFY(write(l.absolute(l.track(t.id).lyrics),source));auto p=LyricsEditService::prepareOffset(l,l.lyricsSaveSnapshot(t.id),value);
        QVERIFY(!p.valid());QVERIFY(!p.error().isEmpty());QVERIFY(!LyricsEditService::commitOffset(l,p,t.id,true,nullptr));QCOMPARE(read(l,t.id),source);
    }
    void offsetConflictFailureCancellationAndDisplayDelay(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto a=seed(l,"a"),b=seed(l,"b");QVERIFY(l.saveLyricsPreservingText(a.id,bilingual()));
        PlaybackFacade player(&l,nullptr,true);LyricsSearchCoordinator search(&l);LyricsSearchBridge bridge(&l,&player,&search);
        QVERIFY(bridge.setTargetTracks({QString::number(a.id),QString::number(b.id)},false,true));QSignalSpy savedSignal(&bridge,&LyricsSearchBridge::saved),replacement(&bridge,&LyricsSearchBridge::replacementRequested);
        QVERIFY(!bridge.saveText("[00:01]Keep this unsaved draft",true));QCOMPARE(replacement.size(),1);bridge.cancelReplacement(replacement.first()[0].toString());
        QVERIFY(bridge.beginFileOffset());QVERIFY(bridge.previewFileOffset(500));bridge.cancelFileOffset();QCOMPARE(read(l,a.id),bilingual());
        QVERIFY(bridge.beginFileOffset());QVERIFY(!bridge.previewFileOffset(std::numeric_limits<double>::infinity()));QVERIFY(!bridge.saveFileOffset());bridge.cancelFileOffset();
        QVERIFY(bridge.beginFileOffset());QVERIFY(!bridge.previewFileOffset(1.5));bridge.cancelFileOffset();
        QVERIFY(bridge.beginFileOffset());QVERIFY(bridge.previewFileOffset(500));QVERIFY(write(l.absolute(l.track(a.id).lyrics),"[00:03]External"));
        QVERIFY(!bridge.saveFileOffset());QVERIFY(bridge.fileOffset()["preview"].toString().contains("世界你好"));QCOMPARE(read(l,a.id),QByteArray("[00:03]External"));bridge.cancelFileOffset();
        QVERIFY(l.saveLyricsPreservingText(a.id,bilingual()));QVERIFY(bridge.beginFileOffset());QVERIFY(bridge.previewFileOffset(500));
        QSqlQuery q(import_fixture::database(l));QVERIFY(q.exec("CREATE TRIGGER deny_offset BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT RAISE(ABORT,'controlled offset save failure'); END"));
        QVERIFY(!bridge.saveFileOffset());QCOMPARE(read(l,a.id),bilingual());QVERIFY(bridge.fileOffset()["active"].toBool());QVERIFY(bridge.fileOffset()["preview"].toString().contains("offset:500"));
        QVERIFY(q.exec("DROP TRIGGER deny_offset"));QVERIFY(l.setLyricOffset(a.id,700));QVERIFY(bridge.saveFileOffset());QCOMPARE(l.lyricOffset(a.id),700);
        QCOMPARE(savedSignal.size(),0);QCOMPARE(bridge.targetTrackKey(),QString::number(a.id));QVERIFY(bridge.draftText().contains("Keep this unsaved draft"));
        const auto saved=read(l,a.id);QCOMPARE(Lyrics::parse(saved).lines.first().time,qint64(1500));
        LyricsModel model(&l,&player);player.play({a.id},a.id);QTRY_COMPARE(model.timestampAt(0),qint64(1500));QCOMPARE(model.offsetMs(),700);
        QVERIFY(bridge.beginFileOffset());QVERIFY(bridge.previewFileOffset(-300));QVERIFY(bridge.setTargetTrack(QString::number(b.id)));QVERIFY(!bridge.saveFileOffset());QCOMPARE(read(l,a.id),saved);
        QVERIFY(bridge.setTargetTrack(QString::number(a.id)));QVERIFY(bridge.beginFileOffset());QVERIFY(bridge.previewFileOffset(100));
        QVERIFY(QFile::remove(l.absolute(l.track(a.id).lyrics)));QVERIFY(!bridge.saveFileOffset());QVERIFY(!QFileInfo::exists(l.absolute(l.track(a.id).lyrics)));
        search.shutdown();QVERIFY(player.shutdown());
    }
    void pendingTranslationProtection_data(){QTest::addColumn<bool>("offset");QTest::newRow("O13-offset")<<true;QTest::newRow("B06-instrumental")<<false;}
    void pendingTranslationProtection(){
        QFETCH(bool,offset);QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"preview");QVERIFY(l.saveLyrics(t.id,mono()));
        auto browser=std::make_unique<Browser>();auto *backend=browser.get();QmlShell shell(&l,nullptr,true,nullptr,std::move(browser));QVERIFY(shell.load());
        auto *tb=shell.translationBridge();auto *translation=shell.translationCoordinator();QVERIFY(tb->setTargetTrack(QString::number(t.id)));
        auto key=tb->prepareBrowser("bing","en");QVERIFY(!key.isEmpty());QVERIFY(tb->start(key));QTRY_VERIFY(backend->token);
        auto staleOffset=LyricsEditService::prepareOffset(l,l.lyricsSaveSnapshot(t.id),300,translation);
        auto staleBatch=LyricsEditService::prepareInstrumental(l,{t.id},translation);
        const auto token=backend->token;backend->answer(QStringLiteral("世界你好"));QTRY_VERIFY(backend->token!=token);backend->answer(QStringLiteral("走回家"));QTRY_VERIFY(tb->preview());
        const auto preview=tb->previewText();QString error;
        if(offset)QVERIFY(!LyricsEditService::commitOffset(l,staleOffset,t.id,true,translation,&error));
        else {QVERIFY(LyricsEditService::stepInstrumental(l,staleBatch,true,translation));QCOMPARE(staleBatch.result()["failed"].toInt(),1);}
        QCOMPARE(read(l,t.id),mono());QCOMPARE(tb->previewText(),preview);
        auto noOp=LyricsEditService::prepareOffset(l,l.lyricsSaveSnapshot(t.id),0,translation);QVERIFY(noOp.valid());QVERIFY(!noOp.changed());
        QVERIFY(LyricsEditService::commitOffset(l,noOp,t.id,false,translation,&error));QCOMPARE(tb->previewText(),preview);
        auto p=LyricsEditService::prepareOffset(l,l.lyricsSaveSnapshot(t.id),300,translation);QVERIFY(p.confirmationRequired());
        auto batch=LyricsEditService::prepareInstrumental(l,{t.id},translation);QCOMPARE(batch.overview()["risk"].toInt(),1);
        if(offset)QVERIFY(!LyricsEditService::commitOffset(l,p,t.id,false,translation,&error));
        else {QVERIFY(LyricsEditService::stepInstrumental(l,batch,false,translation));QCOMPARE(batch.result()["skipped"].toInt(),1);}
        QCOMPARE(tb->previewText(),preview);QSqlQuery q(import_fixture::database(l));
        QVERIFY(q.exec("CREATE TRIGGER deny_preview BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT RAISE(ABORT,'controlled preview save failure'); END"));
        if(offset)QVERIFY(!LyricsEditService::commitOffset(l,p,t.id,true,translation,&error));
        else {batch=LyricsEditService::prepareInstrumental(l,{t.id},translation);QVERIFY(LyricsEditService::stepInstrumental(l,batch,true,translation));QCOMPARE(batch.result()["failed"].toInt(),1);}
        QCOMPARE(read(l,t.id),mono());QCOMPARE(tb->previewText(),preview);QVERIFY(tb->preview());QVERIFY(q.exec("DROP TRIGGER deny_preview"));
        if(offset)QVERIFY(LyricsEditService::commitOffset(l,p,t.id,true,translation,&error));
        else {batch=LyricsEditService::prepareInstrumental(l,{t.id},translation);QVERIFY(LyricsEditService::stepInstrumental(l,batch,true,translation));QCOMPARE(batch.result()["succeeded"].toInt(),1);}
        QVERIFY(!tb->preview());QVERIFY(offset?read(l,t.id).contains("offset:300"):l.isInstrumental(t.id));
    }
    void mixedBatchRiskAuthorizationAndRestart_data(){QTest::addColumn<bool>("authorize");QTest::newRow("B07-safe-only")<<false;QTest::newRow("B09-explicit-risk")<<true;}
    void mixedBatchRiskAuthorizationAndRestart(){
        QFETCH(bool,authorize);QTemporaryDir d;const auto root=d.filePath("data");QVector<qint64> ids;
        {Library l(root);QVERIFY(l.open());for(int i=0;i<6;++i)ids.append(seed(l,QString::number(i)).id);
            QVERIFY(l.markInstrumental(ids[1]));QVERIFY(l.saveLyricsPreservingText(ids[2],mono()));
            auto human=bilingual();human.replace("[re:relaxer-bilingual]\n","");QVERIFY(l.saveLyricsPreservingText(ids[3],human));QVERIFY(l.saveLyricsPreservingText(ids[4],bilingual()));
            auto batch=LyricsEditService::prepareInstrumental(l,{ids[0],ids[1],ids[2],ids[3],ids[4],ids[5],ids[0]});
            QCOMPARE(batch.overview()["total"].toInt(),6);QCOMPARE(batch.overview()["safe"].toInt(),2);QCOMPARE(batch.overview()["already"].toInt(),1);QCOMPARE(batch.overview()["risk"].toInt(),3);
            while(LyricsEditService::stepInstrumental(l,batch,authorize)){}
            const auto r=batch.result();QCOMPARE(r["succeeded"].toInt(),authorize?5:2);QCOMPARE(r["skipped"].toInt(),authorize?1:4);QCOMPARE(r["failed"].toInt(),0);
            QVERIFY(!LyricsEditService::stepInstrumental(l,batch,true));QVERIFY(l.isInstrumental(ids[0]));QVERIFY(l.isInstrumental(ids[5]));
            if(!authorize){QCOMPARE(read(l,ids[2]),mono());QCOMPARE(read(l,ids[3]),human);QCOMPARE(read(l,ids[4]),bilingual());}}
        Library reopened(root);QVERIFY(reopened.open());for(auto id:ids)QCOMPARE(reopened.isInstrumental(id),authorize||id==ids[0]||id==ids[1]||id==ids[5]);
    }
    void batchPartialFailureAndFrozenIdentity(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());QVector<qint64> ids;for(int i=0;i<4;++i)ids.append(seed(l,QString::number(i)).id);
        QVERIFY(l.saveLyricsPreservingText(ids[1],bilingual()));auto b=LyricsEditService::prepareInstrumental(l,ids);
        const auto external=QByteArray("[00:03]External new lyrics");QVERIFY(write(l.absolute(l.track(ids[1]).lyrics),external));
        QSqlQuery q(import_fixture::database(l));QVERIFY(q.exec(QString("CREATE TRIGGER deny_one BEFORE UPDATE OF lyrics ON tracks WHEN NEW.id=%1 BEGIN SELECT RAISE(ABORT,'controlled batch failure'); END").arg(ids[2])));
        QVERIFY(l.remove({ids[3]}));const auto extra=seed(l,"new-unselected");
        while(LyricsEditService::stepInstrumental(l,b,true)){}
        QCOMPARE(b.result()["succeeded"].toInt(),1);QCOMPARE(b.result()["failed"].toInt(),3);QCOMPARE(read(l,ids[1]),external);QVERIFY(!l.isInstrumental(ids[2]));QVERIFY(l.track(ids[2]).lyrics.isEmpty());QVERIFY(l.track(extra.id).lyrics.isEmpty());
    }
    void offsetIdentityDestinationAndBatchTeardown(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto a=seed(l,"identity"),b=seed(l,"not-started");QVERIFY(l.saveLyricsPreservingText(a.id,mono()));
        auto plan=LyricsEditService::prepareOffset(l,l.lyricsSaveSnapshot(a.id),100);QVERIFY(plan.valid());QSqlQuery q(import_fixture::database(l));
        QVERIFY(q.exec(QString("UPDATE tracks SET hash='changed-identity' WHERE id=%1").arg(a.id)));QVERIFY(!LyricsEditService::commitOffset(l,plan,a.id,false,nullptr));QCOMPARE(read(l,a.id),mono());
        const auto other=l.absolute("other-source.lrc");QVERIFY(write(other,"[00:03]Other source"));QVERIFY(q.exec(QString("UPDATE tracks SET lyrics='other-source.lrc' WHERE id=%1").arg(a.id)));
        auto conflict=LyricsEditService::prepareOffset(l,l.lyricsSaveSnapshot(a.id),100);QVERIFY(!conflict.valid());QCOMPARE(LyricsDocumentReader::readFile(other).bytes,QByteArray("[00:03]Other source"));
        {auto shell=std::make_unique<QmlShell>(&l,nullptr,true);QVERIFY(shell->load());auto *bridge=shell->lyricsSearchBridge();QVERIFY(bridge->prepareInstrumentalBatch({QString::number(b.id)}));QVERIFY(bridge->startInstrumentalBatch());
            QVERIFY(QMetaObject::invokeMethod(shell->window(),"requestQuit"));QVERIFY(!shell->window()->isVisible());QVERIFY(!bridge->batchRunning());shell.reset();}
        QCoreApplication::processEvents();QVERIFY(l.track(b.id).lyrics.isEmpty());QVERIFY(!l.isInstrumental(b.id));
    }
    void batchCancellationAndBoundedFeedback(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());QStringList keys;for(int i=0;i<100;++i)keys.append(QString::number(seed(l,QString::number(i)).id));
        QmlShell shell(&l,nullptr,true);QVERIFY(shell.load());auto *b=shell.lyricsSearchBridge();QSignalSpy completed(b,&LyricsSearchBridge::editCompleted);
        QVERIFY(b->prepareInstrumentalBatch(keys));b->cancelInstrumentalBatch();QVERIFY(b->batchResult()["cancelled"].toBool());QCOMPARE(b->batchResult()["succeeded"].toInt(),0);
        bool cancel=true;connect(&l,&Library::changed,this,[&]{if(cancel&&b->batchRunning()){cancel=false;b->cancelInstrumentalBatch();}});
        QVERIFY(b->prepareInstrumentalBatch(keys));QVERIFY(b->startInstrumentalBatch());QVERIFY(!b->startInstrumentalBatch(true));QTRY_VERIFY(!b->batchRunning());
        QCOMPARE(b->batchResult()["succeeded"].toInt(),1);QCOMPARE(b->batchResult()["cancelledCount"].toInt(),99);
        QSqlQuery q(import_fixture::database(l));QVERIFY(q.exec("CREATE TRIGGER deny_batch BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT RAISE(ABORT,'controlled bulk failure'); END"));
        keys.removeFirst();keys.append(QString::number(seed(l,"hundred").id));const auto before=shell.feedbackBridge()->history().size();
        QTimer heartbeat;int ticks=0;heartbeat.setInterval(0);connect(&heartbeat,&QTimer::timeout,this,[&]{++ticks;});heartbeat.start();
        QVERIFY(b->prepareInstrumentalBatch(keys));QVERIFY(b->startInstrumentalBatch());QTRY_VERIFY_WITH_TIMEOUT(!b->batchRunning(),20000);heartbeat.stop();
        QCOMPARE(b->batchResult()["failed"].toInt(),100);QCOMPARE(b->batchResult()["omitted"].toInt(),92);QCOMPARE(b->batchResult()["details"].toString().count('\n'),7);
        QVERIFY(ticks>1);QCOMPARE(shell.feedbackBridge()->history().size(),before+1);QCOMPARE(completed.size(),3);QVERIFY(shell.warnings().isEmpty());
    }
    void widgetsAndQmlCommandParity_data(){QTest::addColumn<bool>("qml");QTest::newRow("qml")<<true;QTest::newRow("widgets")<<false;}
    void widgetsAndQmlCommandParity(){
        QFETCH(bool,qml);QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto a=seed(l,"parity-a"),b=seed(l,"parity-b");QVERIFY(l.saveLyricsPreservingText(a.id,bilingual()));
        if(qml){PlaybackFacade p(&l,nullptr,true);LyricsSearchCoordinator s(&l);LyricsSearchBridge bridge(&l,&p,&s);QVERIFY(bridge.setTargetTrack(QString::number(a.id)));QVERIFY(bridge.beginFileOffset());QVERIFY(bridge.previewFileOffset(500));QVERIFY(bridge.saveFileOffset());
            QVERIFY(bridge.prepareInstrumentalBatch({QString::number(a.id),QString::number(b.id)}));QVERIFY(bridge.startInstrumentalBatch(true));QTRY_VERIFY(!bridge.batchRunning());s.shutdown();QVERIFY(p.shutdown());}
        else {LyricsLookup dialog(&l,{a.id,b.id},nullptr,QUrl("http://127.0.0.1:9/api"),{},true);LyricsLookupDiagnostics::choose(dialog,a.id);LyricsLookupDiagnostics::offset(dialog,500);QCOMPARE(read(l,a.id),QByteArray("[offset:500]\n")+bilingual());
            QTimer answer;connect(&answer,&QTimer::timeout,this,[]{if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))box->button(QMessageBox::Yes)->click();});answer.start(10);LyricsLookupDiagnostics::instrumental(dialog);}
        QVERIFY(l.isInstrumental(a.id));QVERIFY(l.isInstrumental(b.id));QCOMPARE(read(l,a.id),QStringLiteral("纯音乐，请欣赏").toUtf8());
    }
    void qmlOffsetAndWorkspaceBatchDialogs(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto t=seed(l,"ui-offset");QVERIFY(l.saveLyricsPreservingText(t.id,bilingual()));
        QmlShell shell(&l,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));auto *bridge=shell.lyricsSearchBridge();
        QVERIFY(bridge->setTargetTracks({QString::number(t.id)},false,true));auto *ws=visual(w->contentItem(),"lyricsWorkspace");QVERIFY(ws);QVERIFY(QMetaObject::invokeMethod(ws,"open"));
        auto *open=visual(w->contentItem(),"lyricsFileOffsetButton");QVERIFY(click(w,open));auto *dialog=w->findChild<QObject *>("lyricsFileOffsetDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("opened").toBool());
        QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!dialog->property("visible").toBool());QVERIFY(ws->isVisible());QCOMPARE(read(l,t.id),bilingual());
        QVERIFY(click(w,open));QTRY_VERIFY(dialog->property("opened").toBool());auto *amount=visual(w->contentItem(),"lyricsFileOffsetAmount");QVERIFY(amount);
        auto *input=qobject_cast<QQuickItem *>(amount->property("contentItem").value<QObject *>());QVERIFY(input);input->forceActiveFocus();QTest::keyClick(w,Qt::Key_A,Qt::ControlModifier);
        QTest::keyClick(w,Qt::Key_5);QTest::keyClick(w,Qt::Key_0);QTest::keyClick(w,Qt::Key_0);QTest::keyClick(w,Qt::Key_Return);
        QTRY_COMPARE(bridge->fileOffset()["requestedMs"].toInt(),500);QVERIFY(shot(shell,"ui-offset-preview"));
        QVERIFY(click(w,visual(w->contentItem(),"lyricsFileOffsetSave")));QTRY_VERIFY(!dialog->property("visible").toBool());QCOMPARE(read(l,t.id),QByteArray("[offset:500]\n")+bilingual());
        QVERIFY(click(w,visual(w->contentItem(),"lyricsMultiSelect")));QVERIFY(click(w,visual(w->contentItem(),"lyricsSelectAll")));QVERIFY(click(w,visual(w->contentItem(),"lyricsBatchInstrumental")));
        auto *batch=w->findChild<QObject *>("batchInstrumentalDialog");QVERIFY(batch);QTRY_VERIFY(batch->property("opened").toBool());QCOMPARE(bridge->batchOverview()["total"].toInt(),1);
        QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!batch->property("visible").toBool());QVERIFY(ws->isVisible());QVERIFY(!l.isInstrumental(t.id));QCOMPARE(read(l,t.id),QByteArray("[offset:500]\n")+bilingual());QVERIFY(shell.warnings().isEmpty());
    }
    void qmlBulkEntryAndEscapeWithNotification_data(){
        QTest::addColumn<bool>("authorize");QTest::addColumn<bool>("deny");
        QTest::newRow("U04-safe-only")<<false<<false;QTest::newRow("U04-explicit-risk")<<true<<false;QTest::newRow("U06-partial-success")<<true<<true;
    }
    void qmlBulkEntryAndEscapeWithNotification(){
        QFETCH(bool,authorize);QFETCH(bool,deny);
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());auto a=seed(l,"safe"),b=seed(l,"risk"),c=seed(l,"already");QVERIFY(l.saveLyricsPreservingText(b.id,bilingual()));QVERIFY(l.markInstrumental(c.id));
        QSqlQuery q(import_fixture::database(l));if(deny)QVERIFY(q.exec(QString("CREATE TRIGGER deny_gui_one BEFORE UPDATE OF lyrics ON tracks WHEN NEW.id=%1 BEGIN SELECT RAISE(ABORT,'controlled GUI save failure'); END").arg(a.id)));
        QmlShell shell(&l,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));auto *root=w->contentItem();
        auto *table=visual(root,"libraryListContainer");QVERIFY(table);QVERIFY(QMetaObject::invokeMethod(table,"beginBulk"));QVERIFY(click(w,visual(root,"bulkSelectAll")));QTRY_COMPARE(w->property("bulkKeys").toList().size(),3);
        QVERIFY(click(w,visual(root,"bulkInstrumental")));auto *dialog=w->findChild<QObject *>("batchInstrumentalDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("opened").toBool());QTRY_VERIFY(w->property("popupOpen").toBool());QVERIFY(shot(shell,"ui-batch-review"));
        QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!dialog->property("visible").toBool());QVERIFY(w->property("bulkActive").toBool());QVERIFY(l.track(a.id).lyrics.isEmpty());QTRY_VERIFY(!w->property("popupOpen").toBool());
        QVERIFY(click(w,visual(root,"bulkInstrumental")));QTRY_VERIFY(dialog->property("opened").toBool());if(authorize)QVERIFY(click(w,visual(root,"batchInstrumentalRiskConsent")));
        QVERIFY(click(w,visual(root,"batchInstrumentalCommit")));QTRY_VERIFY(!shell.lyricsSearchBridge()->batchRunning());
        QCOMPARE(l.isInstrumental(a.id),!deny);QVERIFY(l.isInstrumental(c.id));QCOMPARE(l.isInstrumental(b.id),authorize);if(!authorize)QCOMPARE(read(l,b.id),bilingual());QVERIFY(shell.feedbackBridge()->visible());
        QCOMPARE(shell.lyricsSearchBridge()->batchResult()["failed"].toInt(),deny?1:0);QCOMPARE(shell.feedbackBridge()->current()["category"].toString(),deny?QString("Warning"):QString("Information"));
        QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!w->property("bulkActive").toBool());QVERIFY(click(w,visual(root,"playbackExpandButton")));QTRY_COMPARE(w->property("playerProgress").toDouble(),1.0);
        QTest::keyClick(w,Qt::Key_Escape);QTRY_COMPARE(w->property("playerProgress").toDouble(),0.0);QVERIFY(shell.feedbackBridge()->visible());QVERIFY(!w->property("popupOpen").toBool());QVERIFY(shot(shell,"ui-batch-result-notification"));QVERIFY(shell.warnings().isEmpty());
    }
};
QTEST_MAIN(LyricsEditTests)
#include "lyrics-edit-tests.moc"
