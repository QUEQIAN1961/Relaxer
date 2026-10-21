#include "c4c5-ui-fixture.h"
#include <QApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSignalSpy>
#include <QUuid>
#include <QWheelEvent>
using namespace c4c5_fixture;
namespace {
QString output;
int serial=0;
QRectF rect(QQuickItem *item){return item->mapRectToScene(QRectF(QPointF(),item->size()));}
void record(const QString &stage,QJsonObject value){
    value["stage"]=stage;value["test"]=QString::fromLatin1(QTest::currentTestFunction());
    value["utc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const auto path=QDir(output).filePath(QString("%1-%2.json").arg(++serial,4,10,QChar('0')).arg(stage));
    QFile file(path);if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly))qFatal("Cannot preserve C6 evidence");
    file.write(QJsonDocument(value).toJson());file.flush();
}
void shot(QQuickWindow *w,const QString &name){
    const auto path=QDir(output).filePath(QString("%1-%2.png").arg(++serial,4,10,QChar('0')).arg(name));
    if(!w->grabWindow().save(path))qFatal("Cannot preserve C6 screenshot");
}
bool prepare(Fixture &f){
    if(!f.ok)return false;
    auto *w=f.shell->window();w->showNormal();w->setGeometry(45,50,1240,790);w->requestActivate();
    if(!QTest::qWaitForWindowExposed(w))return false;
    f.shell->playbackBridge()->pause();QTest::qWait(100);return true;
}
QJsonArray rows(QQuickItem *list){
    QJsonArray values;
    for(auto *row:delegates(list))values.append(QJsonObject{{"index",row->property("index").toInt()},
        {"current",row->property("current").toBool()},{"height",row->height()},
        {"scale",row->childItems().first()->scale()},{"targetScale",row->property("activeScale").toDouble()}});
    return values;
}
QJsonArray requests(TranslationHttpMock &http){
    QJsonArray result;
    for(int i=0;i<http.requestCount();++i)result.append(QJsonObject{
        {"requestLine",QString::fromUtf8(http.request(i).split('\n').first()).trimmed()},
        {"body",QJsonDocument::fromJson(http.requestBody(i)).object()}});
    return result; // Headers, including the fake credential, are deliberately excluded.
}
}
class C6EvidenceTests final:public QObject{
    Q_OBJECT
private slots:
    void initTestCase(){
        auto base=qEnvironmentVariable("RELAXER_C6_EVIDENCE");
        if(base.isEmpty())base=QCoreApplication::applicationDirPath()+"/Evidence/C6";
        output=base+"/Measurements-"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+"-"+QUuid::createUuid().toString(QUuid::Id128);
        QVERIFY(QDir().mkpath(output));qInfo().noquote()<<"C6_EVIDENCE"<<output;
    }
    void cleanup(){record("test-result",{{"failed",QTest::currentTestFailed()}});}
    void lyricsRuntimeAndBoundaries(){
        Fixture f(60);QVERIFY(prepare(f));auto *w=f.shell->window();auto *p=f.shell->facade();auto *m=f.shell->lyricsModel();
        QByteArray bilingual;
        for(int i=0;i<60;++i)bilingual+=QString("[00:%1.00]Line %2 original\n[00:%1.00]第 %2 行译文\n").arg(i,2,10,QChar('0')).arg(i).toUtf8();
        QVERIFY(f.library.saveLyrics(f.a.id,bilingual));QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));
        QTRY_COMPARE(m->count(),60);QTRY_VERIFY(p->seekable());
        p->seek(3000);QTRY_COMPARE(m->currentIndex(),3);QTest::qWait(230);
        auto *panel=visual(w->contentItem(),"lyricsPanel");auto *list=visual(w->contentItem(),"lyricsList");QVERIFY(panel&&list);
        const auto before=rows(list);QHash<int,qreal> heights;
        for(auto *row:delegates(list))heights[row->property("index").toInt()]=row->height();
        p->seek(4000);QTRY_COMPARE(m->currentIndex(),4);QTest::qWait(250);
        record("lyrics-scale-raw",{{"before",before},{"after",rows(list)},{"actualPosition",double(p->position())}});
        bool active=false,previous=false;double drift=0;
        for(auto *row:delegates(list)){
            const int index=row->property("index").toInt();
            if(heights.contains(index))drift=qMax(drift,qAbs(row->height()-heights[index]));
            if(index==4){QVERIFY(qAbs(row->childItems().first()->scale()-1.2)<.001);active=true;}
            if(index==3){QVERIFY(qAbs(row->childItems().first()->scale()-1.0)<.001);previous=true;}
        }
        record("lyrics-scale",{{"input","decode-only backend paused; real seeks 3000 and 4000"},{"before",before},{"after",rows(list)},{"heightDrift",drift}});
        QVERIFY(active&&previous);QCOMPARE(drift,0.0);
        const auto original=m->data(m->index(0),LyricsModel::Original).toString();const auto translated=m->data(m->index(0),LyricsModel::Translation).toString();
        QCOMPARE(original,QString("Line 0 original"));QCOMPARE(translated,QString("第 0 行译文"));
        const auto autoBefore=list->property("contentY").toReal();p->seek(25000);QTRY_COMPARE(m->currentIndex(),25);QTest::qWait(60);
        const auto autoAfter=list->property("contentY").toReal();QVERIFY(autoAfter>autoBefore);
        const auto point=list->mapToScene(QPointF(list->width()/2,list->height()/2));
        QWheelEvent wheel(point,w->mapToGlobal(point.toPoint()),QPoint(),QPoint(0,-240),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        wheel.setTimestamp(++QTest::lastMouseTimestamp);QCoreApplication::sendEvent(w,&wheel);QTRY_VERIFY(panel->property("manualScroll").toBool());
        QTest::qWait(100);QVERIFY(QMetaObject::invokeMethod(list,"cancelFlick"));QTRY_VERIFY(!list->property("moving").toBool());
        const auto manual=list->property("contentY").toReal();p->seek(12000);QTRY_COMPARE(m->currentIndex(),12);QTest::qWait(70);
        QCOMPARE(list->property("contentY").toReal(),manual);
        auto *back=visual(w->contentItem(),"lyricsReturnCurrent");QVERIFY(back);QVERIFY(QMetaObject::invokeMethod(back,"click"));
        QVERIFY(!panel->property("manualScroll").toBool());QTest::qWait(60);
        record("lyrics-scroll",{{"autoBefore",autoBefore},{"autoAfter",autoAfter},{"manualY",manual},{"returnedY",list->property("contentY").toReal()},{"original",original},{"translation",translated}});shot(w,"lyrics-bilingual");
        QVERIFY(m->setOffsetMs(5000));p->seek(6000);QTRY_COMPARE(m->currentIndex(),1);
        QVERIFY(!m->setOffsetMs(5001));QVERIFY(m->setOffsetMs(-5000));p->seek(0);QTRY_COMPARE(m->currentIndex(),5);
        QVERIFY(!m->setOffsetMs(-5001));QVERIFY(m->setOffsetMs(2500));m->reload();QTRY_COMPARE(m->offsetMs(),2500);
        record("lyrics-offset",{{"positiveBoundary",5000},{"negativeBoundary",-5000},{"outOfRangeRejected",true},{"stored",f.library.lyricOffset(f.a.id)},{"reloaded",m->offsetMs()}});
        TranslationSummary old;old.trackId=f.a.id;old.trackHash=f.library.track(f.a.id).hash;old.state=TranslationState::CompletedPendingSave;
        old.prepared=ai::lyrics::prepare(f.source,"a",{});old.sourceDigest=old.prepared.sourceDigest;old.translations=QStringList(60,"stale");
        p->play({f.a.id,f.b.id},f.b.id);QTRY_COMPARE(m->trackKey(),QString::number(f.b.id));m->presentTranslation(old);QVERIFY(!m->preview());
        const auto noLyrics=seed(f.library,"No lyrics","no-lyrics");QVERIFY(noLyrics.id);QVERIFY(wave(f.library.absolute(noLyrics.file)));
        p->play({noLyrics.id},noLyrics.id);QTRY_COMPARE(m->trackKey(),QString::number(noLyrics.id));QTRY_COMPARE(m->count(),0);QVERIFY(m->status().contains("暂无歌词"));
        record("lyrics-empty-and-old-result",{{"count",m->count()},{"status",m->status()},{"preview",m->preview()}});shot(w,"lyrics-empty");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void fullscreenEscapeButtonAndRestore(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *p=f.shell->facade();const auto geometry=w->geometry();const auto queue=p->queue().items;
        for(int i=0;i<3;++i){
            QVERIFY(QMetaObject::invokeMethod(w,"toggleFullscreen"));QTRY_COMPARE(w->visibility(),QWindow::FullScreen);QTest::qWait(100);shot(w,QString("fullscreen-%1").arg(i));
            if(i!=1){QTest::keyClick(w,Qt::Key_Escape);if(i==2)QTest::keyClick(w,Qt::Key_Escape);}
            else{QTRY_VERIFY(w->property("playerProgress").toDouble()>.999);auto *button=visual(w->contentItem(),"fullscreenButton");QVERIFY(button);QVERIFY(button->isVisible()&&button->isEnabled());QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,rect(button).center().toPoint());}
            QTRY_VERIFY(w->visibility()!=QWindow::FullScreen);QTRY_COMPARE(w->geometry(),geometry);
            QCOMPARE(f.shell->facade(),p);QCOMPARE(p->queue().items,queue);
            record("fullscreen-restore",{{"cycle",i},{"exit",i==1?"Qt mouse event on actual button":"Qt key event Escape (not physical keyboard)"},{"x",w->x()},{"y",w->y()},{"width",w->width()},{"height",w->height()},{"playbackInstances",f.shell->findChildren<Playback *>().size()},{"sqliteConnections",QSqlDatabase::connectionNames().size()}});
        }
        f.shell->playbackBridge()->play();QTRY_VERIFY(p->position()>500);const auto position=p->position();
        QVERIFY(QMetaObject::invokeMethod(w,"toggleFullscreen"));QTest::qWait(150);QVERIFY(p->position()>=position);
        p->next();QTRY_COMPARE(p->current().id,f.b.id);QTest::keyClick(w,Qt::Key_Escape);QTRY_COMPARE(w->geometry(),geometry);
        record("fullscreen-playback",{{"beforePosition",double(position)},{"afterPosition",double(p->position())},{"currentTrack",QString::number(p->current().id)},{"playing",p->playing()}});
        QPointer<PlaybackFacade> guard=p;QVERIFY(f.shell->shutdown());f.shell.reset();QVERIFY(!guard);
    }
    void actualAnimationFrames(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *clip=visual(root,"playerBackdropClip");auto *panel=visual(root,"nextQueuePanel");QVERIFY(clip&&panel);
        for(int scenario=0;scenario<5;++scenario){
            const bool opening=scenario!=1; if(scenario==3)w->resize(1000,660);
            if(scenario==4){QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QTest::qWait(500);}
            QVERIFY(QMetaObject::invokeMethod(w,opening?"showLyrics":"closeLyrics"));
            QElapsedTimer timer;timer.start();bool reversed=false;int frames=0;double minY=1e9,maxY=-1e9;double minH=1e9,maxH=-1e9;
            while(timer.elapsed()<600){
                // Reverse an observed intermediate frame. A synchronous
                // screenshot may consume the first 100 ms before Qt's
                // animation clock has advanced at all on a loaded runner.
                const auto progress=w->property("playerProgress").toDouble();
                if(scenario==2&&progress>.1&&progress<.9&&!reversed){const auto before=clip->height();QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QVERIFY(qAbs(clip->height()-before)<2);reversed=true;}
                const auto r=rect(panel);minY=qMin(minY,r.y());maxY=qMax(maxY,r.y());minH=qMin(minH,clip->height());maxH=qMax(maxH,clip->height());
                record("animation-frame",{{"scenario",scenario},{"ms",double(timer.elapsed())},{"x",r.x()},{"y",r.y()},{"width",r.width()},{"height",r.height()},{"opacity",panel->opacity()},{"visible",panel->isVisible()},{"atLeft",w->property("upcomingAtLeft").toBool()},{"backdropHeight",clip->height()},{"progress",w->property("playerProgress").toDouble()},{"coverRunning",w->property("coverTravelling").toBool()}});
                if(frames%12==0)shot(w,QString("animation-%1-frame-%2").arg(scenario).arg(frames));++frames;QTest::qWait(10);
            }
            record("animation-summary",{{"scenario",scenario},{"frames",frames},{"minY",minY},{"maxY",maxY},{"minBackdrop",minH},{"maxBackdrop",maxH},{"finalProgress",w->property("playerProgress").toDouble()}});
            QVERIFY(frames>15);
            if(scenario==2)QVERIFY2(reversed,"No intermediate frame was observed for reversal");
            // Reversing early intentionally travels only a short distance;
            // full transitions must leave the bottom edge, reversal must move
            // and settle without jumping (checked above at the reversal).
            if(scenario==2)QVERIFY(maxY-minY>1);else QVERIFY(maxY-minY>20);
            QVERIFY(maxH-minH>20);
            const bool finalOpen=opening&&scenario!=2;QTRY_VERIFY(qAbs(w->property("playerProgress").toDouble()-(finalOpen?1:0))<.001);
            QCOMPARE(w->property("upcomingAtLeft").toBool(),finalOpen);
        }
    }
    void localAiPositiveNegativeAndPersistence(){
        Fixture f(2);QVERIFY(prepare(f));auto *bridge=f.shell->translationBridge();auto *m=f.shell->lyricsModel();
        QTRY_COMPARE(m->count(),2);QCOMPARE(f.http.requestCount(),0);QVERIFY(bridge->classify(1));
        auto token=bridge->prepareAi();QVERIFY(!token.isEmpty());QVERIFY(!bridge->start("0"));bridge->declineConsent();QVERIFY(!bridge->start(token));QCOMPARE(f.http.requestCount(),0);
        auto respond=[&]{f.http.enqueue(completion(QString::fromUtf8(translationJson({{0,"你好月亮"},{1,"你好世界"}}))));};
        respond();token=bridge->prepareAi();QVERIFY(!token.isEmpty());QVERIFY(bridge->start(token));QVERIFY(!bridge->start(token));QTRY_VERIFY(bridge->preview());
        QCOMPARE(f.http.requestCount(),1);QCOMPARE(LyricsDocumentReader::readFile(f.library.absolute(f.library.track(f.a.id).lyrics)).bytes,f.source);
        record("ai-preview-unsaved",{{"state",bridge->state()},{"requests",requests(f.http)},{"databaseUnchanged",true}});QVERIFY(bridge->discard());QTRY_VERIFY(!m->preview());
        respond();token=bridge->prepareAi();QVERIFY(!token.isEmpty());QVERIFY(bridge->start(token));QTRY_VERIFY(bridge->preview());QVERIFY(bridge->save());QTRY_COMPARE(bridge->state(),QString("Saved"));
        QCOMPARE(f.http.requestCount(),2);const auto bytes=LyricsDocumentReader::readFile(f.library.absolute(f.library.track(f.a.id).lyrics)).bytes;QVERIFY(bytes.contains("relaxer-bilingual"));
        f.shell.reset();f.shell=std::make_unique<QmlShell>(&f.library,nullptr,true,f.runtime.get());QVERIFY(f.shell->load());f.shell->facade()->play({f.a.id,f.b.id},f.a.id);
        QTRY_VERIFY(f.shell->lyricsModel()->data(f.shell->lyricsModel()->index(0),LyricsModel::HasTranslation).toBool());
        record("ai-save-reopen",{{"requests",requests(f.http)},{"savedBytes",QString::fromUtf8(bytes)},{"translation",f.shell->lyricsModel()->data(f.shell->lyricsModel()->index(0),LyricsModel::Translation).toString()}});
        QCOMPARE(f.http.requestCount(),2);QVERIFY(f.shell->warnings().isEmpty());
    }
    void folderBridgeMatrixAndHarnessFixtureDiagnosis(){
        QTemporaryDir dir;const auto data=dir.filePath("data");qint64 count=0;
        {
            Library lib(data);QVERIFY(lib.open());QmlShell shell(&lib,nullptr,true);auto *bridge=shell.importBridge();
            const auto folder=dir.filePath("中文 folder with spaces");QVERIFY(QDir().mkpath(folder+"/子目录"));
            const auto badHarness=folder+QString("/\\u4e2d\\u6587 %1.wav").arg(0);
            const bool created=wave(badHarness);QVERIFY(!created);QCOMPARE(QDir(folder).entryList(QDir::Files).size(),0);
            record("r15-fixture-diagnosis",{{"attemptedPath",badHarness},{"writeSucceeded",created},{"directoryFiles",0},{"cause","R15 literal backslash-u filename creates nonexistent subdirectories; writeWav return was unchecked"}});
            QVERIFY(import_fixture::midi(folder+"/一.mid",1001));QVERIFY(import_fixture::midi(folder+"/二.mid",1002));QVERIFY(import_fixture::midi(folder+"/子目录/three.mid",1003));
            QFile text(folder+"/说明.txt");QVERIFY(text.open(QIODevice::WriteOnly));text.write("not audio");text.close();
            const auto one=dir.filePath("one.mid"),two=dir.filePath("two.mid"),three=dir.filePath("three.mid");
            QVERIFY(import_fixture::midi(one,1004));QVERIFY(import_fixture::midi(two,1005));QVERIFY(import_fixture::midi(three,1006));
            auto start=[&](QStringList paths){QList<QUrl> urls;for(const auto &path:paths)urls<<QUrl::fromLocalFile(path);return bridge->start(urls);};
            QVERIFY(start({one}));QTRY_VERIFY_WITH_TIMEOUT(!bridge->busy(),30000);QCOMPARE(lib.trackCount(),qint64(1));record("import-single",QJsonObject::fromVariantMap(bridge->result()));
            QVERIFY(start({two,three}));QTRY_VERIFY_WITH_TIMEOUT(!bridge->busy(),30000);QCOMPARE(lib.trackCount(),qint64(3));record("import-multiple",QJsonObject::fromVariantMap(bridge->result()));
            QVERIFY(start({folder}));QTRY_VERIFY_WITH_TIMEOUT(!bridge->busy(),30000);QCOMPARE(lib.trackCount(),qint64(6));QCOMPARE(bridge->result()["ignoredPaths"].toInt(),1);record("import-recursive-folder",QJsonObject::fromVariantMap(bridge->result()));
            QVERIFY(start({folder}));QTRY_VERIFY_WITH_TIMEOUT(!bridge->busy(),30000);QCOMPARE(bridge->result()["duplicates"].toInt(),3);record("import-repeat",QJsonObject::fromVariantMap(bridge->result()));
            const auto empty=dir.filePath("empty");QVERIFY(QDir().mkpath(empty));
            QVERIFY(start({empty}));QTRY_VERIFY_WITH_TIMEOUT(!bridge->busy(),30000);record("import-empty",QJsonObject::fromVariantMap(bridge->result()));
            QVERIFY(start({dir.filePath("does not exist")}));QTRY_VERIFY_WITH_TIMEOUT(!bridge->busy(),30000);QCOMPARE(bridge->result()["ignoredPaths"].toInt(),1);record("import-missing",QJsonObject::fromVariantMap(bridge->result()));
            const auto broken=dir.filePath("broken.wav"),fresh=dir.filePath("fresh.mid");QFile bad(broken);QVERIFY(bad.open(QIODevice::WriteOnly));bad.write("bad wave");bad.close();QVERIFY(import_fixture::midi(fresh,1007));
            QVERIFY(start({broken,fresh}));QTRY_VERIFY_WITH_TIMEOUT(!bridge->busy(),30000);QCOMPARE(bridge->result()["failed"].toInt(),1);QCOMPARE(bridge->result()["added"].toInt(),1);QVERIFY(!bridge->result()["errors"].toStringList().isEmpty());record("import-partial",QJsonObject::fromVariantMap(bridge->result()));
            QTRY_COMPARE(shell.libraryModel()->count(),7);count=lib.trackCount();QCOMPARE(QSqlDatabase::connectionNames().size(),1);QVERIFY(shell.shutdown());
        }
        Library reopened(data);QVERIFY(reopened.open());QCOMPARE(reopened.trackCount(),count);record("import-reopened",{{"tracks",double(reopened.trackCount())},{"connections",QSqlDatabase::connectionNames().size()}});
    }
    void importCompletionRenderedInMainWindow_data(){
        QTest::addColumn<int>("theme");QTest::newRow("light")<<0;QTest::newRow("dark")<<1;
    }
    void importCompletionRenderedInMainWindow(){
        QFETCH(int,theme);
        QTemporaryDir dir(QDir::tempPath()+"/Fix2 中文 spaces-XXXXXX");QVERIFY(dir.isValid());
        Library lib(dir.filePath("资料 data"));QVERIFY(lib.open());const auto schema=import_fixture::schema(lib);
        QmlShell shell(&lib,nullptr,true);QVERIFY(shell.settingsBridge()->setTheme(theme));QVERIFY(shell.load());
        auto *w=shell.window();w->showNormal();w->setGeometry(40,45,1240,790);QVERIFY(QTest::qWaitForWindowExposed(w));
        auto *panel=visual(w->contentItem(),"importPanel"),*label=visual(w->contentItem(),"importStatusText");QVERIFY(panel&&label);
        auto *dialog=w->findChild<QObject *>("importDialog");QVERIFY(dialog);
        auto *bridge=shell.importBridge();QSignalSpy frames(w,&QQuickWindow::frameSwapped);
        auto writeWave=[&](const QString &path,int variation){
            QFile file(path);if(!file.open(QIODevice::WriteOnly))return false;
            QDataStream data(&file);data.setByteOrder(QDataStream::LittleEndian);
            data.writeRawData("RIFF",4);data<<quint32(32036);data.writeRawData("WAVEfmt ",8);
            data<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);
            data.writeRawData("data",4);data<<quint32(32000);
            for(int index=0;index<16000;++index)data<<qint16(((index*variation)%127-63)*100);
            return data.status()==QDataStream::Ok&&file.flush()&&file.size()==32044;
        };
        const auto a=dir.filePath("第一首 song.wav"),b=dir.filePath("第二首 song.wav"),c=dir.filePath("第三首 song.wav"),d=dir.filePath("第四首 song.wav");
        QVERIFY(writeWave(a,1));QVERIFY(writeWave(b,2));QVERIFY(writeWave(c,3));QVERIFY(writeWave(d,4));
        const auto missing=dir.filePath("不存在的 目录"),empty=dir.filePath("正常 空目录");QVERIFY(QDir().mkpath(empty));
        const auto broken=dir.filePath("损坏 音频.wav");QFile bad(broken);QVERIFY(bad.open(QIODevice::WriteOnly));QCOMPARE(bad.write("bad WAV"),qint64(7));bad.close();
        QJsonArray allSamples;QJsonArray matrix;int busyRendered=0;
        auto submit=[&](const QStringList &paths){
            QVariantList urls;for(const auto &path:paths)urls.append(QUrl::fromLocalFile(path));
            QVariant accepted;const bool called=QMetaObject::invokeMethod(dialog,"submitFiles",Q_RETURN_ARG(QVariant,accepted),Q_ARG(QVariant,QVariant(urls)));
            return called&&accepted.toBool();
        };
        auto sample=[&](const QString &scenario,qint64 ms,bool screenshot){
            const int before=frames.size();w->requestUpdate();const auto image=w->grabWindow();
            int ink=0;const auto region=rect(label);const double scale=image.width()/double(w->width());
            const auto pixels=QRect(qRound(region.x()*scale),qRound(region.y()*scale),qRound(region.width()*scale),qRound(region.height()*scale)).intersected(image.rect());
            const QColor background=w->property("designTokens").value<QObject *>()->property("panel").value<QColor>();
            for(int y=pixels.top();y<=pixels.bottom();y+=2)for(int x=pixels.left();x<=pixels.right();x+=2){
                const auto color=image.pixelColor(x,y);if(qMax(qAbs(color.red()-background.red()),qMax(qAbs(color.green()-background.green()),qAbs(color.blue()-background.blue())))>25)++ink;
            }
            const bool visible=panel->isVisible()&&label->isVisible()&&!image.isNull()&&ink>8;
            if(bridge->busy()&&visible)++busyRendered;
            allSamples.append(QJsonObject{{"scenario",scenario},{"ms",double(ms)},{"busy",bridge->busy()},{"panelVisible",panel->isVisible()},
                {"labelVisible",label->isVisible()},{"text",label->property("text").toString()},{"inkPixels",ink},
                {"rendered",visible},{"frameSwapsBefore",before},{"frameSwapsAfter",frames.size()},
                {"panelY",panel->y()},{"panelHeight",panel->height()},{"windowWidth",w->width()},{"windowHeight",w->height()}});
            if(screenshot){const auto path=QDir(output).filePath(QString("%1-import-%2-%3-%4.png").arg(++serial,4,10,QChar('0')).arg(theme).arg(scenario).arg(ms));QVERIFY(image.save(path));}
        };
        qint64 tracks=0;
        auto execute=[&](const QString &name,const QStringList &paths,int added,int duplicates,int failed,int ignored,bool waitExpiry){
            QVERIFY(submit(paths));QVERIFY(bridge->busy());QVERIFY(panel->isVisible());
            QVERIFY(!panel->property("completionVisible").toBool());sample(name+"-busy",0,true);
            QTRY_VERIFY_WITH_TIMEOUT(!bridge->busy(),30000);
            QCOMPARE(bridge->result()["added"].toInt(),added);QCOMPARE(bridge->result()["duplicates"].toInt(),duplicates);
            QCOMPARE(bridge->result()["failed"].toInt(),failed);QCOMPARE(bridge->result()["ignoredPaths"].toInt(),ignored);
            tracks+=added;QTRY_COMPARE(shell.libraryModel()->count(),tracks);QCOMPARE(lib.trackCount(),tracks);
            QVERIFY(panel->isVisible());QCOMPARE(label->property("text").toString(),bridge->status());
            if(name=="missing")QVERIFY(label->property("text").toString().contains("未找到可导入的音频文件"));
            QElapsedTimer timer;timer.start();int rendered=0;const int start=allSamples.size();
            do{
                if(name=="missing"&&timer.elapsed()>250&&timer.elapsed()<400){
                    QVERIFY(shell.appBridge()->navigate(3));w->resize(1000,660);
                    QVERIFY(QMetaObject::invokeMethod(dialog,"open"));QVERIFY(QMetaObject::invokeMethod(dialog,"close"));
                }
                sample(name,timer.elapsed(),rendered==0||timer.elapsed()>3700&&timer.elapsed()<3850);
                const auto row=allSamples.last().toObject();if(row["rendered"].toBool()&&!row["busy"].toBool())++rendered;
                QVERIFY(panel->x()>=0&&panel->width()<=w->width());QVERIFY(panel->y()>=0&&panel->y()+panel->height()<=w->height());
                QTest::qWait(60);
            }while(waitExpiry?panel->isVisible()&&timer.elapsed()<5200:timer.elapsed()<180);
            QVERIFY(rendered>0);
            if(waitExpiry){QVERIFY(!panel->isVisible());QVERIFY(timer.elapsed()>=3500&&timer.elapsed()<5200);}
            matrix.append(QJsonObject{{"scenario",name},{"theme",theme},{"result",QJsonObject::fromVariantMap(bridge->result())},
                {"finalText",bridge->status()},{"visibleRenderedSamples",rendered},{"sampleStart",start},{"observedMs",double(timer.elapsed())},{"autoHidden",!panel->isVisible()}});
        };
        execute("missing",{missing},0,0,0,1,true);QVERIFY(!QTest::currentTestFailed());
        execute("empty",{empty},0,0,0,0,false);QVERIFY(!QTest::currentTestFailed());
        execute("success",{a,b},2,0,0,0,false);QVERIFY(!QTest::currentTestFailed());
        execute("duplicate",{a},0,1,0,0,false);QVERIFY(!QTest::currentTestFailed());
        execute("mixed-ignored",{c,missing},1,0,0,1,false);QVERIFY(!QTest::currentTestFailed());
        execute("partial-failure",{d,broken},1,0,1,0,true);QVERIFY(!QTest::currentTestFailed());
        QStringList cancelFiles;for(int index=10;index<58;++index){const auto path=dir.filePath(QString("取消 %1.wav").arg(index));QVERIFY(writeWave(path,index));cancelFiles.append(path);}
        QVERIFY(submit(cancelFiles));QVERIFY(bridge->busy());QVERIFY(panel->isVisible());QVERIFY(bridge->cancel());
        QTRY_VERIFY_WITH_TIMEOUT(!bridge->busy(),30000);QVERIFY(bridge->result()["cancelled"].toBool());
        QVERIFY(panel->isVisible());QCOMPARE(label->property("text").toString(),bridge->status());QVERIFY(bridge->status().startsWith("已取消"));sample("cancelled",0,true);
        tracks+=bridge->result()["added"].toInt();QTRY_COMPARE(shell.libraryModel()->count(),tracks);QCOMPARE(lib.trackCount(),tracks);
        matrix.append(QJsonObject{{"scenario","cancelled"},{"result",QJsonObject::fromVariantMap(bridge->result())},{"finalText",bridge->status()}});
        QVERIFY(busyRendered>0);QCOMPARE(import_fixture::schema(lib),schema);QCOMPARE(QSqlDatabase::connectionNames().size(),1);
        record("import-main-gui",{{"theme",theme},{"source","real Main.qml ImportDialog.submitFiles; native Qt synthetic events, no physical mouse"},
            {"matrix",matrix},{"samples",allSamples},{"busyRenderedSamples",busyRendered},{"frameSwaps",frames.size()},{"schemaUnchanged",true}});
        QVERIFY(shell.warnings().isEmpty());QVERIFY(shell.shutdown());
    }
    void userReviewExitWarnsAndOnlyConfirmationStopsSearch(){
        QTest::failOnWarning(QRegularExpression("QProcess: Destroyed while process.*"));
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();
        auto *service=f.shell->findChild<LyricsSearchCoordinator *>();QVERIFY(service);
        Track missing;missing.title="Unfinished search";missing.hash="unfinished-search";missing.file="Music/unfinished-search.wav";missing.duration=60000;missing.id=f.library.insert(missing);QVERIFY(missing.id);QVERIFY(wave(f.library.absolute(missing.file)));
        // All actions below occur before processing the queued search pump.
        // This exercises the real start/close/cancel path without contacting a source.
        auto *bridge=f.shell->lyricsSearchBridge();QVERIFY(bridge->setTargetTracks({QString::number(missing.id)},true));
        QVERIFY(bridge->searchTargets(true));QVERIFY(bridge->busy());
        QVERIFY(!w->close());auto *confirm=w->findChild<QObject *>("closeTasksConfirmation");QVERIFY(confirm);QVERIFY(confirm->property("visible").toBool());
        auto *body=qvariant_cast<QQuickItem *>(confirm->property("contentItem"));QVERIFY(body);QVERIFY(body->property("text").toString().contains("还有歌词尚未查找完毕"));
        QVERIFY(QMetaObject::invokeMethod(confirm,"reject"));QVERIFY(f.shell->lyricsSearchBridge()->busy());QVERIFY(w->isVisible());
        QVERIFY(QMetaObject::invokeMethod(w,"requestQuit"));QVERIFY(confirm->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(confirm,"accept"));
        QVERIFY(!f.shell->lyricsSearchBridge()->busy());QCOMPARE(service->summary().requests,0);
        record("unfinished-search-exit",{{"warningShown",true},{"rejectedExitKeptTask",true},{"confirmedExitStoppedTask",true},{"networkRequests",service->summary().requests}});
        QVERIFY(f.shell->shutdown());
    }
    void userReviewCompletedWorkspaceCanAlwaysOpen(){
        Fixture f(2);QVERIFY(prepare(f));auto *w=f.shell->window();
        const QVariant keys=QStringList{QString::number(f.a.id),QString::number(f.b.id)};
        QVERIFY(QMetaObject::invokeMethod(w,"openLyricsTools",Q_ARG(QVariant,keys),Q_ARG(QVariant,QVariant(true))));
        auto *workspace=w->findChild<QObject *>("lyricsWorkspace");QVERIFY(workspace);QTRY_VERIFY(workspace->property("visible").toBool());
        const auto rows=f.shell->lyricsSearchBridge()->targetTracks();QCOMPARE(rows.size(),2);
        for(const auto &value:rows){const auto row=value.toMap();QCOMPARE(row["searchState"].toString(),QString("complete"));
            auto *icon=visual(w->contentItem(),"lyricsStateIcon-"+row["trackKey"].toString());QVERIFY(icon);QCOMPARE(icon->property("text").toString(),QString("✓"));}
        record("completed-search-workspace-reopened",{{"tracks",QJsonArray::fromVariantList(rows)}});shot(w,"completed-search-workspace");
    }
    void userReviewSmoothReturnAndFixedTimestamp(){
        Fixture f(60);QVERIFY(prepare(f));auto *w=f.shell->window();auto *p=f.shell->facade();auto *m=f.shell->lyricsModel();
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_VERIFY(w->property("playerProgress").toDouble()>.999);QTRY_COMPARE(m->count(),60);QTRY_VERIFY(p->seekable());
        auto *panel=visual(w->contentItem(),"lyricsPanel");auto *list=visual(w->contentItem(),"lyricsList");QVERIFY(panel&&list);
        auto *follow=w->findChild<QObject *>("lyricsFollowMotion");QVERIFY(follow);
        for(bool reduced:{false,true}){
            QVERIFY(f.shell->settingsBridge()->setPreference("reduceMotion",reduced));
            p->seek(reduced?4000:24000);QTRY_COMPARE(m->currentIndex(),reduced?4:24);
            QJsonArray samples;QElapsedTimer timer;timer.start();QSet<int> positions;
            while(timer.elapsed()<500){const auto y=list->property("contentY").toDouble();positions.insert(qRound(y));samples.append(QJsonObject{{"ms",double(timer.elapsed())},{"y",y},{"running",follow->property("running").toBool()}});QTest::qWait(20);}
            record("smooth-scroll-with-reduce-motion",{{"reduceMotion",reduced},{"frames",samples},{"distinctPositions",positions.size()}});QVERIFY(positions.size()>3);
        }
        const auto point=list->mapToScene(QPointF(list->width()/2,list->height()/2));
        auto wheel=[&]{QWheelEvent event(point,w->mapToGlobal(point.toPoint()),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);event.setTimestamp(++QTest::lastMouseTimestamp);QCoreApplication::sendEvent(w,&event);};
        wheel();QTRY_VERIFY(panel->property("manualScroll").toBool());QTest::qWait(80);QVERIFY(QMetaObject::invokeMethod(list,"cancelFlick"));
        list->setProperty("contentY",list->property("contentY").toDouble()+800);QTest::qWait(70);
        auto *stamp=visual(w->contentItem(),"lyricsPreviewTime");QVERIFY(stamp);QVERIFY(stamp->isVisible());
        const auto fixedY=rect(stamp).center().y();QVERIFY(qAbs(fixedY-rect(panel).center().y())<1);
        const auto selected=panel->property("previewIndex").toInt();QVERIFY(selected>=0);const auto time=m->timestampAt(selected);
        record("fixed-preview-timestamp",{{"row",selected},{"ms",double(time)},{"label",stamp->property("text").toString()},{"fixedY",fixedY}});shot(w,"manual-scroll-timestamp");
        QVERIFY(QMetaObject::invokeMethod(stamp,"click"));QTRY_VERIFY(qAbs(p->position()-time)<250);QTRY_VERIFY(!panel->property("manualScroll").toBool());QTRY_VERIFY(!follow->property("running").toBool());
        wheel();QTest::qWait(2300);QVERIFY(panel->property("manualScroll").toBool());wheel();
        QTest::mouseMove(w,point.toPoint());QTest::qWait(1000);QVERIFY(panel->property("manualScroll").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!panel->property("manualScroll").toBool(),3500);
        record("three-second-return",{{"resetBySecondWheel",true},{"hoverDidNotBlockReturn",true},{"manualScroll",panel->property("manualScroll").toBool()}});
        QTRY_VERIFY(!follow->property("running").toBool());QVERIFY(!stamp->isVisible());
    }
    void userReviewToolsOnlyHideAfterLeavingQuestionAndMenu(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_VERIFY(w->property("playerProgress").toDouble()>.999);
        auto *panel=visual(w->contentItem(),"lyricsPanel");auto *question=visual(w->contentItem(),"lyricToolsQuestion");QVERIFY(panel&&question);
        QTRY_VERIFY(!w->property("coverTravelling").toBool());
        QTest::mouseMove(w,QPoint(30,40));QTest::qWait(60);
        QTest::mouseMove(w,rect(panel).center().toPoint());QTest::qWait(200);
        record("question-hover-initial",{{"opacity",question->opacity()},{"panelVisible",panel->isVisible()},{"panelX",rect(panel).x()},{"panelY",rect(panel).y()},{"panelWidth",panel->width()},{"panelHeight",panel->height()}});shot(w,"question-hover-initial");QTRY_VERIFY(question->opacity()>.99);
        QTest::mouseMove(w,rect(question).center().toPoint());QTest::qWait(3150);QVERIFY(question->opacity()>.99);
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,rect(question).center().toPoint());
        auto *menu=w->findChild<QObject *>("lyricToolsMenu");QVERIFY(menu);QTRY_VERIFY(menu->property("visible").toBool());
        auto *body=qvariant_cast<QQuickItem *>(menu->property("contentItem"));QVERIFY(body);
        QTest::mouseMove(w,rect(body).center().toPoint());QTest::qWait(3150);QVERIFY(menu->property("visible").toBool());
        auto *art=visual(w->contentItem(),"currentArtwork");QVERIFY(art);QTest::mouseMove(w,rect(art).center().toPoint());
        QTest::qWait(1500);QVERIFY(menu->property("visible").toBool());QTRY_VERIFY_WITH_TIMEOUT(!menu->property("visible").toBool(),2500);
        record("question-menu-idle",{{"staysWhileQuestionHovered",true},{"staysWhileMenuHovered",true},{"closedAfterLeavingBoth",true}});
    }
    void userReviewPageInteriorNeverFlashesBlack_data(){
        QTest::addColumn<bool>("reduced");
        QTest::addColumn<int>("theme");
        QTest::newRow("light-reduced-animation")<<true<<0;
        QTest::newRow("light-normal-animation")<<false<<0;
        QTest::newRow("dark-reduced-animation")<<true<<1;
        QTest::newRow("dark-normal-animation")<<false<<1;
    }
    void userReviewPageInteriorNeverFlashesBlack(){
        QFETCH(bool,reduced);
        QFETCH(int,theme);
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();
        QVERIFY(f.shell->settingsBridge()->setTheme(theme));
        QVERIFY(f.shell->settingsBridge()->setPreference("reduceMotion",reduced));
        auto *paper=visual(w->contentItem(),"paperSurface");QVERIFY(paper);
        auto *surface=visual(w->contentItem(),"pageSurface");QVERIFY(surface);
        for(int destination:{1,3,0}){
            auto *button=visual(w->contentItem(),"navigation"+QString::number(destination));QVERIFY(button);
            QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,rect(button).center().toPoint());
            if(destination==3){
                w->showMinimized();QTRY_COMPARE(w->visibility(),QWindow::Minimized);
                w->showNormal();QVERIFY(QTest::qWaitForWindowExposed(w));
            }
            QElapsedTimer timer;timer.start();QJsonArray frames;double minimumMean=255,maximumDark=0;QImage worst;
            while(timer.elapsed()<400){
                const auto image=w->grabWindow();QVERIFY(!image.isNull());const auto area=rect(paper);const auto dpr=image.devicePixelRatio();
                int sum=0,dark=0,count=0;
                // Interior pixels matter: a shrunken black snapshot can leave
                // the outer padding white while obscuring the entire page.
                for(double y=.05;y<.96;y+=.05)for(double x=.05;x<.96;x+=.05){
                    const int value=qGray(image.pixelColor(qRound((area.x()+area.width()*x)*dpr),qRound((area.y()+area.height()*y)*dpr)).rgb());
                    sum+=value;dark+=value<(theme==1?12:60);++count;
                }
                const double mean=double(sum)/count,ratio=double(dark)/count;
                if(mean<minimumMean){minimumMean=mean;worst=image;}
                maximumDark=qMax(maximumDark,ratio);
                frames.append(QJsonObject{{"ms",double(timer.elapsed())},{"mean",mean},{"darkRatio",ratio},{"surfaceOpacity",surface->opacity()}});
                QTest::qWait(7);
            }
            const auto path=QDir(output).filePath(QString("%1-page-interior-%2-%3.png").arg(++serial,4,10,QChar('0')).arg(reduced).arg(destination));
            QVERIFY(worst.save(path));
            record("sidebar-page-interior",{{"reduced",reduced},{"theme",theme},{"restoredDuringTransition",destination==3},{"destination",destination},{"minimumMean",minimumMean},{"maximumDarkRatio",maximumDark},{"frames",frames}});
            QCOMPARE(f.shell->appBridge()->page(),destination);
            QVERIFY2(minimumMean>(theme==1?17:190)&&maximumDark<.15,"Page interior flashed black during navigation or window restore");
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void userReviewPageFramesAndSettingsLayout(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();QVERIFY(f.shell->settingsBridge()->setTheme(0));
        auto *paper=visual(w->contentItem(),"paperSurface");QVERIFY(paper);
        for(int destination:{1,3,0}){
            auto *button=visual(w->contentItem(),"navigation"+QString::number(destination));QVERIFY(button);
            QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,rect(button).center().toPoint());
            QElapsedTimer timer;timer.start();QJsonArray frames;int darkest=255;
            while(timer.elapsed()<500){
                const auto image=w->grabWindow();QVERIFY(!image.isNull());const auto area=rect(paper);const auto dpr=image.devicePixelRatio();
                int light=255;for(double fraction:{.2,.5,.8}){const auto c=image.pixelColor(qRound((area.x()+5)*dpr),qRound((area.y()+area.height()*fraction)*dpr));light=qMin(light,qGray(c.rgb()));}
                darkest=qMin(darkest,light);frames.append(QJsonObject{{"ms",double(timer.elapsed())},{"edgeBrightness",light}});QTest::qWait(12);
            }
            record("sidebar-no-black-frame",{{"destination",destination},{"darkestPaddingPixel",darkest},{"frames",frames}});QVERIFY2(darkest>100,"Light-theme page padding flashed black");
        }
        f.shell->appBridge()->navigate(3);QTest::qWait(80);
        auto *cover=visual(w->contentItem(),"playerCoverBackground");auto *reduce=visual(w->contentItem(),"reduceMotionSetting");QVERIFY(cover&&reduce);
        QVERIFY(qAbs(rect(cover).center().y()-rect(reduce).center().y())<1);QVERIFY(rect(reduce).left()>rect(cover).left());
        auto *settings=visual(w->contentItem(),"settingsScroll");QVERIFY(settings);auto *flick=qvariant_cast<QQuickItem *>(settings->property("contentItem"));QVERIFY(flick);QCOMPARE(flick->property("boundsBehavior").toInt(),0);
        auto *shortcuts=visual(w->contentItem(),"shortcutsPage");auto *app=visual(w->contentItem(),"shortcut-app-toggle");auto *global=visual(w->contentItem(),"shortcut-global-toggle");QVERIFY(shortcuts&&app&&global);
        const auto a=rect(app),g=rect(global);
        record("settings-widths-raw",{{"appWidth",a.width()},{"globalWidth",g.width()},{"cardWidth",shortcuts->width()}});shot(w,"settings-widths-raw");
        QVERIFY(qAbs(a.width()-g.width())<1);QVERIFY(a.width()>180);
        QVERIFY(rect(shortcuts).right()-g.right()<65);
        record("settings-review-layout",{{"appWidth",a.width()},{"globalWidth",g.width()},{"remainingRight",rect(shortcuts).right()-g.right()},{"boundsBehavior",flick->property("boundsBehavior").toInt()}});
        flick->setProperty("contentY",qMax(0.,flick->property("contentHeight").toDouble()-flick->height()));shot(w,"settings-review-layout");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);C6EvidenceTests tests;return QTest::qExec(&tests,argc,argv);}
#include "qml-c6-evidence-tests.moc"
