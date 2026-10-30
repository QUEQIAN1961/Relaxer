#include "search-parameters-fixture.h"
#include "lyrics-replacement-fixture.h"
#include "c4c5-ui-fixture.h"
#include <QMediaPlayer>
using namespace search_fixture;
namespace {
QByteArray bytes(Library &l,qint64 id){return LyricsDocumentReader::readFile(l.absolute(l.track(id).lyrics)).bytes;}
bool write(const QString &path,const QByteArray &text){QFile f(path);return f.open(QIODevice::WriteOnly|QIODevice::Truncate)&&f.write(text)==text.size();}
}
class LegacyUiHardeningTests final:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QCOMPARE(QGuiApplication::platformName(),QString("offscreen"));}
    void frozenRiskListIsBoundedAndDoesNotAuthorize(){
        Fixture f;QVERIFY(f.ok);QVector<qint64> ids;
        for(int i=0;i<23;++i){auto t=search_fixture::seed(f.lib,QString("risk%1").arg(i),QString(120,'x')+QString::number(i));QVERIFY(f.lib.saveLyricsPreservingText(t.id,replacement_fixture::bilingual()));ids.append(t.id);}
        auto batch=LyricsEditService::prepareInstrumental(f.lib,ids);QVERIFY(batch.valid());const auto before=batch.overview();
        QCOMPARE(before["risk"].toInt(),23);QCOMPARE(before["riskTargets"].toList().size(),20);QCOMPARE(before["riskOmitted"].toInt(),3);
        const auto frozen=before["riskTargets"].toList().first().toMap();QCOMPARE(frozen["artist"].toString(),QString("Artist"));QVERIFY(!frozen["reason"].toString().isEmpty());
        QSqlQuery q(import_fixture::database(f.lib));QVERIFY(q.exec(QString("UPDATE tracks SET title='Changed after prepare' WHERE id=%1").arg(ids[0])));
        QCOMPARE(batch.overview()["riskTargets"].toList(),before["riskTargets"].toList());
        while(!batch.finished())LyricsEditService::stepInstrumental(f.lib,batch,false);
        QCOMPARE(batch.result()["skipped"].toInt(),23);QCOMPARE(batch.result()["succeeded"].toInt(),0);for(auto id:ids)QCOMPARE(bytes(f.lib,id),replacement_fixture::bilingual());
    }
    void localImport_data(){
        QTest::addColumn<QString>("kind");
        for(const auto *kind:{"new","existing","human-translation","stale-file","stale-target","cancel","draft","write-failure"})QTest::newRow(kind)<<QString::fromLatin1(kind);
    }
    void localImport(){
        QFETCH(QString,kind);Fixture f;QVERIFY(f.ok);const auto id=f.track.id;
        if(kind!="new"&&kind!="write-failure")QVERIFY(f.lib.saveLyricsPreservingText(id,replacement_fixture::bilingual()));
        const QByteArray imported=kind=="human-translation"?replacement_fixture::mono():QByteArray("[00:01.00]Different original\n[00:02.00]Different second\n");
        const auto path=f.dir.filePath("input.lrc");QVERIFY(write(path,imported));QSignalSpy confirm(f.bridge.get(),&LyricsSearchBridge::replacementRequested),failed(f.bridge.get(),&LyricsSearchBridge::operationFailed);
        QString draft;
        if(kind=="draft"){QVERIFY(!f.bridge->saveText("Unsaved manual draft",true));f.bridge->cancelReplacement(confirm.last()[0].toString());draft=f.bridge->draftText();}
        const auto token=f.bridge->beginLocalEdit();QVERIFY(!token.isEmpty());const auto original=bytes(f.lib,id);
        if(kind=="stale-file")QVERIFY(write(f.lib.absolute(f.lib.track(id).lyrics),"External change"));
        if(kind=="stale-target"){const auto b=search_fixture::seed(f.lib,"two","Second");QVERIFY(f.bridge->setTargetTrack(QString::number(b.id)));}
        if(kind=="write-failure"){QVERIFY(QDir().rmdir(f.lib.absolute("Lyrics")));QVERIFY(write(f.lib.absolute("Lyrics"),"Blocked parent"));}
        const int confirmations=confirm.size();const bool saved=f.bridge->importLocalLyrics(QUrl::fromLocalFile(path),true,token);
        if(kind=="new"||kind=="human-translation"){QVERIFY(saved);QVERIFY(bytes(f.lib,id).contains("[00:01.00]"));if(kind=="human-translation")QVERIFY(bytes(f.lib,id).contains(QStringLiteral("世界你好").toUtf8()));}
        else if(kind=="existing"||kind=="cancel"){QVERIFY(!saved);QCOMPARE(confirm.size(),confirmations+1);f.bridge->cancelReplacement(confirm.last()[0].toString());QCOMPARE(bytes(f.lib,id),original);}
        else {QVERIFY(!saved);QVERIFY(!failed.isEmpty());if(kind=="draft")QCOMPARE(f.bridge->draftText(),draft);if(kind!="stale-file"&&kind!="write-failure")QCOMPARE(bytes(f.lib,id),original);}
    }
    void feedbackReasonUsesCodeAndPreservesDiagnostic(){
        FeedbackBridge feedback;feedback.post("Playback","Error","无法播放","Access denied raw backend", "op42","9",true,false,QMediaPlayer::ResourceError);
        const auto e=feedback.current();QVERIFY(e["reason"].toString().contains("无法读取"));QCOMPARE(e["details"].toString(),QString("Access denied raw backend"));QCOMPARE(e["operationId"].toString(),QString("op42"));QCOMPARE(e["targetId"].toString(),QString("9"));
        feedback.post("Other","Error","操作失败","unknown raw", "op43");QVERIFY(feedback.current()["reason"].toString().contains("详情"));QCOMPARE(feedback.history().size(),2);
    }
    void actualQmlDialogsRouteAndSafeNotification(){
        QTemporaryDir d;Library lib(d.filePath("data"));QVERIFY(lib.open());const auto a=search_fixture::seed(lib);QVERIFY(lib.saveLyricsPreservingText(a.id,replacement_fixture::bilingual()));
        QmlShell shell(&lib,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();auto *bridge=shell.lyricsSearchBridge();
        auto *ws=c4c5_fixture::visual(root,"lyricsWorkspace");QVERIFY(ws);QVERIFY(!ws->isVisible());
        QVERIFY(bridge->requestLocalImport(QString::number(a.id)));QTRY_VERIFY(ws->isVisible());QVERIFY(!ws->property("fileEditToken").toString().isEmpty());
        auto *picker=w->findChild<QObject *>("lyricsImportDialog");QVERIFY(picker);QTRY_VERIFY(picker->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(picker,"close"));
        const auto path=d.filePath("changed.lrc");QVERIFY(write(path,"[00:01.00]New original\n[00:02.00]New words\n"));
        QVERIFY(!bridge->importLocalLyrics(QUrl::fromLocalFile(path),true,ws->property("fileEditToken").toString()));auto *replace=w->findChild<QObject *>("lyricsTranslationConflict");
        QVERIFY(replace);QTRY_VERIFY(replace->property("opened").toBool());QMetaObject::invokeMethod(replace,"reject");
        QCOMPARE(bytes(lib,a.id),replacement_fixture::bilingual());
        QVERIFY(bridge->prepareInstrumentalBatch({QString::number(a.id)}));auto *batch=w->findChild<QObject *>("batchInstrumentalDialog");QVERIFY(batch);
        QVERIFY(QMetaObject::invokeMethod(batch,"open"));QTRY_VERIFY(batch->property("opened").toBool());
        auto *risk=c4c5_fixture::visual(root,"batchInstrumentalRiskDetails");QVERIFY(risk);QVERIFY(QMetaObject::invokeMethod(risk,"clicked"));
        risk->setProperty("checked",true);QTRY_VERIFY(c4c5_fixture::visual(root,"batchRisk-"+QString::number(a.id))!=nullptr);
        QVERIFY(QMetaObject::invokeMethod(batch,"reject"));QTRY_VERIFY(!bridge->batchPrepared());
        auto *settings=w->findChild<QObject *>("lyricsSearchSettingsDialog");QVERIFY(settings);QVERIFY(QMetaObject::invokeMethod(settings,"open"));
        QTRY_VERIFY(settings->property("opened").toBool());auto *close=c4c5_fixture::visual(root,"lyricsSearchSettingsClose");QVERIFY(close);QCOMPARE(close->property("text").toString(),QStringLiteral("关闭"));QVERIFY(QMetaObject::invokeMethod(close,"clicked"));QTRY_VERIFY(!settings->property("visible").toBool());
        shell.feedbackBridge()->post("Playback","Error","测试摘要","raw English", "gui",{},true,false,QMediaPlayer::ResourceError);
        auto *notice=c4c5_fixture::visual(root,"feedbackNotice");QVERIFY(notice);QTRY_VERIFY(notice->isVisible());QTRY_VERIFY2(notice->mapToScene(QPointF()).y()>w->height()/2, qPrintable(QString("notice y=%1 height=%2 parent=%3x%4 window=%5x%6").arg(notice->y()).arg(notice->height()).arg(notice->parentItem()->width()).arg(notice->parentItem()->height()).arg(w->width()).arg(w->height())));QVERIFY(!w->property("popupOpen").toBool());
        QMetaObject::invokeMethod(settings,"open");QTRY_VERIFY(!notice->isVisible());QMetaObject::invokeMethod(settings,"close");QTRY_VERIFY(notice->isVisible());
        QVERIFY(!c4c5_fixture::visual(root,"feedbackReason")->property("text").toString().contains("raw English"));
        QVERIFY(!c4c5_fixture::visual(root,"playerFullscreen"));
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_VERIFY(w->property("lyricsOpen").toBool());
        auto *collapse=c4c5_fixture::visual(root,"playerCollapse");QVERIFY(collapse);
        QVERIFY(QMetaObject::invokeMethod(collapse,"clicked"));QTRY_VERIFY(!w->property("lyricsOpen").toBool());
        QVERIFY(shell.warnings().isEmpty());QVERIFY(shell.shutdown());
    }
};
QTEST_MAIN(LegacyUiHardeningTests)
#include "legacy-ui-hardening-tests.moc"
