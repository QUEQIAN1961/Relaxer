#include "ui-bridge/qml-shell.h"
#include "translation-fixture.h"
#include "import-fixture.h"
#include "window.h"
#include <QApplication>
#include <QComboBox>
#include <QSignalSpy>
#include <QSpinBox>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
class BrowserFake final:public TranslationBrowserBackend {
public:
    QList<quint64> calls;int resumes=0;
    void translate(quint64 token,const QString &,const QString &,const QString &)override{calls.append(token);}
    void cancel()override{}void resume()override{++resumes;}void shutdown()override{}
};
bool wave20(const QString &path){
    QFile file(path);if(!file.open(QIODevice::WriteOnly))return false;QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF",4);out<<quint32(320036);out.writeRawData("WAVEfmt ",8);
    out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);
    out.writeRawData("data",4);out<<quint32(320000);QByteArray pcm(320000,'\0');out.writeRawData(pcm.data(),pcm.size());return out.status()==QDataStream::Ok;
}
struct Fixture {
    QTemporaryDir dir;Library library{dir.filePath("data")};TranslationHttpMock http;
    std::unique_ptr<AiRuntime> runtime;std::unique_ptr<QmlShell> shell;Track a,b;bool ok=false;
    QByteArray source="[00:01.00]Hello moon\n[00:02.00]Walk home\n";
    Fixture(){
        if(!library.open()||!http.listen())return;a=seed(library,"Alpha","a");b=seed(library,"Beta","b");
        if(!a.id||!b.id||!wave20(library.absolute(a.file))||!wave20(library.absolute(b.file))||!library.saveLyrics(a.id,source)||!library.saveLyrics(b.id,source))return;
        runtime=std::make_unique<AiRuntime>(&library,DeepSeekProvider::MockEndpoint{http.baseUrl()},std::make_unique<FakeCredentialStore>());
        if(!configureRuntime(*runtime))return;shell=std::make_unique<QmlShell>(&library,nullptr,true,runtime.get());shell->facade()->play({a.id,b.id},a.id);ok=true;
    }
    void response(const QString &prefix="A"){http.enqueue(completion(QString::fromUtf8(translationJson({{0,prefix+"月亮你好"},{1,prefix+"走回家"}}))));}
    QByteArray bytes(qint64 id){return LyricsDocumentReader::readFile(library.absolute(library.track(id).lyrics)).bytes;}
};
}
class QmlC4C5BridgeTests:public QObject {
    Q_OBJECT
private slots:
    void settingsDraftValidationAndWidgetsRoundTrip(){
        QTemporaryDir dir;Library library(dir.path());QVERIFY(library.open());const auto schema=import_fixture::schema(library);
        {
            SettingsBridge bridge(&library);QVERIFY(bridge.edit("threshold",91));QCOMPARE(library.setting("threshold",80).toInt(),80);
            bridge.resetDraft();QCOMPARE(bridge.draft()["threshold"].toInt(),80);
            QVERIFY(bridge.edit("threshold",69));QVERIFY(!bridge.save());QCOMPARE(library.setting("threshold",80).toInt(),80);
            bridge.resetDraft();QVERIFY(!bridge.edit("unknown",true));QVERIFY(bridge.edit("threshold",91));QVERIFY(bridge.edit("theme",1));
            QVERIFY(bridge.edit("showLyricTranslation",false));QVERIFY(bridge.save());
        }
        {
            Window widgets(&library);widgets.showSettings();
            auto *theme=widgets.findChild<QComboBox *>("themeSetting");auto *threshold=widgets.findChild<QSpinBox *>("thresholdSetting");QVERIFY(theme);QVERIFY(threshold);
            QCOMPARE(theme->currentIndex(),1);QCOMPARE(threshold->value(),91);theme->setCurrentIndex(0);threshold->setValue(95);widgets.close();
        }
        SettingsBridge bridge(&library);QCOMPARE(bridge.values()["theme"].toInt(),0);QCOMPARE(bridge.values()["threshold"].toInt(),95);
        QVERIFY(!bridge.values()["showLyricTranslation"].toBool());QCOMPARE(import_fixture::schema(library),schema);
        QVERIFY(bridge.savePalette("夜色","#15171c","#21252d","#ff819d"));QCOMPARE(library.setting("theme").toInt(),3);
        QCOMPARE(bridge.appearances().size(),1);QVERIFY(bridge.useAppearance(bridge.appearances().first().toMap()["id"].toString()));
        QVERIFY(!bridge.savePalette("","bad","#fff","#123456"));
    }
    void realImportFilesFolderDuplicatesAndRefresh(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto schema=import_fixture::schema(library);
        const auto folder=dir.filePath("中文 folder");QVERIFY(QDir().mkpath(folder));
        const auto one=folder+"/one 音乐.wav",two=folder+"/two.mid";QVERIFY(wave20(one));QVERIFY(import_fixture::midi(two,42));
        ImportCoordinator service(&library);ImportBridge bridge(&library,&service);TrackListModel source(&library);TrackFilterModel model(&source);AppBridge app(&library);
        connect(&bridge,&ImportBridge::contentsChanged,&source,&TrackListModel::refresh);
        connect(&bridge,&ImportBridge::contentsChanged,&app,&AppBridge::refresh);
        const auto list=library.createPlaylist("Import");QVERIFY(bridge.start({QUrl::fromLocalFile(one)},QString::number(list)));
        QVERIFY(!bridge.start({QUrl::fromLocalFile(two)}));QTRY_VERIFY_WITH_TIMEOUT(!bridge.busy(),30000);
        qInfo().noquote()<<QJsonDocument(QJsonObject::fromVariantMap(bridge.result())).toJson(QJsonDocument::Compact);
        QVERIFY2(bridge.result()["added"].toInt()==1,qPrintable(bridge.status()+" "+bridge.result()["errors"].toStringList().join(";")));
        QTRY_COMPARE(model.count(),1);QTRY_COMPARE(app.trackCount(),1);
        QCOMPARE(library.tracks(list).size(),1);QCOMPARE(bridge.result()["added"].toInt(),1);
        model.setSelectedId(model.idAt(0));const auto selection=model.selectedId();
        QVERIFY(bridge.start({QUrl::fromLocalFile(folder)}));QTRY_VERIFY_WITH_TIMEOUT(!bridge.busy(),30000);QTRY_COMPARE(model.count(),2);QCOMPARE(model.selectedId(),selection);
        QCOMPARE(bridge.result()["added"].toInt(),1);QCOMPARE(bridge.result()["duplicates"].toInt(),1);
        QVERIFY(!bridge.start({QUrl("https://example.invalid/audio.mp3")}));
        QVERIFY(bridge.start({QUrl::fromLocalFile(dir.filePath("missing.wav"))}));QTRY_VERIFY_WITH_TIMEOUT(!bridge.busy(),30000);QCOMPARE(library.trackCount(),qint64(2));
        QVERIFY(bridge.result()["ignoredPaths"].toInt()+bridge.result()["failed"].toInt()>0||!bridge.result()["fatalError"].toString().isEmpty());
        const auto empty=dir.filePath("empty");QVERIFY(QDir().mkpath(empty));QVERIFY(bridge.start({QUrl::fromLocalFile(empty)}));QTRY_VERIFY_WITH_TIMEOUT(!bridge.busy(),30000);QCOMPARE(library.trackCount(),qint64(2));
        const auto fresh=dir.filePath("new.mid"),broken=dir.filePath("broken.wav");QVERIFY(import_fixture::midi(fresh,999));QFile bad(broken);QVERIFY(bad.open(QIODevice::WriteOnly));bad.write("invalid audio fixture");bad.close();
        QVERIFY(bridge.start({QUrl::fromLocalFile(fresh),QUrl::fromLocalFile(broken)}));QTRY_VERIFY_WITH_TIMEOUT(!bridge.busy(),30000);
        QCOMPARE(bridge.result()["added"].toInt(),1);QCOMPARE(bridge.result()["failed"].toInt(),1);QVERIFY(bridge.result()["partialSuccess"].toBool());QTRY_COMPARE(model.count(),3);
#ifdef Q_OS_WIN
        const auto locked=dir.filePath("access denied.wav");QVERIFY(wave20(locked));
        std::unique_ptr<void,decltype(&CloseHandle)> handle(CreateFileW(reinterpret_cast<LPCWSTR>(locked.utf16()),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr),&CloseHandle);
        QVERIFY(handle.get()!=INVALID_HANDLE_VALUE);QVERIFY(bridge.start({QUrl::fromLocalFile(locked)}));QTRY_VERIFY_WITH_TIMEOUT(!bridge.busy(),30000);
        QCOMPARE(bridge.result()["failed"].toInt(),1);QCOMPARE(library.trackCount(),qint64(3));
#endif
        QCOMPARE(import_fixture::schema(library),schema);QCOMPARE(QSqlDatabase::connectionNames().size(),1);
    }
    void ignoredAndEmptyImportSummaries_data(){
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<int>("added");QTest::addColumn<int>("duplicates");
        QTest::addColumn<int>("failed");QTest::addColumn<int>("ignored");
        QTest::addColumn<bool>("noAudioHint");
        QTest::newRow("missing-directory")<<QString("missing")<<0<<0<<0<<1<<true;
        QTest::newRow("empty-directory")<<QString("empty")<<0<<0<<0<<0<<false;
        QTest::newRow("unsupported-file")<<QString("unsupported")<<0<<0<<0<<1<<true;
        QTest::newRow("missing-and-valid-chinese-spaced")<<QString("valid")<<1<<0<<0<<1<<false;
        QTest::newRow("missing-and-corrupt-audio")<<QString("corrupt")<<0<<0<<1<<1<<false;
        QTest::newRow("missing-and-duplicate")<<QString("duplicate")<<0<<1<<0<<1<<false;
    }
    void ignoredAndEmptyImportSummaries(){
        QFETCH(QString,scenario);QFETCH(int,added);QFETCH(int,duplicates);
        QFETCH(int,failed);QFETCH(int,ignored);QFETCH(bool,noAudioHint);
        QTemporaryDir dir(QDir::tempPath()+"/Relaxer 中文 spaces-XXXXXX");QVERIFY(dir.isValid());
        Library library(dir.filePath("资料 data"));QVERIFY(library.open());
        const auto schema=import_fixture::schema(library);
        ImportCoordinator service(&library);ImportBridge bridge(&library,&service);
        const auto missing=dir.filePath("不存在的 音乐目录");QVERIFY(!QFileInfo::exists(missing));
        QList<QUrl> sources;
        if(scenario=="empty"){
            const auto empty=dir.filePath("空 音乐目录");QVERIFY(QDir().mkpath(empty));sources<<QUrl::fromLocalFile(empty);
        }else if(scenario=="unsupported"){
            QFile text(dir.filePath("说明 文件.txt"));QVERIFY(text.open(QIODevice::WriteOnly));
            QCOMPARE(text.write("not audio"),qint64(9));text.close();sources<<QUrl::fromLocalFile(text.fileName());
        }else{
            sources<<QUrl::fromLocalFile(missing);
            if(scenario=="valid"||scenario=="duplicate"){
                const auto midi=dir.filePath("测试 音乐.mid");QVERIFY(import_fixture::midi(midi,123));
                if(scenario=="duplicate"){
                    QVERIFY(bridge.start({QUrl::fromLocalFile(midi)}));QTRY_VERIFY_WITH_TIMEOUT(!bridge.busy(),30000);
                    QCOMPARE(bridge.result()["added"].toInt(),1);
                }
                sources<<QUrl::fromLocalFile(midi);
            }else if(scenario=="corrupt"){
                QFile corrupt(dir.filePath("损坏 音乐.wav"));QVERIFY(corrupt.open(QIODevice::WriteOnly));
                QVERIFY(corrupt.write("invalid audio fixture")>0);corrupt.close();sources<<QUrl::fromLocalFile(corrupt.fileName());
            }
        }
        QVERIFY(bridge.start(sources));QTRY_VERIFY_WITH_TIMEOUT(!bridge.busy(),30000);
        const auto result=bridge.result();
        QCOMPARE(result["added"].toInt(),added);QCOMPARE(result["duplicates"].toInt(),duplicates);
        QCOMPARE(result["failed"].toInt(),failed);QCOMPARE(result["ignoredPaths"].toInt(),ignored);
        QCOMPARE(result["unstarted"].toInt(),0);QVERIFY(!result["cancelled"].toBool());
        QVERIFY(result["fatalError"].toString().isEmpty());
        QCOMPARE(result["errors"].toStringList().isEmpty(),failed==0);
        QCOMPARE(bridge.status().contains("未找到可导入的音频文件"),noAudioHint);
        QVERIFY(bridge.status().contains(QString("新增%1，重复%2，失败%3，未开始0").arg(added).arg(duplicates).arg(failed)));
        if(ignored&&!noAudioHint)QVERIFY(bridge.status().contains("已忽略1个路径"));
        QCOMPARE(library.trackCount(),qint64(added+duplicates));
        QCOMPARE(import_fixture::schema(library),schema);
        qInfo().noquote()<<scenario<<bridge.status()<<QJsonDocument(QJsonObject::fromVariantMap(result)).toJson(QJsonDocument::Compact);
    }
    void importCancellationAndCloseBarrier(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto folder=dir.filePath("many");QVERIFY(QDir().mkpath(folder));
        for(int i=0;i<60;++i)QVERIFY(import_fixture::midi(folder+QString("/%1.mid").arg(i),i));
        auto shell=std::make_unique<QmlShell>(&library,nullptr,true);QSignalSpy closed(shell->importBridge(),&ImportBridge::readyToClose);
        QVERIFY(shell->importBridge()->start({QUrl::fromLocalFile(folder)}));QVERIFY(shell->importBridge()->busy());shell->importBridge()->beginShutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(),1,30000);QVERIFY(!shell->importBridge()->busy());QVERIFY(shell->importBridge()->closing());
        QVERIFY(shell->importCoordinator()->isIdle());QVERIFY(!shell->importBridge()->start({QUrl::fromLocalFile(folder)}));shell.reset();
        auto shell2=std::make_unique<QmlShell>(&library,nullptr,true);QVERIFY(shell2->importBridge()->start({QUrl::fromLocalFile(folder)}));shell2.reset();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QCOMPARE(QSqlDatabase::connectionNames().size(),1);
    }
    void noAutomaticRequestAndExplicitOneShotConsent(){
        Fixture f;QVERIFY(f.ok);auto *bridge=f.shell->translationBridge();QTRY_COMPARE(f.shell->lyricsModel()->count(),2);
        QCOMPARE(f.http.requestCount(),0);QVERIFY(bridge->prepareAi().isEmpty());QCOMPARE(bridge->state(),QString("NoTask"));
        QVERIFY(bridge->classify(1));f.response();const auto task=bridge->prepareAi();QVERIFY(!task.isEmpty());
        QCOMPARE(bridge->state(),QString("WaitingForConsent"));QCOMPARE(f.http.requestCount(),0);QVERIFY(bridge->prepareAi().isEmpty());
        QVERIFY(!bridge->start("999999"));QCOMPARE(f.http.requestCount(),0);QVERIFY(bridge->start(task));QVERIFY(!bridge->start(task));
        QTRY_COMPARE(bridge->state(),QString("CompletedPendingSave"));QCOMPARE(f.http.requestCount(),1);QCOMPARE(f.bytes(f.a.id),f.source);
        QTRY_VERIFY(f.shell->lyricsModel()->preview());QVERIFY(!bridge->cancel());QVERIFY(bridge->discard());
        QTRY_COMPARE(bridge->state(),QString("Cancelled"));QTRY_VERIFY(!f.shell->lyricsModel()->preview());QCOMPARE(f.bytes(f.a.id),f.source);
    }
    void previewSaveCacheAndDecline(){
        Fixture f;QVERIFY(f.ok);auto *bridge=f.shell->translationBridge();QTRY_COMPARE(f.shell->lyricsModel()->count(),2);QVERIFY(bridge->classify(1));
        auto task=bridge->prepareAi();QVERIFY(!task.isEmpty());bridge->declineConsent();QVERIFY(!bridge->start(task));QCOMPARE(f.http.requestCount(),0);
        f.response();task=bridge->prepareAi();QVERIFY(bridge->start(task));QTRY_VERIFY(bridge->preview());QVERIFY(bridge->save());
        QTRY_COMPARE(bridge->state(),QString("Saved"));QTRY_VERIFY(!f.shell->lyricsModel()->preview());QVERIFY(f.bytes(f.a.id).contains("relaxer-bilingual"));
        const auto requests=f.http.requestCount();f.shell->lyricsModel()->reload();QTRY_VERIFY(f.shell->lyricsModel()->data(f.shell->lyricsModel()->index(0),LyricsModel::HasTranslation).toBool());
        QCoreApplication::processEvents();QCOMPARE(f.http.requestCount(),requests);QVERIFY(!bridge->save());
    }
    void callbacksAreBoundToTwoSongsAndDestroyedFrontend(){
        Fixture f;QVERIFY(f.ok);f.http.setHold(true);auto *bridge=f.shell->translationBridge();QTRY_COMPARE(f.shell->lyricsModel()->count(),2);
        QVERIFY(bridge->classify(1));f.response("A");const auto a=bridge->prepareAi();QVERIFY(bridge->start(a));QTRY_COMPARE(f.http.heldCount(),1);
        f.shell->facade()->play({f.a.id,f.b.id},f.b.id);QTRY_COMPARE(f.shell->lyricsModel()->trackKey(),QString::number(f.b.id));
        QVERIFY(bridge->classify(1));f.response("B");const auto b=bridge->prepareAi();QVERIFY(bridge->start(b));QCOMPARE(bridge->state(),QString("Queued"));
        QVERIFY(f.http.releaseHeld());QTRY_COMPARE(f.http.requestCount(),2);QTRY_COMPARE(f.http.heldCount(),1);QVERIFY(!f.shell->lyricsModel()->preview());QCOMPARE(f.bytes(f.b.id),f.source);
        QVERIFY(f.http.releaseHeld());QTRY_VERIFY(bridge->preview());QTRY_COMPARE(f.shell->lyricsModel()->data(f.shell->lyricsModel()->index(0),LyricsModel::Translation).toString(),QString("B月亮你好"));
        f.shell->facade()->play({f.a.id,f.b.id},f.a.id);QTRY_COMPARE(f.shell->lyricsModel()->trackKey(),QString::number(f.a.id));
        QTRY_COMPARE(f.shell->lyricsModel()->data(f.shell->lyricsModel()->index(0),LyricsModel::Translation).toString(),QString("A月亮你好"));QVERIFY(bridge->save());
        QCOMPARE(f.bytes(f.b.id),f.source);QCOMPARE(f.http.requestCount(),2);f.shell.reset();QCoreApplication::processEvents();
    }
    void keyAndConfigurationUseFakeStoreWithoutExposure(){
        Fixture f;QVERIFY(f.ok);auto *bridge=f.shell->aiBridge();
        QVERIFY(bridge->metaObject()->indexOfProperty("apiKey")<0);QVERIFY(!bridge->refreshModels(false));QCOMPARE(f.http.requestCount(),0);
        QVERIFY(bridge->edit("translationStylePrompt","Custom local style"));QVERIFY(bridge->save());QCOMPARE(f.runtime->configuration().translationStylePrompt,QString("Custom local style"));
        QVERIFY(bridge->storeKey("fake-c4c5-secret"));QVERIFY(bridge->checkCredential());
        QVERIFY(!QJsonDocument(QJsonObject::fromVariantMap(bridge->draft())).toJson().contains("fake-c4c5-secret"));
        QVERIFY(!QJsonDocument(f.library.setting("aiServiceConfigV1").toObject()).toJson().contains("fake-c4c5-secret"));
        QVERIFY(!bridge->status().contains("fake-c4c5-secret"));QVERIFY(bridge->removeKey());QVERIFY(!bridge->checkCredential());QCOMPARE(f.http.requestCount(),0);
        QVERIFY(bridge->edit("targetLanguage","en"));QVERIFY(!bridge->save());bridge->resetDraft();QCOMPARE(bridge->draft()["targetLanguage"].toString(),QString("zh-CN"));
    }
    void failureAndConsentTrackSwitchCannotDispatch(){
        Fixture f;QVERIFY(f.ok);auto *bridge=f.shell->translationBridge();QTRY_COMPARE(f.shell->lyricsModel()->count(),2);QVERIFY(bridge->classify(1));
        const auto old=bridge->prepareAi();QVERIFY(!old.isEmpty());f.shell->facade()->play({f.a.id,f.b.id},f.b.id);QVERIFY(!bridge->start(old));QCOMPARE(f.http.requestCount(),0);
        QVERIFY(bridge->classify(1));f.http.enqueue("{\"error\":{\"message\":\"mock failure\"}}",500);const auto next=bridge->prepareAi();QVERIFY(bridge->start(next));
        QTRY_COMPARE(bridge->state(),QString("Failed"));QCOMPARE(f.http.requestCount(),1);QVERIFY(!bridge->save());QCOMPARE(f.bytes(f.b.id),f.source);
    }
    void browserAttentionResumeAndLateMessagesAreIsolated(){
        Fixture f;QVERIFY(f.ok);f.shell.reset();auto backend=std::make_unique<BrowserFake>();auto *fake=backend.get();
        f.shell=std::make_unique<QmlShell>(&f.library,nullptr,true,f.runtime.get(),std::move(backend));f.shell->facade()->play({f.a.id,f.b.id},f.a.id);
        auto *bridge=f.shell->translationBridge();const auto task=bridge->prepareBrowser("bing","en");QVERIFY(!task.isEmpty());QVERIFY(fake->calls.isEmpty());
        QVERIFY(bridge->start(task));QTRY_COMPARE(fake->calls.size(),1);const auto first=fake->calls.first();
        emit fake->attention(first,"请人工验证");QTRY_COMPARE(bridge->state(),QString("NeedsAttention"));QVERIFY(bridge->resume());QVERIFY(!bridge->resume());QCOMPARE(fake->resumes,1);
        emit fake->completed(first,"第一行译文",{});QTRY_COMPARE(fake->calls.size(),2);emit fake->completed(first,"stale",{});QCoreApplication::processEvents();QCOMPARE(bridge->completedLines(),1);
        emit fake->completed(fake->calls.last(),"第二行译文",{});QTRY_VERIFY(bridge->preview());QCOMPARE(f.bytes(f.a.id),f.source);
        QVERIFY(bridge->discard());QCOMPARE(bridge->state(),QString("Cancelled"));QCOMPARE(f.http.requestCount(),0);
    }
    void closingFrontendCancelsHeldMockWithoutSaving(){
        Fixture f;QVERIFY(f.ok);f.http.setHold(true);f.response();auto *bridge=f.shell->translationBridge();QVERIFY(bridge->classify(1));const auto task=bridge->prepareAi();QVERIFY(bridge->start(task));
        QTRY_COMPARE(f.http.heldCount(),1);QPointer<LyricsModel> model=f.shell->lyricsModel();f.shell.reset();QVERIFY(model.isNull());
        f.http.releaseHeld();QCoreApplication::processEvents();QCOMPARE(f.bytes(f.a.id),f.source);QCOMPARE(f.http.requestCount(),1);
    }
    void modelCatalogRequiresExplicitActionAndDeduplicates(){
        Fixture f;QVERIFY(f.ok);auto *bridge=f.shell->aiBridge();f.http.enqueue("{\"object\":\"list\",\"data\":[{\"id\":\"deepseek-flash\"}]}");
        QVERIFY(!bridge->refreshModels(false));QCOMPARE(f.http.requestCount(),0);QVERIFY(bridge->refreshModels(true));QVERIFY(!bridge->refreshModels(true));
        QTRY_VERIFY(!bridge->busy());QCOMPARE(f.http.requestCount(),1);QVERIFY(!bridge->models().isEmpty());QVERIFY(f.http.request(0).startsWith("GET /models"));
        QCoreApplication::processEvents();QCOMPARE(f.http.requestCount(),1);
    }
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);QmlC4C5BridgeTests tests;return QTest::qExec(&tests,argc,argv);}
#include "qml-c4c5-bridge-tests.moc"
