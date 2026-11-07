#include "c4c5-ui-fixture.h"
#include <QSignalSpy>
#include <QJsonArray>
#include <QBuffer>
#include <QImageReader>
#include <QQmlComponent>
#include <QQmlContext>
#include "online-track-matcher.h"
#include "ncm-restore.h"
#include "ui-bridge/online-recommendation.h"
using namespace c4c5_fixture;
static bool chooseOnlineSource(QQuickWindow *window,int index){auto *page=visual(window->contentItem(),"discoverPage");return page&&QMetaObject::invokeMethod(page,"chooseSource",Q_ARG(QVariant,index));}

class OnlineMusicProbe final:public QObject {
    Q_OBJECT
private slots:
    void savedAiConfigurationRestoresSilentlyAndReportsOnlyProblems(){
        Fixture f;QVERIFY(f.ok);
        const auto makeRuntime=[&]{auto credentials=std::make_unique<FakeCredentialStore>();credentials->store("deepseek","test-only-fake-key",nullptr);return std::make_unique<AiRuntime>(&f.library,DeepSeekProvider::MockEndpoint{f.http.baseUrl()},std::move(credentials));};
        auto restored=makeRuntime();QVERIFY(!restored->validateConfiguration(restored->configuration().defaults).isEmpty());
        QSignalSpy problems(restored.get(),&AiRuntime::startupConfigurationFailed),catalog(restored.get(),&AiRuntime::modelCatalogFinished);
        f.http.enqueue(R"({"object":"list","data":[{"id":"deepseek-flash","context_window":32768,"max_output_tokens":8192,"input_modalities":["text"],"output_modalities":["text"],"effort":{"supported_levels":["low","high","max"],"default_level":"low"}}]})");
        restored->restoreSavedConfiguration();QTRY_COMPARE(catalog.count(),1);QVERIFY2(problems.isEmpty(),qPrintable(problems.isEmpty()?QString():problems.first().first().toString()));QVERIFY(restored->validateConfiguration(restored->configuration().defaults).isEmpty());
        QCOMPARE(restored->configuration().defaults.modelId,QString("deepseek-flash"));restored->restoreSavedConfiguration();QTest::qWait(30);QCOMPARE(f.http.requestCount(),1);
        auto failed=makeRuntime();QSignalSpy failedProblems(failed.get(),&AiRuntime::startupConfigurationFailed);f.http.enqueue(R"({"error":{"message":"Test credential rejected"}})",401);
        failed->restoreSavedConfiguration();QTRY_COMPARE(failedProblems.count(),1);QCOMPARE(f.http.requestCount(),2);
        AiRuntime missing(&f.library,DeepSeekProvider::MockEndpoint{f.http.baseUrl()},std::make_unique<FakeCredentialStore>());QSignalSpy missingProblems(&missing,&AiRuntime::startupConfigurationFailed);missing.restoreSavedConfiguration();QCOMPARE(missingProblems.count(),1);QCOMPARE(f.http.requestCount(),2);
    }
    void recommendationInputAndOutputFitTheSelectedModel(){
        QVector<Track> tracks;QVariantList songs;
        for(int i=0;i<300;++i){Track t;t.title=QString(300,QChar(0x6b4c));t.artist=QString(300,QChar(0x624b));t.album=t.title;t.favorite=i%2;t.heard=i*10000;tracks.append(t);songs.append(QVariantMap{{"id",QString::number(i+1)},{"title",t.title},{"artist",t.artist},{"album",t.album}});}
        for(const qint64 context:{4096,8192,32768}){
            auto model=confirmedFlash(context);model.capabilities.maxOutputTokens=1024;
            const auto request=recommendationRequest({"deepseek","deepseek-flash",true,"max"},model,tracks,songs);QVERIFY(!request.messages.isEmpty());QVERIFY(request.maxTokens<=1024);QVERIFY(!request.thinkingEnabled);
            qint64 bytes=128;for(const auto &message:request.messages)bytes+=message.toObject().value("content").toString().toUtf8().size();QVERIFY(bytes+request.maxTokens+256<=context);
            const auto data=QJsonDocument::fromJson(request.messages[1].toObject().value("content").toString().toUtf8()).object();QVERIFY(data.value("candidates").toArray().size()>=10);QVERIFY(data.value("candidates").toArray().size()<=80);QVERIFY(request.messages[0].toObject().value("content").toString().contains("10首"));
        }
    }
    void publicSearchCategoriesAndCollectionTracks(){
        if(!qEnvironmentVariableIsSet("RELAXER_SEARCH_CATEGORIES_PROBE"))QSKIP("Focused public search check is opt-in");
        Fixture f;QVERIFY(f.ok);
        const QStringList kinds{"search","search-artists","search-albums","search-playlists"};
        for(auto *service:{static_cast<QObject *>(f.shell->onlineMusicBridge()),static_cast<QObject *>(f.shell->qqMusicBridge())}){
            for(int category=0;category<4;++category){
                QVERIFY(QMetaObject::invokeMethod(service,"showPage",Q_ARG(QString,kinds[category]),Q_ARG(QString,QString("周杰伦")),Q_ARG(bool,false)));
                QVERIFY2(QTest::qWaitFor([&]{return !service->property("catalogLoading").toBool()&&!service->property("busy").toBool();},25000),qPrintable(service->property("status").toString()));
                const auto results=service->property(category?"searchResults":"songs").toList();qInfo()<<service->metaObject()->className()<<kinds[category]<<results.size()<<service->property("status").toString();QVERIFY(!results.isEmpty());
                if(category==1||category==2){const auto row=results.first().toMap();QVERIFY(!row.value("id").toString().isEmpty());QVERIFY(!row.value("name").toString().isEmpty());QVERIFY(!row.value("cover").toString().isEmpty());
                    QVERIFY(QMetaObject::invokeMethod(service,"showPage",Q_ARG(QString,row.value("kind").toString()),Q_ARG(QString,row.value("id").toString()),Q_ARG(bool,false)));
                    QVERIFY2(QTest::qWaitFor([&]{return !service->property("catalogLoading").toBool()&&!service->property("busy").toBool();},25000),qPrintable(service->property("status").toString()));
                    qInfo()<<"collection tracks"<<service->property("songs").toList().size()<<service->property("status").toString();QVERIFY(!service->property("songs").toList().isEmpty());
                }
            }
        }
    }
    void downloadsUseTheDefaultFolderWithoutChoosingAFile(){
        Fixture f;QVERIFY(f.ok);auto *downloads=f.shell->onlineDownloadBridge();QCOMPARE(downloads->downloadDirectory(),QDir(f.library.root()).filePath("Downloads"));
        QObject::disconnect(downloads,&OnlineDownloadBridge::resolveRequested,nullptr,nullptr);TranslationHttpMock server;QVERIFY(server.listen());QSignalSpy completed(downloads,&OnlineDownloadBridge::downloaded);
        connect(downloads,&OnlineDownloadBridge::resolveRequested,downloads,[&](const QVariantMap &song,const QString &){downloads->chooseDestination(song,server.baseUrl().resolved(QUrl("/audio")),"flac");});
        const QByteArray flac("fLaCfixture"),mp3("ID3fixture");server.enqueue(flac);QVariantMap first{{"id","8101"},{"title","Default FLAC"},{"artist","Fixture"}};downloads->enqueue("netease",first,"hires");QVERIFY(downloads->busy());QTRY_COMPARE(completed.count(),1);
        auto saved=completed.first().first().toString();QCOMPARE(QFileInfo(saved).absolutePath(),downloads->downloadDirectory());QCOMPARE(QFileInfo(saved).suffix(),QString("flac"));QFile file(saved);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),flac);file.close();
        server.enqueue(mp3);QVariantMap second{{"provider","qq"},{"id","8102"},{"mid","default-mp3"},{"title","Default MP3"},{"artist","Fixture"}};downloads->chooseDestination(second,server.baseUrl().resolved(QUrl("/audio")),"mp3");QTRY_COMPARE(completed.count(),2);saved=completed.last().first().toString();QCOMPARE(QFileInfo(saved).absolutePath(),downloads->downloadDirectory());QFile secondFile(saved);QVERIFY(secondFile.open(QIODevice::ReadOnly));QCOMPARE(secondFile.readAll(),mp3);secondFile.close();
        const auto count=downloads->rowCount();downloads->enqueue("netease",first,"hires");QCOMPARE(downloads->rowCount(),count);QCOMPARE(server.requestCount(),2);
        auto *w=f.shell->window();w->showNormal();QVERIFY(f.shell->appBridge()->navigate(3));const auto folder=qEnvironmentVariable("RELAXER_HEADER_SCREENSHOTS");if(!folder.isEmpty())QDir().mkpath(folder);
        for(const auto size:{QSize(1240,790),QSize(414,790)}){
            w->resize(size);QTest::qWait(400);auto *root=w->contentItem();auto *path=visual(root,"downloadDirectoryText"),*open=visual(root,"openDownloadDirectory"),*local=visual(root,"openDataRoot");QVERIFY(path&&open&&local);QCOMPARE(path->property("text").toString(),downloads->downloadDirectory());QCOMPARE(open->property("text").toString(),QString("打开默认下载目录"));const auto p=path->mapRectToScene(path->boundingRect()),b=open->mapRectToScene(open->boundingRect()),l=local->mapRectToScene(local->boundingRect());QVERIFY(qAbs(p.center().y()-b.center().y())<1);QVERIFY(qAbs(b.right()-l.right())<1);QVERIFY(b.top()>l.bottom());QVERIFY(p.width()>0);QVERIFY(p.right()<b.left());if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath(size.width()>500?"default-download-directory-wide.png":"default-download-directory-narrow.png"));
        }
        QVERIFY(f.shell->appBridge()->navigate(4));
        for(const auto size:{QSize(1240,790),QSize(414,540)}){
            w->resize(size);QTest::qWait(400);auto *root=w->contentItem();auto *heading=visual(root,"discoverHeading"),*download=visual(root,"onlineDownloadsButton"),*account=visual(root,"onlineAccountButton");QVERIFY(heading&&download&&account);const auto h=heading->mapRectToScene(heading->boundingRect()),d=download->mapRectToScene(download->boundingRect()),a=account->mapRectToScene(account->boundingRect());QVERIFY(qAbs(h.center().y()-d.center().y())<1);QVERIFY(h.right()<d.left());QVERIFY(a.right()<=w->width());if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath(size.width()>500?"discovery-header-wide.png":"discovery-header-narrow.png"));
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void downloadQueueIsSerialReportsBytesAndKeepsNewestFirst(){
        Fixture f;QVERIFY(f.ok);auto *downloads=f.shell->onlineDownloadBridge();
        QObject::disconnect(downloads,&OnlineDownloadBridge::resolveRequested,nullptr,nullptr);
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QList<QPointer<QTcpSocket>> held;int requests=0;
        connect(&server,&QTcpServer::newConnection,&server,[&]{while(server.hasPendingConnections()){
            auto *socket=server.nextPendingConnection();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            connect(socket,&QTcpSocket::readyRead,&server,[&,socket]{const auto input=socket->readAll();if(!input.contains("GET ")||socket->property("sent").toBool())return;socket->setProperty("sent",true);++requests;held.append(socket);
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: 12000000\r\nConnection: close\r\n\r\n");socket->write("ID3"+QByteArray(10499997,'a'));socket->flush();});
        }});
        QSignalSpy resolved(downloads,&OnlineDownloadBridge::resolveRequested),finished(downloads,&OnlineDownloadBridge::downloaded),resets(downloads,&QAbstractItemModel::modelReset);
        connect(downloads,&OnlineDownloadBridge::resolveRequested,downloads,[&](const QVariantMap &song,const QString &quality){QCOMPARE(quality,QString("hires"));downloads->chooseDestination(song,QUrl(QString("http://127.0.0.1:%1/audio").arg(server.serverPort())),"mp3");});
        const QVariantMap first{{"provider","netease"},{"id","7101"},{"title","Queue first"},{"artist","Fixture"},{"duration",180000}},second{{"provider","qq"},{"id","7102"},{"mid","queue-second"},{"title","Queue second"},{"artist","Fixture"},{"duration",180000}},third{{"provider","netease"},{"id","7103"},{"title","Queue cancelled"},{"artist","Fixture"}};
        QVERIFY(downloads->enqueueTo(first,f.dir.filePath("first.mp3"),"hires"));QTRY_COMPARE(requests,1);QVERIFY(downloads->enqueueTo(second,f.dir.filePath("second.mp3"),"hires"));QVERIFY(downloads->enqueueTo(third,f.dir.filePath("third.mp3"),"hires"));
        QCOMPARE(downloads->rowCount(),3);QCOMPARE(downloads->data(downloads->index(0),Qt::UserRole+1).toString(),QString("Queue cancelled"));QCOMPARE(resolved.count(),1);QVERIFY(!downloads->enqueueTo(first,f.dir.filePath("duplicate.mp3"),"hires"));
        downloads->cancelTask(downloads->task("netease",third).value("taskId").toInt());QCOMPARE(downloads->task("netease",third).value("state").toString(),QString("cancelled"));
        QTRY_COMPARE(downloads->task("netease",first).value("received").toLongLong(),qint64(10500000));QCOMPARE(downloads->task("netease",first).value("total").toLongLong(),qint64(12000000));QCOMPARE(requests,1);QCOMPARE(resets.count(),0);
        auto *w=f.shell->window();w->showNormal();w->resize(1240,790);QVERIFY(f.shell->appBridge()->navigate(4));QTest::qWait(400);auto *button=visual(w->contentItem(),"onlineDownloadsButton");QVERIFY(button);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,button->mapToScene(button->boundingRect().center()).toPoint());
        auto *popup=w->findChild<QObject *>("onlineDownloadList");QVERIFY(popup);QTRY_VERIFY(popup->property("opened").toBool());QTest::qWait(200);auto *bytes=visual(w->contentItem(),"downloadBytes-1");QVERIFY(bytes);QCOMPARE(bytes->property("text").toString(),QString("10.5MB/12MB"));
        const auto folder=qEnvironmentVariable("RELAXER_DOWNLOAD_UI_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath("download-queue-wide.png"));w->resize(414,540);QTest::qWait(350);w->grabWindow().save(QDir(folder).filePath("download-queue-narrow.png"));}
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,button->mapToScene(button->boundingRect().center()).toPoint());QTRY_VERIFY(!popup->property("visible").toBool());
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,button->mapToScene(button->boundingRect().center()).toPoint());QTRY_VERIFY(popup->property("opened").toBool());
        auto *heading=visual(w->contentItem(),"discoverHeading");QVERIFY(heading);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,heading->mapToScene(heading->boundingRect().center()).toPoint());QTRY_VERIFY(!popup->property("visible").toBool());held.first()->write(QByteArray(1500000,'b'));held.first()->disconnectFromHost();QTRY_COMPARE(finished.count(),1);QTRY_COMPARE(requests,2);QCOMPARE(resolved.count(),2);QVERIFY(downloads->isDownloaded("netease",first));
        downloads->cancel();QTRY_VERIFY(!downloads->busy());QCOMPARE(downloads->task("qq",second).value("state").toString(),QString("cancelled"));QVERIFY(!QFileInfo::exists(f.dir.filePath("second.mp3")));QCOMPARE(requests,2);
        // A failed resolver advances the queue; a cancelled resolver's late answer is ignored.
        QObject::disconnect(downloads,&OnlineDownloadBridge::resolveRequested,nullptr,nullptr);QSignalSpy lateResolved(downloads,&OnlineDownloadBridge::resolveRequested);
        auto retry=third;retry["id"]="7104";QVERIFY(downloads->enqueueTo(retry,f.dir.filePath("retry.mp3"),"hires"));QTRY_COMPARE(lateResolved.count(),1);const auto cancelled=lateResolved.first().first().toMap();downloads->cancel();
        QVERIFY(downloads->enqueueTo(third,f.dir.filePath("third.mp3"),"hires"));QTRY_COMPARE(lateResolved.count(),2);downloads->chooseDestination(cancelled,QUrl(QString("http://127.0.0.1:%1/late").arg(server.serverPort())),"mp3");QTest::qWait(100);QCOMPARE(requests,2);
        downloads->resolutionFailed(lateResolved.last().first().toMap(),"Fixture permission denied");QVERIFY(!downloads->busy());QCOMPARE(downloads->task("netease",third).value("state").toString(),QString("failed"));QCOMPARE(resets.count(),0);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void downloadToolbarRemainsVisibleBeforeAndAfterItsArrowAnimation(){
        Fixture f;QVERIFY(f.ok);auto *w=f.shell->window();w->showNormal();w->resize(1000,650);QVERIFY(f.shell->appBridge()->navigate(4));QTest::qWait(400);
        auto *button=visual(w->contentItem(),"onlineDownloadsButton");QVERIFY(button);auto *glyph=button->property("contentItem").value<QQuickItem *>();QVERIFY(glyph);
        const auto check=[&](const QString &name){
            const auto frame=w->grabWindow();const auto rect=glyph->mapRectToScene(glyph->boundingRect());const double sx=double(frame.width())/w->width(),sy=double(frame.height())/w->height();const auto crop=frame.copy(QRectF(rect.x()*sx,rect.y()*sy,rect.width()*sx,rect.height()*sy).toAlignedRect());
            int ink=0;for(int y=0;y<crop.height();++y)for(int x=0;x<crop.width();++x)if(crop.pixelColor(x,y).lightness()<120)++ink;
            const auto folder=qEnvironmentVariable("RELAXER_DOWNLOAD_UI_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);crop.save(QDir(folder).filePath(name+".png"));}
            qInfo()<<name<<"visible icon pixels"<<ink;return ink>12;
        };
        QVERIFY(check("toolbar-idle"));QVERIFY(QMetaObject::invokeMethod(glyph,"added"));QTRY_VERIFY(!glyph->property("animating").toBool());QTest::qWait(100);QVERIFY(check("toolbar-after-arrow"));
        w->hide();w->show();QTest::qWait(200);QVERIFY(check("toolbar-after-show"));QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void downloadGlyphMorphsAndCatalogStatusOmitsPlaybackNoise(){
        Fixture f;QVERIFY(f.ok);auto *w=f.shell->window();w->showNormal();w->resize(840,650);
        QQmlComponent component(qmlEngine(w),QUrl("qrc:/qml/components/DownloadGlyph.qml"));QVERIFY2(component.isReady(),qPrintable(component.errorString()));std::unique_ptr<QObject> object(component.create(qmlContext(w)));auto *glyph=qobject_cast<QQuickItem *>(object.get());QVERIFY(glyph);glyph->setParentItem(w->contentItem());glyph->setPosition({400,240});glyph->setSize({48,48});QTest::qWait(50);
        QCOMPARE(glyph->property("circleAmount").toReal(),0.);QVERIFY(QMetaObject::invokeMethod(glyph,"added"));QTRY_VERIFY(glyph->property("arrowOffset").toReal()>0);QTRY_VERIFY(glyph->property("arrowOffset").toReal()<0);QTRY_VERIFY(!glyph->property("animating").toBool());QCOMPARE(glyph->property("arrowOffset").toReal(),0.);
        const auto folder=qEnvironmentVariable("RELAXER_DOWNLOAD_UI_SCREENSHOTS");if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("icon-download.png"));
        glyph->setProperty("active",true);glyph->setProperty("progress",.65);QTest::qWait(90);const auto amount=glyph->property("circleAmount").toReal();QVERIFY(amount>0&&amount<1);QTRY_COMPARE(glyph->property("circleAmount").toReal(),1.);QTest::qWait(100);if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("icon-progress.png"));
        glyph->setProperty("complete",true);QTest::qWait(70);QVERIFY(glyph->property("checkAmount").toReal()>0&&glyph->property("checkAmount").toReal()<1);QTRY_COMPARE(glyph->property("checkAmount").toReal(),1.);if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("icon-completed.png"));
        QQmlComponent fakeComponent(qmlEngine(w));fakeComponent.setData(R"(import QtQml
QtObject {
    signal changed()
    signal played(string id)
    signal pageRequested(string kind,string argument,bool refresh)
    property bool loggedIn:false;property bool busy:false;property bool catalogLoading:false
    property string nickname:"";property string avatarUrl:"";property string qrImage:"";property string quality:"hires"
    property string status:"歌单 · 199 首歌曲"
    property var playlists:[];property var songs:[];property var searchResults:[];property var currentPlaylist:({id:"123",name:"Test playlist",count:199,cover:""})
    function activate(){} function showPage(kind,argument,refresh){pageRequested(kind,argument,refresh)} function cancelRecommendation(){}
    function playSong(id){played(String(id))}
})",QUrl());QVERIFY2(fakeComponent.isReady(),qPrintable(fakeComponent.errorString()));std::unique_ptr<QObject> fake(fakeComponent.create(qmlContext(w)));QVERIFY(fake);
        fake->setProperty("songs",QVariantList{QVariantMap{{"id","7101"},{"title","Stable row"},{"artist","Fixture"},{"album","Album"},{"reason",""}},QVariantMap{{"id","7102"},{"title","Second row"},{"artist","Fixture"},{"album","Album"},{"reason",""}}});
        fake->setProperty("playlists",QVariantList{QVariantMap{{"id","123"},{"name","First playlist"}},QVariantMap{{"id","456"},{"name","Second playlist"}}});
        QQmlContext context(qmlContext(w));context.setContextProperty("onlineMusic",fake.get());context.setContextProperty("qqMusic",fake.get());
        auto *localPage=visual(w->contentItem(),"localPage"),*pageHost=visual(w->contentItem(),"pageSurface");QVERIFY(localPage&&pageHost);localPage->setVisible(false);glyph->setVisible(false);
        QQmlComponent pageComponent(qmlEngine(w),QUrl("qrc:/qml/pages/DiscoverPage.qml"));std::unique_ptr<QObject> pageObject(pageComponent.createWithInitialProperties({{"parent",QVariant::fromValue(pageHost)}},&context));auto *page=qobject_cast<QQuickItem *>(pageObject.get());QVERIFY2(page,qPrintable(pageComponent.errorString()));page->setParentItem(pageHost);page->setPosition(localPage->mapToItem(pageHost,QPointF()));
        QVERIFY(QMetaObject::invokeMethod(page,"showPage",Q_ARG(QVariant,"playlist"),Q_ARG(QVariant,"123"),Q_ARG(QVariant,false)));
        for(const auto size:{QSize(840,650),QSize(414,540)}){
            page->setSize(size-QSize(36,60));QTest::qWait(80);auto *songs=visual(page,"onlineSongs"),*spinner=visual(page,"onlineCatalogSpinner");QVERIFY(songs&&spinner);const auto top=songs->y();auto *play=visual(page,"onlinePlay-7101");QVERIFY(play);QCOMPARE(play->width(),30.);
            for(int repeat=0;repeat<3;++repeat)for(const auto text:{QString("歌单 · 199 首歌曲"),QString("正在获取网易云播放地址…"),QString("正在播放 · 网易云音乐"),QString("正在获取 QQ 音乐播放地址…"),QString("正在播放 · QQ 音乐")}){fake->setProperty("busy",text.startsWith("正在获取"));fake->setProperty("status",text);QTest::qWait(25);QCOMPARE(songs->y(),top);QVERIFY(!spinner->isVisible());}
            // Silent validation is pending internally while its catalog is already on screen.
            for(int repeat=0;repeat<3;++repeat){fake->setProperty("status","歌单 · 199 首歌曲");fake->setProperty("busy",false);fake->setProperty("catalogLoading",true);QTest::qWait(25);QCOMPARE(songs->y(),top);QVERIFY(!visual(page,"onlineCatalogStatus")->isVisible());fake->setProperty("catalogLoading",false);QTest::qWait(25);QCOMPARE(songs->y(),top);}
        }
        for(const auto &text:{QString("歌单 · 199 首歌曲"),QString("QQ 音乐歌单 · 199 首歌曲"),QString("正在获取网易云播放地址…"),QString("正在获取 QQ 音乐播放地址…"),QString("正在播放 · 网易云音乐"),QString("正在播放 · QQ 音乐")}){QVariant result;QVERIFY(QMetaObject::invokeMethod(page,"visibleStatus",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,text)));QVERIFY(result.toString().isEmpty());}
        for(const auto &text:{QString("当前账号暂不能播放这首歌，请检查登录状态或歌曲权限"),QString("找到 20 首歌曲"),QString("AI 正在结合本地收听记录挑选歌曲…"),QString("已读取 10 首，后续歌曲读取失败，请重新打开歌单重试")}){QVariant result;QVERIFY(QMetaObject::invokeMethod(page,"visibleStatus",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,text)));QCOMPARE(result.toString(),text);}
        QVariant result;QVERIFY(QMetaObject::invokeMethod(page,"visibleStatus",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,"歌单 · 199 首歌曲 · 2 首暂不可用")));QCOMPARE(result.toString(),QString("2 首暂不可用"));
        page->setSize(localPage->size());QTest::qWait(100);
        const auto center=[](QQuickItem *item){return item->mapToScene(item->boundingRect().center()).toPoint();};
        const auto moveAway=[&]{QTest::mouseMove(w,{835,5});QTest::qWait(30);};
        auto *firstRow=visual(page,"onlineSong-7101"),*secondRow=visual(page,"onlineSong-7102");QVERIFY(firstRow&&secondRow);
        QSignalSpy played(fake.get(),SIGNAL(played(QString))),pages(fake.get(),SIGNAL(pageRequested(QString,QString,bool)));
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,center(firstRow));moveAway();QVERIFY(firstRow->property("selected").toBool());QCOMPARE(played.count(),0);
        const auto selectedColor=firstRow->property("color").value<QColor>();QVERIFY(selectedColor.alpha()>0);QVERIFY(!secondRow->property("selected").toBool());
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,center(secondRow));moveAway();QVERIFY(secondRow->property("selected").toBool());QVERIFY(!firstRow->property("selected").toBool());QCOMPARE(secondRow->property("color").value<QColor>(),selectedColor);
        QTest::mouseDClick(w,Qt::LeftButton,Qt::NoModifier,center(secondRow));QCOMPARE(played.count(),1);QCOMPARE(played.first().first().toString(),QString("7102"));
        auto *download=visual(page,"downloadOnline-7102"),*play=visual(page,"onlinePlay-7102");QVERIFY(download&&play);
        auto *downloadBackground=download->property("background").value<QQuickItem *>(),*playBackground=play->property("background").value<QQuickItem *>();QVERIFY(downloadBackground&&playBackground);
        QTest::mouseMove(w,center(download));QTRY_VERIFY(download->property("hovered").toBool());const auto hoverColor=downloadBackground->property("color").value<QColor>();QVERIFY(hoverColor.alpha()>0);
        if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("discovery-song-selected-download-hover.png"));
        QTest::mouseMove(w,center(play));QTRY_VERIFY(play->property("hovered").toBool());QCOMPARE(playBackground->property("color").value<QColor>(),hoverColor);
        auto *downloads=f.shell->onlineDownloadBridge();QObject::disconnect(downloads,&OnlineDownloadBridge::resolveRequested,nullptr,nullptr);QSignalSpy resolved(downloads,&OnlineDownloadBridge::resolveRequested);
        QTest::mouseMove(w,center(download));QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,center(download));QTRY_COMPARE(resolved.count(),1);QTRY_VERIFY(download->property("pending").toBool());QVERIFY(downloadBackground->property("color").value<QColor>().alpha()>0);
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,center(download));QCOMPARE(downloads->rowCount(),1);
        f.http.enqueue("ID3fixture");downloads->chooseDestination(resolved.first().first().toMap(),f.http.baseUrl().resolved(QUrl("/audio")),"mp3");QTRY_VERIFY(download->property("downloaded").toBool());
        QVERIFY(downloadBackground->property("color").value<QColor>().alpha()>0);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,center(download));QCOMPARE(downloads->rowCount(),1);
        QVERIFY(QMetaObject::invokeMethod(page,"showHome"));QTest::qWait(100);auto *firstCard=visual(page,"onlinePlaylist-123"),*secondCard=visual(page,"onlinePlaylist-456");QVERIFY(firstCard&&secondCard);auto *cardBackground=visual(firstCard,"onlinePlaylistBackground");QVERIFY(cardBackground);
        const auto requests=pages.count();QTest::mouseMove(w,center(firstCard));QTRY_VERIFY(cardBackground->property("color").value<QColor>().alpha()>0);
        if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("discovery-playlist-hover.png"));
        moveAway();QCOMPARE(cardBackground->property("color").value<QColor>().alpha(),0);
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,center(firstCard));QCOMPARE(pages.count(),requests+1);QCOMPARE(pages.last().at(0).toString(),QString("playlist"));QCOMPARE(pages.last().at(1).toString(),QString("123"));QVERIFY(!page->property("showPlaylists").toBool());
        QVERIFY(QMetaObject::invokeMethod(page,"showHome"));QTest::qWait(80);secondCard=visual(page,"onlinePlaylist-456");QVERIFY(secondCard);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,center(secondCard));QCOMPARE(pages.count(),requests+3);QCOMPARE(pages.last().at(1).toString(),QString("456"));
        QVERIFY(QMetaObject::invokeMethod(page,"chooseSource",Q_ARG(QVariant,1)));QTest::qWait(50);QVERIFY(!visual(page,"onlineSong-7102")->property("selected").toBool());
        auto *actions=visual(page,"onlinePlaylistActions"),*back=visual(page,"onlinePlaylistBack"),*pin=visual(page,"pinOnlinePlaylist");QVERIFY(actions&&back&&pin);QCOMPARE(actions->height(),56.);QCOMPARE(back->height()+pin->height()+4,actions->height());QVERIFY(pin->property("text").toString().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(page,"submitSearch",Q_ARG(QVariant,"Saved search")));QTest::qWait(30);QVERIFY(visual(page,"onlineSearchTabs")->isVisible());QVERIFY(!visual(page,"onlineDiscoveryTabs")->isVisible());
        if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("discovery-search-categories.png"));
        for(int category=0;category<4;++category){auto *button=visual(page,"onlineSearchCategory-"+QString::number(category));QVERIFY(button);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,center(button));QCOMPARE(pages.last().at(0).toString(),QStringList({"search","search-artists","search-albums","search-playlists"})[category]);QCOMPARE(pages.last().at(1).toString(),QString("Saved search"));}
        page->setVisible(false);page->setVisible(true);QTest::qWait(30);QVERIFY(!page->property("searchMode").toBool());QCOMPARE(page->property("searchQuery").toString(),QString("Saved search"));QCOMPARE(pages.last().at(0).toString(),QString("featured"));
        QVERIFY(QMetaObject::invokeMethod(page,"submitSearch",Q_ARG(QVariant,"Clear me")));QVERIFY(QMetaObject::invokeMethod(page,"editSearch",Q_ARG(QVariant,"")));QVERIFY(!page->property("searchMode").toBool());
        QVERIFY(QMetaObject::invokeMethod(page,"showPage",Q_ARG(QVariant,"playlist"),Q_ARG(QVariant,"123"),Q_ARG(QVariant,false)));
        for(const int width:{840,414}){
            w->resize(width,650);QTest::qWait(420);page->setSize(localPage->size());QTest::qWait(60);
            auto *heading=visual(page,"onlinePlaylistHeading");QVERIFY(heading&&heading->isVisible());QCOMPARE(actions->height(),heading->height());
            QTest::mouseMove(w,center(back));QTRY_VERIFY(back->property("hovered").toBool());QVERIFY(back->property("background").value<QQuickItem *>()->property("color").value<QColor>().alpha()>0);
            QTest::mouseMove(w,center(pin));QTRY_VERIFY(pin->property("hovered").toBool());QVERIFY(pin->property("background").value<QQuickItem *>()->property("color").value<QColor>().alpha()>0);
            if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath(QString("discovery-playlist-actions-%1.png").arg(width)));
        }
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,center(back));QCOMPARE(pages.last().at(0).toString(),QString("featured"));
        pageObject.reset();localPage->setVisible(true);w->resize(840,650);QTest::qWait(420);moveAway();auto *metadata=visual(w->contentItem(),"playerMetadata"),*metadataBackground=visual(w->contentItem(),"playerMetadataHover");QVERIFY(metadata&&metadataBackground);QCOMPARE(metadataBackground->property("color").value<QColor>().alpha(),0);
        QTest::mouseMove(w,center(metadata));QTRY_VERIFY(metadataBackground->property("color").value<QColor>().alpha()>0);if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("player-metadata-hover.png"));
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,center(metadata));QVERIFY(w->property("lyricsOpen").toBool());QTRY_VERIFY(!w->property("coverTravelling").toBool());QCOMPARE(metadata->opacity(),0.);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void pinningDailyResultsAlsoSavesTheirPlaylistCache(){
        QTemporaryDir folder;Library library(folder.filePath("data"));QVERIFY(library.open());const QVariantMap snapshot{{"songs",QVariantList{QVariantMap{{"id","1"},{"title","Saved track"}}}},{"playlist",QVariantMap{{"id","123"},{"provider","qq"},{"name","Daily list"}}},{"status","ready"}};
        {OnlinePageCache cache(&library,"qq");QVariantMap restored;QCOMPARE(cache.begin("daily",false,restored),OnlinePageCache::Fetch);cache.finish(snapshot);QVERIFY(library.pinCloudPlaylist("qq","123","Daily list","",1)>0);cache.savePinned("playlist:123");QCOMPARE(cache.begin("playlist:123",false,restored),OnlinePageCache::Cached);QCOMPARE(restored,snapshot);}
        {OnlinePageCache cache(&library,"qq");QVariantMap restored;QCOMPARE(cache.begin("playlist:123",false,restored),OnlinePageCache::Validate);QCOMPARE(restored,snapshot);QVERIFY(cache.quiet());cache.finish(restored);QVERIFY(!cache.quiet());}
    }
    void discoveryPagesReuseResultsUntilExplicitRefresh(){
        if(!qEnvironmentVariableIsSet("RELAXER_DISCOVERY_CACHE_PROBE"))QSKIP("Focused public catalog cache check is opt-in");
        Fixture f;QVERIFY(f.ok);const auto qqSession=qEnvironmentVariable("RELAXER_QQ_SAVED_SESSION");if(!qqSession.isEmpty()){QDir().mkpath(QDir(f.library.root()).filePath("Online"));QVERIFY(QFile::copy(qqSession,QDir(f.library.root()).filePath("Online/qq-session.bin")));}auto *w=f.shell->window();w->showNormal();w->resize(1240,790);QVERIFY(f.shell->appBridge()->navigate(4));
        auto *page=visual(w->contentItem(),"discoverPage");QVERIFY(page);auto *ne=f.shell->onlineMusicBridge();auto *qq=f.shell->qqMusicBridge();
        for(auto *service:{static_cast<QObject *>(ne),static_cast<QObject *>(qq)}){
            const bool isQq=service==qq;QVERIFY(chooseOnlineSource(w,isQq?1:0));auto *network=service->findChild<QNetworkAccessManager *>();QVERIFY(network);QSignalSpy requests(network,&QNetworkAccessManager::finished);
            QVERIFY(QTest::qWaitFor([&]{return !service->property("catalogLoading").toBool()&&!service->property("playlists").toList().isEmpty();},20000));const auto featured=service->property("playlists").toList();const int initial=requests.count();
            for(int i=0;i<4;++i)QVERIFY(QMetaObject::invokeMethod(page,"showHome"));QTest::qWait(200);QCOMPARE(requests.count(),initial);QCOMPARE(service->property("playlists").toList(),featured);
            QVERIFY(QMetaObject::invokeMethod(page,"showPage",Q_ARG(QVariant,"search"),Q_ARG(QVariant,QStringLiteral("莫扎特")),Q_ARG(QVariant,false)));QVERIFY(QTest::qWaitFor([&]{return !service->property("catalogLoading").toBool();},20000));QVERIFY2(!service->property("songs").toList().isEmpty(),qPrintable(service->property("status").toString()));const auto searchRows=service->property("songs").toList();const int searchRequests=requests.count();
            QVERIFY(QMetaObject::invokeMethod(page,"showHome"));QVERIFY(QMetaObject::invokeMethod(page,"showPage",Q_ARG(QVariant,"search"),Q_ARG(QVariant,QStringLiteral("莫扎特")),Q_ARG(QVariant,false)));QTest::qWait(200);QCOMPARE(requests.count(),searchRequests);QCOMPARE(service->property("songs").toList(),searchRows);
            auto *refresh=visual(w->contentItem(),"onlineRefresh");QVERIFY(refresh);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,refresh->mapToScene(refresh->boundingRect().center()).toPoint());QVERIFY(QTest::qWaitFor([&]{return !service->property("catalogLoading").toBool()&&requests.count()>searchRequests;},20000));
            f.http.setHold(true);QVERIFY(QMetaObject::invokeMethod(page,"showPage",Q_ARG(QVariant,"ai"),Q_ARG(QVariant,""),Q_ARG(QVariant,false)));QVERIFY(QTest::qWaitFor([&]{return f.http.heldCount()>0;},20000));const auto candidate=service->property("songs").toList().first().toMap();
            f.http.enqueue(completion(QString::fromUtf8(QJsonDocument(QJsonObject{{"recommendations",QJsonArray{QJsonObject{{"id",candidate.value("id").toString()},{"reason","缓存验证"}}}}}).toJson(QJsonDocument::Compact))));QVERIFY(f.http.releaseHeld());QVERIFY(QTest::qWaitFor([&]{return !service->property("catalogLoading").toBool();},10000));const auto aiRows=service->property("songs").toList();QCOMPARE(aiRows.size(),1);const auto apiCount=requests.count();const auto aiCount=f.http.requestCount();
            QVERIFY(QMetaObject::invokeMethod(page,"showHome"));QVERIFY(QMetaObject::invokeMethod(page,"showPage",Q_ARG(QVariant,"ai"),Q_ARG(QVariant,""),Q_ARG(QVariant,false)));QTest::qWait(200);QCOMPARE(requests.count(),apiCount);QCOMPARE(f.http.requestCount(),aiCount);QCOMPARE(service->property("songs").toList(),aiRows);f.http.setHold(false);
        }
        auto *bar=visual(w->contentItem(),"playerBar"),*cover=visual(w->contentItem(),"bottomCover"),*play=visual(w->contentItem(),"playPauseButton");QVERIFY(bar&&cover&&play);QVERIFY(qAbs(bar->height()-98*.9)<.01);QVERIFY(qAbs(play->width()-42*.9)<.01);const auto coverRect=cover->mapRectToItem(bar,cover->boundingRect());QVERIFY(qAbs(coverRect.left()-coverRect.top())<.01);QVERIFY(qAbs(coverRect.top()-(bar->height()-coverRect.bottom()))<.01);QCOMPARE(w->property("miniWidth").toInt(),340);QCOMPARE(w->property("miniHeight").toInt(),58);
        const auto folder=qEnvironmentVariable("RELAXER_DISCOVERY_CACHE_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath("discovery-cached-ai-wide.png"));w->resize(414,540);QTest::qWait(450);w->grabWindow().save(QDir(folder).filePath("discovery-cached-ai-narrow.png"));}
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));qInfo()<<"Both providers reuse featured/search/AI results; only the refresh button adds catalog requests; footer and play button are 90%, mini dimensions unchanged";
    }
    void pinnedCatalogRestoresImmediatelyAndIdenticalValidationDoesNotReset(){
        if(!qEnvironmentVariableIsSet("RELAXER_DISCOVERY_CACHE_PROBE"))QSKIP("Focused pinned catalog persistence check is opt-in");
        Fixture f;QVERIFY(f.ok);QStringList keys;QList<QVariantList> snapshots;
        for(const auto &provider:{QString("netease"),QString("qq")}){
            const auto id=provider=="qq"?QString("8044204090"):QString("3778678");const auto key=f.shell->libraryBridge()->pinCloudPlaylist({{"id",id},{"provider",provider},{"name",provider+" cached playlist"},{"cover",""},{"count",0}});QVERIFY(key!="0");keys.append(key);
            QVERIFY(f.shell->libraryBridge()->openPlaylist(key));QObject *service=provider=="qq"?static_cast<QObject *>(f.shell->qqMusicBridge()):static_cast<QObject *>(f.shell->onlineMusicBridge());QVERIFY(QTest::qWaitFor([&]{return !service->property("catalogLoading").toBool()&&f.shell->libraryModel()->count()>100;},25000));snapshots.append(service->property("songs").toList());
        }
        QTest::qWait(150);f.shell.reset();f.shell=std::make_unique<QmlShell>(&f.library,nullptr,true,f.runtime.get());QVERIFY(f.shell->load());auto *w=f.shell->window();w->showNormal();w->resize(1240,790);
        for(int i=0;i<keys.size();++i){
            QObject *service=i?static_cast<QObject *>(f.shell->qqMusicBridge()):static_cast<QObject *>(f.shell->onlineMusicBridge());auto *network=service->findChild<QNetworkAccessManager *>();QVERIFY(network);QSignalSpy requests(network,&QNetworkAccessManager::finished);
            QVERIFY(f.shell->libraryBridge()->openPlaylist(keys[i]));QCOMPARE(f.shell->libraryModel()->count(),snapshots[i].size());QVERIFY(service->property("catalogLoading").toBool());QVERIFY(!service->property("busy").toBool());QSignalSpy resets(f.shell->libraryModel(),&QAbstractItemModel::modelReset);
            QVERIFY(QTest::qWaitFor([&]{return !service->property("catalogLoading").toBool();},25000));const auto updated=service->property("songs").toList();const bool same=updated==snapshots[i];if(same)QCOMPARE(resets.count(),0);else{QVERIFY(resets.count()<=1);for(int row=0;row<qMin(updated.size(),snapshots[i].size());++row)if(updated[row]!=snapshots[i][row]){QStringList fields;const auto a=updated[row].toMap(),b=snapshots[i][row].toMap();for(auto it=a.begin();it!=a.end();++it)if(it.value()!=b.value(it.key()))fields.append(it.key());qInfo()<<"Changed catalog row"<<row<<"fields"<<fields;break;}}const auto validationResets=resets.count();QVERIFY(requests.count()>0);const auto count=requests.count();
            QVERIFY(f.shell->libraryBridge()->openPlaylist("0"));QVERIFY(f.shell->libraryBridge()->openPlaylist(keys[i]));QCOMPARE(f.shell->libraryModel()->count(),service->property("songs").toList().size());QTest::qWait(250);QCOMPARE(requests.count(),count);
            auto *source=visual(w->contentItem(),"cloudPlaylistSource"),*heading=visual(w->contentItem(),"libraryHeading");QVERIFY(source&&heading);QCOMPARE(source->property("text").toString(),i?QString("QQ 音乐"):QString("网易云音乐"));const auto sr=source->mapRectToScene(source->boundingRect()),hr=heading->mapRectToScene(heading->boundingRect());QVERIFY(sr.left()>=hr.right());QVERIFY(qAbs(sr.center().y()-hr.center().y())<3);
            qInfo()<<"Pinned cache provider"<<(i?"QQ":"Netease")<<"restored rows"<<snapshots[i].size()<<"validation unchanged"<<same<<"validation model resets"<<validationResets;
        }
        const auto folder=qEnvironmentVariable("RELAXER_DISCOVERY_CACHE_SCREENSHOTS");if(!folder.isEmpty()){w->grabWindow().save(QDir(folder).filePath("cached-cloud-source-wide.png"));w->resize(414,540);QTest::qWait(450);w->grabWindow().save(QDir(folder).filePath("cached-cloud-source-narrow.png"));}
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void pinnedCloudPlaylistsUseLocalLayoutAndKeepPlatformMetadata(){
        if(!qEnvironmentVariableIsSet("RELAXER_CLOUD_LAYOUT_PROBE"))QSKIP("Cloud playlist layout check is opt-in");
        Fixture f;QVERIFY(f.ok);auto *w=f.shell->window();w->showNormal();w->resize(1240,790);auto *bridge=f.shell->libraryBridge();auto *model=f.shell->libraryModel();auto *facade=f.shell->facade();
        auto *netease=f.shell->onlineMusicBridge();auto *qq=f.shell->qqMusicBridge();QObject::disconnect(facade,nullptr,netease,nullptr);QObject::disconnect(facade,nullptr,qq,nullptr);
        connect(facade,&PlaybackFacade::onlineSourceRequested,facade,[&](qint64 id){facade->setOnlineSource(id,QUrl::fromLocalFile(f.library.absolute(f.a.file)));});
        const auto folder=qEnvironmentVariable("RELAXER_CLOUD_LAYOUT_SCREENSHOTS");if(!folder.isEmpty())QDir().mkpath(folder);
        for(const auto &provider:{QString("netease"),QString("qq")}){
            const bool isQq=provider=="qq";const auto remote=isQq?QString("8044204090"):QString("3778678");const int expected=isQq?633:200;
            if(isQq){qq->activate();qq->openPlaylist(remote);QVERIFY2(QTest::qWaitFor([&]{return !qq->busy()&&qq->songs().size()==expected;},25000),qPrintable(qq->status()));}
            else{netease->activate();netease->openPlaylist(remote);QVERIFY2(QTest::qWaitFor([&]{return !netease->busy()&&netease->songs().size()==expected;},25000),qPrintable(netease->status()));}
            const auto metadata=isQq?qq->currentPlaylist():netease->currentPlaylist();const auto key=bridge->pinCloudPlaylist(metadata);QVERIFY(key!="0");
            QVERIFY(QMetaObject::invokeMethod(w,"openPlaylist",Q_ARG(QVariant,key)));QTRY_COMPARE(f.shell->appBridge()->page(),0);QTRY_COMPARE(bridge->playlistId(),key);QTRY_COMPARE_WITH_TIMEOUT(model->count(),expected,25000);QTest::qWait(400);
            auto *page=visual(w->contentItem(),"localPage");QVERIFY(page&&page->isVisible());QVERIFY(page->property("isCloud").toBool());QVERIFY(!visual(w->contentItem(),"discoverMusic"));auto *discover=visual(w->contentItem(),"discoverPage");QVERIFY(!discover||!discover->isVisible());
            QVERIFY(bridge->playlistDetails().value("cover").toUrl().scheme().startsWith("http"));QVERIFY(!visual(w->contentItem(),"playlistUp")->isEnabled());QVERIFY(!visual(w->contentItem(),"playlistDown")->isEnabled());
            QVERIFY(!f.library.renamePlaylist(key.toLongLong(),"Should not rename"));QVERIFY(!f.library.playlistCover(key.toLongLong(),f.dir.filePath("cover.png")));QVERIFY(!f.library.reorder(key.toLongLong(),{}));
            QCOMPARE(bridge->cloudPlaylist(key).value("name"),metadata.value("name"));QCOMPARE(bridge->cloudPlaylist(key).value("cover"),metadata.value("cover"));
            QVERIFY(QMetaObject::invokeMethod(w,"ensurePlaylistEditor"));auto *editor=w->findChild<QObject *>("playlistEditor");QVERIFY(editor);QVERIFY(QMetaObject::invokeMethod(editor,"edit",Q_ARG(QVariant,key),Q_ARG(QVariant,bridge->playlistName())));QTRY_VERIFY(editor->property("opened").toBool());QVERIFY(editor->property("cloud").toBool());QVERIFY(!visual(w->contentItem(),"playlistChooseCover")->isEnabled());
            auto *input=visual(w->contentItem(),"playlistNameInput");QVERIFY(input);const auto remark=provider+QStringLiteral(" 私人备注");input->setProperty("text",remark);editor->setProperty("iconMark","star");input->forceActiveFocus();QTest::keyClick(w,Qt::Key_Return);QTRY_VERIFY(!editor->property("visible").toBool());QTRY_COMPARE(bridge->playlistName(),remark);QCOMPARE(bridge->playlistMarkers().value(key).toString(),QString("icon:star"));QCOMPARE(bridge->cloudPlaylist(key).value("name"),metadata.value("name"));QCOMPARE(bridge->cloudPlaylist(key).value("cover"),metadata.value("cover"));
            {Library reopened(f.library.root());QVERIFY(reopened.open());QCOMPARE(reopened.setting("qml.cloudPlaylistRemarks").toObject().value(key).toString(),remark);}
            QTest::qWait(300);if(!folder.isEmpty()){w->grabWindow().save(QDir(folder).filePath(provider+"-local-wide.png"));w->resize(414,540);QTest::qWait(450);w->grabWindow().save(QDir(folder).filePath(provider+"-local-narrow.png"));w->resize(1240,790);QTest::qWait(300);}
            const auto title=model->data(model->index(0,0),TrackListModel::Title).toString();model->setFilterText(title);QVERIFY(model->count()>0);const auto first=model->idAt(0);bridge->playAll();QTRY_COMPARE(facade->current().id,first.toLongLong());QCOMPARE(facade->queue().items.size(),model->count());QVERIFY(facade->onlineActive());model->clearFilters();QTRY_COMPARE(model->count(),expected);facade->stopOnline();
        }
        QVERIFY(bridge->openPlaylist("0"));QTRY_COMPARE(model->count(),2);QVERIFY(!bridge->playlistDetails().value("cloud").toBool());QCOMPARE(f.library.trackCount(),qint64(2));QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void discoveryQualitySurvivesReopeningAndHeaderMenusFit(){
        Fixture f;QVERIFY(f.ok);auto *netease=f.shell->onlineMusicBridge();auto *qq=f.shell->qqMusicBridge();
        QCOMPARE(netease->quality(),QString("hires"));QCOMPARE(qq->quality(),QString("hires"));netease->setQuality("lossless");QCOMPARE(qq->quality(),QString("lossless"));
        {Library reopened(f.library.root());QVERIFY(reopened.open());QmlShell shell(&reopened,nullptr,true,f.runtime.get());QCOMPARE(shell.onlineMusicBridge()->quality(),QString("lossless"));QCOMPARE(shell.qqMusicBridge()->quality(),QString("lossless"));}
        qq->setQuality("hires");QCOMPARE(netease->quality(),QString("hires"));
        if(qEnvironmentVariableIsSet("RELAXER_HEADER_LIVE_PROBE")){
            QDir().mkpath(QDir(f.library.root()).filePath("Online"));
            for(const auto &provider:{QString("netease"),QString("qq")}){const auto source=qEnvironmentVariable(provider=="qq"?"RELAXER_QQ_SAVED_SESSION":"RELAXER_NETEASE_SAVED_SESSION");QVERIFY(!source.isEmpty());QVERIFY(QFile::copy(source,QDir(f.library.root()).filePath("Online/"+provider+"-session.bin")));}
            netease->activate();qq->activate();QVERIFY(QTest::qWaitFor([&]{return netease->loggedIn()&&qq->loggedIn()&&!netease->busy()&&!qq->busy();},20000));
            QVERIFY(netease->avatarUrl().scheme().startsWith("http"));QVERIFY(qq->avatarUrl().scheme().startsWith("http"));
        }
        auto *w=f.shell->window();w->showNormal();w->resize(1240,790);QVERIFY(f.shell->appBridge()->navigate(4));QTest::qWait(500);
        auto *account=visual(w->contentItem(),"onlineAccountButton");auto *refresh=visual(w->contentItem(),"onlineRefresh");auto *search=visual(w->contentItem(),"onlineSearchButton");QVERIFY(account&&refresh&&search);
        auto *menu=w->findChild<QObject *>("onlineAccountMenu");auto *quality=w->findChild<QObject *>("onlineQualityMenu");QVERIFY(menu&&quality);
        const auto folder=qEnvironmentVariable("RELAXER_HEADER_SCREENSHOTS");if(!folder.isEmpty())QDir().mkpath(folder);
        for(const auto size:{QSize(1240,790),QSize(414,540)}){
            w->resize(size);QTest::qWait(400);const auto accountRect=account->mapRectToScene(account->boundingRect()),refreshRect=refresh->mapRectToScene(refresh->boundingRect()),searchRect=search->mapRectToScene(search->boundingRect());
            QVERIFY(refreshRect.right()<=searchRect.left());QVERIFY(searchRect.right()<=accountRect.left());QVERIFY(accountRect.right()<=w->width());
            auto *heading=visual(w->contentItem(),"discoverHeading");QVERIFY(heading);const auto headingRect=heading->mapRectToScene(heading->boundingRect());QVERIFY(qAbs(headingRect.center().y()-refreshRect.center().y())<1);QVERIFY(headingRect.right()<visual(w->contentItem(),"onlineDownloadsButton")->mapRectToScene(visual(w->contentItem(),"onlineDownloadsButton")->boundingRect()).left());
            QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,account->mapToScene(QPointF(12,18)).toPoint());QTRY_VERIFY(menu->property("opened").toBool());QTest::qWait(280);
            auto *content=menu->property("contentItem").value<QQuickItem *>();QVERIFY(content);auto *entry=visual(content,"onlineQualityEntry");QVERIFY(entry);
            const auto menuBounds=content->mapRectToScene(content->boundingRect());QVERIFY(menuBounds.left()>=0&&menuBounds.right()<=w->width());
            QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,entry->mapToScene(entry->boundingRect().center()).toPoint());QTRY_VERIFY(quality->property("opened").toBool());QTest::qWait(280);
            auto *qualityContent=quality->property("contentItem").value<QQuickItem *>();QVERIFY(qualityContent);const auto qualityBounds=qualityContent->mapRectToScene(qualityContent->boundingRect());QVERIFY(qualityBounds.left()>=0&&qualityBounds.right()<=w->width());
            QVERIFY(qualityBounds.intersected(menuBounds).width()<16);
            QVERIFY(menu->property("width").toReal()<140);QVERIFY(quality->property("width").toReal()<120);QVERIFY(qualityBounds.right()<=menuBounds.left()+4);
            QVERIFY(qAbs(menuBounds.right()+4-accountRect.right())<2);QVERIFY(menuBounds.top()>accountRect.bottom());
            auto *arrow=visual(content,"onlineQualityArrow");QVERIFY(arrow);QCOMPARE(arrow->rotation(),0.);
            if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath(size.width()<500?"account-quality-narrow.png":"account-quality-wide.png"));
            auto *lossless=visual(qualityContent,"onlineQuality-lossless");QVERIFY(lossless);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,lossless->mapToScene(lossless->boundingRect().center()).toPoint());QTRY_VERIFY(!menu->property("visible").toBool()&&!quality->property("visible").toBool());QCOMPARE(netease->quality(),QString("lossless"));
            const auto openMenus=[&]{QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,account->mapToScene(QPointF(12,18)).toPoint());if(!QTest::qWaitFor([&]{return menu->property("opened").toBool();}))return false;QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,entry->mapToScene(entry->boundingRect().center()).toPoint());return QTest::qWaitFor([&]{return quality->property("opened").toBool();});};
            QVERIFY(openMenus());QVERIFY(heading);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,heading->mapToScene(heading->boundingRect().center()).toPoint());QTRY_VERIFY(!menu->property("visible").toBool()&&!quality->property("visible").toBool());
            QVERIFY(openMenus());QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!quality->property("visible").toBool());QVERIFY(menu->property("visible").toBool());QTest::qWait(300);QVERIFY(!quality->property("visible").toBool());QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!menu->property("visible").toBool());
            QVERIFY(openMenus());QTest::mouseClick(w,netease->loggedIn()?Qt::LeftButton:Qt::RightButton,Qt::NoModifier,account->mapToScene(QPointF(12,18)).toPoint());QTRY_VERIFY(!menu->property("visible").toBool()&&!quality->property("visible").toBool());
            QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,search->mapToScene(search->boundingRect().center()).toPoint());auto *popup=w->findChild<QObject *>("onlineSearchPopup");QVERIFY(popup);QTRY_VERIFY(popup->property("opened").toBool());QTest::qWait(250);
            auto *input=visual(w->contentItem(),"onlineSearch");QVERIFY(input);const auto inputBounds=input->mapRectToScene(input->boundingRect());QVERIFY(inputBounds.left()>=0&&inputBounds.right()<=w->width());
            if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath(size.width()<500?"account-search-narrow.png":"account-search-wide.png"));QVERIFY(QMetaObject::invokeMethod(popup,"close"));QTest::qWait(150);
        }
        if(netease->loggedIn()){QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,account->mapToScene(account->boundingRect().center()).toPoint());QTRY_VERIFY(menu->property("opened").toBool());QVERIFY(QMetaObject::invokeMethod(menu,"close"));QTest::qWait(200);}
        QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,account->mapToScene(QPointF(12,18)).toPoint());QTRY_VERIFY(menu->property("opened").toBool());auto *menuContent=menu->property("contentItem").value<QQuickItem *>();auto *sourceEntry=visual(menuContent,"onlineSourceEntry");QVERIFY(sourceEntry);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,sourceEntry->mapToScene(sourceEntry->boundingRect().center()).toPoint());auto *sources=w->findChild<QObject *>("onlineSourceMenu");QVERIFY(sources);QTRY_VERIFY(sources->property("opened").toBool());auto *sourceContent=sources->property("contentItem").value<QQuickItem *>();QVERIFY(sourceContent);const auto sourceBounds=sourceContent->mapRectToScene(sourceContent->boundingRect());QVERIFY(sourceBounds.right()<=w->width());if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("account-source-menu.png"));auto *qqChoice=visual(sourceContent,"onlineSource-qq");QVERIFY(qqChoice);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,qqChoice->mapToScene(qqChoice->boundingRect().center()).toPoint());QTRY_VERIFY(visual(w->contentItem(),"discoverPage")->property("qqSelected").toBool());QTRY_VERIFY(!menu->property("visible").toBool());
        QVERIFY(chooseOnlineSource(w,1));w->resize(1240,790);QTest::qWait(500);if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("qq-header-wide.png"));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void cloudSubscriptionsAndLocalMatchingStatistics(){
        Fixture f;QVERIFY(f.ok);
        QVariantMap playlist{{"provider","netease"},{"id","3778678"},{"name","Cloud fixture"},{"cover","https://example.invalid/cover.jpg"},{"count",200}};
        const auto key=f.shell->libraryBridge()->pinCloudPlaylist(playlist);QVERIFY(key!="0");QCOMPARE(f.shell->libraryBridge()->pinCloudPlaylist(playlist),key);
        QCOMPARE(f.library.playlists().size(),1);QCOMPARE(f.library.playlists().first().provider,QString("netease"));QCOMPARE(f.library.playlists().first().count,200);
        f.shell->playlistModel()->refresh();QVERIFY(f.shell->playlistModel()->get(0).value("cloud").toBool());QCOMPARE(f.shell->libraryBridge()->cloudPlaylist(key).value("id").toString(),QString("3778678"));
        Track local;local.id=123;local.title="A Beautiful Song";local.artist="Artist Name";local.duration=180000;Track remote=local;remote.title="A beautiful-song";
        QCOMPARE(matchOnlineTrack(remote,{local}),qint64(123));remote.title+=" (Live)";QCOMPARE(matchOnlineTrack(remote,{local}),qint64(0));remote=local;remote.artist="Another Artist";QCOMPARE(matchOnlineTrack(remote,{local}),qint64(0));remote=local;remote.duration+=30000;QCOMPARE(matchOnlineTrack(remote,{local}),qint64(0));remote=local;auto duplicate=local;duplicate.id=456;QCOMPARE(matchOnlineTrack(remote,{local,duplicate}),qint64(0));
        QCOMPARE(matchOnlineTrack(remote,{local,duplicate},false),local.id);remote.duration=0;QCOMPARE(matchOnlineTrack(remote,{local}),local.id);remote.title="A Beautiful Songs";QCOMPARE(matchOnlineTrack(remote,{local}),qint64(0));remote=local;
        remote.hash="netease:statistics-fixture";remote.duration=1000;const auto heard=f.library.totalHeard(),plays=f.library.totalPlays();auto session=f.library.startOnlineSession(remote);QVERIFY(session.id);
        QVERIFY(f.library.saveOnlineSession(session,800,80));QCOMPARE(f.library.totalHeard(),heard+800);QCOMPARE(f.library.totalPlays(),plays+1);
        session.sought=true;QVERIFY(f.library.saveOnlineSession(session,0,80));QCOMPARE(f.library.totalPlays(),plays);QCOMPARE(f.library.trackCount(),qint64(2));
        QVERIFY(f.shell->libraryBridge()->deletePlaylist(key));QCOMPARE(f.library.playlists().size(),0);
    }
    void matchedOnlinePlaybackAccumulatesOnTheLocalSong(){
        Fixture f;QVERIFY(f.ok);Track local=f.a;local.id=0;local.hash="matched-listening-fixture";local.title="Association Fixture";local.artist="Fixture Artist";local.duration=60000;
        const auto localId=f.library.insert(local);QVERIFY(localId>0);auto *facade=f.shell->facade();QObject::disconnect(facade,nullptr,f.shell->onlineMusicBridge(),nullptr);
        connect(facade,&PlaybackFacade::onlineSourceRequested,facade,[&](qint64 id){facade->setOnlineSource(id,QUrl::fromLocalFile(f.library.absolute(f.a.file)));});
        Track remote=local;remote.id=OnlineMusicBridge::TrackBase+991;remote.hash="netease:991";remote.title="association fixture";
        facade->playOnline({remote},remote.id);QCOMPARE(facade->current().matchedLocalId,localId);QCOMPARE(f.shell->playbackBridge()->matchedLocalTrackId(),QString::number(localId));
        QVERIFY(QTest::qWaitFor([&]{return facade->playing()&&f.library.track(localId).heard>500;},5000));facade->stopOnline();QVERIFY(f.library.track(localId).heard>500);QCOMPARE(f.library.trackCount(),qint64(3));
    }
    void automaticNcmDownloadRestoresBothCodecsAndBlocksDuplicates(){
        const auto fixtures=qEnvironmentVariable("RELAXER_NCM_FIXTURES");if(fixtures.isEmpty())QSKIP("Owned audio fixtures are opt-in");
        Fixture f;QVERIFY(f.ok);TranslationHttpMock server;QVERIFY(server.listen());auto *downloads=f.shell->onlineDownloadBridge();QSignalSpy completed(downloads,&OnlineDownloadBridge::downloaded);
        int index=0;for(const auto &format:{QString("mp3"),QString("flac")}){
            QFile input(QDir(fixtures).filePath("fixture-"+format+".ncm")),original(QDir(fixtures).filePath("fixture."+format));QVERIFY(input.open(QIODevice::ReadOnly));QVERIFY(original.open(QIODevice::ReadOnly));const auto expected=original.readAll();
            server.enqueue(input.readAll());QVariantMap row{{"provider","netease"},{"id",QString::number(++index)},{"title","NCM fixture "+format},{"artist","Test Artist"},{"duration",1200}};
            const auto base=f.dir.filePath("restored-"+format);QVERIFY(downloads->downloadTo(row,server.baseUrl().resolved(QUrl("/fixture.ncm")),"ncm",base+".mp3"));
            QVERIFY2(QTest::qWaitFor([&]{return !downloads->busy();},10000),qPrintable(downloads->status()));QCOMPARE(completed.count(),index);
            QFile output(base+"."+format);QVERIFY2(output.open(QIODevice::ReadOnly),qPrintable(downloads->status()));QCOMPARE(output.readAll(),expected);
            QVERIFY(downloads->isDownloaded("netease",row));const auto requests=server.requestCount();QVERIFY(!downloads->downloadTo(row,server.baseUrl(),"ncm",f.dir.filePath("duplicate.mp3")));QTest::qWait(30);QCOMPARE(server.requestCount(),requests);
            auto otherPlatform=row;otherPlatform["provider"]="qq";otherPlatform["mid"]="cross-provider-"+format;QVERIFY(downloads->isDownloaded("qq",otherPlatform));QVERIFY(!downloads->downloadTo(otherPlatform,server.baseUrl(),"mp3",f.dir.filePath("cross-provider.mp3")));QCOMPARE(server.requestCount(),requests);
            OnlineDownloadBridge reopened(&f.library);QVERIFY(reopened.isDownloaded("netease",row));QVERIFY(reopened.isDownloaded("qq",otherPlatform));
            QVERIFY(output.remove());QVERIFY(!downloads->isDownloaded("netease",row));QVERIFY(!downloads->isDownloaded("qq",otherPlatform));
        }
        const auto local=f.library.track(f.a.id);QVariantMap existing{{"provider","netease"},{"id","existing-local"},{"title",local.title},{"artist",local.artist},{"duration",local.duration}};
        QVERIFY(downloads->isDownloaded("netease",existing));QVERIFY(!downloads->downloadTo(existing,server.baseUrl(),"mp3",f.dir.filePath("blocked.mp3")));QCOMPARE(server.requestCount(),2);
        qInfo()<<"NCM MP3/FLAC restored byte-for-byte; repeated downloads and existing local tracks blocked before media request";
    }
    void liveDiscoveryCardsAndCloudSidebar(){
        if(!qEnvironmentVariableIsSet("RELAXER_DISCOVERY_LIVE_PROBE"))QSKIP("Public discovery catalog is opt-in");
        Fixture f;QVERIFY(f.ok);auto *w=f.shell->window();w->showNormal();w->resize(1240,790);
        QVERIFY(QMetaObject::invokeMethod(w,"navigateOriginal",Q_ARG(QVariant,1)));QTRY_COMPARE(f.shell->appBridge()->page(),4);
        auto *netease=f.shell->onlineMusicBridge();QVERIFY2(QTest::qWaitFor([&]{return !netease->busy()&&!netease->playlists().isEmpty();},20000),qPrintable(netease->status()));
        const auto folder=qEnvironmentVariable("RELAXER_DISCOVERY_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);QTest::qWait(500);w->grabWindow().save(QDir(folder).filePath("discovery-netease-cards.png"));}
        netease->openPlaylist("3778678");QVERIFY2(QTest::qWaitFor([&]{return !netease->busy()&&netease->songs().size()==200;},20000),qPrintable(netease->status()));
        const auto key=f.shell->libraryBridge()->pinCloudPlaylist(netease->currentPlaylist());QVERIFY(key!="0");
        QTRY_VERIFY(visual(w->contentItem(),"sidebarPlaylist-"+key));auto *row=visual(w->contentItem(),"sidebarPlaylist-"+key);QVERIFY(row->property("cloud").toBool());
        QVERIFY(QMetaObject::invokeMethod(w,"openPlaylist",Q_ARG(QVariant,key)));QTRY_COMPARE(f.shell->appBridge()->page(),0);QTRY_COMPARE(f.shell->libraryBridge()->playlistId(),key);QTRY_COMPARE(f.shell->libraryModel()->count(),200);
        if(!folder.isEmpty()){QTest::qWait(600);w->grabWindow().save(QDir(folder).filePath("discovery-pinned-cloud.png"));}
        QVERIFY(QMetaObject::invokeMethod(w,"navigateOriginal",Q_ARG(QVariant,1)));QTRY_COMPARE(f.shell->appBridge()->page(),4);
        auto *qq=f.shell->qqMusicBridge();QVERIFY(chooseOnlineSource(w,1));
        QVERIFY2(QTest::qWaitFor([&]{return !qq->busy()&&!qq->playlists().isEmpty();},20000),qPrintable(qq->status()));
        QVERIFY(!qq->playlists().first().toMap().value("cover").toString().isEmpty());
        QVERIFY(visual(w->contentItem(),"onlinePlaylistCards")->isVisible());
        if(!folder.isEmpty()){QTest::qWait(500);w->grabWindow().save(QDir(folder).filePath("discovery-qq-cards.png"));w->resize(414,540);QTest::qWait(500);w->grabWindow().save(QDir(folder).filePath("discovery-narrow.png"));}
        qInfo()<<"Discovery cards Netease"<<netease->playlists().size()<<"QQ"<<qq->playlists().size()<<"cloud sidebar route verified";
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void authorizedProviderDownloadResources(){
        if(!qEnvironmentVariableIsSet("RELAXER_DOWNLOAD_LIVE_PROBE"))QSKIP("Authorized download resource check is opt-in");
        Fixture f;QVERIFY(f.ok);QDir().mkpath(QDir(f.library.root()).filePath("Online"));
        for(const auto &provider:{QString("netease"),QString("qq")}){const auto source=qEnvironmentVariable(provider=="qq"?"RELAXER_QQ_SAVED_SESSION":"RELAXER_NETEASE_SAVED_SESSION");QVERIFY(!source.isEmpty());QVERIFY(QFile::copy(source,QDir(f.library.root()).filePath("Online/"+provider+"-session.bin")));}
        auto *netease=f.shell->onlineMusicBridge();auto *qq=f.shell->qqMusicBridge();auto *downloads=f.shell->onlineDownloadBridge();
        QObject::disconnect(netease,&OnlineMusicBridge::downloadReady,downloads,&OnlineDownloadBridge::chooseDestination);QObject::disconnect(qq,&QqMusicBridge::downloadReady,downloads,&OnlineDownloadBridge::chooseDestination);
        QSignalSpy neReady(netease,&OnlineMusicBridge::downloadReady),qqReady(qq,&QqMusicBridge::downloadReady);
        netease->activate();qq->activate();QVERIFY2(QTest::qWaitFor([&]{return netease->loggedIn()&&!netease->busy();},20000),qPrintable(netease->status()));QVERIFY2(QTest::qWaitFor([&]{return qq->loggedIn()&&!qq->busy();},20000),qPrintable(qq->status()));
        netease->search(QStringLiteral("莫扎特"));qq->search(QStringLiteral("莫扎特"));QVERIFY(QTest::qWaitFor([&]{return !netease->busy()&&!qq->busy()&&!netease->songs().isEmpty()&&!qq->songs().isEmpty();},20000));
        for(const auto &v:netease->songs()){netease->downloadSong(v.toMap().value("id").toString());QVERIFY(QTest::qWaitFor([&]{return !netease->busy();},20000));if(!neReady.isEmpty())break;}
        QVERIFY2(!neReady.isEmpty(),qPrintable(netease->status()));
        for(const auto &v:qq->songs()){const auto row=v.toMap();if(row.value("requiresSubscription").toBool())continue;qq->downloadSong(row.value("id").toString());QVERIFY(QTest::qWaitFor([&]{return !qq->busy();},20000));if(!qqReady.isEmpty())break;}
        QVERIFY2(!qqReady.isEmpty(),qPrintable(qq->status()));
        for(const auto *spy:{&neReady,&qqReady}){const auto arguments=spy->first();const auto row=arguments[0].toMap();const auto target=f.dir.filePath(row.value("provider").toString()+"-authorized.mp3");QVERIFY(downloads->downloadTo(row,arguments[1].toUrl(),arguments[2].toString(),target));QVERIFY2(QTest::qWaitFor([&]{return !downloads->busy();},45000),qPrintable(downloads->status()));QVERIFY2(downloads->isDownloaded(row.value("provider").toString(),row),qPrintable(downloads->status()));}
        qInfo()<<"Both authenticated providers resolved and saved an authorized audio resource to isolated temporary data";
    }
    void thumbnailIsImmediateAndDelayedOriginalCannotReplaceTheNextTrack(){
        Fixture f;QVERIFY(f.ok);auto *facade=f.shell->facade();auto *bridge=f.shell->playbackBridge();auto *cache=f.shell->onlineArtworkCache();
        QObject::disconnect(facade,nullptr,f.shell->onlineMusicBridge(),nullptr);
        TranslationHttpMock low,high;QVERIFY(low.listen());QVERIFY(high.listen());high.setHold(true);
        auto png=[](int edge,const QColor &color){QImage image(edge,edge,QImage::Format_RGB32);image.fill(color);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");return bytes;};
        for(int i=0;i<16;++i)low.enqueue(png(128,Qt::gray));
        Track a;a.id=OnlineMusicBridge::TrackBase+11;a.title="Progressive A";a.hash="netease:11";
        Track b=a;b.id++;b.title="Progressive B";b.hash="netease:12";
        const auto aPath=f.dir.filePath("original-a.png"),bPath=f.dir.filePath("original-b.png");
        cache->prepare(a,low.baseUrl().resolved(QUrl("/a.png")),high.baseUrl().resolved(QUrl("/a.png")),aPath);
        cache->prepare(b,low.baseUrl().resolved(QUrl("/b.png")),high.baseUrl().resolved(QUrl("/b.png")),bPath);
        connect(facade,&PlaybackFacade::onlineSourceRequested,facade,[&](qint64 id){facade->setOnlineSource(id,QUrl::fromLocalFile(f.library.absolute(f.a.file)));});
        int emptyChanges=0;connect(bridge,&PlaybackBridge::artworkChanged,bridge,[&]{if(bridge->coverUrl().isEmpty())++emptyChanges;});
        facade->playOnline({a,b},a.id);QCOMPARE(bridge->coverUrl(),QUrl(a.cover));
        auto *upcoming=f.shell->upcomingModel();upcoming->refresh();QCOMPARE(upcoming->firstCoverUrl(),QUrl(b.cover));
        QSignalSpy resets(upcoming,&QAbstractItemModel::modelReset);
        QTRY_VERIFY(bridge->coverUrl().isLocalFile());QTRY_VERIFY(upcoming->firstCoverUrl().isLocalFile());
        QCOMPARE(QImageReader(bridge->coverUrl().toLocalFile()).size(),QSize(128,128));QCOMPARE(high.requestCount(),0);QCOMPARE(resets.count(),0);
        auto *w=f.shell->window();w->showNormal();w->resize(1240,790);QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));
        QTRY_COMPARE(high.heldCount(),1);const auto firstLow=bridge->coverUrl();QVERIFY(firstLow.isLocalFile());QVERIFY(!QFileInfo::exists(aPath));
        auto *art=visual(w->contentItem(),"currentArtwork");QVERIFY(art);
        auto *canvas=art->property("imageItem").value<QObject *>();QVERIFY(canvas);
        auto loaded=[&]{bool ready=false;QMetaObject::invokeMethod(canvas,"isImageLoaded",Q_RETURN_ARG(bool,ready),Q_ARG(QUrl,art->property("displayedUrl").toUrl()));return ready;};
        QTRY_VERIFY(loaded());
        const auto folder=qEnvironmentVariable("RELAXER_PROGRESSIVE_ARTWORK_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath("held-original-shows-thumbnail.png"));}
        bridge->next();QCOMPARE(facade->current().id,b.id);QVERIFY(bridge->coverUrl().isLocalFile());const auto secondLow=bridge->coverUrl();
        QTRY_COMPARE(high.heldCount(),2);high.enqueue(png(1024,Qt::red));QVERIFY(high.releaseHeld());QTRY_VERIFY(QFileInfo::exists(aPath));
        QCOMPARE(bridge->coverUrl(),secondLow);high.enqueue(png(1024,Qt::blue));QVERIFY(high.releaseHeld());
        QTRY_COMPARE(bridge->coverUrl(),QUrl::fromLocalFile(bPath));QTRY_VERIFY(loaded());
        QCOMPARE(art->property("activeDirection").toInt(),0);
        QCOMPARE(QImage(bPath).pixelColor(512,512),QColor(Qt::blue));QCOMPARE(emptyChanges,0);QVERIFY(facade->playing());
        if(!folder.isEmpty()){QTest::qWait(220);w->grabWindow().save(QDir(folder).filePath("original-replaces-thumbnail.png"));}
        qInfo()<<"Progressive artwork: immediate current/next URLs, five-row prefetch, held original keeps thumbnail, late previous original ignored";
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void liveCompletePlaylistsAndHighResolutionCovers(){
        if(!qEnvironmentVariableIsSet("RELAXER_ONLINE_PLAYLIST_FIX_PROBE"))QSKIP("Targeted live playlist/artwork check is opt-in");
        Fixture f;QVERIFY(f.ok);auto *w=f.shell->window();w->showNormal();w->resize(1240,790);
        QVERIFY(f.shell->appBridge()->navigate(4));w->grabWindow();const auto localId=f.shell->facade()->current().id;
        auto *netease=f.shell->onlineMusicBridge();netease->openPlaylist("3778678");
        QVERIFY2(QTest::qWaitFor([&]{return !netease->busy()&&netease->songs().size()>=200;},30000),qPrintable(netease->status()));
        QCOMPARE(netease->songs().size(),200);QCOMPARE(f.shell->facade()->current().id,localId);QVERIFY(f.shell->facade()->playing());
        const auto neteaseFirst=netease->songs().first().toMap();QVERIFY(!neteaseFirst.value("thumbnail").toString().isEmpty());
        qInfo()<<"Complete Netease playlist rows"<<netease->songs().size();
        const auto folder=qEnvironmentVariable("RELAXER_ONLINE_PLAYLIST_FIX_SCREENSHOTS");
        auto *list=visual(w->contentItem(),"onlineSongs");QVERIFY(list);
        QTRY_VERIFY(visual(w->contentItem(),"onlineCover-"+neteaseFirst.value("id").toString()));
        if(!folder.isEmpty()){QDir().mkpath(folder);QTest::qWait(600);w->grabWindow().save(QDir(folder).filePath("netease-playlist-top.png"));QMetaObject::invokeMethod(list,"positionViewAtEnd");QTest::qWait(600);w->grabWindow().save(QDir(folder).filePath("netease-playlist-end.png"));}
        // Switching lists while a batch is in flight must discard that batch.
        netease->openPlaylist("3778678");netease->search(QStringLiteral("莫扎特"));
        QVERIFY2(QTest::qWaitFor([&]{return !netease->busy()&&!netease->songs().isEmpty();},20000),qPrintable(netease->status()));QVERIFY(netease->songs().size()<=50);
        const auto session=qEnvironmentVariable("RELAXER_QQ_SAVED_SESSION");QVERIFY(!session.isEmpty());QDir().mkpath(QDir(f.library.root()).filePath("Online"));
        QVERIFY(QFile::copy(session,QDir(f.library.root()).filePath("Online/qq-session.bin")));auto *qq=f.shell->qqMusicBridge();qq->activate();
        QVERIFY2(QTest::qWaitFor([&]{return qq->loggedIn()&&!qq->busy();},20000),qPrintable(qq->status()));
        QVERIFY(chooseOnlineSource(w,1));
        qq->openPlaylist("8044204090");
        QVERIFY2(QTest::qWaitFor([&]{return !qq->busy()&&qq->songs().size()>=633;},30000),qPrintable(qq->status()));
        QCOMPARE(qq->songs().size(),633);QCOMPARE(f.shell->facade()->current().id,localId);QVERIFY(f.shell->facade()->playing());
        qInfo()<<"Complete QQ playlist rows"<<qq->songs().size();
        if(!folder.isEmpty()){QMetaObject::invokeMethod(list,"positionViewAtBeginning");QTest::qWait(600);w->grabWindow().save(QDir(folder).filePath("qq-playlist-top.png"));QMetaObject::invokeMethod(list,"positionViewAtEnd");QTest::qWait(600);w->grabWindow().save(QDir(folder).filePath("qq-playlist-end.png"));w->resize(414,540);QTest::qWait(600);w->grabWindow().save(QDir(folder).filePath("qq-playlist-narrow.png"));w->resize(1240,790);}
        auto *facade=f.shell->facade();qq->search(QStringLiteral("莫扎特"));
        QVERIFY2(QTest::qWaitFor([&]{return !qq->busy()&&!qq->songs().isEmpty();},20000),qPrintable(qq->status()));
        QVariantMap freeSong;for(const auto &value:qq->songs()){const auto row=value.toMap();if(!row.value("requiresSubscription").toBool()&&row.value("duration").toLongLong()>60000){freeSong=row;break;}}
        QVERIFY(!freeSong.isEmpty());qq->playSong(freeSong.value("id").toString());
        auto *bridge=f.shell->playbackBridge();
        QVERIFY(!bridge->coverUrl().isEmpty());f.shell->upcomingModel()->refresh();QVERIFY(!f.shell->upcomingModel()->firstCoverUrl().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));
        QVERIFY2(QTest::qWaitFor([&]{return facade->playing()&&bridge->coverUrl().isLocalFile()&&QImageReader(bridge->coverUrl().toLocalFile()).size().width()>=800;},30000),qPrintable(qq->status()));
        auto cover=QImage(bridge->coverUrl().toLocalFile());QVERIFY(cover.width()>=800);qInfo()<<"QQ playback cover pixels"<<cover.size();
        QVERIFY(chooseOnlineSource(w,0));netease->playSong("33894312");
        QVERIFY2(QTest::qWaitFor([&]{return facade->current().id==OnlineMusicBridge::TrackBase+33894312&&facade->playing()&&bridge->coverUrl().isLocalFile()&&QImageReader(bridge->coverUrl().toLocalFile()).size().width()>=600;},30000),qPrintable(netease->status()));
        cover=QImage(bridge->coverUrl().toLocalFile());QVERIFY(cover.width()>=600);qInfo()<<"Netease playback cover pixels"<<cover.size();
        QCOMPARE(f.library.trackCount(),qint64(2));QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void providerQueuesAndLogoutStaySeparate(){
        Fixture f;QVERIFY(f.ok);auto *facade=f.shell->facade();
        Track qq;qq.id=QqMusicBridge::TrackBase+123;qq.title="QQ online";qq.hash="qqmusic:test";qq.duration=60000;
        connect(facade,&PlaybackFacade::onlineSourceRequested,facade,[&](qint64 id){facade->setOnlineSource(id,QUrl::fromLocalFile(f.library.absolute(f.a.file)));});
        facade->playOnline({qq},qq.id);QTRY_VERIFY(facade->playing());
        QVERIFY(!f.shell->onlineMusicBridge()->busy());QVERIFY(!f.shell->qqMusicBridge()->busy());
        f.shell->onlineMusicBridge()->logout();QVERIFY(facade->onlineActive());QCOMPARE(facade->current().id,qq.id);
        f.shell->qqMusicBridge()->logout();QVERIFY(!facade->onlineActive());
        QCOMPARE(f.library.trackCount(),qint64(2));
    }
    void liveQqSearchBothQrAndAiCandidates(){
        if(!qEnvironmentVariableIsSet("RELAXER_QQ_LIVE_PROBE"))QSKIP("QQ live checks are opt-in");
        Fixture f;QVERIFY(f.ok);auto *w=f.shell->window();w->showNormal();w->resize(1240,790);QVERIFY(f.shell->appBridge()->navigate(4));w->grabWindow();
        QVERIFY(chooseOnlineSource(w,1));
        auto *qq=f.shell->qqMusicBridge();qq->activate();qq->search(QStringLiteral("莫扎特"));
        QVERIFY2(QTest::qWaitFor([&]{return !qq->songs().isEmpty()&&!qq->busy();},20000),qPrintable(qq->status()));
        QVERIFY(!f.shell->onlineMusicBridge()->busy());qInfo()<<"Native QQ search results"<<qq->songs().size();
        for(const auto &channel:{QString("qq"),QString("wx")}){
            qq->setLoginChannel(channel);qq->startLogin();
            QVERIFY2(QTest::qWaitFor([&]{return !qq->qrImage().isEmpty();},20000),qPrintable(qq->status()));
            const auto image=QImage::fromData(QByteArray::fromBase64(qq->qrImage().section(',',1).toLatin1()));QVERIFY(!image.isNull());
            qInfo()<<"Native QQ login channel"<<channel<<"QR dimensions"<<image.size();QTest::qWait(2100);qq->stopLogin();
        }
        f.http.setHold(true);qq->recommend();QVERIFY2(QTest::qWaitFor([&]{return f.http.heldCount()>0;},20000),qPrintable(qq->status()));
        const auto candidate=qq->songs().first().toMap();const QJsonArray choices{QJsonObject{{"id",candidate.value("id").toString()},{"reason","本地偏好与新歌探索"}},QJsonObject{{"id","invented"}}};
        f.http.enqueue(completion(QString::fromUtf8(QJsonDocument(QJsonObject{{"recommendations",choices}}).toJson(QJsonDocument::Compact))));QVERIFY(f.http.releaseHeld());
        QVERIFY(QTest::qWaitFor([&]{return !qq->busy();},10000));QCOMPARE(qq->songs().size(),1);QCOMPARE(qq->songs().first().toMap().value("id"),candidate.value("id"));
        QVERIFY(!f.http.requestBody(0).contains("musickey"));
        const auto folder=qEnvironmentVariable("RELAXER_QQ_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath("discover-qq-wide.png"));w->resize(414,540);QTest::qWait(400);w->grabWindow().save(QDir(folder).filePath("discover-qq-narrow.png"));}
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void liveQqSavedSessionPlayback(){
        const auto session=qEnvironmentVariable("RELAXER_QQ_SAVED_SESSION");
        if(!qEnvironmentVariableIsSet("RELAXER_QQ_LIVE_PROBE")||session.isEmpty())QSKIP("Authorized preview session is opt-in");
        Fixture f;QVERIFY(f.ok);QDir().mkpath(QDir(f.library.root()).filePath("Online"));
        QVERIFY(QFile::copy(session,QDir(f.library.root()).filePath("Online/qq-session.bin")));
        auto *qq=f.shell->qqMusicBridge();qq->activate();
        QVERIFY2(QTest::qWaitFor([&]{return qq->loggedIn()&&!qq->busy();},20000),qPrintable(qq->status()));
        qInfo()<<"QQ saved session restored; membership"<<qq->membership();qq->search(QStringLiteral("莫扎特"));
        QVERIFY2(QTest::qWaitFor([&]{return !qq->songs().isEmpty()&&!qq->busy();},20000),qPrintable(qq->status()));
        QVariantMap row;for(const auto &value:qq->songs()){const auto candidate=value.toMap();if(!candidate.value("requiresSubscription").toBool()&&candidate.value("duration").toLongLong()>60000){row=candidate;break;}}
        QVERIFY2(!row.isEmpty(),"No free full-length song in this live search response");qq->playSong(row.value("id").toString());
        auto *facade=f.shell->facade();QVERIFY2(QTest::qWaitFor([&]{return facade->playing()&&facade->position()>2000;},25000),qPrintable(qq->status()));
        QVERIFY(facade->duration()>60000);QVERIFY(facade->seekable());
        qInfo()<<"Native QQ playback duration"<<facade->duration()<<"position"<<facade->position();
        f.shell->playbackBridge()->pause();const auto target=facade->duration()/2;f.shell->playbackBridge()->seek(target);
        QVERIFY(QTest::qWaitFor([&]{return qAbs(facade->position()-target)<500;},5000));f.shell->playbackBridge()->play();
        QVERIFY(QTest::qWaitFor([&]{return facade->position()>target+300;},10000));
        QCOMPARE(f.library.trackCount(),qint64(2));QVERIFY(!f.shell->onlineMusicBridge()->busy());
    }
    void onlineQueueKeepsLocalPersistenceAndUsesExistingControls(){
        Fixture f;QVERIFY(f.ok);auto *facade=f.shell->facade();auto *bridge=f.shell->playbackBridge();
        QVERIFY(QTest::qWaitFor([&]{return facade->seekable();}));
        bridge->pause();facade->seek(4000);QVERIFY(facade->save());
        const auto localQueue=f.library.setting("queue");const auto localCurrent=f.library.setting("current");
        Track one;one.id=OnlineMusicBridge::TrackBase+1;one.title="Online one";one.artist="Online artist";one.hash="netease:1";one.duration=60000;
        Track two=one;two.id++;two.title="Online two";two.hash="netease:2";
        QSignalSpy sources(facade,&PlaybackFacade::onlineSourceRequested);
        connect(facade,&PlaybackFacade::onlineSourceRequested,facade,[&](qint64 id){facade->setOnlineSource(id,QUrl::fromLocalFile(f.library.absolute(f.a.file)));});
        facade->playOnline({one,two},one.id);
        QTRY_COMPARE(bridge->title(),QString("Online one"));QTRY_VERIFY(bridge->playing());
        QTRY_COMPARE(f.shell->queueModel()->count(),2);QTRY_COMPARE(f.shell->upcomingModel()->firstTitle(),QString("Online two"));
        QTRY_VERIFY(bridge->seekable());bridge->pause();facade->seek(12000);QTRY_VERIFY(facade->position()>=11900);
        QCOMPARE(f.library.setting("queue"),localQueue);QCOMPARE(f.library.setting("current"),localCurrent);
        bridge->next();QTRY_COMPARE(bridge->title(),QString("Online two"));QVERIFY(sources.count()>=2);
        bridge->previous();QTRY_COMPARE(bridge->title(),QString("Online one"));
        facade->stopOnline();QTRY_COMPARE(bridge->trackId(),QString::number(f.a.id));
        QCOMPARE(facade->queue().items,(QVector<qint64>{f.a.id,f.b.id}));QVERIFY(!facade->playing());
        QCOMPARE(f.library.trackCount(),qint64(2));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void liveNeteaseSearchQrAndFullPlayback(){
        if(!qEnvironmentVariableIsSet("RELAXER_NETEASE_LIVE_PROBE"))QSKIP("Live probe is opt-in; no network in the default contract check");
        Fixture f;QVERIFY(f.ok);auto *w=f.shell->window();w->showNormal();w->resize(1240,790);
        QVERIFY(f.shell->appBridge()->navigate(4));w->grabWindow();QVERIFY(visual(w->contentItem(),"discoverPage"));
        auto *online=f.shell->onlineMusicBridge();online->startLogin();
        QVERIFY2(QTest::qWaitFor([&]{return !online->qrImage().isEmpty();},20000),qPrintable(online->status()));
        const auto qr=QImage::fromData(QByteArray::fromBase64(online->qrImage().section(',',1).toLatin1()));QVERIFY(qr.width()>100);online->stopLogin();
        online->search(QStringLiteral("莫扎特"));
        QVERIFY2(QTest::qWaitFor([&]{return !online->songs().isEmpty()&&!online->busy();},20000),qPrintable(online->status()));
        qInfo()<<"Native Netease search results"<<online->songs().size()<<"QR dimensions"<<qr.size();
        // Publicly playable song; stream URLs and cookies are never printed.
        online->playSong("33894312");
        QVERIFY2(QTest::qWaitFor([&]{return f.shell->facade()->onlineActive()&&f.shell->facade()->playing()&&f.shell->facade()->position()>2000;},30000),qPrintable(online->status()));
        QVERIFY(f.shell->facade()->duration()>60000);QVERIFY(f.shell->facade()->seekable());
        qInfo()<<"Native full-stream duration"<<f.shell->facade()->duration()<<"position"<<f.shell->facade()->position();
        f.shell->playbackBridge()->pause();const auto target=f.shell->facade()->duration()/2;f.shell->playbackBridge()->seek(target);
        QVERIFY(QTest::qWaitFor([&]{return qAbs(f.shell->facade()->position()-target)<500;},5000));
        f.shell->playbackBridge()->play();QVERIFY(QTest::qWaitFor([&]{return f.shell->facade()->position()>target+300;},10000));
        const auto folder=qEnvironmentVariable("RELAXER_NETEASE_SCREENSHOTS");
        if(!folder.isEmpty()){QDir().mkpath(folder);QTest::qWait(300);w->grabWindow().save(QDir(folder).filePath("discover-wide.png"));
            w->resize(414,540);QTest::qWait(400);w->grabWindow().save(QDir(folder).filePath("discover-narrow.png"));}
        QCOMPARE(f.library.trackCount(),qint64(2));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void aiUsesConfiguredRuntimeAndOnlyReturnsRealCandidates(){
        if(!qEnvironmentVariableIsSet("RELAXER_NETEASE_LIVE_PROBE"))QSKIP("Online candidates are opt-in");
        Fixture f;QVERIFY(f.ok);auto *online=f.shell->onlineMusicBridge();f.http.setHold(true);online->recommend();
        QVERIFY2(QTest::qWaitFor([&]{return f.http.heldCount()>0;},20000),qPrintable(online->status()));
        QVERIFY(!online->songs().isEmpty());const auto candidate=online->songs().first().toMap();
        const QJsonArray choices{QJsonObject{{"id","invalid-candidate"},{"reason","ignore"}},QJsonObject{{"id",candidate.value("id").toString()},{"reason","偏好相近的探索歌曲"}}};
        f.http.enqueue(completion(QString::fromUtf8(QJsonDocument(QJsonObject{{"recommendations",choices}}).toJson(QJsonDocument::Compact))));QVERIFY(f.http.releaseHeld());
        QVERIFY(QTest::qWaitFor([&]{return !online->busy();},10000));QCOMPARE(online->songs().size(),1);
        QCOMPARE(online->songs().first().toMap().value("id"),candidate.value("id"));
        const auto request=QJsonDocument::fromJson(f.http.requestBody(0)).object();QCOMPARE(request.value("model").toString(),f.runtime->configuration().defaults.modelId);
        QVERIFY(!f.http.requestBody(0).contains("MUSIC_U"));
    }
};
QTEST_MAIN(OnlineMusicProbe)
#include "online-music-probe.moc"
