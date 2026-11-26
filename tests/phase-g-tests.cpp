#include "ui-bridge/qml-shell.h"
#include "phase-c-version.h"
#include "ui-bridge/additional-music-bridge.h"
#include "disk-cache.h"
#include "importer.h"
#include <QVideoFrame>
#include <QAudioBuffer>
#include <QtEndian>
#include "audio-fader.h"
#include "ui-bridge/account-membership.h"
#include "ui-bridge/ui-images.h"
#include <QDataStream>
#include <QCryptographicHash>
#include <QEnterEvent>
#include <QAudioBufferOutput>
#include <QTcpServer>
#include <QTcpSocket>
#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QJsonDocument>
#include <QJSValue>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQmlComponent>
#include <QImageReader>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QScopeGuard>
#include <QtTest>
#include <cstring>
#include <functional>
using namespace relaxer;
class PhaseGTests final : public QObject {
    Q_OBJECT
private slots:
    void nativeCatalogsPreserveOpaqueIdsAndMembership();
    void searchCancellationKeepsOnlyTheLatestCatalog();
    void encryptedSodaAudioIsNeverReportedAsACompletedDownload();
    void newPlatformPinsAreAcceptedWithoutChangingLocalPlaylistSchema();
    void sidebarSettingsAndDeferredPagesLoad();
    void platformLoginAndCancellationUseIsolatedMockSessions();
    void expandedBackdropKeepsAPrewarmedThumbnailUnderTheHighResolutionImage();
    void explicitOnlineBatchesKeepTheirOrderAcrossModes();
    void onlineBatchNeverWritesIntoALocalPlaylistOrRestartsTheCurrentSong();
    void platformButtonsAndGeneralGroupsFitWideAndNarrowWindows();
    void platformAccountNamesUseMeasuredSpaceAndRecoverOnExpansion();
    void discoverySelectionAndDownloadBatchesUseTheVisibleSongs();
    void nativeWindowsTransitionsRetainAFullClientArea();
    void sidebarMorphAndDragKeepAnimatedIntermediateStates();
    void newPreferencesValidateAndPreserveLegacyAnimationSettings();
    void cacheBudgetEvictsGeneratedFilesAndProtectsAccountsAndActiveAudio();
    void displayedOnlineCoversAreReusedFromDiskAfterRestart();
    void audioFaderRampsAndCancelsAPendingPause();
    void localProgressSurvivesLoadingAndHonorsTheMemorySwitch();
    void onlineProgressRestoresPausedWithoutPersistingStreamUrls();
    void lightModeSkipsBothShortcutMiniTransitions();
    void sharedUpcomingActuallyAdvancesAcrossLocalAndOnlineAudio();
    void cloudVipAndCoverlessRowsSurviveFontSizeChanges();
    void compactChromeAndFixedPlaylistHeaderFollowThePreferences();
    void sidebarResizeDragsPersistAndRespectCompactNavigation();
    void widthToggleKeepsTheRightEdgeAndRestoresFromTheNarrowestWindow();
    void membershipModesAndDiscoverySettingsFollowAccountFacts();
    void catalogScrollBarsCanBeDragged();
    void volumeHoverUsesSafeTrianglesOnBothWindows();
    void playlistCoversFiltersAndWideVolumeFit();
    void bufferedProgressUsesKnownRangesAndKeepsVolumeUnchanged();
    void transientOnlineFailuresRetryAtTheSamePositionAndStopAfterTwoRetries();
    void videoMirrorFallbackResumesAndCannotReviveASkippedTrack();
    void dashVideoAndAudioShareTheQtClockAndQualityCeiling();
    void onlineVideoVisibilityRequestsOnlyTheNeededTracksAndKeepsAudioCache();
    void audioOnlyRejectsMergedSourcesAndKeepsTheVideoPreference();
    void onlineVideoPreferenceKeepsLocalVideoAndPartControls();
    void localVideoAudioAndLyricsModesUseCoversAndSubtitles();
    void regressionLayoutsUseAvailableTextSpace();
    void regressionHistoryAndSettingsWaitForPaintedFonts();
    void biliFoldersUsersAndPagingRemainCompatibleWithSidebarPins();
    void discoveryAutoPagesAndRetainsQueriesAcrossSources();
    void videoEntryAndLetterboxPreferencesDoNotReopenOnEveryTrack();
    void videoChromePartsAndCoverRatiosFitBothLayouts();
    void historyEntriesAndFallbackNavigationStayReachable();
    void historyTableSortsAllRowsAndShowsPlatformLogos();
    void completedPlaybackHistoryPreservesSourcesAndDoesNotLogRestores();
    void videoAutoExpansionCanBeEnabledAfterEarlierPlayback();
    void collectionSearchPagesWithinTheCurrentBiliFolderAndUser();
    void startupVideoCoverAndClosingMetadataKeepTheirBounds();
    void backgroundingRetainsArtworkWithoutDefaultFrames();
    void queueEditorAndEmptyFooterRespectControlsAndAudio();
    void queueMenusStayUsableInCompactAndMiniWindows();
    void queueEditingOffersDirectActionsAndConsistentExits();
    void queuedCloudLocationUsesPinsAndReportsInaccessibleCollections();
    void unavailableQueueItemsSkipUntilPlayableAndStopAfterOneRound();
    void bilibiliPartSelectionResolvesCidAndPreservesQueueAndProgress();
    void videoPartsContinueNaturallyBeforeAdvancingTheQueue();
    void bilibiliSigningCatalogLoginAndCancellationUseMockData();
    void localAndCloudVideoShareTheExistingQueueAndVideoOutput();

};
namespace {
class Reply final:public QNetworkReply {
    QByteArray m_bytes;qsizetype m_offset=0;bool m_done=false;
public:
    Reply(const QNetworkRequest &r,const QByteArray &data,QObject *parent,int delay=0):QNetworkReply(parent),m_bytes(data){
        setRequest(r);setUrl(r.url());setAttribute(QNetworkRequest::HttpStatusCodeAttribute,200);open(QIODevice::ReadOnly);
        if(r.url().path()=="/passport/open/get_access_ticket/")
            setRawHeader("bd-ticket-guard-server-data",QJsonDocument(QJsonObject{{"ticket","synthetic-ticket"},{"ts_sign","synthetic-signature"},{"client_cert","synthetic-certificate"}}).toJson(QJsonDocument::Compact).toBase64());
        QTimer::singleShot(delay,this,[this]{if(m_done)return;m_done=true;setFinished(true);emit readyRead();emit finished();});
    }
    void abort()override{if(m_done)return;m_done=true;setError(OperationCanceledError,"mock cancelled");setFinished(true);emit finished();}
    qint64 bytesAvailable()const override{return m_bytes.size()-m_offset+QNetworkReply::bytesAvailable();}
    qint64 readData(char *data,qint64 max)override{const auto size=qMin(max,qint64(m_bytes.size()-m_offset));if(size<=0)return -1;memcpy(data,m_bytes.constData()+m_offset,size);m_offset+=size;return size;}
};
class Transport final:public QNetworkAccessManager {
public:
    QList<QByteArray> responses;QStringList paths;QList<QUrl> urls;QList<bool> http2;
    std::function<QByteArray(const QNetworkRequest &)> respond;std::function<int(const QNetworkRequest &)> delay;
    QNetworkReply *createRequest(Operation,const QNetworkRequest &r,QIODevice *)override{
        paths.append(r.url().path());urls.append(r.url());http2.append(r.attribute(QNetworkRequest::Http2AllowedAttribute,true).toBool());return new Reply(r,respond?respond(r):responses.isEmpty()?QByteArray("{}"):responses.takeFirst(),this,delay?delay(r):0);
    }
};
QQuickItem *visual(QQuickItem *root,const QString &name){if(root->objectName()==name)return root;for(auto *child:root->childItems())if(auto *item=visual(child,name))return item;return nullptr;}
struct Fixture {
    QTemporaryDir dir;Library library{dir.filePath("data")};bool ok=library.open();PlaybackFacade facade{&library,nullptr,true};LyricsModel lyrics{&library,&facade};OnlineArtworkCache artwork{&library,&facade};Transport transport;
};
QByteArray biliPage(int first,int count,bool more,const QString &cover={}){
    QJsonArray rows;for(int i=first;i<first+count;++i)rows.append(QJsonObject{{"bvid",QString("BVmock%1").arg(i)},{"title",QString("Generated %1").arg(i)},{"duration",4},{"cover",cover},{"author","Mock"}});
    return QJsonDocument(QJsonObject{{"code",0},{"data",QJsonObject{{"medias",rows},{"result",rows},{"numPages",more?3:2},{"has_more",more},{"info",QJsonObject{{"title","Synthetic favorites"},{"media_count",45},{"cover",cover}}}}}}).toJson();
}
QByteArray biliNav(){return R"({"code":0,"data":{"isLogin":true,"uname":"Mock","mid":42,"wbi_img":{"img_url":"https://image.example.test/7cd084941338484aae1ad9425b84077c.png","sub_url":"https://image.example.test/4932caff0ff746eab6f01bf08b70ac45.png"}}})";}
QString syntheticVideo(const QString &folder){
    QDir().mkpath(folder);const auto path=QDir(folder).filePath("generated.mp4");QProcess generator;
    generator.start(toolPath("ffmpeg"),{"-v","error","-f","lavfi","-i","color=c=blue:s=160x90:r=10","-f","lavfi","-i","anullsrc=r=8000:cl=mono","-t","3","-c:v","mpeg4","-c:a","aac","-shortest","-y",path});
    if(!generator.waitForFinished(15000)||generator.exitCode()!=0)return {};return path;
}
QString syntheticWave(const QString &folder){
    QDir().mkpath(folder);const auto path=QDir(folder).filePath("synthetic.wav");QFile file(path);if(!file.open(QIODevice::WriteOnly))return {};
    const quint32 bytes=8000*2*12;QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF",4);out<<quint32(36+bytes);out.writeRawData("WAVEfmt ",8);out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);out.writeRawData("data",4);out<<bytes;file.write(QByteArray(bytes,'\0'));return path;
}
}
void PhaseGTests::nativeCatalogsPreserveOpaqueIdsAndMembership(){
        const auto soda=QJsonDocument::fromJson(R"({"data":{"results":[{"entity":{"track_wrapper":{"track":{"id":"7184673012345678901","name":"Synthetic","duration_ms":123000,"is_vip":true,"artists":[{"name":"Mock Artist"}],"album":{"name":"Fixture","url_cover":{"urls":["https://img.example.test/"],"uri":"cover.png"}}}}}}]}})").object();
        const auto rows=AdditionalMusicBridge::normalizeSongs(AdditionalMusicBridge::Qishui,soda);
        QCOMPARE(rows.size(),1);const auto song=rows.first().toMap();QCOMPARE(song.value("id").toString(),QString("7184673012345678901"));QCOMPARE(song.value("duration").toLongLong(),123000);QVERIFY(song.value("requiresSubscription").toBool());QCOMPARE(song.value("artist").toString(),QString("Mock Artist"));
        const auto kg=QJsonDocument::fromJson(R"({"data":{"lists":[{"FileHash":"ABC123","SongName":"Synthetic","SingerName":"Mock","Duration":123,"PayType":1},{"FileHash":"ABC123","SongName":"Synthetic","Duration":123}]}})").object();
        const auto kugou=AdditionalMusicBridge::normalizeSongs(AdditionalMusicBridge::Kugou,kg);QCOMPARE(kugou.size(),1);QCOMPARE(kugou.first().toMap().value("duration").toLongLong(),123000);QVERIFY(kugou.first().toMap().value("requiresSubscription").toBool());
    }
void PhaseGTests::searchCancellationKeepsOnlyTheLatestCatalog(){
        Fixture f;QVERIFY(f.ok);AdditionalMusicBridge bridge(AdditionalMusicBridge::Kugou,&f.library,&f.facade,&f.lyrics,&f.artwork,nullptr,&f.transport);
        f.transport.responses={R"({"status":1,"data":{"lists":[{"FileHash":"OLD","SongName":"Old","Duration":1}]}})",R"({"status":1,"data":{"lists":[{"FileHash":"NEW","SongName":"New","Duration":2}]}})"};
        bridge.search("old");bridge.search("new");QTRY_VERIFY(!bridge.busy());QCOMPARE(bridge.songs().size(),1);QCOMPARE(bridge.songs().first().toMap().value("id").toString(),QString("NEW"));QVERIFY(!bridge.catalogLoading());
        // §7: authentication/paging/cover requests must not be conflated with
        // search requests. Each search issues exactly one request; the stale
        // one leaves no additional traffic. Kugou then batches one cover
        // lookup for the surviving catalog only.
        int searchRequests=0,coverRequests=0;
        for(const auto &path:f.transport.paths){if(path=="/v2/search/song")++searchRequests;else if(path=="/v1/audio/audio")++coverRequests;}
        QCOMPARE(searchRequests,2);
        QCOMPARE(coverRequests,1);
        QSignalSpy ready(&bridge,&AdditionalMusicBridge::downloadReady);QSignalSpy failed(&bridge,&AdditionalMusicBridge::downloadFailed);
        f.transport.responses={R"({"status":1,"url":["https://audio.example.test/synthetic.flac"],"extName":"flac"})"};
        auto song=bridge.songs().first().toMap();song["_downloadTask"]=42;bridge.resolveDownload(song,"lossless");QTRY_COMPARE(ready.size(),1);QCOMPARE(failed.size(),0);QCOMPARE(ready.first().at(0).toMap().value("_downloadTask").toInt(),42);QCOMPARE(ready.first().at(2).toString(),QString("flac"));
    }
void PhaseGTests::encryptedSodaAudioIsNeverReportedAsACompletedDownload(){
        Fixture f;QVERIFY(f.ok);AdditionalMusicBridge bridge(AdditionalMusicBridge::Qishui,&f.library,&f.facade,&f.lyrics,&f.artwork,nullptr,&f.transport);bridge.activate();
        f.transport.responses={R"({"track_player":{"video_model":"{\"video_list\":[{\"main_url\":\"https://audio.example.test/synthetic.m4a\",\"spade_a\":\"mock-key\"}]}"}})"};
        QSignalSpy ready(&bridge,&AdditionalMusicBridge::downloadReady);QSignalSpy failed(&bridge,&AdditionalMusicBridge::downloadFailed);
        bridge.resolveDownload({{"id","123"},{"provider","qishui"},{"_downloadTask",7}},"standard");QTRY_COMPARE(failed.size(),1);QCOMPARE(ready.size(),0);QCOMPARE(failed.first().at(0).toMap().value("_downloadTask").toInt(),7);
    }
void PhaseGTests::newPlatformPinsAreAcceptedWithoutChangingLocalPlaylistSchema(){
        Fixture f;QVERIFY(f.ok);const auto kg=f.library.pinCloudPlaylist("kugou","opaque-hash","Synthetic KG","",1);const auto soda=f.library.pinCloudPlaylist("qishui","7184673012345678901","Synthetic Soda","",2);QVERIFY(kg>0);QVERIFY(soda>kg);QVERIFY(f.library.createPlaylist("Local")>0);QCOMPARE(f.library.pinCloudPlaylist("unknown","1","Invalid","",1),0);
    }
void PhaseGTests::sidebarSettingsAndDeferredPagesLoad(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));QmlShell shell(&library,nullptr,true);QVERIFY2(shell.load(),qPrintable(shell.warnings().join('\n')));auto *window=shell.window();auto *root=window->contentItem();
        QTRY_VERIFY(visual(root,"navigation5"));QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"manageMusicPlatforms"));QVERIFY(visual(root,"customizeSidebar"));
        auto rows=shell.settingsBridge()->values().value("sidebarSections").toList();
        for(auto &v:rows){auto row=v.toMap();row["visible"]=row.value("id").toInt()==3;v=row;}
        QVERIFY(shell.settingsBridge()->setPreference("sidebarSections",rows));QTRY_VERIFY(!visual(root,"navigation0"));QVERIFY(visual(root,"navigation3"));
        auto *dialog=window->findChild<QObject *>("sidebarEditor");QVERIFY(dialog);QVERIFY(QMetaObject::invokeMethod(dialog,"openEditor"));QTRY_VERIFY(dialog->property("visible").toBool());QTRY_COMPARE(dialog->property("scale").toReal(),1.);
        QTRY_VERIFY(visual(root,"sidebarDrag-0"));auto *handle=visual(root,"sidebarDrag-0");
        const auto from=handle->mapToScene(QPointF(handle->width()/2,handle->height()/2)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
        QTest::mouseMove(window,from+QPoint(0,30),20);QTest::mouseMove(window,from+QPoint(0,96),20);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,from+QPoint(0,96));
        QCOMPARE(shell.settingsBridge()->values().value("sidebarSections").toList(),rows);
        QVERIFY(QMetaObject::invokeMethod(dialog,"save"));
        const auto saved=shell.settingsBridge()->values().value("sidebarSections").toList();
        QCOMPARE(saved.at(2).toMap().value("id").toInt(),0);
        const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);window->grabWindow().save(QDir(folder).filePath("settings.png"));}
        QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
    }
void PhaseGTests::platformLoginAndCancellationUseIsolatedMockSessions(){
    for(auto platform:{AdditionalMusicBridge::Kugou,AdditionalMusicBridge::Qishui}){
        Fixture f;QVERIFY(f.ok);AdditionalMusicBridge bridge(platform,&f.library,&f.facade,&f.lyrics,&f.artwork,nullptr,&f.transport);
        if(platform==AdditionalMusicBridge::Kugou)f.transport.responses={R"({"data":{"qrcode":"synthetic-qr"}})",R"({"data":{"status":4,"token":"synthetic-token","userid":"12345","nickname":"Mock Account"}})"};
        else f.transport.responses={R"({"data":{"qrcode_index_url":"https://example.test/login","token":"synthetic-token"}})",R"({"data":{"status":"confirmed","auth_code":"synthetic-auth-code"}})",R"({"data":{"access_code":"synthetic-access-code","open_id":"synthetic-open-id","expires_time":3600}})",R"({"data":{}})",R"({"data":{"my_info":{"id":"12345","nickname":"Mock Account"}}})"};
        QSignalSpy completed(&bridge,&AdditionalMusicBridge::loginCompleted);
        bridge.startLogin();QTRY_VERIFY(!bridge.qrImage().isEmpty());QTRY_COMPARE_WITH_TIMEOUT(completed.size(),1,6000);QVERIFY(bridge.loggedIn());QCOMPARE(bridge.nickname(),QString("Mock Account"));
        bridge.logout();QVERIFY(!bridge.loggedIn());QVERIFY(bridge.nickname().isEmpty());
        f.transport.responses={R"({"data":{"qrcode":"cancelled","token":"cancelled"}})"};bridge.startLogin();bridge.stopLogin();QCoreApplication::processEvents();QVERIFY(!bridge.loggedIn());QVERIFY(bridge.qrImage().isEmpty());
    }
}
void PhaseGTests::expandedBackdropKeepsAPrewarmedThumbnailUnderTheHighResolutionImage(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());
    const auto online=DiskCache::online(&library);QVERIFY(QDir().mkpath(online));
    const qint64 id=3999999999999LL;
    const QUrl thumbUrl("https://img.example.test/thumb.png"),originalUrl("https://img.example.test/original.png");
    const auto key=QCryptographicHash::hash((QString("kugou:synthetic\n")+thumbUrl.toString(QUrl::FullyEncoded)+'\n'+originalUrl.toString(QUrl::FullyEncoded)).toUtf8(),QCryptographicHash::Sha256).toHex();
    const auto thumb=QDir(online).filePath("thumbnail-v2-"+key+".png"),original=QDir(online).filePath("cover-v2-"+key+".png");
    QImage low(160,160,QImage::Format_RGB32);low.fill(QColor("#d35b95"));QVERIFY(low.save(thumb));
    QImage high(1024,1024,QImage::Format_RGB32);high.fill(QColor("#539a82"));QVERIFY(high.save(original));
    Track track;track.id=id;track.title="Synthetic cover";track.artist="Mock artist";track.hash="kugou:synthetic";
    shell.onlineArtworkCache()->prepare(track,thumbUrl,originalUrl,original);
    shell.facade()->playOnline({track},id);QCOMPARE(shell.onlineArtworkCache()->thumbnailCover(),QUrl::fromLocalFile(thumb));
    auto *window=shell.window();auto *root=window->contentItem();auto *warm=visual(root,"playerBackdropWarmup");QVERIFY(warm);
    QTRY_COMPARE(warm->property("status").toInt(),1);QVERIFY(QMetaObject::invokeMethod(window,"showLyrics"));
    QTRY_VERIFY(visual(root,"playerBackdropPreview"));auto *preview=visual(root,"playerBackdropPreview");auto *backdrop=visual(root,"playerBackdrop");QVERIFY(backdrop);
    QCOMPARE(preview->property("status").toInt(),1);QCOMPARE(preview->property("source").toUrl(),warm->property("source").toUrl());QVERIFY(preview->property("source").toUrl().toString().contains("thumbnail-"));
    QTRY_COMPARE(backdrop->property("status").toInt(),1);QVERIFY(backdrop->property("source").toUrl().toString().contains("cover-v2-"));QCOMPARE(preview->property("status").toInt(),1);
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
}
void PhaseGTests::explicitOnlineBatchesKeepTheirOrderAcrossModes(){
    for(auto mode:{Mode::Sequential,Mode::ListLoop,Mode::SingleLoop,Mode::Shuffle}){
        Queue q;q.set({1,2,3,4},1);q.setMode(mode);q.upcoming();
        QVERIFY(q.enqueue({4,6,7,6,1},true));QCOMPARE(q.current(),1);QCOMPARE(q.items,(QVector<qint64>{1,4,6,7,2,3}));
        QCOMPARE(q.upcoming(3),(QVector<qint64>{4,6,7}));QCOMPARE(q.next(false),4);QCOMPARE(q.next(false),6);QCOMPARE(q.next(false),7);
        QVERIFY(q.enqueue({2,9,2},false));QCOMPARE(q.current(),7);QCOMPARE(q.items.last(),9);QCOMPARE(q.items.at(q.items.size()-2),2);
    }
}
void PhaseGTests::onlineBatchNeverWritesIntoALocalPlaylistOrRestartsTheCurrentSong(){
    Fixture f;QVERIFY(f.ok);Track a;a.id=1000000000001LL;a.hash="netease:1";a.title="Synthetic A";Track b=a;b.id++;b.hash="netease:2";Track c=a;c.id+=2;c.hash="netease:3";
    QVERIFY(f.facade.batchOnline({a},"next"));QVERIFY(!f.facade.playing());f.facade.playOnline({a,b},a.id);QSignalSpy sources(&f.facade,&PlaybackFacade::onlineSourceRequested);
    QVERIFY(f.facade.batchOnline({b,c},"next"));QCOMPARE(sources.size(),0);QCOMPARE(f.facade.current().id,a.id);QVERIFY(!f.facade.playing());QCOMPARE(f.facade.upcoming().mid(0,2),(QVector<qint64>{b.id,c.id}));
    Track local;local.hash="synthetic-local-file";local.file="Music/synthetic.wav";local.title="Local";const auto localId=f.library.insert(local);QVERIFY(localId>0);local.id=localId;
    const auto before=f.facade.queue().items;QVERIFY(!f.facade.batchOnline({local},"append"));QCOMPARE(f.facade.queue().items,before);
    const auto localList=f.library.createPlaylist("Synthetic local"),cloud=f.library.pinCloudPlaylist("netease","1","Synthetic cloud","",2);
    QVERIFY(!f.library.addToPlaylist(localList,{a.id}));QVERIFY(!f.library.addToPlaylist(cloud,{localId}));QCOMPARE(f.library.tracks(localList).size(),0);
    f.facade.next();QCOMPARE(f.facade.current().id,b.id);QCOMPARE(sources.size(),1);
}
void PhaseGTests::platformButtonsAndGeneralGroupsFitWideAndNarrowWindows(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));QmlShell shell(&library,nullptr,true);QVERIFY2(shell.load(),qPrintable(shell.warnings().join('\n')));
    auto *w=shell.window();auto *root=w->contentItem();QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"settingsAppearanceGroup"));
    for(const auto *name:{"settingsAppearanceGroup","settingsFontGroup","settingsInteractionGroup","settingsPlaybackGroup","dataRootRow"})QVERIFY2(visual(root,name),name);
    for(int width:{1240,1000,560,414}){
        w->resize(width,790);QTRY_COMPARE(root->width(),qreal(width));
        auto *themeRow=visual(root,"settingsAppearanceCell");QTRY_COMPARE(themeRow->property("stacked").toBool(),width<620);
        auto *settingsViewport=visual(root,"settingsScroll"),*divider=visual(root,"settingsHeadingDivider");QVERIFY(settingsViewport&&divider);QTRY_VERIFY(qAbs(settingsViewport->mapToScene(QPointF()).y()-divider->mapToScene(QPointF(0,divider->height())).y())<.5);
        auto *settingsFlick=settingsViewport->property("contentItem").value<QObject *>();QVERIFY(settingsFlick);settingsFlick->setProperty("contentY",100);QTest::qWait(30);QVERIFY(visual(root,"settingsAppearanceCard")->mapToScene(QPointF()).y()<settingsViewport->mapToScene(QPointF()).y());
        const auto topFolder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!topFolder.isEmpty()){QDir().mkpath(topFolder);w->grabWindow().save(QDir(topFolder).filePath(QString("settings-scrolled-top-%1.png").arg(width)));}settingsFlick->setProperty("contentY",0);

        for(const auto &name:{QString("settingsAppearanceGroup"),QString("settingsFontGroup"),QString("settingsInteractionGroup"),QString("settingsPlaybackGroup"),QString("dataRootRow")}){
            auto *group=visual(root,name),*heading=visual(root,name+"Heading"),*body=visual(root,name+"Body");QVERIFY(group);QVERIFY(heading);QVERIFY(body);
            QTRY_COMPARE(group->property("stacked").toBool(),width<620);
            if(width>=620){QTRY_VERIFY(body->x()>=heading->width()+24*.7-.1);QCOMPARE(body->y(),heading->y());}
            else{QCOMPARE(body->x(),qreal(0));QTRY_VERIFY(body->y()>=heading->height()+10);}
        }
        const bool themeInline=themeRow->property("optionsInline").toBool();
        const bool closeInline=visual(root,"settingsCloseCell")->property("optionsInline").toBool();
        QCOMPARE(visual(root,"qmlThemeSetting")->isVisible(),!themeInline);QCOMPARE(visual(root,"closeSetting")->isVisible(),!closeInline);
        for(int i=0;i<4;++i)QCOMPARE(visual(root,QString("themeOption-%1").arg(i))->isVisible(),themeInline);
        for(int i=0;i<2;++i)QCOMPARE(visual(root,QString("closeOption-%1").arg(i))->isVisible(),closeInline);
        if(width>=620){
            for(const auto &name:{QString("settingsAppearanceCell"),QString("settingsCloseCell"),QString("settingsThresholdCell")}){
                auto *labels=visual(root,name+"Labels"),*controls=visual(root,name+"Controls");QVERIFY(labels);QVERIFY(controls);
                QTRY_VERIFY(qAbs(labels->y()+labels->height()/2-controls->y()-controls->height()/2)<.5);
            }
            QVERIFY(QMetaObject::invokeMethod(visual(root,"themeOption-1"),"clicked"));QCOMPARE(shell.settingsBridge()->values().value("theme").toInt(),1);
            QVERIFY(QMetaObject::invokeMethod(visual(root,"themeOption-0"),"clicked"));QCOMPARE(shell.settingsBridge()->values().value("theme").toInt(),0);
            QVERIFY(QMetaObject::invokeMethod(visual(root,"themeOption-2"),"clicked"));QCOMPARE(shell.settingsBridge()->values().value("theme").toInt(),2);
            QVERIFY(QMetaObject::invokeMethod(visual(root,"closeOption-1"),"clicked"));QVERIFY(shell.settingsBridge()->values().value("closeToTray").toBool());
            QVERIFY(QMetaObject::invokeMethod(visual(root,"closeOption-0"),"clicked"));QVERIFY(!shell.settingsBridge()->values().value("closeToTray").toBool());
        }
        QVERIFY(QMetaObject::invokeMethod(visual(root,"manageMusicPlatforms"),"clicked"));QTRY_COMPARE(w->property("settingsSection").toString(),QString("platforms"));QTRY_VERIFY(visual(root,"platformAccounts"));
        auto *accounts=visual(root,"platformAccounts");QTRY_VERIFY(accounts->isVisible());QVERIFY(accounts->width()<=width);
        for(int i=0;i<4;++i){auto *logo=visual(root,QString("platformLogo-%1").arg(i));QVERIFY(logo);QTRY_COMPARE(logo->property("status").toInt(),1);if(i==3){QCOMPARE(logo->property("sourceClipRect").toRectF(),QRectF(0,0,160,160));QCOMPARE(QImageReader(":/music-platforms/kugou.png").size(),QSize(600,160));}
            QCOMPARE(visual(root,QString("platformName-%1").arg(i))->isVisible(),accounts->property("showPlatformNames").toBool());
            QCOMPARE(visual(root,QString("platformAccountName-%1").arg(i))->isVisible(),!visual(root,QString("platformRow-%1").arg(i))->property("displayedName").toString().isEmpty());
            auto *avatar=visual(root,QString("platformAvatar-%1").arg(i));QVERIFY(avatar);QTRY_VERIFY(avatar->property("ready").toBool());
            for(const auto &prefix:{QString("platformLogin-"),QString("platformWebsite-")}){
                auto *button=visual(root,prefix+QString::number(i));QVERIFY(button);QVERIFY(button->width()>30);
                QList<QQuickItem *> pending=button->childItems();bool found=false;
                while(!pending.isEmpty()){auto *item=pending.takeFirst();pending.append(item->childItems());if(item->property("truncated").isValid()&&!item->property("text").toString().isEmpty()){found=true;QVERIFY2(!item->property("truncated").toBool(),qPrintable(prefix+QString::number(i)));}}
                QVERIFY(found);
            }
        }
        const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath(QString("platforms-%1.png").arg(width)));}
        QVERIFY(QMetaObject::invokeMethod(visual(root,"platformSettingsBack"),"clicked"));QTRY_COMPARE(w->property("settingsSection").toString(),QString("settings"));QTRY_VERIFY(!accounts->isVisible());
        QTRY_COMPARE(visual(root,"settingsAppearanceCell")->property("stacked").toBool(),width<620);
        if(!folder.isEmpty()){
            w->grabWindow().save(QDir(folder).filePath(QString("general-%1.png").arg(width)));
            auto *settingsScroll=w->findChild<QObject *>("settingsScroll");auto *flick=settingsScroll->property("contentItem").value<QObject *>();QVERIFY(flick);
            for(const auto &card:QStringList{"settingsPlaybackCard","settingsStorageCard"}){flick->setProperty("contentY",visual(root,card)->y());QTest::qWait(30);w->grabWindow().save(QDir(folder).filePath(QString("%1-%2.png").arg(card).arg(width)));}
            flick->setProperty("contentY",0);QTest::qWait(30);
        }
    }
    w->resize(1240,790);QTRY_VERIFY(visual(root,"themeOption-3")->isVisible());QVERIFY(QMetaObject::invokeMethod(visual(root,"themeOption-3"),"clicked"));QTRY_COMPARE(w->property("settingsSection").toString(),QString("appearance"));
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
}
void PhaseGTests::discoverySelectionAndDownloadBatchesUseTheVisibleSongs(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());Transport transport;
    AdditionalMusicBridge source(AdditionalMusicBridge::Kugou,&library,shell.facade(),shell.lyricsModel(),shell.onlineArtworkCache(),nullptr,&transport);
    auto *context=QQmlEngine::contextForObject(shell.window())->engine()->rootContext();const auto originalOnline=context->contextProperty("onlineMusic").value<QObject *>(),originalKugou=context->contextProperty("kugouMusic").value<QObject *>();context->setContextProperty("onlineMusic",&source);context->setContextProperty("kugouMusic",&source);
    const auto restoreSources=qScopeGuard([&]{context->setContextProperty("onlineMusic",originalOnline);context->setContextProperty("kugouMusic",originalKugou);});
    QVERIFY(shell.appBridge()->navigate(4));QTRY_VERIFY(visual(shell.window()->contentItem(),"discoverPage"));auto *page=visual(shell.window()->contentItem(),"discoverPage");
    QVERIFY(QMetaObject::invokeMethod(page,"chooseSource",Q_ARG(QVariant,3)));QTRY_VERIFY(!source.busy());
    transport.responses={R"({"status":1,"data":{"lists":[{"FileHash":"A","SongName":"Alpha","Duration":10},{"FileHash":"B","SongName":"Beta","Duration":20},{"FileHash":"C","SongName":"Gamma","Duration":30}]}})"};
    QVERIFY(QMetaObject::invokeMethod(page,"submitSearch",Q_ARG(QVariant,"synthetic")));QTRY_COMPARE(source.songs().size(),3);
    auto *avatar=visual(shell.window()->contentItem(),"onlineAccountAvatar");QVERIFY(avatar);
    const QColor quadrants[]={QColor("#ed4050"),QColor("#33cc55"),QColor("#2255ee"),QColor("#f2dd22")};
    QImage sharp(256,256,QImage::Format_RGB32);for(int y=0;y<256;++y)for(int x=0;x<256;++x)sharp.setPixelColor(x,y,quadrants[(y>=128?2:0)+(x>=128?1:0)]);const auto avatarPath=dir.filePath("synthetic-avatar.png");QVERIFY(sharp.save(avatarPath));
    // Authenticate the synthetic source so the logged-out person glyph does
    // not remain painted on top of a forcibly shown avatar.
    transport.responses={R"({"data":{"qrcode":"synthetic-avatar-qr"}})",QJsonDocument(QJsonObject{{"data",QJsonObject{{"status",4},{"token","synthetic-avatar-token"},{"userid","12345"},{"nickname","Mock Account"},{"avatar",QUrl::fromLocalFile(avatarPath).toString()}}}}).toJson()};
    source.startLogin();QTRY_VERIFY_WITH_TIMEOUT(source.loggedIn(),6000);QTRY_VERIFY(avatar->isVisible());QTRY_COMPARE(avatar->property("file").toUrl(),QUrl::fromLocalFile(avatarPath));QTRY_VERIFY(avatar->property("ready").toBool());
    auto completeAvatar=[&]{
        const auto frame=shell.window()->grabWindow();if(frame.isNull())return false;const auto origin=avatar->mapToScene(QPointF());const qreal ratio=qreal(frame.width())/shell.window()->width();
        for(int i=0;i<4;++i){const auto color=frame.pixelColor(qRound((origin.x()+avatar->width()*(i%2?.75:.25))*ratio),qRound((origin.y()+avatar->height()*(i/2?.75:.25))*ratio));const auto expected=quadrants[i];if(qAbs(color.red()-expected.red())>25||qAbs(color.green()-expected.green())>25||qAbs(color.blue()-expected.blue())>25)return false;}
        // The optical radius is now 8% with a continuous corner. A 4% inset
        // is inside the image; probe the actual cutout near the outer corner.
        const auto corner=frame.pixelColor(qRound((origin.x()+avatar->width()*.01)*ratio),qRound((origin.y()+avatar->height()*.01)*ratio));return qMax(qAbs(corner.red()-quadrants[0].red()),qMax(qAbs(corner.green()-quadrants[0].green()),qAbs(corner.blue()-quadrants[0].blue())))>40;
    };const bool avatarComplete=QTest::qWaitFor(completeAvatar);
    if(!avatarComplete){const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty())shell.window()->grabWindow().save(QDir(folder).filePath("avatar-failure.png"));}
    QVERIFY(avatarComplete);
    QVERIFY(!visual(shell.window()->contentItem(),"onlineMultiSelect"));QTRY_VERIFY(visual(shell.window()->contentItem(),"onlineSong-A"));QVERIFY(shell.settingsBridge()->setPreference("playlistCovers",false));QTRY_VERIFY(visual(shell.window()->contentItem(),"onlineCover-A")->isVisible());
    auto *first=visual(shell.window()->contentItem(),"onlineSong-A");
    QTest::mouseClick(shell.window(),Qt::RightButton,Qt::NoModifier,first->mapToScene(QPointF(first->width()/2,first->height()/2)).toPoint());
    auto *songMenu=shell.window()->findChild<QObject *>("onlineSongMenu");QVERIFY(songMenu);QTRY_VERIFY(songMenu->property("visible").toBool());
    auto *multi=shell.window()->findChild<QObject *>("onlineContextMultiSelect");QVERIFY(multi);QVERIFY(QMetaObject::invokeMethod(multi,"triggered"));QVERIFY(QMetaObject::invokeMethod(songMenu,"dismiss"));QVERIFY(page->property("selectionMode").toBool());
    QVERIFY(QMetaObject::invokeMethod(page,"chooseSong",Q_ARG(QVariant,"C"),Q_ARG(QVariant,2),Q_ARG(QVariant,int(Qt::ShiftModifier))));
    QCOMPARE(page->property("selectedIds").value<QJSValue>().property("length").toInt(),3);
    auto *next=visual(shell.window()->contentItem(),"onlineBatchNext");QVERIFY(next);QVERIFY(next->isEnabled());
    // This UI test checks queue commands; it never resolves or plays a network stream.
    QObject::disconnect(shell.facade(),&PlaybackFacade::onlineSourceRequested,nullptr,nullptr);
    QVERIFY(QMetaObject::invokeMethod(page,"actOnSelection",Q_ARG(QVariant,"play")));QCOMPARE(shell.facade()->queue().items.size(),3);QCOMPARE(library.trackCount(),0);
    auto *quickNext=visual(shell.window()->contentItem(),"nextOnline-B");QVERIFY(quickNext);QVERIFY(quickNext->isEnabled());auto *glyph=visual(quickNext,"nextSongGlyph");QVERIFY(glyph);QQmlExpression curve(QQmlEngine::contextForObject(glyph),glyph,"preferredRendererType === Shape.CurveRenderer");QVERIFY(curve.evaluate().toBool());if(QGuiApplication::platformName()=="windows"&&qEnvironmentVariable("QSG_RHI_BACKEND")=="d3d11")QTRY_COMPARE(glyph->property("rendererType").toInt(),glyph->property("preferredRendererType").toInt());const auto playingId=shell.facade()->current().id;
    QVERIFY(QMetaObject::invokeMethod(quickNext,"clicked"));QVERIFY(quickNext->property("confirmed").toBool());QCOMPARE(shell.facade()->current().id,playingId);QCOMPARE(shell.facade()->upcoming().first(),source.songs().at(1).toMap().value("internalId").toLongLong());
    const auto quickShotFolder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!quickShotFolder.isEmpty()){QDir().mkpath(quickShotFolder);QTest::qWait(50);shell.window()->grabWindow().save(QDir(quickShotFolder).filePath("online-next-confirmed.png"));}
    QTRY_VERIFY(!quickNext->property("confirmed").toBool());
    auto *more=visual(shell.window()->contentItem(),"moreOnline-B");QVERIFY(more);QVERIFY(QMetaObject::invokeMethod(more,"clicked"));QTRY_VERIFY(songMenu->property("visible").toBool());
    auto *songPanel=qvariant_cast<QQuickItem *>(songMenu->property("background"));QVERIFY(songPanel);QTRY_VERIFY(songPanel->height()>0);
    QTRY_VERIFY(qAbs(songPanel->mapRectToScene(songPanel->boundingRect()).top()-more->mapToScene(QPointF(0,more->height())).y())<=2);
    if(!quickShotFolder.isEmpty())shell.window()->grabWindow().save(QDir(quickShotFolder).filePath("online-song-menu.png"));
    QVERIFY(QMetaObject::invokeMethod(songMenu,"dismiss"));
    QVERIFY(shell.settingsBridge()->setPreference("discoveryActions",QVariantList{QString("download")}));QTRY_VERIFY(!visual(shell.window()->contentItem(),"nextOnline-B"));QVERIFY(!visual(shell.window()->contentItem(),"moreOnline-B"));QVERIFY(visual(shell.window()->contentItem(),"downloadOnline-B")->isVisible());
    QVERIFY(shell.settingsBridge()->setPreference("discoveryActions",QVariantList{QString("download"),QString("next"),QString("more")}));
    QVERIFY(shell.settingsBridge()->saveDiscoveryActionLayout({QVariantMap{{"id",2},{"visible",true}},QVariantMap{{"id",1},{"visible",true}},QVariantMap{{"id",0},{"visible",true}}}));
    QTRY_VERIFY(visual(shell.window()->contentItem(),"moreOnline-B"));
    QTRY_VERIFY(visual(shell.window()->contentItem(),"moreOnline-B")->mapToScene(QPointF()).x()<visual(shell.window()->contentItem(),"nextOnline-B")->mapToScene(QPointF()).x());
    QVERIFY(visual(shell.window()->contentItem(),"nextOnline-B")->mapToScene(QPointF()).x()<visual(shell.window()->contentItem(),"downloadOnline-B")->mapToScene(QPointF()).x());
    QVERIFY(QMetaObject::invokeMethod(page,"selectAll",Q_ARG(QVariant,true)));QCOMPARE(page->property("selectedIds").value<QJSValue>().property("length").toInt(),0);
    QVERIFY(QMetaObject::invokeMethod(page,"selectAll",Q_ARG(QVariant,false)));
    shell.window()->resize(414,540);QTRY_VERIFY(visual(shell.window()->contentItem(),"onlineBatchMenuButton")->isVisible());
    QVERIFY(!visual(shell.window()->contentItem(),"onlineBatchActions")->isVisible());QVERIFY(visual(shell.window()->contentItem(),"onlineSongs")->height()>=60);
    QVERIFY(QMetaObject::invokeMethod(visual(shell.window()->contentItem(),"onlineBatchMenuButton"),"clicked"));
    auto *batchMenu=shell.window()->findChild<QObject *>("onlineBatchMenu");QVERIFY(batchMenu);QTRY_VERIFY(batchMenu->property("visible").toBool());
    auto *menuBackground=qvariant_cast<QQuickItem *>(batchMenu->property("background"));QVERIFY(menuBackground);
    // Popup x/y are relative to its positioning item and may be negative when
    // Qt flips it left. Verify the actual panel in window coordinates.
    QTRY_VERIFY(menuBackground->width()>0&&menuBackground->height()>0);
    QTRY_VERIFY(QRectF(0,0,414,540).contains(menuBackground->mapRectToScene(menuBackground->boundingRect())));
    QVERIFY(QMetaObject::invokeMethod(batchMenu,"dismiss"));
    const auto screenshotFolder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!screenshotFolder.isEmpty())shell.window()->grabWindow().save(QDir(screenshotFolder).filePath("discovery-multi-414.png"));
    // Do not dispatch the queued transfer timers: these are synthetic metadata
    // rows, and no real network resource is used by this test.
    QVERIFY(QMetaObject::invokeMethod(page,"actOnSelection",Q_ARG(QVariant,"download")));QCOMPARE(shell.onlineDownloadBridge()->rowCount(),3);
    for(int i=0;i<shell.onlineDownloadBridge()->rowCount();++i)shell.onlineDownloadBridge()->cancelTask(shell.onlineDownloadBridge()->data(shell.onlineDownloadBridge()->index(i),Qt::UserRole).toInt());
    transport.responses={R"({"data":{"songs":[]}})"};source.openPlaylist("synthetic-list");QTRY_VERIFY(!source.busy());QTRY_VERIFY(visual(shell.window()->contentItem(),"onlinePlaylistBack")->isVisible());QCOMPARE(visual(shell.window()->contentItem(),"onlinePlaylistBack")->property("symbol").toString(),QString("log-out"));
    if(!screenshotFolder.isEmpty()){shell.window()->resize(1240,790);QTest::qWait(60);shell.window()->grabWindow().save(QDir(screenshotFolder).filePath("discovery-playlist-return.png"));}
    context->setContextProperty("onlineMusic",originalOnline);context->setContextProperty("kugouMusic",originalKugou);
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join(QChar(10))));
}
void PhaseGTests::platformAccountNamesUseMeasuredSpaceAndRecoverOnExpansion(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));std::vector<std::unique_ptr<QObject>> mocks;QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());
    auto *w=shell.window();auto *context=QQmlEngine::contextForObject(w)->engine()->rootContext();
    QQmlComponent component(context->engine());component.setData(R"(
        import QtQuick
        QtObject {
            property bool loggedIn:true
            property string nickname:""
            property url avatarUrl:""
            property string membership:"测试会员"
            property string loginChannel:"qq"
            property string qrImage:""
            property string status:""
            property int activations:0
            property int logouts:0
            property int logins:0
            function logout(){logouts++;loggedIn=false}
            function startLogin(){logins++}
            signal loginCompleted()
            function activate(){activations++}
            function stopLogin(){}
        }
    )",QUrl());QVERIFY2(component.isReady(),qPrintable(component.errorString()));
    const QStringList keys={"onlineMusic","qqMusic","sodaMusic","kugouMusic"};
    const QStringList names={QStringLiteral("五字短名字"),QStringLiteral("六字测试名字"),QStringLiteral("十字名字测试一二三四"),QStringLiteral("超过十字的测试用户名一二三四五六")};
    QList<QObject *> original;
    for(int i=0;i<keys.size();++i){original.append(context->contextProperty(keys[i]).value<QObject *>());mocks.emplace_back(component.create());QVERIFY(mocks.back());mocks.back()->setProperty("nickname",names[i]);context->setContextProperty(keys[i],mocks.back().get());}
    const auto restore=qScopeGuard([&]{if(auto *accounts=visual(w->contentItem(),"platformAccounts"))accounts->setProperty("loginSource",QVariant::fromValue(original.first()));for(int i=0;i<keys.size();++i)context->setContextProperty(keys[i],original[i]);});
    QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(w->contentItem(),"manageMusicPlatforms"));QVERIFY(QMetaObject::invokeMethod(visual(w->contentItem(),"manageMusicPlatforms"),"clicked"));QTRY_VERIFY(visual(w->contentItem(),"platformAccounts"));auto *accounts=visual(w->contentItem(),"platformAccounts");QTRY_VERIFY(accounts->isVisible());
    for(auto &mock:mocks)QVERIFY(mock->property("activations").toInt()>0);
    QQmlComponent measureComponent(context->engine());measureComponent.setData("import QtQuick\nText { visible:false }",QUrl());
    std::unique_ptr<QObject> probe(measureComponent.create());QVERIFY(probe);qobject_cast<QQuickItem *>(probe.get())->setParentItem(w->contentItem());
    bool sawFull=false,sawTen=false,sawFive=false,sawHiddenShort=false,sawHiddenLong=false,sawCollapsed=false;
    auto verify=[&]{
        const bool platforms=accounts->property("showPlatformNames").toBool();if(!platforms)sawCollapsed=true;
        for(int i=0;i<4;++i){
            auto *row=visual(w->contentItem(),QString("platformRow-%1").arg(i));auto *text=visual(w->contentItem(),QString("platformAccountText-%1").arg(i));
            const auto full=mocks[i]->property("nickname").toString(),displayed=text->property("text").toString();const auto space=row->property("nameSpace").toReal();
            probe->setProperty("font",text->property("font"));
            const auto measure=[&](const QString &value){probe->setProperty("text",value);return qCeil(probe->property("implicitWidth").toReal());};
            const auto expected=measure(full)<=space?full:platforms?full.left(10)+"...":full.size()>=6&&measure(full.left(5)+"...")<=space?full.left(5)+"...":QString();
            QCOMPARE(displayed,expected);QCOMPARE(visual(w->contentItem(),QString("platformName-%1").arg(i))->isVisible(),platforms);
            if(!displayed.isEmpty()){
                QTRY_VERIFY2(text->implicitWidth()<=text->width()+.5,qPrintable(displayed));
            }
            if(displayed==full)sawFull=true;else if(displayed==full.left(10)+"...")sawTen=true;else if(displayed==full.left(5)+"...")sawFive=true;else if(full.size()<=5)sawHiddenShort=true;else sawHiddenLong=true;
        }
    };
    for(int width=760;width>=220;width-=10){accounts->setProperty("width",width);QTest::qWait(40);verify();if(QTest::currentTestFailed())return;
        const auto state=accounts->property("showPlatformNames");QTest::qWait(5);QCOMPARE(accounts->property("showPlatformNames"),state);
    }
    QVERIFY(sawFull);QVERIFY(sawTen);QVERIFY(sawFive);QVERIFY(sawHiddenShort);QVERIFY(sawHiddenLong);QVERIFY(sawCollapsed);
    accounts->setProperty("width",760);QTRY_VERIFY(accounts->property("showPlatformNames").toBool());verify();if(QTest::currentTestFailed())return;
    // Equal character counts have different pixel widths: they must not use a
    // character-count threshold or a fixed window-width cutoff.
    for(auto &mock:mocks)mock->setProperty("nickname","iiiiiiiiii");accounts->setProperty("width",360);QTRY_VERIFY(accounts->property("showPlatformNames").toBool());
    for(auto &mock:mocks)mock->setProperty("nickname","WWWWWWWWWW");QTRY_VERIFY(!accounts->property("showPlatformNames").toBool());
    for(auto &mock:mocks)mock->setProperty("nickname","iiiiiiiiii");QTRY_VERIFY(accounts->property("showPlatformNames").toBool());
    for(int width:{760,300}){accounts->setProperty("width",width);QTest::qWait(30);for(int i=0;i<4;++i){
        auto *button=visual(w->contentItem(),QString("platformLogin-%1").arg(i));QVERIFY(button);QVERIFY(!visual(w->contentItem(),QString("platformLogout-%1").arg(i)));mocks[i]->setProperty("loggedIn",true);QTRY_COMPARE(button->property("text").toString(),QString("退出"));const auto logoutCount=mocks[i]->property("logouts").toInt();QVERIFY(QMetaObject::invokeMethod(button,"clicked"));QCOMPARE(mocks[i]->property("logouts").toInt(),logoutCount+1);QTRY_COMPARE(button->property("text").toString(),QString("登录"));QCOMPARE(mocks[i]->property("logins").toInt(),width==760?0:1);QVERIFY(QMetaObject::invokeMethod(button,"clicked"));auto *login=w->findChild<QObject *>("platformLoginDialog");QVERIFY(login);QTRY_VERIFY(login->property("visible").toBool());QCOMPARE(mocks[i]->property("logins").toInt(),width==760?1:2);QVERIFY(QMetaObject::invokeMethod(login,"close"));
    }}
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join(QChar(10))));
}
void PhaseGTests::nativeWindowsTransitionsRetainAFullClientArea(){
#ifdef Q_OS_WIN
    if(QGuiApplication::platformName()!="windows")QSKIP("Native Windows check is run separately from offscreen UI tests");
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();QVERIFY(QTest::qWaitForWindowExposed(w));
    const auto handle=reinterpret_cast<HWND>(w->winId());const auto style=GetWindowLongPtrW(handle,GWL_STYLE);QVERIFY((style&WS_CAPTION)==WS_CAPTION);QVERIFY(style&WS_MINIMIZEBOX);
    RECT outer{},client{};GetWindowRect(handle,&outer);GetClientRect(handle,&client);QCOMPARE(client.right-client.left,outer.right-outer.left);QCOMPARE(client.bottom-client.top,outer.bottom-outer.top);
    const auto size=w->size();w->showMinimized();QTRY_VERIFY(IsIconic(handle));w->showNormal();QTRY_VERIFY(!IsIconic(handle));QVERIFY(QTest::qWaitForWindowExposed(w));QCOMPARE(w->size(),size);
    GetWindowRect(handle,&outer);GetClientRect(handle,&client);QCOMPARE(client.bottom-client.top,outer.bottom-outer.top);
    QVERIFY(library.setSetting("qml.reduceMotion",true));
    w->resize(900,600);const auto normalSize=w->size();QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());QCOMPARE(reinterpret_cast<HWND>(w->winId()),handle);
    GetWindowRect(handle,&outer);GetClientRect(handle,&client);QCOMPARE(client.right,outer.right-outer.left);QCOMPARE(client.bottom,outer.bottom-outer.top);
    QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(!w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());QCOMPARE(w->size(),normalSize);
    BOOL disabled=FALSE;const auto hr=DwmGetWindowAttribute(handle,DWMWA_TRANSITIONS_FORCEDISABLED,&disabled,sizeof(disabled));if(SUCCEEDED(hr))QVERIFY(disabled);
#else
    QSKIP("Windows-only compositor policy");
#endif
}
void PhaseGTests::sidebarMorphAndDragKeepAnimatedIntermediateStates(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",false));QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"customizeSidebar"));
    auto *dialog=w->findChild<QObject *>("sidebarEditor");QVERIFY(dialog);QVERIFY(QMetaObject::invokeMethod(dialog,"openEditor"));QTRY_COMPARE(dialog->property("scale").toReal(),1.);
    auto *check=visual(root,"sidebarShown-0");QVERIFY(check);auto *morph=visual(check,"checkMorph");QVERIFY(morph);QCOMPARE(morph->property("progress").toReal(),1.);
    const auto click=check->mapToScene(QPointF(check->width()/2,check->height()/2)).toPoint();QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,click);QVERIFY(!check->property("checked").toBool());QVERIFY(morph->property("progress").toReal()>0);QTRY_COMPARE(morph->property("progress").toReal(),0.);
    auto *handle=visual(root,"sidebarDrag-0");auto *neighbor=visual(root,"sidebarDraft-5");QVERIFY(handle);QVERIFY(neighbor);const auto oldY=neighbor->mapToScene(QPointF()).y();const auto from=handle->mapToScene(QPointF(handle->width()/2,handle->height()/2)).toPoint();
    QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(w,from+QPoint(0,96),20);QVERIFY(dialog->property("dragging").toBool());QCOMPARE(dialog->property("dragTo").toInt(),2);QTRY_VERIFY(neighbor->mapToScene(QPointF()).y()<oldY);
    const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("sidebar-drag.png"));
    QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,from+QPoint(0,96));QVERIFY(dialog->property("settling").toBool());QTRY_VERIFY(!dialog->property("settling").toBool());QVERIFY(!dialog->property("dragging").toBool());
    QVERIFY(QMetaObject::invokeMethod(dialog,"save"));QCOMPARE(shell.settingsBridge()->values().value("sidebarSections").toList().at(2).toMap().value("id").toInt(),0);
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join(QChar(10))));
}
void PhaseGTests::newPreferencesValidateAndPreserveLegacyAnimationSettings(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));SettingsBridge settings(&library);
    QCOMPARE(settings.values().value("animationMode").toInt(),2);QVERIFY(settings.values().value("reduceMotion").toBool());
    QVERIFY(settings.setPreference("animationMode",1));QVERIFY(!library.setting("qml.reduceMotion").toBool());QCOMPARE(settings.values().value("animationMode").toInt(),1);
    QVERIFY(settings.setPreference("reduceMotion",true));QCOMPARE(settings.values().value("animationMode").toInt(),2);
    QVERIFY(settings.setPreference("animationMode",0));QVERIFY(!settings.values().value("reduceMotion").toBool());QVERIFY(!settings.setPreference("animationMode",3));
    QCOMPARE(settings.values().value("cacheLimitMB").toInt(),1024);QVERIFY(settings.values().value("rememberPosition").toBool());QVERIFY(!settings.values().value("playbackFade").toBool());
    QVERIFY(!settings.setPreference("cacheLimitMB",255));QVERIFY(!settings.setPreference("cacheLimitMB",40961));QVERIFY(settings.setPreference("cacheLimitMB",256));QVERIFY(settings.setPreference("cacheLimitMB",40960));
    QVERIFY(settings.setPreference("playbackFade",true));QVERIFY(settings.setPreference("rememberPosition",false));
    QVERIFY(!settings.setPreference("discoveryActions",QVariantList{QString("next"),QString("next")}));QVERIFY(!settings.setPreference("discoveryActions",QVariantList{QString("unknown")}));
    QVERIFY(settings.setPreference("discoveryActions",QVariantList{QString("more")}));SettingsBridge reopened(&library);QCOMPARE(reopened.values().value("discoveryActions").toList(),QVariantList{QString("more")});QVERIFY(reopened.values().value("playbackFade").toBool());QVERIFY(!reopened.values().value("rememberPosition").toBool());
    QVariantList layout{QVariantMap{{"id",2},{"visible",true}},QVariantMap{{"id",0},{"visible",false}},QVariantMap{{"id",1},{"visible",true}}};QVERIFY(reopened.saveDiscoveryActionLayout(layout));SettingsBridge ordered(&library);QCOMPARE(ordered.values().value("discoveryActionLayout").toList(),layout);QCOMPARE(ordered.values().value("discoveryActions").toList(),(QVariantList{QString("more"),QString("next")}));
    layout[1]=QVariantMap{{"id",0},{"visible",true}};QVERIFY(ordered.saveDiscoveryActionLayout(layout));QCOMPARE(ordered.values().value("discoveryActions").toList(),(QVariantList{QString("more"),QString("download"),QString("next")}));
}
void PhaseGTests::cacheBudgetEvictsGeneratedFilesAndProtectsAccountsAndActiveAudio(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto folder=DiskCache::online(&library);const auto legacy=QDir(library.root()).filePath("Online");QVERIFY(QDir().mkpath(legacy));
    auto write=[](const QString &path,int size){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(QByteArray(size,'x'))==size;};
    const auto old=QDir(folder).filePath("old.png"),active=QDir(DiskCache::root(&library)).filePath("active.flac"),account=QDir(legacy).filePath("netease-session.bin"),music=QDir(library.root()).filePath("synthetic-user.flac");
    QVERIFY(write(old,60));QVERIFY(write(active,60));QVERIFY(write(account,100));QVERIFY(write(music,100));
    DiskCache::activeFiles.insert(active);const auto unpin=qScopeGuard([&]{DiskCache::activeFiles.remove(active);});
    QCOMPARE(DiskCache::usage(&library),qint64(120));QCOMPARE(DiskCache::trim(&library,{},80),qint64(60));QVERIFY(!QFile::exists(old));QVERIFY(QFile::exists(active));QVERIFY(QFile::exists(account));QVERIFY(QFile::exists(music));
    DiskCache::activeFiles.remove(active);QCOMPARE(DiskCache::trim(&library,{},0),qint64(0));QVERIFY(QFile::exists(account));QVERIFY(QFile::exists(music));
}
void PhaseGTests::audioFaderRampsAndCancelsAPendingPause(){
    qreal audible=1;bool paused=false;AudioFader fade([&](qreal gain){audible=gain;});fade.reset(0);QCOMPARE(audible,qreal(0));
    fade.to(1,true);QTRY_VERIFY(audible>0&&audible<1);QTRY_COMPARE(audible,qreal(1));
    fade.to(0,true,[&]{paused=true;});QVERIFY(!paused);QTRY_VERIFY(audible<1&&audible>0);fade.to(1,true);QTRY_COMPARE(audible,qreal(1));QVERIFY(!paused);
    fade.to(0,true,[&]{paused=true;});QTRY_VERIFY(paused);QCOMPARE(audible,qreal(0));fade.to(1,false);QCOMPARE(audible,qreal(1));
}
void PhaseGTests::localProgressSurvivesLoadingAndHonorsTheMemorySwitch(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto wave=syntheticWave(QDir(library.root()).filePath("Music"));QVERIFY(!wave.isEmpty());
    Track track;track.file="Music/synthetic.wav";track.hash="synthetic-wave";track.title="Synthetic";track.duration=12000;const auto id=library.insert(track);QVERIFY(id>0);
    QVERIFY(library.setSettings({{"queue",QJsonArray{double(id)}},{"current",double(id)},{"position",5000}}));
    {Playback playback(&library,nullptr,true);playback.restore();QCOMPARE(playback.position(),qint64(5000));QVERIFY(playback.save());QCOMPARE(library.setting("position").toInt(),5000);QTRY_VERIFY(playback.seekable());QTRY_VERIFY(qAbs(playback.position()-5000)<100);QVERIFY(playback.shutdown());}
    QVERIFY(library.setSetting("rememberPosition",false));{Playback playback(&library,nullptr,true);playback.restore();QCOMPARE(playback.position(),qint64(0));QVERIFY(playback.save());QCOMPARE(library.setting("position").toInt(),0);}
}
void PhaseGTests::onlineProgressRestoresPausedWithoutPersistingStreamUrls(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto wave=syntheticWave(dir.filePath("audio"));QVERIFY(!wave.isEmpty());
    Track track;track.id=2000000000001LL;track.hash="qqmusic:synthetic-mid";track.title="Synthetic online";track.duration=12000;track.onlineMetadata={{"id","1"},{"mid","synthetic-mid"},{"mediaMid","synthetic-media"},{"url","https://example.test/synthetic-secret-stream"},{"token","synthetic-credential"}};
    qint64 savedPosition=0;
    {PlaybackFacade facade(&library,nullptr,true);facade.playOnline({track},track.id);facade.setOnlineSource(track.id,QUrl::fromLocalFile(wave));QTRY_VERIFY(facade.seekable());facade.seek(5000);QVERIFY(facade.save());savedPosition=qint64(library.setting("onlinePlayback").toObject().value("position").toDouble());QVERIFY(savedPosition>=5000);QVERIFY(facade.shutdown());}
    const auto bytes=QJsonDocument(library.setting("onlinePlayback").toObject()).toJson();QVERIFY(!bytes.contains("synthetic-secret-stream"));QVERIFY(!bytes.contains("synthetic-credential"));
    {PlaybackFacade facade(&library,nullptr,true);QSignalSpy requested(&facade,&PlaybackFacade::onlineSourceRequested);facade.restore(true);QVERIFY(facade.onlineActive());QCOMPARE(facade.current().id,track.id);QVERIFY(facade.position()>=savedPosition);QVERIFY(!facade.playing());QCOMPARE(requested.size(),0);QCOMPARE(facade.current().onlineMetadata.value("mid").toString(),QString("synthetic-mid"));facade.toggle();QCOMPARE(requested.size(),1);facade.setOnlineSource(track.id,QUrl::fromLocalFile(wave));QTRY_VERIFY(facade.seekable());QTRY_VERIFY(facade.position()>=savedPosition);QVERIFY(facade.shutdown());}
    QVERIFY(library.setSetting("rememberPosition",false));{PlaybackFacade facade(&library,nullptr,true);facade.restore(true);QVERIFY(!facade.onlineActive());QVERIFY(facade.save());QVERIFY(library.setting("onlinePlayback").toObject().isEmpty());}
}
void PhaseGTests::lightModeSkipsBothShortcutMiniTransitions(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();
    QVERIFY(shell.settingsBridge()->setPreference("animationMode",1));QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"settingsStorageCard"));
    for(const auto *name:{"settingsAppearanceCard","settingsPlaybackCard","settingsStorageCard","playbackFadeSetting","rememberPositionSetting","cacheLimitSetting","customizeDiscoveryActions","lyricColorFollowSetting","lyricColorCustomSetting"})QVERIFY2(visual(root,name),name);
    QCOMPARE(visual(root,"settingsThresholdCell")->property("description").toString(),QString());auto *limit=visual(root,"cacheLimitSetting");QCOMPARE(limit->property("from").toInt(),256);QCOMPARE(limit->property("to").toInt(),40960);QCOMPARE(limit->property("value").toInt(),1024);
    QQmlExpression typedLimit(QQmlEngine::contextForObject(limit),limit,"valueFromText('40960 MB')");QCOMPARE(typedLimit.evaluate().toInt(),40960);
    QVERIFY(shell.settingsBridge()->setPreference("lyricColor",QString("#123456")));QVERIFY(QMetaObject::invokeMethod(visual(root,"lyricColorFollowSetting"),"clicked"));QVERIFY(shell.settingsBridge()->values().value("lyricColor").toString().isEmpty());
    QVERIFY(QMetaObject::invokeMethod(visual(root,"customizeDiscoveryActions"),"clicked"));auto *dialog=w->findChild<QObject *>("discoveryActionsDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
    QTRY_VERIFY(dialog->property("opened").toBool());QTRY_VERIFY(visual(root,"discoveryShown-0"));auto *downloadChoice=visual(root,"discoveryShown-0");QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,downloadChoice->mapToScene(QPointF(downloadChoice->width()/2,downloadChoice->height()/2)).toPoint());QVERIFY(!downloadChoice->property("checked").toBool());
    auto *handle=visual(root,"discoveryDrag-2");QVERIFY(handle);const auto grab=handle->mapToScene(QPointF(handle->width()/2,handle->height()/2)).toPoint();QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,grab);QTest::mouseMove(w,grab-QPoint(0,96),20);QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,grab-QPoint(0,96));QTRY_VERIFY(!dialog->property("settling").toBool());
    const auto editorShot=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!editorShot.isEmpty()){QDir().mkpath(editorShot);QTest::qWait(80);w->grabWindow().save(QDir(editorShot).filePath("discovery-shortcuts-editor.png"));}
    QCOMPARE(shell.settingsBridge()->values().value("discoveryActions").toList().size(),3);QVERIFY(QMetaObject::invokeMethod(dialog,"save"));QCOMPARE(shell.settingsBridge()->values().value("discoveryActions").toList(),(QVariantList{QString("more"),QString("next")}));QTRY_VERIFY(!dialog->property("visible").toBool());
    QSignalSpy miniFrames(w,SIGNAL(miniTransitionProgressChanged()));QVERIFY(miniFrames.isValid());
    QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());QVERIFY(miniFrames.size()<=2);
    miniFrames.clear();QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QVERIFY(!w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());QVERIFY(miniFrames.size()<=2);
    QVERIFY(QMetaObject::invokeMethod(w,"enterMiniFromEdge",Q_ARG(QVariant,0),Q_ARG(QVariant,false)));QVERIFY(w->property("miniSwitching").toBool());QVERIFY(!w->property("skipMiniAnimation").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());
    QVERIFY(shell.settingsBridge()->setPreference("animationMode",2));QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(!w->property("miniSwitching").toBool());QVERIFY(shell.settingsBridge()->values().value("reduceMotion").toBool());
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
}

void PhaseGTests::sharedUpcomingActuallyAdvancesAcrossLocalAndOnlineAudio(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
    const auto waveA=syntheticWave(library.absolute("Music/A")),waveB=syntheticWave(library.absolute("Music/B")),waveC=syntheticWave(dir.filePath("cloud"));
    Track a;a.file="Music/A/synthetic.wav";a.hash="wave-a";a.title="Local A";a.duration=12000;const auto idA=library.insert(a);QVERIFY(idA>0);
    Track b=a;b.file="Music/B/synthetic.wav";b.hash="wave-b";b.title="Local B";const auto idB=library.insert(b);QVERIFY(idB>0);
    Track c;c.id=1000000000007LL;c.hash="netease:7";c.title="Cloud C";c.duration=12000;c.onlineMetadata={{"id","7"}};
    const auto playlist=library.createPlaylist("Local only");QVERIFY(library.addToPlaylist(playlist,{idA,idB}));
    QVector<qint64> savedQueue;
    {
        PlaybackFacade facade(&library,nullptr,true);facade.setMode(Mode::Sequential);UpcomingModel upcoming(&library,&facade);QueueListModel queue(&library,&facade);
        facade.backend()->media()->setAudioBufferOutput(new QAudioBufferOutput(facade.backend()->media()));
        connect(&facade,&PlaybackFacade::onlineSourceRequested,&facade,[&](qint64 id){for(auto *player:facade.findChildren<QMediaPlayer *>())if(player!=facade.backend()->media()&&!player->audioBufferOutput())player->setAudioBufferOutput(new QAudioBufferOutput(player));facade.setOnlineSource(id,QUrl::fromLocalFile(waveC));});
        facade.play({idA,idB},idA);QTRY_VERIFY(facade.playing());QTRY_VERIFY(facade.seekable());
        QVERIFY(facade.enqueueTracks({c},true));QCOMPARE(facade.current().id,idA);QVERIFY(!facade.onlineActive());QVERIFY(facade.playing());
        QTRY_COMPARE(upcoming.ids(),(QVector<qint64>{c.id,idB}));QTRY_COMPARE(queue.count(),3);
        QCOMPARE(queue.data(queue.index(queue.indexForKey(QString::number(c.id))),TrackListModel::Title).toString(),c.title);
        QTRY_COMPARE(facade.duration(),qint64(12000));QTest::qWait(50);facade.seek(11850);QTRY_COMPARE_WITH_TIMEOUT(facade.current().id,c.id,5000);QVERIFY(facade.onlineActive());QTRY_VERIFY(facade.seekable());QTRY_VERIFY(facade.playing());
        QVERIFY(facade.enqueueTracks({library.track(idA)},true));QTRY_COMPARE(upcoming.ids(),(QVector<qint64>{idA,idB}));
        QTRY_COMPARE(facade.duration(),qint64(12000));QTest::qWait(50);facade.seek(11850);QTRY_COMPARE_WITH_TIMEOUT(facade.current().id,idA,5000);QVERIFY(!facade.onlineActive());QTRY_VERIFY(facade.seekable());
        QTRY_COMPARE(upcoming.ids(),(QVector<qint64>{idB}));QVERIFY(upcoming.activate(0));QCOMPARE(facade.current().id,idB);QTRY_VERIFY(facade.playing());QTRY_VERIFY(facade.seekable());QTRY_COMPARE(facade.duration(),qint64(12000));QTest::qWait(50);facade.seek(11850);QTRY_VERIFY_WITH_TIMEOUT(!facade.playing(),5000);QCOMPARE(facade.current().id,idB);facade.toggle();QTRY_VERIFY(facade.playing());QCOMPARE(facade.current().id,idB);
        QVERIFY(facade.enqueueTracks({c},true));QTRY_COMPARE(upcoming.firstTitle(),c.title);QVERIFY(upcoming.activate(0));QCOMPARE(facade.current().id,c.id);QTRY_VERIFY(facade.seekable());
        facade.seek(4000);savedQueue=facade.queue().items;QVERIFY(facade.shutdown());
    }
    {PlaybackFacade restored(&library,nullptr,true);restored.restore(true);QCOMPARE(restored.queue().items,savedQueue);QCOMPARE(restored.current().id,c.id);QCOMPARE(restored.current().title,c.title);QVERIFY(!restored.playing());QVERIFY(restored.position()>=4000);QVERIFY(restored.enqueueTracks({library.track(idA)},true));restored.playUpcoming(0);QCOMPARE(restored.current().id,idA);QVERIFY(!restored.onlineActive());QTRY_VERIFY(restored.seekable());restored.seek(3000);savedQueue=restored.queue().items;QVERIFY(restored.shutdown());}
    {PlaybackFacade restored(&library,nullptr,true);restored.restore(true);QCOMPARE(restored.queue().items,savedQueue);QCOMPARE(restored.current().id,idA);QVERIFY(!restored.onlineActive());QVERIFY(!restored.playing());QVERIFY(restored.position()>=3000);QVERIFY(restored.shutdown());}
    QCOMPARE(library.trackCount(),qint64(2));const auto unchanged=library.tracks(playlist);QCOMPARE(unchanged.size(),2);QCOMPARE(unchanged[0].id,idA);QCOMPARE(unchanged[1].id,idB);
}
void PhaseGTests::cloudVipAndCoverlessRowsSurviveFontSizeChanges(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());
    auto *w=shell.window();auto *root=w->contentItem();auto *bridge=shell.libraryBridge();QObject::disconnect(bridge,&LibraryBridge::cloudPlaylistRequested,nullptr,nullptr);
    const QVariantMap details{{"id","mock-list"},{"provider","netease"},{"name","Synthetic cloud playlist"},{"count",1}};
    const auto id=bridge->pinCloudPlaylist(details);QVERIFY(!id.isEmpty());QVERIFY(bridge->openPlaylist(id));
    bridge->setCloudSongs("netease",details,{QVariantMap{{"id","7"},{"title",QString("很长的歌曲名称用于确认会员标记始终完整可见").repeated(4)},{"artist","Synthetic Artist"},{"album","Synthetic Album"},{"duration",12000},{"requiresSubscription",true}}});
    QTRY_VERIFY(visual(root,"trackVip-1000000000007"));
    for(const int size:{15,20,12}){
        QVERIFY(shell.settingsBridge()->setPreference("fontSize",size));
        for(const int width:{1240,560,414}){
            w->resize(width,790);QTRY_COMPARE(w->property("narrowLayout").toBool(),width<547);QTest::qWait(60);auto *badge=visual(root,"trackVip-1000000000007");auto *title=visual(root,"trackTitle-1000000000007");QVERIFY(badge&&title);
            QTRY_VERIFY(badge->isVisible());QTRY_VERIFY(badge->x()+badge->width()<=badge->parentItem()->width()+.5);QTRY_VERIFY(title->x()+title->width()+5<=badge->x());
            QTRY_COMPARE(title->property("font").value<QFont>().pixelSize(),size);
            QVERIFY(shell.settingsBridge()->setPreference("playlistCovers",false));auto *cover=visual(root,"trackCover-1000000000007");QVERIFY(cover);QTRY_VERIFY(!cover->isVisible());QTRY_COMPARE(title->x(),qreal(8));QVERIFY(visual(root,"playlistCover")->isVisible());
            QVERIFY(visual(root,"trackDelegate")->height()>=size*2);
            const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()&&size==20){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath(QString("cloud-vip-font20-%1.png").arg(width)));}
            QVERIFY(shell.settingsBridge()->setPreference("playlistCovers",true));QTRY_VERIFY(cover->isVisible());
        }
    }
    w->resize(1240,790);QTest::qWait(40);QVERIFY(shell.settingsBridge()->setPreference("fontSize",15));QVERIFY(shell.settingsBridge()->setPreference("playlistHeaderAnimation",false));auto *cloudHeader=visual(root,"playlistHeader");QTRY_VERIFY(cloudHeader->property("collapsed").toBool());QTRY_VERIFY(cloudHeader->height()<100);auto *cloudList=visual(root,"trackListView");const auto headerY=cloudHeader->mapToScene(QPointF()).y();cloudList->setProperty("contentY",cloudList->property("contentY").toReal()+60);QTRY_VERIFY(cloudHeader->mapToScene(QPointF()).y()<headerY-59);QVERIFY(cloudHeader->property("collapsed").toBool());QVERIFY(shell.settingsBridge()->setPreference("playlistHeaderAnimation",true));QTRY_VERIFY(!cloudHeader->property("collapsed").toBool());
    QVERIFY(!shell.settingsBridge()->setPreference("fontSize",11));QVERIFY(!shell.settingsBridge()->setPreference("fontSize",21));
    w->resize(1240,790);QTest::qWait(40);
    Track local;local.file="Music/synthetic-unused.wav";local.hash="cover-scope-fixture";local.title="Local cover fixture";local.duration=12000;const auto localId=library.insert(local);QVERIFY(localId>0);const auto localList=library.createPlaylist("Custom playlist");QVERIFY(library.addToPlaylist(localList,{localId}));QVERIFY(library.favorite(localId,true));
    QVERIFY(shell.settingsBridge()->setPreference("playlistCovers",false));QVERIFY(bridge->openPlaylist(QString::number(localList)));QTRY_VERIFY(visual(root,"trackCover-"+QString::number(localId)));QTRY_VERIFY(!visual(root,"trackCover-"+QString::number(localId))->isVisible());QVERIFY(visual(root,"playlistCover")->isVisible());QCOMPARE(visual(root,"trackTitle-"+QString::number(localId))->x(),qreal(8));
    for(const auto &scope:QStringList{"-1","0"}){QVERIFY(bridge->openPlaylist(scope));QTRY_VERIFY(visual(root,"trackCover-"+QString::number(localId))->isVisible());}
    auto session=library.startSession(localId,12000);QVERIFY(library.saveSession(session,1000,80,true));QVERIFY(bridge->openPlaylist("-2"));QVERIFY(shell.appBridge()->navigate(2));QTRY_VERIFY(visual(root,"recentTable"));QVERIFY(visual(root,"recentTable")->property("showCovers").toBool());
    QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"uiFontSetting"));QVERIFY(QMetaObject::invokeMethod(visual(root,"uiFontSetting"),"clicked"));
    auto *dialog=w->findChild<QObject *>("uiFontDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());dialog->setProperty("chosenSize",18);
    QTRY_VERIFY(visual(root,"saveRegionFonts"));QVERIFY(QMetaObject::invokeMethod(visual(root,"saveRegionFonts"),"clicked"));QTRY_VERIFY(!dialog->property("visible").toBool());QCOMPARE(shell.settingsBridge()->values().value("fontSize").toInt(),18);QCOMPARE(library.setting("fontSize").toInt(),18);QVERIFY(shell.settingsBridge()->resetFont());QCOMPARE(shell.settingsBridge()->values().value("fontSize").toInt(),15);
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
}
void PhaseGTests::compactChromeAndFixedPlaylistHeaderFollowThePreferences(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));
    QVector<qint64> ids;for(int i=0;i<30;++i){const auto wave=syntheticWave(library.absolute(QString("Music/mock-%1").arg(i)));Track t;t.file=QDir(library.root()).relativeFilePath(wave);t.hash=t.file;t.title=i?QString("Long synthetic title ").repeated(12):"Short title";t.artist="Artist";ids.append(library.insert(t));}
    const auto id=library.createPlaylist("Synthetic playlist");QVERIFY(library.addToPlaylist(id,ids));QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();w->resize(1240,790);QVERIFY(shell.libraryBridge()->openPlaylist(QString::number(id)));
    auto *bar=visual(root,"playerBar"),*center=visual(root,"playerCenter"),*metadata=visual(root,"playerMetadata"),*panel=visual(root,"nextQueuePanel");QVERIFY(bar&&center&&metadata&&panel);QTRY_COMPARE(visual(root,"sidebar")->width(),qreal(205));QVERIFY(qAbs(bar->height()-79.38)<1);QVERIFY(qAbs(center->y()-((bar->height()-center->height())/2+3))<.5);
    auto *brand=visual(root,"sidebarBrand"),*caption=visual(root,"sidebarBrandCaption");QVERIFY(brand&&caption);for(int size:{15,20}){QVERIFY(shell.settingsBridge()->setPreference("fontSize",size));QTest::qWait(30);QVERIFY(!caption->property("truncated").toBool());QVERIFY(brand->width()*brand->scale()<=brand->parentItem()->width()+.5);QVERIFY(qAbs(brand->x()-(brand->parentItem()->width()-brand->x()-brand->width()*brand->scale()))<.5);}QVERIFY(shell.settingsBridge()->setPreference("fontSize",15));
    for(int width:{1240,1000,800}){w->resize(width,790);QTest::qWait(40);QTRY_VERIFY(qAbs(center->x()+center->width()/2-bar->width()/2)<.5);QVERIFY(metadata->mapRectToScene(QRectF(QPointF(),metadata->size())).right()<=visual(root,"currentFavoriteButton")->mapToScene(QPointF()).x()-bar->property("coverMargin").toReal()*2+2.1);}
    w->resize(1240,790);QTest::qWait(40);shell.facade()->enqueueTracks({library.track(ids[0]),library.track(ids[1])},true);QTRY_VERIFY(shell.upcomingModel()->count()>0);QTRY_VERIFY(!visual(root,"currentTitle")->property("truncated").toBool());
    const auto inlineWidth=panel->width();
    for(int width:{1240,1100,1000,900,800,700,600}){
        w->resize(width,790);QTest::qWait(50);auto *volume=visual(root,"muteButton");QVERIFY(volume);auto *volumeImage=volume->property("contentItem").value<QQuickItem *>();QVERIFY(volumeImage);auto *nextIcon=visual(root,"nextCompactIcon");QVERIFY(nextIcon);
        QTRY_VERIFY2(qAbs(nextIcon->mapToScene(QPointF()).x()-volumeImage->mapToScene(QPointF()).x())<.5,qPrintable(QString("width=%1 next=%2 volume=%3 progress=%4 panel=%5 padding=%6").arg(width).arg(nextIcon->mapToScene(QPointF()).x()).arg(volumeImage->mapToScene(QPointF()).x()).arg(w->property("narrowProgress").toReal()).arg(panel->width()).arg(volume->property("leftPadding").toReal())));
        if(width>=700)QCOMPARE(panel->width(),inlineWidth);
        QVERIFY(panel->x()>=center->x()+center->width());
        const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()&&(width==1240||width==800||width==600)){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath(QString("footer-next-aligned-%1.png").arg(width)));}
    }
    w->resize(1240,790);QTest::qWait(50);
    QVERIFY(!visual(root,"nextCompactArtist")->isVisible());QVERIFY(panel->width()<250);const auto right=panel->x()+panel->width(),bottom=panel->y()+panel->height(),small=panel->width();panel->setProperty("panelExpanded",true);QTRY_VERIFY(panel->width()>small+20);QVERIFY(qAbs(panel->x()+panel->width()-right)<.5);QVERIFY(qAbs(panel->y()+panel->height()-bottom)<.5);panel->setProperty("panelExpanded",false);
    QVERIFY(shell.settingsBridge()->setPreference("playlistHeaderAnimation",false));auto *header=visual(root,"playlistHeader"),*list=visual(root,"trackListView"),*heading=visual(root,"trackHeading-2");QVERIFY(header&&list&&heading);QTRY_VERIFY(header->property("collapsed").toBool());QTRY_VERIFY(header->height()<100);const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("playlist-fixed-before.png"));const auto before=header->mapToScene(QPointF()).y();const auto titleY=visual(root,"libraryHeading")->mapToScene(QPointF()).y();
    list->setProperty("contentY",list->property("contentY").toReal()+150);QTRY_VERIFY(header->mapToScene(QPointF()).y()<before-140);QCOMPARE(visual(root,"libraryHeading")->mapToScene(QPointF()).y(),titleY);QVERIFY(qAbs(heading->mapToScene(QPointF()).y()-list->mapToScene(QPointF()).y())<1);if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("playlist-fixed-scrolled.png"));
    QVERIFY(shell.settingsBridge()->setPreference("playlistHeaderAnimation",true));QTRY_VERIFY(!header->property("collapsed").toBool());QTRY_VERIFY(header->height()>=159);
    QVERIFY(QMetaObject::invokeMethod(w,"animatePlayer",Q_ARG(QVariant,true)));QTRY_VERIFY(w->property("upcomingAtLeft").toBool());
    for(int width:{1240,1000,700}){w->resize(width,790);QTest::qWait(60);for(bool expanded:{false,true}){panel->setProperty("panelExpanded",expanded);QTest::qWait(40);QVERIFY(panel->x()+panel->width()<=center->x()-2*bar->property("coverMargin").toReal()+.5);}}
    if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("player-expanded-queue.png"));
    w->resize(1240,790);QTest::qWait(60);QVERIFY(shell.settingsBridge()->setPreference("animationMode",0));
    for(bool open:{false,true}){
        QVERIFY(QMetaObject::invokeMethod(w,"animatePlayer",Q_ARG(QVariant,open)));bool left=w->property("upcomingAtLeft").toBool();qreal fixedWidth=panel->width();
        QElapsedTimer timer;timer.start();while(timer.elapsed()<600){QTest::qWait(10);const bool nextLeft=w->property("upcomingAtLeft").toBool();if(nextLeft!=left){left=nextLeft;fixedWidth=panel->width();}else QVERIFY2(qAbs(panel->width()-fixedWidth)<.5,"Queue width must stay stable within each side of the player transition");}
        QCOMPARE(w->property("upcomingAtLeft").toBool(),open);QVERIFY(!w->property("upcomingRelocating").toBool());
    }
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
}

void PhaseGTests::sidebarResizeDragsPersistAndRespectCompactNavigation(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));
    {
        QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();w->resize(1240,790);QTest::qWait(50);
        auto *sidebar=visual(root,"sidebar"),*handle=visual(root,"sidebarResizeHandle");QVERIFY(sidebar&&handle);QCOMPARE(sidebar->width(),qreal(205));QTRY_VERIFY(handle->property("enabled").toBool());
        auto drag=[&](int delta){const auto start=handle->mapToScene(QPointF(handle->width()/2,handle->height()/2)).toPoint();QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(w,start+QPoint(delta,0),30);QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,start+QPoint(delta,0));};
        drag(95);QTRY_COMPARE(sidebar->width(),qreal(300));QCOMPARE(library.setting("sidebarWidth").toInt(),300);
        SettingsBridge reopened(&library);QCOMPARE(reopened.values().value("sidebarWidth").toInt(),300);
        drag(-200);QTRY_COMPARE(sidebar->width(),qreal(160));drag(500);QTRY_COMPARE(sidebar->width(),qreal(400));
        QVERIFY(!shell.settingsBridge()->setPreference("sidebarWidth",159));QVERIFY(!shell.settingsBridge()->setPreference("sidebarWidth",401));QVERIFY(!shell.settingsBridge()->setPreference("sidebarWidth",205.5));
        w->resize(600,790);QTest::qWait(50);QTRY_COMPARE(sidebar->width(),qreal(260));QVERIFY(visual(root,"paperSurface")->width()>=340);QCOMPARE(library.setting("sidebarWidth").toInt(),400);
        w->resize(1240,790);QTest::qWait(50);QTRY_COMPARE(sidebar->width(),qreal(400));QVERIFY(QMetaObject::invokeMethod(visual(root,"sidebarLogo"),"clicked"));QTRY_COMPARE(sidebar->width(),qreal(84));QTRY_VERIFY(!handle->property("enabled").toBool());QVERIFY(QMetaObject::invokeMethod(visual(root,"sidebarLogo"),"clicked"));QTRY_COMPARE(sidebar->width(),qreal(400));QTRY_VERIFY(handle->property("enabled").toBool());
        w->resize(414,790);QTest::qWait(50);QTRY_COMPARE(sidebar->width(),qreal(0));QTRY_VERIFY(!handle->isVisible());w->resize(1240,790);QTest::qWait(50);QTRY_COMPARE(sidebar->width(),qreal(400));
        QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(!w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());QTRY_COMPARE(sidebar->width(),qreal(400));QCOMPARE(library.setting("sidebarWidth").toInt(),400);
        const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath("sidebar-custom-width.png"));}
        QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
    }
    {QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());shell.window()->resize(1240,790);QTRY_COMPARE(visual(shell.window()->contentItem(),"sidebar")->width(),qreal(400));QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));}
}


void PhaseGTests::widthToggleKeepsTheRightEdgeAndRestoresFromTheNarrowestWindow(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();
    const auto wave=syntheticWave(dir.path());QVector<qint64> queueIds;for(int i=0;i<2;++i){Track track;track.file=wave;track.hash=QString("width-toggle-queue-%1").arg(i);track.title="Width toggle queue";track.duration=12000;queueIds.append(library.insert(track));QVERIFY(queueIds.last()>0);}shell.facade()->play(queueIds,queueIds[0]);QTRY_VERIFY(shell.upcomingModel()->count()>0);
    w->resize(1000,700);w->setPosition(20,30);w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));QTest::qWait(100);
    auto click=[&]{auto *button=visual(root,"playbackWidthButton");if(!button||!button->isVisible())return false;const auto point=button->mapToScene(QPointF(18,18)).toPoint();if(!QRect(0,0,w->width(),w->height()).contains(point))return false;QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,point);return true;};
    for(int pass=0;pass<2;++pass){const int right=w->x()+w->width(),top=w->y(),height=w->height();QVERIFY(click());QTRY_COMPARE(w->width(),414);QTRY_COMPARE(w->x()+w->width(),right);QCOMPARE(w->height(),height);QCOMPARE(w->y(),top);QTRY_VERIFY(w->property("narrowLayout").toBool());QTest::qWait(80);QVERIFY(click());QTRY_COMPARE(w->width(),1000);QCOMPARE(w->x()+w->width(),right);QCOMPARE(w->height(),height);}
    QCOMPARE(library.setting("wideWindowWidth").toInt(),1000);SettingsBridge reopened(&library);QCOMPARE(reopened.values().value("wideWindowWidth").toInt(),1000);
    QVERIFY(shell.settingsBridge()->setPreference("animationMode",0));
    for(int target:{414,1000}){
        const int from=w->width(),right=w->x()+w->width(),height=w->height();QSignalSpy widths(w,&QWindow::widthChanged);QSignalSpy frames(w,&QQuickWindow::frameSwapped);
        bool synchronized=true,earlySidebar=false,earlyVolume=false,earlyQueue=false,queueStable=true;int earlySamples=0;QStringList syncFailures;qreal queueWidth=-1,queueY=-1;
        const auto observe=connect(w,&QWindow::widthChanged,this,[&](int width){
            if(!w->property("widthSwitchActive").toBool())return;
            const qreal p=w->property("widthSwitchProgress").toReal(),layout=w->property("narrowProgress").toReal(),expected=target==414?p:1-p;
            synchronized&=qAbs(layout-expected)<.006;if(qAbs(layout-expected)>=.006)syncFailures.append(QString("width %1 p %2 layout %3 expected %4 phase %5").arg(width).arg(p).arg(layout).arg(expected).arg(w->property("widthSwitchProgress").toDouble()));
            if(p>.03&&p<.98){auto *queue=visual(root,"nextQueuePanel");if(queueWidth<0){queueWidth=queue->width();queueY=queue->y();}else queueStable&=qAbs(queue->width()-queueWidth)<.5&&qAbs(queue->y()-queueY)<.5;}
            if(p>.03&&p<.4){++earlySamples;auto *sidebar=visual(root,"sidebar"),*volume=visual(root,"playerRight"),*queue=visual(root,"nextQueuePanel");if(target>from){earlySidebar|=sidebar->width()>0&&sidebar->opacity()>0;earlyVolume|=volume->opacity()>0;earlyQueue|=queue->width()>0&&queue->opacity()>0;}else{earlySidebar|=sidebar->opacity()<.98;earlyVolume|=volume->opacity()<.98;earlyQueue|=queue->opacity()<.98;}}
        });
        QVERIFY(click());QTRY_VERIFY(w->property("widthSwitchRunning").toBool());
        bool intermediate=false;QStringList samples;QElapsedTimer timer;timer.start();while(timer.elapsed()<450){QTest::qWait(15);const int width=w->width();samples.append(QString::number(timer.elapsed())+":"+QString::number(width)+"/"+w->property("widthSwitchProgress").toString());intermediate|=width>qMin(from,target)&&width<qMax(from,target);QVERIFY(qAbs(w->x()+width-right)<=1);QCOMPARE(w->height(),height);}
        disconnect(observe);QVERIFY(queueStable);QVERIFY2(synchronized,qPrintable(syncFailures.join(";")));QVERIFY(earlySamples>0);QVERIFY(earlySidebar);QVERIFY(earlyVolume);QVERIFY(earlyQueue);
        for(const auto &row:widths){const int width=row[0].toInt();intermediate|=width>qMin(from,target)&&width<qMax(from,target);samples.append(QString("signal:%1").arg(width));}QVERIFY2(intermediate,qPrintable(samples.join(",")));QVERIFY2(frames.size()>=3,qPrintable(QString("Only %1 rendered frames").arg(frames.size())));qInfo()<<"width transition"<<from<<target<<"size changes"<<widths.size()<<"presented frames"<<frames.size();QTRY_COMPARE(w->width(),target);QTRY_VERIFY(!w->property("widthSwitchRunning").toBool());QTRY_VERIFY(!w->property("widthSwitchActive").toBool());QTest::qWait(30);auto *footer=visual(root,"playPauseButton"),*filters=visual(root,"libraryFilterRow");QVERIFY(footer&&filters);const auto footerX=footer->mapToScene(QPointF()).x(),filterHeight=filters->height();QTest::qWait(360);QVERIFY(qAbs(footer->mapToScene(QPointF()).x()-footerX)<.5);QVERIFY(qAbs(filters->height()-filterHeight)<.5);
    }
    QVERIFY(shell.settingsBridge()->setPreference("animationMode",2));

#ifdef Q_OS_WIN
    if(QGuiApplication::platformName()=="windows"){w->showMaximized();QTRY_COMPARE(w->visibility(),QWindow::Maximized);QTest::qWait(120);const int right=w->x()+w->width(),height=w->height(),wide=w->width();QVERIFY(click());QTRY_COMPARE(w->width(),414);QTRY_COMPARE(w->x()+w->width(),right);QCOMPARE(w->height(),height);QTest::qWait(80);QVERIFY(click());QTRY_COMPARE(w->width(),wide);QCOMPARE(w->height(),height);}
#endif
    w->showNormal();QTest::mouseMove(w,QPoint(5,5));
    auto *bar=visual(root,"playerBar"),*toggle=visual(root,"playbackWidthButton"),*panel=visual(root,"nextQueuePanel"),*cover=visual(root,"bottomCover");QVERIFY(bar&&toggle&&panel&&cover);
    for(int width:{1240,700,546,480,414}){
        w->resize(width,700);QTest::qWait(80);
        const qreal right=toggle->mapToScene(QPointF(toggle->width(),0)).x();QVERIFY(qAbs(w->width()-right-bar->property("coverMargin").toReal())<.5);
        if(width>=547)QVERIFY(qAbs(right-panel->mapToScene(QPointF(panel->width(),0)).x())<.5);
        else {
            auto *first=visual(root,"compactQueueButton");QVERIFY(first);QVERIFY(first->mapToScene(QPointF()).x()-cover->mapToScene(QPointF(cover->width(),0)).x()>=28);
            QQuickItem *previous=nullptr;for(const auto *name:{"compactQueueButton","currentFavoriteButton","previousButton","playPauseButton","nextButton","playMode","muteButton","playbackWidthButton"}){auto *button=visual(root,name);QVERIFY(button&&button->isVisible());if(previous){const qreal gap=button->mapToScene(QPointF()).x()-previous->mapToScene(QPointF(previous->width(),0)).x();QVERIFY(qAbs(gap-bar->property("compactSpacing").toReal())<.5);}previous=button;}
        }
        const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath(QString("footer-controls-%1.png").arg(width)));}
    }
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
}
void PhaseGTests::membershipModesAndDiscoverySettingsFollowAccountFacts(){
    QCOMPARE(accountMembership("netease",QJsonObject{{"vipType",11}}),QString("网易云音乐会员"));QVERIFY(accountMembership("netease",{}).isEmpty());QCOMPARE(accountMembership("qq",QJsonObject{{"identity",QJsonObject{{"svip",1}}}}),QString("超级会员"));
    QCOMPARE(accountMembership("kugou",QJsonObject{{"is_vip",true},{"expire_time",1000}},2000000),QString("普通账号"));QCOMPARE(accountMembership("qishui",QJsonObject{{"vip_info",QJsonObject{{"vip_status",1},{"expire_time",3000}}}},2000000),QString("汽水音乐会员"));QVERIFY(accountMembership("kugou",QJsonObject{{"nickname","VIP fan"}}).isEmpty());
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));Transport transport;QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());AdditionalMusicBridge source(AdditionalMusicBridge::Kugou,&library,shell.facade(),shell.lyricsModel(),shell.onlineArtworkCache(),nullptr,&transport);
    auto *w=shell.window();auto *root=w->contentItem();auto *context=QQmlEngine::contextForObject(w)->engine()->rootContext();auto original=context->contextProperty("kugouMusic").value<QObject *>();context->setContextProperty("kugouMusic",&source);const auto restore=qScopeGuard([&]{context->setContextProperty("kugouMusic",original);});
    transport.responses={R"({"data":{"qrcode":"synthetic-qr"}})",R"({"data":{"status":4,"token":"synthetic-token","userid":"12345","nickname":"Mock Member","is_vip":true}})"};source.startLogin();QSignalSpy completed(&source,&AdditionalMusicBridge::loginCompleted);QTRY_COMPARE_WITH_TIMEOUT(completed.size(),1,6000);QVERIFY(source.vip());
    QVERIFY(shell.appBridge()->navigate(4));QTRY_VERIFY(visual(root,"discoverPage"));auto *page=visual(root,"discoverPage");page->setProperty("sourceIndex",3);
    transport.responses={R"({"status":1,"data":{"lists":[{"FileHash":"VIP","SongName":"Synthetic premium song","Duration":10,"PayType":1}]}})"};QVERIFY(QMetaObject::invokeMethod(page,"submitSearch",Q_ARG(QVariant,"synthetic")));QTRY_COMPARE(source.songs().size(),1);
    auto *badge=visual(root,"onlineAccountMembership"),*avatar=visual(root,"onlineAccountAvatar");QVERIFY(badge&&avatar);
    for(int mode=0;mode<4;++mode){QVERIFY(shell.settingsBridge()->setPreference("vipMarkMode",mode));QTRY_COMPARE(badge->isVisible(),mode==1||mode==2);QTRY_VERIFY(visual(root,"onlineVip-VIP"));QTRY_COMPARE(visual(root,"onlineVip-VIP")->isVisible(),mode==0||mode==2);if(badge->isVisible()){QVERIFY(badge->x()+badge->width()<avatar->x());QVERIFY(qAbs(badge->y()+badge->height()/2-avatar->y()-avatar->height()/2)<.5);}}
    QVERIFY(shell.settingsBridge()->setPreference("vipMarkMode",2));const auto membershipFolder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!membershipFolder.isEmpty()){QDir().mkpath(membershipFolder);QTest::qWait(80);w->grabWindow().save(QDir(membershipFolder).filePath("discovery-account-vip.png"));}
    source.logout();QTRY_VERIFY(!badge->isVisible());QVERIFY(!source.vip());
    QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"settingsDiscoveryCard"));auto *discovery=visual(root,"settingsDiscoveryCard");QVERIFY(discovery->y()>visual(root,"settingsAppearanceCard")->y());QVERIFY(discovery->y()<visual(root,"settingsPlaybackCard")->y());
    auto *actions=visual(root,"customizeDiscoveryActions");QVERIFY(actions);QVERIFY(discovery->isAncestorOf(actions));
    for(int width:{1240,414}){w->resize(width,790);QTest::qWait(100);QCOMPARE(visual(root,"vipMarkModeSetting")->isVisible(),width==414);for(int i=0;i<4;++i)QCOMPARE(visual(root,QString("vipMarkOption-%1").arg(i))->isVisible(),width==1240);const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);auto *flick=visual(root,"settingsScroll")->property("contentItem").value<QObject *>();flick->setProperty("contentY",discovery->y());QTest::qWait(40);w->grabWindow().save(QDir(folder).filePath(QString("discovery-settings-%1.png").arg(width)));}}
    QVERIFY(!shell.settingsBridge()->setPreference("vipMarkMode",4));SettingsBridge reopened(&library);QCOMPARE(reopened.values().value("vipMarkMode").toInt(),2);QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
}
void PhaseGTests::catalogScrollBarsCanBeDragged(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));QVector<qint64> ids;for(int i=0;i<80;++i){Track t;t.file=QString("Music/synthetic-%1.wav").arg(i);t.hash=t.file;t.title=QString("Synthetic %1").arg(i);t.duration=12000;ids.append(library.insert(t));}const auto playlist=library.createPlaylist("Scroll fixture");QVERIFY(library.addToPlaylist(playlist,ids));QVERIFY(library.setSetting("playlistHeaderAnimation",false));for(auto id:ids){auto session=library.startSession(id,12000);QVERIFY(library.saveSession(session,1000,80,true));}
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();w->resize(1000,700);
    QVERIFY(QTest::qWaitForWindowExposed(w));
    auto drag=[&](QQuickItem *list){if(!list)return false;auto *bar=visual(list,"listScrollBar");if(!bar)bar=visual(list->parentItem()->parentItem(),"listScrollBar");if(!bar)return false;
        // Earlier windows can leave native hover inside this newly exposed window.
        // Deliver a complete pointer boundary before testing the outside position;
        // repeated synthetic moves alone can be coalesced at the previous position.
        const QPoint outside(5,5);QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(w,&leave);
        QEnterEvent enter(outside,outside,w->mapToGlobal(outside));QCoreApplication::sendEvent(w,&enter);
        QTest::mouseMove(w,QPoint(6,5));QTest::mouseMove(w,QPoint(5,5));QTest::qWait(30);if(bar->isVisible()){qInfo()<<"visible outside"<<bar->parentItem()->objectName()<<bar->property("hoverTarget");return false;}
        QTest::mouseMove(w,list->mapToScene(QPointF(list->width()/2,list->height()/2)).toPoint());QTest::qWait(30);if(!bar->isVisible()){qInfo()<<"hidden inside"<<bar->parentItem()->objectName()<<bar->property("size")<<bar->property("hoverTarget")<<list->isVisible();return false;}
        if(bar->mapToScene(QPointF()).y()<list->mapToScene(QPointF()).y()){qInfo()<<"bad top";return false;}
        const auto shots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!shots.isEmpty()){QDir().mkpath(shots);w->grabWindow().save(QDir(shots).filePath("catalog-hover-"+shell.libraryBridge()->playlistId()+".png"));}
auto *thumb=bar->property("contentItem").value<QQuickItem *>();if(!thumb)return false;const auto from=thumb->mapToScene(QPointF(thumb->width()/2,thumb->height()/2)).toPoint();const qreal before=list->property("contentY").toReal();QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(w,from+QPoint(0,60),30);QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,from+QPoint(0,60));QTest::qWait(40);if(list->property("contentY").toReal()<=before+10)qInfo()<<"drag failed"<<bar->x()<<bar->y()<<bar->height()<<from<<before<<list->property("contentY");return list->property("contentY").toReal()>before+10;};
    for(const auto &id:QStringList{"0",QString::number(playlist)}){QVERIFY(shell.libraryBridge()->openPlaylist(id));QTest::qWait(80);QVERIFY(drag(visual(root,"trackListView")));auto *bar=visual(visual(root,"libraryListContainer"),"listScrollBar"),*heading=visual(root,"trackHeading-2");QVERIFY(bar&&heading);QVERIFY(bar->mapToScene(QPointF()).y()>=heading->mapToScene(QPointF(0,heading->height())).y());}
    QVERIFY(shell.libraryBridge()->openPlaylist("-2"));QVERIFY(shell.appBridge()->navigate(2));QTRY_VERIFY(visual(root,"recentTable"));QTest::qWait(80);QVERIFY(drag(visual(visual(root,"recentTable"),"trackListView")));
    QObject::disconnect(shell.libraryBridge(),&LibraryBridge::cloudPlaylistRequested,nullptr,nullptr);QVariantMap details{{"id","scroll-cloud"},{"provider","netease"},{"name","Mock cloud"}};const auto key=shell.libraryBridge()->pinCloudPlaylist(details);QVERIFY(shell.libraryBridge()->openPlaylist(key));QVariantList songs;for(int i=0;i<80;++i)songs.append(QVariantMap{{"id",QString::number(i+1)},{"title",QString("Cloud %1").arg(i)},{"artist","Mock Artist"},{"album","Synthetic"},{"reason",QString()},{"cover",QString()},{"requiresSubscription",false},{"duration",12000}});shell.libraryBridge()->setCloudSongs("netease",details,songs);QVERIFY(shell.appBridge()->navigate(0));QTest::qWait(80);QVERIFY(drag(visual(root,"trackListView")));
    Transport transport;AdditionalMusicBridge source(AdditionalMusicBridge::Kugou,&library,shell.facade(),shell.lyricsModel(),shell.onlineArtworkCache(),nullptr,&transport);auto *context=QQmlEngine::contextForObject(w)->engine()->rootContext();auto *original=context->contextProperty("onlineMusic").value<QObject *>();context->setContextProperty("onlineMusic",&source);const auto restore=qScopeGuard([&]{context->setContextProperty("onlineMusic",original);});
    QVERIFY(shell.appBridge()->navigate(4));QTRY_VERIFY(visual(root,"discoverPage"));auto *page=visual(root,"discoverPage");page->setProperty("showPlaylists",false);auto *online=visual(root,"onlineSongs");online->setProperty("model",songs);QTest::qWait(80);QVERIFY(drag(online));online->setProperty("contentY",0);QTest::qWait(80);page->setProperty("showPlaylists",true);auto *grid=visual(root,"onlinePlaylistCards");QVariantList cards;for(int i=0;i<60;++i)cards.append(QVariantMap{{"id",QString::number(i)},{"name",QString("Playlist %1").arg(i)}});grid->setProperty("model",cards);QTest::qWait(80);QVERIFY(drag(grid));
    const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath("discovery-scrollbar.png"));}grid->setProperty("contentY",0);QTest::qWait(80);QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
}


void PhaseGTests::volumeHoverUsesSafeTrianglesOnBothWindows(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();w->resize(414,700);w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));QTest::qWait(100);
    for(bool mini:{false,true}){
        if(mini){QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());w->grabWindow();QCoreApplication::processEvents();const QPoint point(200,34);QEnterEvent enter(point,point,w->mapToGlobal(point));QCoreApplication::sendEvent(w,&enter);QTest::mouseMove(w,QPoint(199,34));QTest::mouseMove(w,point);QTest::qWait(50);}
        auto *volume=visual(root,mini?"miniVolume":"muteButton");QVERIFY(volume);QTRY_VERIFY2(volume->isVisible(),mini?"mini volume hidden":"normal volume hidden");
        auto *popup=volume->findChild<QObject *>("compactVolumePopup");QVERIFY(popup);
        auto open=[&]{QTest::mouseMove(w,QPoint(3,3));QTest::qWait(20);if(mini){const QPoint point(200,34);QEnterEvent enter(point,point,w->mapToGlobal(point));QCoreApplication::sendEvent(w,&enter);QTest::mouseMove(w,QPoint(199,34));QTest::mouseMove(w,point);QTest::qWait(20);}QTest::mouseMove(w,volume->mapToScene(QPointF(volume->width()/2,volume->height()/2)).toPoint());};
        open();QTRY_VERIFY(popup->property("opened").toBool());auto *area=popup->property("contentItem").value<QQuickItem *>();QVERIFY(area);auto *pw=area->window();QVERIFY(pw);QVERIFY(pw!=w);
        auto move=[&](qreal x,qreal y){QTest::mouseMove(pw,area->mapToScene(QPointF(x,y)).toPoint());QTest::qWait(25);};
        move(area->width()/2,60);move(area->width()*.3,46);QVERIFY(popup->property("opened").toBool());
        const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);pw->grabWindow().save(QDir(folder).filePath(mini?"mini-volume-safe-triangle.png":"volume-safe-triangle.png"));}
        move(2,48);QTRY_VERIFY(!popup->property("visible").toBool());
        open();QTRY_VERIFY(popup->property("opened").toBool());area=popup->property("contentItem").value<QQuickItem *>();pw=area->window();move(area->width()/2,60);move(area->width()/2,46);move(20,20);QVERIFY(popup->property("opened").toBool());
        auto *slider=visual(area,"compactVolumeSlider");QVERIFY(slider);shell.playbackBridge()->setVolume(30);QTest::qWait(20);const auto from=slider->mapToScene(QPointF(slider->width()*.3,slider->height()/2)).toPoint(),to=slider->mapToScene(QPointF(slider->width()*.85,slider->height()/2)).toPoint();QTest::mousePress(pw,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(pw,to,30);QTest::mouseRelease(pw,Qt::LeftButton,Qt::NoModifier,to);QTRY_VERIFY(shell.playbackBridge()->volume()>60);QVERIFY(popup->property("opened").toBool());
        move(area->width()/2,46);move(area->width()/2,60);QVERIFY(popup->property("opened").toBool());move(2,70);QTRY_VERIFY(!popup->property("visible").toBool());
        if(mini){QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(!w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());}
    }
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
}


void PhaseGTests::playlistCoversFiltersAndWideVolumeFit(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));QVERIFY(library.setSetting("playlistHeaderAnimation",false));QVector<qint64> ids;for(int i=0;i<20;++i){Track t;t.file=QString("Music/synthetic-ui-%1.wav").arg(i);t.hash=t.file;t.title=QString("Synthetic %1").arg(i);t.artist="Mock artist";t.album="Mock album";t.duration=12000;ids.append(library.insert(t));}const auto playlist=library.createPlaylist("Synthetic cover fixture");QVERIFY(library.addToPlaylist(playlist,ids));QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));w->resize(1240,790);QVERIFY(shell.libraryBridge()->openPlaylist(QString::number(playlist)));QTest::qWait(80);
    auto *cover=visual(root,"playlistCover"),*heading=visual(root,"libraryHeading"),*actions=visual(root,"playlistPlayActions");QVERIFY(cover&&heading&&actions);const qreal size=cover->width(),actionsX=actions->x();const auto offset=cover->mapToScene(QPointF())-heading->mapToScene(QPointF());
    const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath("playlist-cover-wide.png"));}
    w->resize(414,790);QTest::qWait(100);QVERIFY(qAbs(cover->width()-size)<.5);QVERIFY(qAbs(actions->x()-actionsX)<.5);const auto narrowOffset=cover->mapToScene(QPointF())-heading->mapToScene(QPointF());QVERIFY(qAbs(narrowOffset.x()-offset.x())<.5);QVERIFY(qAbs(narrowOffset.y()-offset.y())<.5);if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath("playlist-cover-narrow.png"));
    QVERIFY(shell.libraryBridge()->openPlaylist("0"));qreal previousSlider=81;
    for(int width:{1240,1000,902,896,750,613,547,546,414}){
        w->resize(width,790);QTest::qWait(60);auto *more=visual(root,"moreLibraryOptions"),*filters=visual(root,"libraryFilterRow"),*play=visual(root,"playAllButton"),*slider=visual(root,"volumeSlider"),*volume=visual(root,"muteButton"),*toggle=visual(root,"playbackWidthButton");QVERIFY(more&&filters&&play&&slider&&volume&&toggle);
        if(more->isVisible()){QCOMPARE(filters->height(),qreal(0));QVERIFY(!filters->isEnabled());QVERIFY(more->mapToScene(QPointF(more->width(),0)).x()<=visual(root,"libraryListContainer")->mapToScene(QPointF(visual(root,"libraryListContainer")->width(),0)).x()+.5);QVERIFY(qAbs(more->mapToScene(QPointF()).y()-play->mapToScene(QPointF()).y())<.5);QVERIFY(QMetaObject::invokeMethod(more,"clicked"));QTRY_VERIFY(filters->height()>0);QVERIFY(filters->isEnabled());QVERIFY(QMetaObject::invokeMethod(more,"clicked"));QTRY_COMPARE(filters->height(),qreal(0));}
        else {QVERIFY(filters->height()>0);QVERIFY(qAbs(filters->mapToScene(QPointF()).y()-play->mapToScene(QPointF()).y())<.5);}
        if(width>=547){QVERIFY(slider->isVisible());QVERIFY(slider->width()>20);QVERIFY(slider->width()<=previousSlider+.5);previousSlider=slider->width();QVERIFY(slider->mapToScene(QPointF()).x()>=volume->mapToScene(QPointF(volume->width(),0)).x()+9.5);QVERIFY(slider->mapToScene(QPointF(slider->width(),0)).x()<=toggle->mapToScene(QPointF()).x()-9.5);
            shell.playbackBridge()->setVolume(30);const auto from=slider->mapToScene(QPointF(slider->width()*.3,slider->height()/2)).toPoint(),to=slider->mapToScene(QPointF(slider->width()*.9,slider->height()/2)).toPoint();QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(w,to,20);QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,to);QTRY_VERIFY(shell.playbackBridge()->volume()>60);
        }else QVERIFY(!slider->isVisible());
        if(!folder.isEmpty())w->grabWindow().save(QDir(folder).filePath(QString("filters-volume-%1.png").arg(width)));
    }
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));
}


void PhaseGTests::bufferedProgressUsesKnownRangesAndKeepsVolumeUnchanged(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
    const auto wave=syntheticWave(dir.filePath("audio"));QFile file(wave);QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();
    // A real throttled HTTP stream of generated silence, with seekable byte ranges.
    QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));qint64 sent=0;
    connect(&server,&QTcpServer::newConnection,&server,[&]{while(server.hasPendingConnections()){
        auto *socket=server.nextPendingConnection();
        connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{
            if(socket->property("responding").toBool()){socket->readAll();return;}
            const auto request=socket->property("request").toByteArray()+socket->readAll();socket->setProperty("request",request);
            if(!request.contains("\r\n\r\n"))return;
            socket->setProperty("responding",true);qint64 offset=0;const auto lower=request.toLower();const auto range=lower.indexOf("range: bytes=");
            if(range>=0)offset=lower.mid(range+13).split('-').first().toLongLong();offset=qBound<qint64>(0,offset,bytes.size()-1);
            QByteArray header=offset>0?"HTTP/1.1 206 Partial Content\r\n":"HTTP/1.1 200 OK\r\n";
            header+="Content-Type: audio/wav\r\nAccept-Ranges: bytes\r\nConnection: close\r\nContent-Length: "+QByteArray::number(bytes.size()-offset)+"\r\n";
            if(offset>0)header+="Content-Range: bytes "+QByteArray::number(offset)+"-"+QByteArray::number(bytes.size()-1)+"/"+QByteArray::number(bytes.size())+"\r\n";
            socket->write(header+"\r\n");auto *timer=new QTimer(socket);timer->setInterval(50);
            connect(timer,&QTimer::timeout,socket,[&,socket,timer,offset]()mutable{
                if(socket->state()!=QAbstractSocket::ConnectedState){timer->stop();return;}
                const auto chunk=bytes.mid(offset,4096);socket->write(chunk);offset+=chunk.size();sent+=chunk.size();
                if(offset>=bytes.size()){timer->stop();socket->disconnectFromHost();}
            });timer->start();
        });
    }});
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *facade=shell.facade();auto *bridge=shell.playbackBridge();
    QObject::disconnect(facade,nullptr,shell.onlineMusicBridge(),nullptr);
    QObject::disconnect(facade,nullptr,shell.qqMusicBridge(),nullptr);
    auto *root=shell.window()->contentItem();auto *progress=visual(root,"playbackProgress");auto *volume=visual(root,"volumeSlider");QVERIFY(progress);QVERIFY(volume);
    QVERIFY(progress->property("showBuffer").toBool());QVERIFY(!volume->property("showBuffer").toBool());QVERIFY(facade->snapshot().buffered.isEmpty());
    Track online;online.id=1000000000099LL;online.hash="netease:99";online.title="Generated HTTP silence";online.duration=12000;
    facade->playOnline({online},online.id);QVERIFY(bridge->bufferedRanges().isEmpty());
    facade->setOnlineSource(online.id,QUrl(QString("http://127.0.0.1:%1/synthetic.wav").arg(server.serverPort())));
    // The first range may contain only one network chunk. Wait for the actual
    // required lead within the original 8s budget, rather than equating nonempty
    // with a full second of buffered media. Keep the numeric lead assertion below.
    QTRY_VERIFY2_WITH_TIMEOUT(([&]{const auto state=facade->snapshot();return !state.buffered.isEmpty()&&state.buffered.latestTime()>state.position+1000;})(),
        qPrintable(QString("buffer position=%1 latest=%2 duration=%3 sent=%4").arg(facade->position()).arg(facade->snapshot().buffered.latestTime()).arg(facade->duration()).arg(sent)),8000);
    QVERIFY(sent>0);const auto initial=facade->snapshot();QVERIFY(initial.buffered.latestTime()>0);QVERIFY(initial.buffered.latestTime()<initial.duration);QVERIFY(initial.buffered.latestTime()>initial.position+1000);
    qInfo()<<"Actual cached media range:"<<initial.position<<initial.buffered.latestTime()<<initial.duration;
    QTRY_VERIFY(!bridge->bufferedRanges().isEmpty());QTRY_VERIFY(visual(progress,"bufferedProgressSegment"));
    const auto activeCache=QDir(DiskCache::online(&library)).entryList({"playing-*.media"},QDir::Files);QCOMPARE(activeCache.size(),1);DiskCache::trim(&library,{},0);QVERIFY(QFileInfo::exists(QDir(DiskCache::online(&library)).filePath(activeCache.first())));
    const auto networkShots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!networkShots.isEmpty()){QDir().mkpath(networkShots);shell.window()->grabWindow().save(QDir(networkShots).filePath("actual-network-buffer.png"));}
    QTRY_VERIFY(facade->seekable());const auto cached=facade->snapshot().buffered;facade->seek(7000);QVERIFY(!facade->snapshot().buffered.isEmpty());QVERIFY(facade->snapshot().buffered.latestTime()>=cached.latestTime());
    QTRY_VERIFY_WITH_TIMEOUT(facade->snapshot().buffered.latestTime()>7000,8000);
    QVERIFY(facade->snapshot().buffered.earliestTime()<7000);
    online.id++;online.hash="netease:100";facade->playOnline({online},online.id);QVERIFY(facade->snapshot().buffered.isEmpty());QTRY_VERIFY(bridge->bufferedRanges().isEmpty());
    facade->failOnlineSource(online.id,"Synthetic resolution failure");QVERIFY(facade->snapshot().buffered.isEmpty());shell.feedbackBridge()->dismiss();
    Track local;local.file=wave;local.hash="synthetic-buffer-wave";local.title="Generated local silence";local.duration=12000;const auto localId=library.insert(local);QVERIFY(localId>0);
    facade->play({localId},localId);QTRY_VERIFY(facade->seekable());QCOMPARE(facade->snapshot().buffered,QMediaTimeRange(0,12000));QVERIFY(QDir(DiskCache::online(&library)).entryList({"playing-*.media"},QDir::Files).isEmpty());
    for(const int width:{1240,414}){shell.window()->resize(width,790);QTest::qWait(360);QCOMPARE(progress->property("bufferedRanges").toList(),bridge->bufferedRanges());QVERIFY(progress->width()>0);}
    shell.window()->resize(1240,790);QVERIFY(QMetaObject::invokeMethod(shell.window(),"animatePlayer",Q_ARG(QVariant,true)));QTest::qWait(360);
    QCOMPARE(progress->property("bufferedRanges").toList(),bridge->bufferedRanges());QVERIFY(progress->width()>0);QVERIFY(QMetaObject::invokeMethod(shell.window(),"animatePlayer",Q_ARG(QVariant,false)));QTest::qWait(360);
    // Check disjoint range geometry and actual colors in the shared QML slider.
    auto *engine=QQmlEngine::contextForObject(progress)->engine();QQmlComponent component(engine,QUrl("qrc:/qml/components/PlaybackSlider.qml"));
    std::unique_ptr<QObject> object(component.create());QVERIFY2(object,qPrintable(component.errorString()));auto *slider=qobject_cast<QQuickItem *>(object.get());QVERIFY(slider);slider->setParentItem(root);
    slider->setX(30);slider->setY(120);slider->setWidth(360);slider->setHeight(30);slider->setProperty("from",0);slider->setProperty("to",12000);slider->setProperty("value",2000);
    slider->setProperty("showBuffer",true);slider->setProperty("bufferedRanges",QVariantList{QVariantMap{{"start",0},{"end",4000}},QVariantMap{{"start",7000},{"end",9000}}});
    auto *background=qvariant_cast<QQuickItem *>(slider->property("background"));QVERIFY(background);QTRY_VERIFY(visual(slider,"bufferedProgressSegment"));
    const auto verifyRanges=[&](){int count=0;for(auto *item:background->childItems())if(item->objectName()=="bufferedProgressSegment"){
        if(count==0){if(qAbs(item->x())>0.01||qAbs(item->width()-120)>0.01)return false;}
        else if(count==1){if(qAbs(item->x()-210)>0.01||qAbs(item->width()-60)>0.01)return false;}++count;
        if(item->property("color").value<QColor>().lightnessF()<=background->property("color").value<QColor>().lightnessF())return false;
    }return count==2;};
    for(const int theme:{0,1}){QVERIFY(shell.settingsBridge()->setPreference("theme",theme));QTRY_VERIFY(verifyRanges());}
    const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);QTest::qWait(50);shell.window()->grabWindow().save(QDir(folder).filePath("buffered-progress.png"));}
    slider->setProperty("showBuffer",false);QTRY_VERIFY(!visual(slider,"bufferedProgressSegment"));
    auto *volumeBackground=qvariant_cast<QQuickItem *>(volume->property("background"));QVERIFY(volumeBackground);
    QCOMPARE(background->property("color").value<QColor>(),volumeBackground->property("color").value<QColor>());
    QVERIFY(shell.warnings().isEmpty());QVERIFY(shell.shutdown());QVERIFY(facade->snapshot().buffered.isEmpty());
}


void PhaseGTests::transientOnlineFailuresRetryAtTheSamePositionAndStopAfterTwoRetries(){
    Fixture f;QVERIFY(f.ok);const auto wave=syntheticWave(f.dir.filePath("audio"));QVERIFY(!wave.isEmpty());
    QTcpServer unavailable;QVERIFY(unavailable.listen(QHostAddress::LocalHost));
    connect(&unavailable,&QTcpServer::newConnection,&unavailable,[&]{while(unavailable.hasPendingConnections()){
        auto *socket=unavailable.nextPendingConnection();connect(socket,&QTcpSocket::readyRead,socket,[socket]{socket->readAll();socket->write("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");socket->disconnectFromHost();});connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    }});
    Track online;online.id=1000000000200LL;online.hash="netease:200";online.title="Synthetic retry";online.duration=12000;
    int attempts=0;bool failAll=false;QSignalSpy failures(&f.facade,&PlaybackFacade::playbackFailure);
    connect(&f.facade,&PlaybackFacade::onlineSourceRequested,&f.facade,[&](qint64 id){
        ++attempts;if(failAll){f.facade.failOnlineSource(id,"Synthetic transient resolver error",true);return;}
        f.facade.setOnlineSource(id,attempts==1?QUrl(QString("http://127.0.0.1:%1/unavailable.wav").arg(unavailable.serverPort())):QUrl::fromLocalFile(wave));
    });
    f.facade.playOnline({online},online.id);QTRY_VERIFY_WITH_TIMEOUT(f.facade.playing()&&f.facade.seekable(),8000);QCOMPARE(attempts,2);QCOMPARE(failures.size(),0);
    f.facade.seek(5000);QTRY_VERIFY(f.facade.position()>=5000);auto *player=f.facade.findChildren<QMediaPlayer *>().last();QVERIFY(player!=f.facade.backend()->media());
    player->errorOccurred(QMediaPlayer::NetworkError,"Synthetic interrupted stream");
    QTRY_COMPARE(attempts,3);QTRY_VERIFY(f.facade.playing()&&f.facade.position()>=5000);QCOMPARE(f.facade.current().id,online.id);QCOMPARE(f.facade.queue().items,QVector<qint64>{online.id});QCOMPARE(failures.size(),0);
    failAll=true;f.facade.playOnline({online},online.id);const auto start=attempts;QTRY_COMPARE_WITH_TIMEOUT(failures.size(),1,6000);QCOMPARE(attempts,start+2);QTest::qWait(1400);QCOMPARE(attempts,start+2);
    failAll=false;f.facade.toggle();QTRY_VERIFY(f.facade.playing());QCOMPARE(f.facade.current().id,online.id);QVERIFY(f.facade.shutdown());
}
void PhaseGTests::bilibiliSigningCatalogLoginAndCancellationUseMockData(){
    Fixture f;QVERIFY(f.ok);BilibiliBridge bridge(&f.library,&f.facade,nullptr,nullptr,&f.transport);
    const auto candidates=BilibiliBridge::playbackUrls(QJsonObject{{"url","https://cn-edge.bilivideo.com/video"},{"backup_url",QJsonArray{"http://upos-mirror.bilivideo.com/video","https://backup.example.test/video","http://upos-mirror.bilivideo.com/video","file:///not-media"}}});
    QCOMPARE(candidates.size(),3);QCOMPARE(candidates.first(),QUrl("https://upos-mirror.bilivideo.com/video"));QCOMPARE(candidates[1],QUrl("https://cn-edge.bilivideo.com/video"));

    const QString keys="7cd084941338484aae1ad9425b84077c" "4932caff0ff746eab6f01bf08b70ac45";
    const auto url=BilibiliBridge::signedUrl("/x/test",{{"foo","114"},{"bar","514"},{"baz","1919810"}},keys,1702204169);
    QVERIFY(!url.isEmpty());QUrlQuery query(url);QCOMPARE(query.queryItemValue("wts"),QString("1702204169"));QCOMPARE(query.queryItemValue("w_rid").size(),32);QVERIFY(BilibiliBridge::signedUrl("/x/test",{},"bad",1).isEmpty());
    const auto escaped=BilibiliBridge::signedUrl("/x/test",{{"keyword","a!'()* b"}},keys,1702204169);QCOMPARE(QUrlQuery(escaped).queryItemValue("keyword",QUrl::FullyDecoded),QString("a b"));
    f.transport.responses={R"({"code":0,"data":{"url":"https://passport.bilibili.com/qrcode/h5/login?oauthKey=synthetic","qrcode_key":"synthetic-qr"}})",R"({"code":0,"data":{"code":0}})",R"({"code":0,"data":{"isLogin":true,"mid":42,"uname":"Synthetic Bili","face":"https://image.example.test/avatar.png","vipStatus":1}})"};
    bridge.startLogin();QTRY_VERIFY(!bridge.qrImage().isEmpty());QTRY_VERIFY_WITH_TIMEOUT(bridge.loggedIn(),6000);QCOMPARE(bridge.nickname(),QString("Synthetic Bili"));QVERIFY(bridge.vip());QCOMPARE(bridge.membership(),QString("大会员"));
    f.transport.responses={R"({"code":0,"data":{"list":[{"id":22,"title":"Synthetic favorites","cover":"","media_count":2}]}})"};bridge.showPage("mine","");QTRY_COMPARE(bridge.playlists().size(),1);
    f.transport.responses={R"({"code":0,"data":{"info":{"title":"Synthetic favorites","media_count":2},"has_more":true,"medias":[{"bvid":"BVsynthetic1","title":"<em>Generated</em> &amp; safe","duration":120,"upper":{"name":"Mock author"},"cover":""}]}})"};bridge.openPlaylist("22");QTRY_COMPARE(bridge.songs().size(),1);QVERIFY(bridge.hasMore());QCOMPARE(bridge.songs().first().toMap().value("title").toString(),QString("Generated & safe"));QCOMPARE(bridge.songs().first().toMap().value("duration").toLongLong(),qint64(120000));
    f.transport.responses={R"({"code":0,"data":{"info":{"title":"Synthetic favorites","media_count":2},"has_more":false,"medias":[{"bvid":"BVsynthetic1","title":"Duplicate","duration":120},{"bvid":"BVsynthetic2","title":"Second","duration":60}]}})"};bridge.loadMore();QTRY_COMPARE(bridge.songs().size(),2);QVERIFY(!bridge.hasMore());
    f.transport.responses={R"({"code":-101,"data":{"wbi_img":{"img_url":"https://i.example.test/7cd084941338484aae1ad9425b84077c.png","sub_url":"https://i.example.test/4932caff0ff746eab6f01bf08b70ac45.png"}}})",R"({"code":0,"data":{"numPages":1,"result":[{"bvid":"BVinitial","title":"Initial","duration":"1:02","author":"Mock"}]}})"};bridge.search("initial");QTRY_COMPARE(bridge.songs().size(),1);QCOMPARE(bridge.songs().first().toMap().value("duration").toLongLong(),qint64(62000));
    f.transport.responses={R"({"code":0,"data":{"numPages":1,"result":[{"bvid":"BVold","title":"Old","duration":1}]}})",R"({"code":0,"data":{"numPages":1,"result":[{"bvid":"BVnew","title":"New","duration":1}]}})"};bridge.search("old");bridge.search("new");QTRY_COMPARE(bridge.songs().size(),1);QCOMPARE(bridge.songs().first().toMap().value("id").toString(),QString("BVnew"));QVERIFY(QUrlQuery(f.transport.urls.last()).hasQueryItem("w_rid"));
    const auto localPlaylist=f.library.createPlaylist("Local only");const auto count=f.library.trackCount();QVERIFY(bridge.batchSongs({"BVnew"},"append"));QCOMPARE(f.library.trackCount(),count);QVERIFY(f.library.tracks(localPlaylist).isEmpty());
    const auto video=syntheticVideo(f.dir.filePath("video"));QVERIFY(!video.isEmpty());QFile content(video);QVERIFY(content.open(QIODevice::ReadOnly));const auto media=content.readAll();
    QTcpServer origin;QVERIFY(origin.listen(QHostAddress::LocalHost));QStringList originPaths;
    connect(&origin,&QTcpServer::newConnection,&origin,[&]{while(origin.hasPendingConnections()){
        auto *socket=origin.nextPendingConnection();connect(socket,&QTcpSocket::readyRead,socket,[&,socket,media]{const auto request=socket->readAll();originPaths.append(QString::fromLatin1(request.split(' ').value(1)));if(request.startsWith("GET /unavailable")){socket->write("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 40000\r\nConnection: close\r\n\r\n"+QByteArray(40000,'x'));}else if(request.startsWith("GET /redirect")){socket->write("HTTP/1.1 302 Found\r\nLocation: /synthetic.mp4\r\nContent-Length: 40000\r\nConnection: close\r\n\r\n"+QByteArray(40000,'x'));}else socket->write("HTTP/1.1 200 OK\r\nContent-Type: video/mp4\r\nContent-Length: "+QByteArray::number(media.size())+"\r\nConnection: close\r\n\r\n"+media);socket->disconnectFromHost();});connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    }});
    const QString stream=QString("http://127.0.0.1:%1/synthetic.mp4").arg(origin.serverPort());
    f.transport.responses={R"({"code":0,"data":{"pages":[{"cid":42}]}})",QJsonDocument(QJsonObject{{"code",0},{"data",QJsonObject{{"durl",QJsonArray{QJsonObject{{"url",QString(stream).replace("synthetic.mp4","unavailable.mp4")},{"backup_url",QJsonArray{QString(stream).replace("synthetic.mp4","redirect.mp4")}}}}}}}}).toJson()};
    QVideoSink frames;f.facade.setVideoSink(&frames);QSignalSpy failed(&f.facade,&PlaybackFacade::playbackFailure);bridge.playSong("BVnew");QTRY_VERIFY_WITH_TIMEOUT(frames.videoFrame().isValid(),6000);QCOMPARE(failed.size(),0);QCOMPARE(originPaths,(QStringList{"/unavailable.mp4","/redirect.mp4","/synthetic.mp4"}));QVERIFY(f.facade.onlineActive());QVERIFY(f.facade.snapshot().video);QCOMPARE(f.facade.current().onlineMetadata.value("cid").toString(),QString("42"));f.facade.stopOnline();
    f.transport.responses={R"({"code":0,"data":{"v_voucher":"synthetic-verification"}})"};bridge.search("challenge");QTRY_VERIFY(bridge.needsVerification());QVERIFY(bridge.status().contains("安全验证"));
    bridge.logout();QVERIFY(!bridge.loggedIn());QVERIFY(bridge.songs().isEmpty());QVERIFY(!QFileInfo::exists(QDir(f.library.root()).filePath("Online/bilibili-session.bin")));
    const auto data=f.library.setting("onlinePlayback").toObject();QVERIFY(!QJsonDocument(data).toJson().contains("synthetic-qr"));
}
void PhaseGTests::localAndCloudVideoShareTheExistingQueueAndVideoOutput(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto wave=syntheticWave(dir.filePath("audio"));
    const auto video=syntheticVideo(dir.path());QVERIFY(!video.isEmpty());
    const auto imported=Importer::importOne(library.root(),video);QVERIFY2(imported.error.isEmpty(),qPrintable(imported.error));const auto videoId=library.insert(imported.track);QVERIFY(videoId>0);QVERIFY(isVideoTrack(library.track(videoId)));
    Track local;local.file=wave;local.hash="synthetic-mixed-audio";local.title="Generated audio";local.duration=12000;const auto audioId=library.insert(local);QVERIFY(audioId>0);
    const auto list=library.createPlaylist("Local mixed media");QVERIFY(library.addToPlaylist(list,{audioId,videoId}));QCOMPARE(library.tracks(list).size(),2);
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.settingsBridge()->setPreference("autoExpandVideo",true));QVERIFY(shell.load());auto *facade=shell.facade();QObject::disconnect(facade,nullptr,shell.bilibiliBridge(),nullptr);
    Track cloud;cloud.id=5000000000042LL;cloud.hash="bilibili:BVsynthetic";cloud.title="Generated cloud video";cloud.duration=3000;cloud.onlineMetadata={{"bvid","BVsynthetic"},{"cid","42"}};
    connect(facade,&PlaybackFacade::onlineSourceRequested,facade,[&](qint64 id){facade->setOnlineSource(id,QUrl::fromLocalFile(video));});
    facade->setMode(Mode::Sequential);facade->play({audioId,videoId},audioId);QTRY_VERIFY(facade->playing());QVERIFY(!shell.playbackBridge()->video());QVERIFY(facade->enqueueTracks({cloud},true));QTRY_COMPARE(facade->upcoming(),(QVector<qint64>{cloud.id,videoId}));
    facade->playUpcoming(0);QTRY_VERIFY(shell.playbackBridge()->video());QTRY_VERIFY(facade->playing());auto *root=shell.window()->contentItem();QTRY_VERIFY(visual(root,"videoPlaybackSurface"));QTRY_VERIFY(visual(root,"videoPlaybackSurface")->isVisible());
    auto *output=visual(root,"activeVideoOutput");QVERIFY(output);auto *sink=qobject_cast<QVideoSink *>(output->property("videoSink").value<QObject *>());QVERIFY(sink);QTRY_VERIFY_WITH_TIMEOUT(sink->videoFrame().isValid(),6000);
    QTRY_VERIFY(shell.window()->property("playerProgress").toReal()>=.999);QTest::qWait(120);
    const auto rendered=shell.window()->grabWindow();const auto center=output->mapToScene(QPointF(output->width()/2,output->height()/2));const auto scale=qreal(rendered.width())/shell.window()->width();const auto color=rendered.pixelColor(qRound(center.x()*scale),qRound(center.y()*scale));QVERIFY2(color.blue()>180&&color.red()<40,qPrintable(color.name()));
    const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);shell.window()->grabWindow().save(QDir(folder).filePath("generated-video-output.png"));}
    facade->playUpcoming(0);QCOMPARE(facade->current().id,videoId);QVERIFY(!facade->onlineActive());QVERIFY(shell.playbackBridge()->video());QTRY_VERIFY_WITH_TIMEOUT(sink->videoFrame().isValid(),6000);QVERIFY(library.tracks(list).size()==2);
    QVERIFY(shell.appBridge()->legalNotice().contains("wiliwili"));QVERIFY(shell.appBridge()->legalNotice().contains("GNU GENERAL PUBLIC LICENSE"));QCOMPARE(shell.appBridge()->version(),QString::fromLatin1(phaseC::Display));
    QVERIFY(QMetaObject::invokeMethod(shell.window(),"closeLyrics"));QVERIFY(shell.appBridge()->navigate(4));QTRY_VERIFY(visual(root,"discoverPage"));auto *discover=visual(root,"discoverPage");QVERIFY(QMetaObject::invokeMethod(discover,"chooseSource",Q_ARG(QVariant,4)));QTRY_COMPARE(visual(root,"discoverHeading")->property("text").toString(),QString("发现视频"));QVERIFY(!visual(root,"onlineDaily")->isVisible());
    QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"openVersionLog"));QVERIFY(QMetaObject::invokeMethod(visual(root,"openVersionLog"),"clicked"));QTRY_VERIFY(visual(root,"openLegalNotices"));QVERIFY(QMetaObject::invokeMethod(visual(root,"openLegalNotices"),"clicked"));auto *legal=shell.window()->findChild<QObject *>("legalNoticeDialog");QVERIFY(legal);QTRY_VERIFY(legal->property("opened").toBool());QVERIFY(QMetaObject::invokeMethod(legal,"close"));
    QTRY_VERIFY(!legal->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(visual(root,"manageMusicPlatforms"),"clicked"));QTRY_VERIFY(visual(root,"platformRow-4"));auto *biliLogo=visual(root,"platformLogo-4");QVERIFY(biliLogo);QTRY_COMPARE(biliLogo->property("status").toInt(),1);
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}


void PhaseGTests::videoMirrorFallbackResumesAndCannotReviveASkippedTrack(){
    Fixture f;QVERIFY(f.ok);const auto wave=syntheticWave(f.dir.filePath("audio"));QFile file(wave);QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();
    QTcpServer origin;QVERIFY(origin.listen(QHostAddress::LocalHost));QStringList paths;
    connect(&origin,&QTcpServer::newConnection,&origin,[&]{while(origin.hasPendingConnections()){
        auto *socket=origin.nextPendingConnection();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{
            const auto request=socket->readAll();const auto path=QString::fromLatin1(request.split(' ').value(1));paths.append(path);
            if(path=="/missing")socket->write("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            else socket->write("HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\nContent-Length: "+QByteArray::number(bytes.size())+"\r\nConnection: close\r\n\r\n"+bytes);
            socket->disconnectFromHost();
        });
    }});
    Track first;first.id=5000000000033LL;first.hash="bilibili:BVgenerated33";first.title="Generated mirror test";first.duration=12000;
    Track second=first;second.id++;second.hash="bilibili:BVgenerated34";
    int resolutions=0;connect(&f.facade,&PlaybackFacade::onlineSourceRequested,&f.facade,[&](qint64 id){++resolutions;f.facade.setOnlineSource(id,QUrl::fromLocalFile(wave));});
    QSignalSpy failures(&f.facade,&PlaybackFacade::playbackFailure);f.facade.playOnline({first},first.id);QTRY_VERIFY(f.facade.seekable());QTRY_VERIFY(f.facade.playing());f.facade.seek(5000);QTRY_VERIFY(f.facade.position()>=5000);
    const QUrl missing(QString("http://127.0.0.1:%1/missing").arg(origin.serverPort())),good(QString("http://127.0.0.1:%1/good").arg(origin.serverPort()));
    f.facade.setOnlineSource(first.id,missing,{},QList<QUrl>{good});QTRY_COMPARE(paths.size(),2);QTRY_VERIFY(f.facade.playing()&&f.facade.position()>=5000);QCOMPARE(failures.size(),0);QCOMPARE(resolutions,1);QCOMPARE(f.facade.current().id,first.id);
    bool watch=false,switched=false;connect(&f.facade,&PlaybackFacade::snapshotChanged,&f.facade,[&](const PlaybackSnapshot &snapshot){
        if(watch&&!switched&&snapshot.transport==QMediaPlayer::StoppedState&&snapshot.track.id==first.id){switched=true;f.facade.playOnline({second},second.id);}
    });
    watch=true;f.facade.setOnlineSource(first.id,missing,{},QList<QUrl>{good});QTRY_VERIFY(switched);QTRY_VERIFY(f.facade.playing());QCOMPARE(f.facade.current().id,second.id);QTest::qWait(200);QCOMPARE(paths.count("/good"),1);QCOMPARE(resolutions,2);QCOMPARE(failures.size(),0);QVERIFY(f.facade.shutdown());
}


void PhaseGTests::dashVideoAndAudioShareTheQtClockAndQualityCeiling(){
    Fixture f;QVERIFY(f.ok);QVector<QByteArray> files;QVector<QJsonObject> media;
    for(int i=0;i<2;++i){
        const auto path=f.dir.filePath(i==0?"video.mp4":"audio.mp4");QProcess encoder;
        QStringList args{"-v","error","-f","lavfi","-i",i==0?"color=c=blue:s=160x90:r=10":"anullsrc=r=96000:cl=stereo","-t","4"};
        args.append(i==0?QStringList{"-c:v","libx264","-pix_fmt","yuv420p","-an"}:QStringList{"-c:a","flac","-strict","-2","-sample_fmt","s32","-vn"});args.append({"-movflags","+dash+global_sidx","-frag_duration","1000000","-y",path});
        encoder.start(toolPath("ffmpeg"),args);QVERIFY(encoder.waitForFinished(15000));QVERIFY2(encoder.exitCode()==0,encoder.readAllStandardError().constData());QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();files.append(bytes);
        QJsonObject info{{"id",i==0?80:30280},{"bandwidth",192000},{"codecs",i==0?"avc1.64000a":"fLaC"},{"width",160},{"height",90}};
        for(qint64 at=0;at+8<=bytes.size();){const auto size=qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData()+at));if(size<8)break;if(bytes.mid(at+4,4)=="sidx"){info["SegmentBase"]=QJsonObject{{"Initialization",QString("0-%1").arg(at-1)},{"indexRange",QString("%1-%2").arg(at).arg(at+size-1)}};break;}at+=size;}
        QVERIFY(info.contains("SegmentBase"));media.append(info);
    }
    // Keep both downloads active when a live quality switch cancels them.
    for(auto &bytes:files){QByteArray padding(1024*1024+8,'\0');qToBigEndian(quint32(padding.size()),reinterpret_cast<uchar *>(padding.data()));padding.replace(4,4,"free");bytes+=padding;}
    QTcpServer origin;QVERIFY(origin.listen(QHostAddress::LocalHost));
    connect(&origin,&QTcpServer::newConnection,&origin,[&]{while(origin.hasPendingConnections()){
        auto *socket=origin.nextPendingConnection();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{const auto request=socket->readAll();const auto bytes=request.contains("/video")?files[0]:files[1];qint64 start=0,end=bytes.size()-1;bool ranged=false;
            for(const auto &line:request.split('\n'))if(line.toLower().startsWith("range: bytes=")){const auto range=line.trimmed().mid(13).split('-');start=range[0].toLongLong();if(range.size()>1&&!range[1].isEmpty())end=qMin(end,range[1].toLongLong());ranged=true;}
            QByteArray header=ranged?"HTTP/1.1 206 Partial Content\r\n":"HTTP/1.1 200 OK\r\n";
            header+="Content-Type: video/mp4\r\nAccept-Ranges: bytes\r\nContent-Length: "+QByteArray::number(end-start+1)+"\r\nConnection: close\r\n";
            if(ranged)header+="Content-Range: bytes "+QByteArray::number(start)+"-"+QByteArray::number(end)+"/"+QByteArray::number(bytes.size())+"\r\n";
            socket->write(header+"\r\n");
            if(ranged){socket->write(bytes.mid(start,end-start+1));socket->disconnectFromHost();return;}
            socket->write(bytes.left(65536));auto *timer=new QTimer(socket);timer->setInterval(20);
            connect(timer,&QTimer::timeout,socket,[socket,timer,bytes,offset=qint64(65536)]()mutable{if(socket->state()!=QAbstractSocket::ConnectedState){timer->stop();return;}const auto chunk=bytes.mid(offset,8192);socket->write(chunk);offset+=chunk.size();if(offset>=bytes.size()){timer->stop();socket->disconnectFromHost();}});timer->start();});
    }});
    for(int i=0;i<2;++i){const auto url=QString("http://127.0.0.1:%1/%2").arg(origin.serverPort()).arg(i==0?"video":"audio");media[i]["urls"]=QJsonArray{url};media[i]["baseUrl"]=url;}
    auto low=media[0];low["id"]=64;auto top=media[0];top["id"]=120;auto regularAudio=media[1];regularAudio["id"]=30280;regularAudio["codecs"]="mp4a.40.2";auto lowAudio=regularAudio;lowAudio["id"]=30216;lowAudio["bandwidth"]=64000;
    auto flac=media[1];flac["id"]=30251;flac["codecs"]="fLaC";
    QJsonObject response{{"dash",QJsonObject{{"video",QJsonArray{top,low,media[0]}},{"audio",QJsonArray{lowAudio,regularAudio}},{"flac",QJsonObject{{"audio",flac}}}}}};
    auto selected=BilibiliBridge::selectDash(response,80,"hires");QCOMPARE(selected.first.value("id").toInt(),80);QCOMPARE(selected.second.value("id").toInt(),30251);
    selected=BilibiliBridge::selectDash(response,100,"standard");QCOMPARE(selected.first.value("id").toInt(),80);QCOMPARE(selected.second.value("id").toInt(),30216);
    auto dash=response["dash"].toObject();dash.remove("flac");response["dash"]=dash;selected=BilibiliBridge::selectDash(response,64,"hires");QCOMPARE(selected.first.value("id").toInt(),64);QCOMPARE(selected.second.value("id").toInt(),30280);
    Track track;track.id=5000000000088LL;track.hash="bilibili:BVdashGenerated";track.title="Generated DASH";track.duration=4000;
    QVideoSink sink;f.facade.setVideoSink(&sink);QSignalSpy failures(&f.facade,&PlaybackFacade::playbackFailure);
    connect(&f.facade,&PlaybackFacade::onlineSourceRequested,&f.facade,[&](qint64 id){f.facade.setOnlineDashSource(id,media[0],media[1]);});f.facade.setMode(Mode::Sequential);f.facade.playOnline({track},track.id);
    QTRY_VERIFY_WITH_TIMEOUT(sink.videoFrame().isValid(),8000);QCOMPARE(failures.size(),0);QTRY_VERIFY(f.facade.seekable());
    QMediaPlayer *player=nullptr;for(auto *candidate:f.facade.findChildren<QMediaPlayer *>())if(candidate!=f.facade.backend()->media())player=candidate;QVERIFY(player);QVERIFY(player->hasAudio());QVERIFY(!player->hasVideo());QVERIFY(sink.videoFrame().isValid());QCOMPARE(sink.videoFrame().size(),QSize(160,90));QTRY_VERIFY(qAbs(sink.videoFrame().startTime()/1000-player->position())<=150);QVERIFY(player->audioBufferOutput());QSignalSpy samples(player->audioBufferOutput(),&QAudioBufferOutput::audioBufferReceived);QTRY_VERIFY(samples.size()>0);QCOMPARE(samples.first().first().value<QAudioBuffer>().format().sampleRate(),96000);
    f.facade.seek(2000);QTRY_VERIFY(f.facade.position()>=2000);
    f.facade.toggle();QTRY_VERIFY(!f.facade.playing());
    f.facade.refreshOnlineSource();QTRY_VERIFY(f.facade.seekable());QTest::qWait(250);
    QTRY_VERIFY(f.facade.position()>=2000);QVERIFY(!f.facade.playing());f.facade.toggle();QTRY_VERIFY(f.facade.playing());
    f.facade.seek(500);
    BilibiliBridge controller(&f.library,&f.facade,nullptr,nullptr,&f.transport);disconnect(&f.facade,nullptr,&controller,nullptr);
    for(int i=0;i<12;++i){f.facade.seek(500);qInfo()<<"Live quality reload"<<i;controller.setQuality(i%2?"80":"64");controller.setAudioQuality(i%2?"hires":"high");QTest::qWait(120);QTRY_VERIFY(f.facade.playing());QTRY_VERIFY(f.facade.seekable());QTRY_VERIFY(f.facade.position()>=500);}
    f.facade.seek(1500);f.facade.toggle();QTRY_VERIFY(!f.facade.playing());controller.setQuality("120");controller.setAudioQuality("standard");QTRY_VERIFY(f.facade.seekable());QTRY_VERIFY(f.facade.position()>=1500);QVERIFY(!f.facade.playing());
    QCOMPARE(failures.size(),0);QVERIFY(f.facade.shutdown());
    // Exercise the normal audio-device path as well as the decode-only tap.
    const auto skip=qgetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");qunsetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");const auto restore=qScopeGuard([skip]{if(skip.isNull())qunsetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");else qputenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS",skip);});
    PlaybackFacade native(&f.library,nullptr,false);QVideoSink nativeSink;native.setVideoSink(&nativeSink);QSignalSpy nativeFailures(&native,&PlaybackFacade::playbackFailure);
    connect(&native,&PlaybackFacade::onlineSourceRequested,&native,[&](qint64 id){native.setOnlineDashSource(id,media[0],media[1]);});native.playOnline({track},track.id);QTRY_VERIFY_WITH_TIMEOUT(nativeSink.videoFrame().isValid(),8000);
    BilibiliBridge nativeController(&f.library,&native,nullptr,nullptr,&f.transport);disconnect(&native,nullptr,&nativeController,nullptr);
    for(int i=0;i<12;++i){native.seek(500);qInfo()<<"Native quality reload"<<i;nativeController.setQuality(i%2?"80":"64");nativeController.setAudioQuality(i%2?"hires":"high");QTest::qWait(120);QTRY_VERIFY(native.playing());QTRY_VERIFY(native.seekable());QTRY_VERIFY(native.position()>=500);}
    native.seek(1500);native.toggle();QTRY_VERIFY(!native.playing());nativeController.setQuality("120");nativeController.setAudioQuality("standard");QTRY_VERIFY(native.seekable());QTRY_VERIFY(native.position()>=1500);QVERIFY(!native.playing());
    QCOMPARE(nativeFailures.size(),0);QVERIFY(native.shutdown());
}


void PhaseGTests::biliFoldersUsersAndPagingRemainCompatibleWithSidebarPins(){
    Fixture f;QVERIFY(f.ok);QDir().mkpath(QDir(f.library.root()).filePath("Online"));QFile session(QDir(f.library.root()).filePath("Online/bilibili-session.bin"));QVERIFY(session.open(QIODevice::WriteOnly));session.write(MusicCookieJar::protect("[]",false));session.close();
    QImage picture(32,32,QImage::Format_RGB32);picture.fill(Qt::blue);const auto path=f.dir.filePath("cover.png");QVERIFY(picture.save(path));const auto cover=QUrl::fromLocalFile(path).toString();
    BilibiliBridge bridge(&f.library,&f.facade,nullptr,nullptr,&f.transport);f.transport.responses={biliNav()};bridge.activate();QTRY_VERIFY(bridge.loggedIn());
    f.transport.responses={R"({"code":0,"data":{"list":[{"id":7,"title":"Synthetic favorites","media_count":45}]}})",biliPage(1,1,false,cover)};bridge.showPage("mine","");QTRY_COMPARE(bridge.playlists().size(),1);QTRY_COMPARE(bridge.playlists().first().toMap().value("cover").toString(),cover);QCOMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("ps"),QString("1"));
    f.transport.responses={biliPage(1,20,true,cover)};bridge.openPlaylist("7");QTRY_COMPARE(bridge.songs().size(),20);QCOMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("ps"),QString("20"));
    const auto pinned=f.library.pinCloudPlaylist("bilibili","7","Synthetic favorites",cover,45);QVERIFY(pinned>0);bridge.savePinnedPlaylist("7");QVERIFY(!f.library.addToPlaylist(pinned, {1}));
    TrackListModel source(&f.library);TrackFilterModel view(&source);PlaylistListModel lists(&f.library);LibraryBridge sidebar(&f.library,&f.facade,&source,&view,&lists);
    connect(&sidebar,&LibraryBridge::cloudPlaylistRequested,&bridge,[&](const QString &provider,const QString &id){QCOMPARE(provider,QString("bilibili"));bridge.openPlaylist(id);});
    connect(&bridge,&BilibiliBridge::songsChanged,&sidebar,[&]{sidebar.setCloudSongs("bilibili",bridge.currentPlaylist(),bridge.songs());});
    QVERIFY(sidebar.openPlaylist(QString::number(pinned)));QTRY_COMPARE(view.count(),20);QSignalSpy inserts(&source,&QAbstractItemModel::rowsInserted);QSignalSpy resets(&source,&QAbstractItemModel::modelReset);
    f.transport.responses={biliPage(21,20,true,cover)};bridge.loadMore();bridge.loadMore();QTRY_COMPARE(bridge.songs().size(),40);QCOMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("pn"),QString("2"));
    f.transport.responses={biliPage(41,5,false,cover)};bridge.loadMore();QTRY_COMPARE(bridge.songs().size(),45);QVERIFY(!bridge.hasMore());QTRY_COMPARE(view.count(),45);QCOMPARE(inserts.size(),2);QCOMPARE(resets.size(),0);
    QSignalSpy playRoute(&sidebar,&LibraryBridge::cloudPlayRequested);
    disconnect(&f.facade,nullptr,&bridge,nullptr);
    connect(&sidebar,&LibraryBridge::cloudPlayRequested,&bridge,[&](const QString &provider,const QStringList &ids,const QString &start){QCOMPARE(provider,QString("bilibili"));bridge.playSongs(ids,start);});
    sidebar.play(view.idAt(1));QCOMPARE(playRoute.size(),1);QCOMPARE(playRoute.first()[2].toString(),QString("BVmock2"));QCOMPARE(f.facade.current().hash,QString("bilibili:BVmock2"));QVERIFY(f.facade.upcoming().size()>=43);
    const auto local=f.library.createPlaylist("Mixed playlist");QVERIFY(local>0);
    Track localTrack;localTrack.hash="local-mixed-playlist";localTrack.file="Music/mixed-fixture.wav";
    const auto localTrackId=f.library.insert(localTrack);QVERIFY(localTrackId>0);
    QVERIFY(f.library.addToPlaylist(local,{localTrackId}));
    const auto onlineKey=view.idAt(1);
    QVERIFY(sidebar.addTracksToPlaylist(QString::number(local),{onlineKey}));
    QVERIFY(sidebar.addTracksToPlaylist(QString::number(local),{onlineKey}));
    QVERIFY(!sidebar.addTracksToPlaylist(QString::number(pinned),{onlineKey}));
    const auto mixed=f.library.playlistEntries(local);
    QCOMPARE(mixed.size(),2);
    QCOMPARE(mixed.first().id,localTrackId);
    QCOMPARE(mixed.last().hash,QString("bilibili:BVmock2"));
    QVERIFY(isVideoTrack(mixed.last()));
    QCOMPARE(f.library.tracks(local).size(),1);
    bridge.showPage("featured","");bridge.openPlaylist("7");QCOMPARE(bridge.songs().size(),45);QVERIFY(!bridge.hasMore());const auto requests=f.transport.urls.size();bridge.loadMore();QCOMPARE(f.transport.urls.size(),requests);
    f.transport.responses={biliNav(),R"({"code":0,"data":{"numPages":1,"result":[{"mid":99,"uname":"Synthetic user","upic":"","videos":3}]}})"};bridge.search("ABCD",1);QTRY_COMPARE(bridge.searchResults().size(),1);QCOMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("search_type"),QString("bili_user"));QVERIFY(bridge.songs().isEmpty());QCOMPARE(bridge.searchResults().first().toMap().value("kind").toString(),QString("user"));
    f.transport.responses={R"({"code":0,"data":{"list":{"vlist":[{"bvid":"BVuser","title":"User upload","length":"2:03","author":"Synthetic user"}]},"page":{"count":1}}})"};bridge.showPage("user","99");QTRY_COMPARE(bridge.songs().size(),1);QCOMPARE(bridge.songs().first().toMap().value("duration").toLongLong(),qint64(123000));
    QVERIFY(f.library.tracks(pinned).isEmpty());for(bool enabled:f.transport.http2)QVERIFY(!enabled);
    const auto cacheFile=QDir(DiskCache::online(&f.library)).filePath("catalog-bilibili-"+QCryptographicHash::hash(QByteArray("playlist:7"),QCryptographicHash::Sha256).toHex()+".json");
    auto persisted=[&]{QFile file(cacheFile);return file.open(QIODevice::ReadOnly)?QJsonDocument::fromJson(file.readAll()).object():QJsonObject{};};
    QTRY_COMPARE_WITH_TIMEOUT(persisted().value("songs").toArray().size(),45,3000);
    Transport restoredTransport;restoredTransport.responses={biliNav(),biliPage(1,20,true,cover)};BilibiliBridge restored(&f.library,&f.facade,nullptr,nullptr,&restoredTransport);restored.openPlaylist("7");QCOMPARE(restored.songs().size(),45);QVERIFY(!restored.hasMore());QCOMPARE(restored.currentPlaylist().value("cover").toString(),cover);QTRY_COMPARE(restored.songs().size(),20);QVERIFY(restored.hasMore());
}
void PhaseGTests::discoveryAutoPagesAndRetainsQueriesAcrossSources(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *engine=QQmlEngine::contextForObject(shell.window())->engine();Transport netTransport,qqTransport,biliTransport;ai::AiRuntime runtime(&library);
    OnlineMusicBridge net(&library,shell.facade(),shell.lyricsModel(),&runtime,shell.onlineArtworkCache(),nullptr,&netTransport);
    QqMusicBridge qq(&library,shell.facade(),shell.lyricsModel(),&runtime,shell.onlineArtworkCache(),nullptr,&qqTransport);BilibiliBridge bili(&library,shell.facade(),nullptr,nullptr,&biliTransport);
    engine->rootContext()->setContextProperty("onlineMusic",&net);engine->rootContext()->setContextProperty("qqMusic",&qq);engine->rootContext()->setContextProperty("bilibili",&bili);
    const auto restoreContext=qScopeGuard([&]{engine->rootContext()->setContextProperty("onlineMusic",shell.onlineMusicBridge());engine->rootContext()->setContextProperty("qqMusic",shell.qqMusicBridge());engine->rootContext()->setContextProperty("bilibili",shell.bilibiliBridge());});
    QVERIFY(shell.appBridge()->navigate(4));auto *root=shell.window()->contentItem();QTRY_VERIFY(visual(root,"discoverPage"));auto *page=visual(root,"discoverPage");
    for(int i=0;i<12;++i){const int source=i%2;QVERIFY(QMetaObject::invokeMethod(page,"chooseSource",Q_ARG(QVariant,source)));QVERIFY(QMetaObject::invokeMethod(page,"submitSearch",Q_ARG(QVariant,QString("ABCD"))));QTest::qWait(5);QCOMPARE(page->property("searchQuery").toString(),QString("ABCD"));}
    for(bool enabled:netTransport.http2)QVERIFY(!enabled);for(bool enabled:qqTransport.http2)QVERIFY(!enabled);
    QVERIFY(QMetaObject::invokeMethod(page,"chooseSource",Q_ARG(QVariant,4)));biliTransport.responses={biliNav(),biliPage(1,20,true)};QVERIFY(QMetaObject::invokeMethod(page,"submitSearch",Q_ARG(QVariant,QString("ABCD"))));QTRY_COMPARE(bili.songs().size(),20);
    auto *list=visual(root,"onlineSongs");QVERIFY(list);QVERIFY(!visual(root,"bilibiliLoadMore"));QTest::qWait(60);QCOMPARE(bili.songs().size(),20);const auto before=list->property("contentY").toReal();
    biliTransport.responses={biliPage(21,20,false)};QVERIFY(QMetaObject::invokeMethod(list,"positionViewAtEnd"));QTRY_COMPARE(bili.songs().size(),40);QTest::qWait(50);QVERIFY(list->property("contentY").toReal()>before+100);
    auto *account=visual(root,"onlineAccountButton");auto *name=visual(root,"onlineAccountName");QVERIFY(account);QVERIFY(name);QCOMPARE(account->property("implicitWidth").toReal(),std::ceil(name->property("implicitWidth").toReal())+52);
    biliTransport.responses={R"({"code":0,"data":{"numPages":1,"result":[{"mid":99,"uname":"Synthetic user","upic":"","videos":3}]}})"};QVERIFY(QMetaObject::invokeMethod(page,"chooseSearchCategory",Q_ARG(QVariant,1)));QTRY_COMPARE(bili.searchResults().size(),1);QTRY_VERIFY(visual(root,"onlinePlaylistCards")->isVisible());
    const auto shots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!shots.isEmpty()){QDir().mkpath(shots);QTest::qWait(80);shell.window()->grabWindow().save(QDir(shots).filePath("bilibili-user-search.png"));}
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));engine->rootContext()->setContextProperty("onlineMusic",shell.onlineMusicBridge());engine->rootContext()->setContextProperty("qqMusic",shell.qqMusicBridge());engine->rootContext()->setContextProperty("bilibili",shell.bilibiliBridge());QVERIFY(shell.shutdown());
}
void PhaseGTests::videoEntryAndLetterboxPreferencesDoNotReopenOnEveryTrack(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto video=syntheticVideo(dir.path());QVERIFY(!video.isEmpty());Track track;track.file=video;track.hash="generated-video-pref";track.title="Generated";track.duration=3000;const auto id=library.insert(track);QVERIFY(id>0);
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());QVERIFY(!shell.settingsBridge()->values().value("autoExpandVideo").toBool());shell.facade()->play({id},id);QTRY_VERIFY(shell.facade()->playing());QVERIFY(!shell.window()->property("lyricsOpen").toBool());QVERIFY(QMetaObject::invokeMethod(shell.window(),"showLyrics"));QTRY_VERIFY(shell.window()->property("playerProgress").toReal()>=.999);
    auto *root=shell.window()->contentItem();auto *surface=visual(root,"videoPlaybackSurface"),*output=visual(root,"activeVideoOutput");QVERIFY(surface);QVERIFY(output);QCOMPARE(surface->property("color").value<QColor>(),QColor("black"));
    QVERIFY(shell.settingsBridge()->setPreference("videoBlackBars",false));shell.window()->resize(414,790);QTest::qWait(360);QCOMPARE(surface->property("color").value<QColor>().alpha(),0);QVERIFY(output->height()<surface->height()*.8);QVERIFY(qAbs(output->width()/output->height()-16./9)<.05);const auto shots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!shots.isEmpty()){QDir().mkpath(shots);shell.window()->grabWindow().save(QDir(shots).filePath("video-transparent-bars-414.png"));}
    QVERIFY(QMetaObject::invokeMethod(shell.window(),"closeLyrics"));QTRY_VERIFY(!shell.window()->property("lyricsOpen").toBool());shell.facade()->play({id},id);QTRY_VERIFY(shell.facade()->playing());QVERIFY(!shell.window()->property("lyricsOpen").toBool());QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"autoExpandVideoSetting"));QVERIFY(visual(root,"videoBlackBarsSetting"));
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}


void PhaseGTests::videoChromePartsAndCoverRatiosFitBothLayouts(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
    const auto file=syntheticVideo(dir.path());QVERIFY(!file.isEmpty());QImage cover(480,270,QImage::Format_RGB32);cover.fill(Qt::red);for(int y=0;y<270;++y)for(int x=60;x<420;++x)cover.setPixelColor(x,y,Qt::blue);const auto coverFile=dir.filePath("wide-cover.png");QVERIFY(cover.save(coverFile));
    UiImages images;QSize imageSize;const auto imageKey="artwork?file="+QString::fromLatin1(QUrl::toPercentEncoding(QUrl::fromLocalFile(coverFile).toString()))+"&resolution=160&radiusRatio=0&aspect=1.333333";const auto cropped=images.requestImage(imageKey,&imageSize,{});QCOMPARE(imageSize,QSize(160,120));QCOMPARE(cropped.pixelColor(5,60),QColor(Qt::blue));
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *facade=shell.facade();disconnect(facade,&PlaybackFacade::onlineSourceRequested,shell.bilibiliBridge(),nullptr);
    Track online;online.id=bilibiliTrackId("BVgeneratedParts");online.hash="bilibili:BVgeneratedParts";online.title="Generated video title";online.artist="Synthetic artist";online.cover=coverFile;online.duration=3000;QJsonArray parts;for(int i=1;i<=80;++i)parts.append(QJsonObject{{"cid",QString::number(9000+i)},{"page",i},{"part",QString("Generated part %1").arg(i)},{"duration",120+i}});online.onlineMetadata={{"pages",parts},{"cid","9001"},{"page",1},{"videoTitle",online.title}};
    connect(facade,&PlaybackFacade::onlineSourceRequested,facade,[&](qint64 id){facade->setOnlineSource(id,QUrl::fromLocalFile(file));});facade->playOnline({online},online.id);QTRY_VERIFY(shell.playbackBridge()->video());QTRY_VERIFY(facade->playing());QTest::qWait(250);facade->toggle();QTRY_VERIFY(!facade->playing());
    auto *w=shell.window();auto *root=w->contentItem();QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_VERIFY(w->property("playerProgress").toReal()>=.999);auto *entry=visual(root,"videoPartsEntry"),*caption=visual(root,"titleBar"),*metadata=visual(root,"videoMetadata"),*surface=visual(root,"videoPlaybackSurface"),*art=visual(root,"videoMetadataCover");QVERIFY(entry&&caption&&metadata&&surface&&art);auto *panel=w->findChild<QObject *>("videoPartsPanel");QVERIFY(panel);
    for(int width:{1240,800,547,414}){
        w->resize(width,790);QTest::qWait(380);entry=visual(root,width<547?"compactVideoPartsEntry":"videoPartsEntry");QVERIFY(entry);QTest::mouseMove(w,QPoint(150,150));QTRY_VERIFY(entry->opacity()>.99);QVERIFY(entry->isVisible());QVERIFY(entry->isEnabled());QVERIFY(qAbs(caption->height()-25.6)<.01);QVERIFY(qAbs(caption->width()-134.4)<.01);if(width<547)QVERIFY(entry->mapToScene(QPointF(entry->width(),0)).x()<=caption->mapToScene(QPointF()).x()-3.9);else{QCOMPARE(entry->y(),qreal(28));QVERIFY(visual(root,"playerCollapse")->mapToScene(QPointF()).x()<art->mapToScene(QPointF()).x());}QVERIFY(qAbs(art->width()/art->height()-16./9)<.001);QVERIFY(metadata->mapToScene(QPointF(0,metadata->height())).y()<surface->mapToScene(QPointF()).y());
        const auto hit=entry->mapToScene(QPointF(entry->width()/2,entry->height()/2)).toPoint();QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,hit);QTRY_VERIFY(panel->property("visible").toBool());QTest::qWait(45);
        auto *partsList=visual(root,"videoPartsList");QVERIFY(partsList);const auto progress=panel->property("reveal").toReal();QVERIFY(progress>0&&progress<1);QCOMPARE(panel->property("opacity").toReal(),qreal(1));
        if(width>=547){const auto finalX=width-34-panel->property("width").toReal();QVERIFY(panel->property("x").toReal()>finalX+5);QVERIFY(qAbs(partsList->mapToScene(QPointF()).x()-16-panel->property("x").toReal())<2);}
        QTRY_VERIFY(panel->property("reveal").toReal()>=.999);
        auto *second=visual(root,"videoPart-2");QVERIFY(second);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,second->mapToScene(QPointF(second->width()/2,second->height()/2)).toPoint());
        QTRY_COMPARE(shell.bilibiliBridge()->currentPart(),2);QTRY_VERIFY(!facade->playing()&&facade->seekable());QCOMPARE(facade->current().onlineMetadata.value("cid").toString(),QString("9002"));QCOMPARE(facade->queue().items,QVector<qint64>{online.id});QVERIFY(panel->property("visible").toBool());QTRY_VERIFY(visual(root,"videoPart-2")->property("current").toBool());
        QVERIFY(QMetaObject::invokeMethod(visual(root,"previousVideoPart"),"clicked"));QTRY_COMPARE(shell.bilibiliBridge()->currentPart(),1);QTRY_VERIFY(!facade->playing()&&facade->seekable());QVERIFY(!visual(root,"previousVideoPart")->isEnabled());
        const auto shots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!shots.isEmpty()){QDir().mkpath(shots);w->grabWindow().save(QDir(shots).filePath(QString("video-chrome-parts-%1.png").arg(width)));}
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,hit);QTRY_VERIFY(!panel->property("visible").toBool());QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,hit);QTRY_VERIFY(panel->property("reveal").toReal()>=.999);
        auto *close=visual(root,"closeVideoParts");QVERIFY(close);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,close->mapToScene(QPointF(close->width()/2,close->height()/2)).toPoint());
        if(width<547){bool passedPlaybackEdge=false;for(int i=0;i<30&&panel->property("visible").toBool();++i){if(partsList->mapToScene(QPointF()).y()-16-partsList->y()>root->height()-visual(root,"playerBar")->height())passedPlaybackEdge=true;QTest::qWait(10);}QVERIFY(passedPlaybackEdge);QVERIFY(panel->property("exitBottom").toReal()>700);}
        QTRY_VERIFY(!panel->property("visible").toBool());
        auto single=facade->current();single.onlineMetadata["pages"]=QJsonArray{parts.first()};facade->updateOnlineTrack(single);QTRY_VERIFY(!entry->isEnabled());QVERIFY(entry->isVisible());QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,hit);QTest::qWait(40);QVERIFY(!panel->property("visible").toBool());single.onlineMetadata["pages"]=parts;facade->updateOnlineTrack(single);QTRY_VERIFY(entry->isEnabled());
    }
    // Expanded 16:9 covers preserve the red edge; the 4:3 provider assertion above still checks list cropping.
    art->setProperty("imageUrl",QUrl::fromLocalFile(coverFile));QTest::qWait(150);const auto frame=w->grabWindow();const auto point=art->mapToScene(QPointF(art->width()*.1,art->height()/2));const auto ratio=qreal(frame.width())/w->width();const auto pixel=frame.pixelColor(qRound(point.x()*ratio),qRound(point.y()*ratio));QVERIFY(pixel.red()>220&&pixel.blue()<30);
    QTest::mouseMove(w,QPoint(180,200));QTRY_VERIFY(entry->opacity()>.99);QTest::qWait(3350);QTRY_VERIFY(entry->opacity()<.01);QVERIFY(!entry->isEnabled());QTest::mouseMove(w,QPoint(190,210));QTRY_VERIFY(entry->opacity()>.99);
    QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QTRY_VERIFY(w->property("playerProgress").toReal()<=.001);auto *bottom=visual(root,"bottomCover");QVERIFY(bottom);QVERIFY(qAbs(bottom->width()/bottom->height()-4./3)<.001);
    Track local;local.file=file;local.cover=coverFile;local.hash="generated-local-video";local.title="Local generated video";local.duration=3000;const auto id=library.insert(local);QVERIFY(id>0);TrackListModel source(&library);QCOMPARE(source.data(source.index(0),TrackListModel::VideoTrack).toBool(),true);local.id=id;QVERIFY(facade->enqueueTracks({local},true));QTRY_VERIFY(shell.upcomingModel()->firstVideo());
    w->resize(1240,790);QTest::qWait(380);auto *nextCover=visual(root,"nextCompactCover");QVERIFY(nextCover);QVERIFY(qAbs(nextCover->width()/nextCover->height()-4./3)<.001);
    const QVariantMap collection{{"provider","bilibili"},{"id","generated-folder"},{"name","Synthetic video collection"},{"cover",coverFile},{"count",1}};const auto pinned=shell.libraryBridge()->pinCloudPlaylist(collection);QVERIFY(pinned!="0");
    {const QSignalBlocker noNetwork(shell.libraryBridge());QVERIFY(shell.libraryBridge()->openPlaylist(pinned));}shell.libraryBridge()->playlistChanged();
    QVariantMap videoRow{{"id","BVgeneratedParts"},{"internalId",QString::number(online.id)},{"title",online.title},{"artist",online.artist},{"album","B站视频"},{"duration",3000},{"cover",coverFile}};shell.libraryBridge()->setCloudSongs("bilibili",collection,{videoRow});QVERIFY(shell.appBridge()->navigate(0));
    QTRY_VERIFY(visual(root,"trackCover-"+QString::number(online.id)));auto *rowCover=visual(root,"trackCover-"+QString::number(online.id)),*collectionCover=visual(root,"playlistCover");QVERIFY(collectionCover);QVERIFY(qAbs(rowCover->width()/rowCover->height()-4./3)<.001);QVERIFY(qAbs(collectionCover->width()/collectionCover->height()-4./3)<.001);
    QVERIFY(shell.appBridge()->navigate(1));QTRY_VERIFY(visual(root,"playlistCardCover-"+pinned));auto *cardCover=visual(root,"playlistCardCover-"+pinned);QVERIFY(qAbs(cardCover->width()/cardCover->height()-4./3)<.001);
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}

void PhaseGTests::unavailableQueueItemsSkipUntilPlayableAndStopAfterOneRound(){
    for(auto mode:{Mode::Sequential,Mode::ListLoop,Mode::SingleLoop,Mode::Shuffle}){
        Fixture f;QVERIFY(f.ok);const auto wave=syntheticWave(f.dir.filePath("audio"));QVERIFY(!wave.isEmpty());
        Track good;good.file=wave;good.hash="generated-playable";good.duration=12000;good.id=f.library.insert(good);QVERIFY(good.id);
        Track missing;missing.file=f.dir.filePath("missing.mp4");missing.hash="generated-missing";missing.id=f.library.insert(missing);QVERIFY(missing.id);
        Track a;a.id=5000000000301LL;a.hash="bilibili:BVunavailableA";a.title="Unavailable A";Track b=a;b.id++;b.hash="bilibili:BVunavailableB";
        QHash<qint64,int> attempts;QSignalSpy failures(&f.facade,&PlaybackFacade::playbackFailure);
        connect(&f.facade,&PlaybackFacade::onlineSourceRequested,&f.facade,[&](qint64 id){++attempts[id];f.facade.failOnlineSource(id,"Synthetic unavailable video",false);});
        f.facade.setMode(mode);f.facade.playOnline({a,b,missing,good},a.id);
        QTRY_VERIFY_WITH_TIMEOUT(f.facade.current().id==good.id&&f.facade.playing(),6000);QCOMPARE(f.facade.queue().items,(QVector<qint64>{a.id,b.id,missing.id,good.id}));QVERIFY(failures.size()<=1);if(!failures.isEmpty())QCOMPARE(failures.first().at(1).toLongLong(),missing.id);QVERIFY(attempts.value(a.id)<=1);QVERIFY(attempts.value(b.id)<=1);QVERIFY(f.facade.shutdown());
    }
    for(auto mode:{Mode::Sequential,Mode::ListLoop,Mode::SingleLoop,Mode::Shuffle}){
        Fixture f;QVERIFY(f.ok);Track a;a.id=5000000000301LL;a.hash="bilibili:BVfailedA";Track b=a;b.id++;b.hash="bilibili:BVfailedB";Track c=a;c.id+=2;c.hash="bilibili:BVfailedC";
        QHash<qint64,int> attempts;QSignalSpy failures(&f.facade,&PlaybackFacade::playbackFailure);
        connect(&f.facade,&PlaybackFacade::onlineSourceRequested,&f.facade,[&](qint64 id){++attempts[id];f.facade.failOnlineSource(id,"Synthetic unavailable video",false);});
        f.facade.setMode(mode);f.facade.playOnline({a,b,c},a.id);QTRY_COMPARE(failures.size(),1);QVERIFY(!f.facade.playing());QCOMPARE(attempts.size(),3);for(auto n:attempts)QCOMPARE(n,1);QTest::qWait(250);QCOMPARE(failures.size(),1);
        f.facade.toggle();QTRY_COMPARE(failures.size(),2);for(auto n:attempts)QVERIFY(n<=2);QVERIFY(f.facade.shutdown());
    }
    Fixture f;QVERIFY(f.ok);Track a;a.id=5000000000301LL;a.hash="bilibili:BVstale";Track b=a;b.id++;b.hash="bilibili:BVcurrent";const auto wave=syntheticWave(f.dir.filePath("audio"));
    connect(&f.facade,&PlaybackFacade::onlineSourceRequested,&f.facade,[&](qint64 id){if(id==a.id)f.facade.failOnlineSource(id,"Synthetic failure",false);else f.facade.setOnlineSource(id,QUrl::fromLocalFile(wave));});
    f.facade.playOnline({a,b},a.id);f.facade.next();QTRY_VERIFY(f.facade.playing());QCOMPARE(f.facade.current().id,b.id);QTest::qWait(200);QCOMPARE(f.facade.current().id,b.id);
    disconnect(&f.facade,&PlaybackFacade::onlineSourceRequested,&f.facade,nullptr);int badStreams=0;
    connect(&f.facade,&PlaybackFacade::onlineSourceRequested,&f.facade,[&](qint64 id){if(id==a.id)++badStreams;f.facade.setOnlineSource(id,QUrl::fromLocalFile(id==a.id?f.dir.filePath("missing-stream.mp4"):wave));});
    f.facade.setMode(Mode::Sequential);f.facade.playOnline({a,b},a.id);QTRY_VERIFY_WITH_TIMEOUT(f.facade.current().id==b.id&&f.facade.playing(),8000);QCOMPARE(badStreams,3);QVERIFY(f.facade.shutdown());
}

void PhaseGTests::bilibiliPartSelectionResolvesCidAndPreservesQueueAndProgress(){
    Fixture f;QVERIFY(f.ok);BilibiliBridge bridge(&f.library,&f.facade,nullptr,nullptr,&f.transport);const auto file=syntheticVideo(f.dir.path());QVERIFY(!file.isEmpty());QFile input(file);QVERIFY(input.open(QIODevice::ReadOnly));const auto media=input.readAll();
    QTcpServer origin;QVERIFY(origin.listen(QHostAddress::LocalHost));
    connect(&origin,&QTcpServer::newConnection,&origin,[&]{while(origin.hasPendingConnections()){
        auto *socket=origin.nextPendingConnection();connect(socket,&QTcpSocket::readyRead,socket,[socket,media]{socket->readAll();socket->write("HTTP/1.1 200 OK\r\nContent-Type: video/mp4\r\nContent-Length: "+QByteArray::number(media.size())+"\r\nConnection: close\r\n\r\n"+media);socket->disconnectFromHost();});connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    }});
    const QString url=QString("http://127.0.0.1:%1/generated.mp4").arg(origin.serverPort());
    const QByteArray view=R"({"code":0,"data":{"title":"Generated video","pages":[{"cid":301,"page":1,"part":"Part one","duration":3},{"cid":302,"page":2,"part":"Part two","duration":3},{"cid":303,"page":3,"part":"Part three","duration":3}]}})";
    const auto source=QJsonDocument(QJsonObject{{"code",0},{"data",QJsonObject{{"durl",QJsonArray{QJsonObject{{"url",url}}}}}}}).toJson();
    f.transport.respond=[&](const QNetworkRequest &r){const auto path=r.url().path();if(path=="/x/web-interface/view")return view;if(path=="/x/web-interface/nav")return biliNav();if(path=="/x/player/wbi/playurl")return source;return QByteArray("{}");};
    bool delaySecondPart=false;
    f.transport.delay=[&](const QNetworkRequest &r){return delaySecondPart&&r.url().path()=="/x/player/wbi/playurl"&&QUrlQuery(r.url()).queryItemValue("cid")=="302"?250:0;};
    Track video;video.id=5000000000301LL;video.hash="bilibili:BVgenerated";video.title="Generated video";video.duration=3000;
    Track local;local.file=syntheticWave(f.dir.filePath("audio"));local.hash="generated-local";local.duration=12000;local.id=f.library.insert(local);QVERIFY(local.id);
    f.facade.playOnline({video,local},video.id);QTRY_VERIFY(f.facade.playing()&&f.facade.seekable());f.facade.toggle();QTRY_VERIFY(!f.facade.playing());f.facade.seek(1000);QTRY_COMPARE(f.facade.position(),qint64(1000));
    QVERIFY(!bridge.selectPart(1));QVERIFY(!bridge.selectPart(99));
    QVERIFY(bridge.selectPart(2));QTRY_VERIFY(!f.facade.playing()&&f.facade.seekable());QCOMPARE(bridge.currentPart(),2);QCOMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("cid"),QString("302"));QCOMPARE(f.facade.current().title,QString("Part two"));f.facade.seek(1500);QTRY_COMPARE(f.facade.position(),qint64(1500));
    QVERIFY(bridge.selectPart(1));QTRY_VERIFY(!f.facade.playing()&&f.facade.seekable()&&f.facade.position()>=950);QCOMPARE(bridge.currentPart(),1);QCOMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("cid"),QString("301"));
    bridge.setQuality("80");QTRY_COMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("qn"),QString("80"));QTRY_VERIFY(!f.facade.playing()&&f.facade.seekable()&&f.facade.position()>=950);QCOMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("cid"),QString("301"));
    const auto viewsBefore=f.transport.paths.count("/x/web-interface/view");QVERIFY(bridge.selectPart(2));QVERIFY(bridge.selectPart(3));QTRY_VERIFY(!f.facade.playing()&&f.facade.seekable());QCOMPARE(bridge.currentPart(),3);QCOMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("cid"),QString("303"));QCOMPARE(f.facade.queue().items,(QVector<qint64>{video.id,local.id}));QCOMPARE(f.transport.paths.count("/x/web-interface/view"),viewsBefore+1);
    // Also let P2's media URL response arrive after a newer P3 response.
    delaySecondPart=true;QVERIFY(bridge.selectPart(2));QTRY_COMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("cid"),QString("302"));QVERIFY(bridge.selectPart(3));QTRY_VERIFY(!f.facade.playing()&&f.facade.seekable());QCOMPARE(bridge.currentPart(),3);QTest::qWait(350);QCOMPARE(bridge.currentPart(),3);QVERIFY(!f.facade.playing());QVERIFY(f.facade.seekable());QCOMPARE(QUrlQuery(f.transport.urls.last()).queryItemValue("cid"),QString("303"));
    QVERIFY(f.facade.save());const auto saved=f.library.setting("onlinePlayback").toObject();const auto metadata=saved.value("tracks").toArray().first().toObject().value("metadata").toObject();QCOMPARE(metadata.value("page").toInt(),3);QCOMPARE(metadata.value("cid").toString(),QString("303"));QVERIFY(metadata.value("partProgress").toObject().value("301").toDouble()>=950);QVERIFY(!metadata.contains("pages"));QVERIFY(!QJsonDocument(saved).toJson().contains(url.toUtf8()));
    f.facade.next();QCOMPARE(f.facade.current().id,local.id);QTRY_VERIFY(f.facade.playing());QVERIFY(f.facade.shutdown());
    QVERIFY(f.library.setSetting("onlinePlayback",saved));PlaybackFacade restored(&f.library,nullptr,true);Transport restoredTransport;BilibiliBridge restoredBridge(&f.library,&restored,nullptr,nullptr,&restoredTransport);restored.restore(true);QVERIFY(!restored.playing());QCOMPARE(restoredBridge.currentPart(),3);
    restoredTransport.responses={view,biliNav(),source};restored.toggle();QTRY_VERIFY(restored.playing()&&restored.seekable());QCOMPARE(QUrlQuery(restoredTransport.urls.last()).queryItemValue("cid"),QString("303"));QCOMPARE(restoredBridge.videoParts().size(),3);QVERIFY(restored.shutdown());
}

void PhaseGTests::videoPartsContinueNaturallyBeforeAdvancingTheQueue(){
    Fixture f;QVERIFY(f.ok);const auto file=syntheticVideo(f.dir.path());QVERIFY(!file.isEmpty());Track video;video.id=5000000000301LL;video.hash="bilibili:BVnatural";video.title="Generated video";video.duration=3000;QJsonArray pages;for(int i=1;i<=3;++i)pages.append(QJsonObject{{"cid",QString::number(300+i)},{"page",i},{"part",QString("Part %1").arg(i)},{"duration",3}});video.onlineMetadata={{"pages",pages},{"videoTitle",video.title}};
    Track local;local.file=syntheticWave(f.dir.filePath("audio"));local.hash="generated-local";local.duration=12000;local.id=f.library.insert(local);QVERIFY(local.id);
    QList<int> requested;connect(&f.facade,&PlaybackFacade::onlineSourceRequested,&f.facade,[&](qint64 id){requested.append(f.facade.current().onlineMetadata.value("page").toInt());f.facade.setOnlineSource(id,QUrl::fromLocalFile(file));});
    f.facade.setMode(Mode::Sequential);f.facade.playOnline({video,local},video.id);
    for(int page=1;page<=3;++page){QTRY_VERIFY(f.facade.playing()&&f.facade.seekable());QCOMPARE(f.facade.current().onlineMetadata.value("page").toInt(),page);f.facade.seek(2800);if(page<3)QTRY_COMPARE_WITH_TIMEOUT(f.facade.current().onlineMetadata.value("page").toInt(),page+1,5000);}
    QTRY_COMPARE_WITH_TIMEOUT(f.facade.current().id,local.id,5000);QTRY_VERIFY(f.facade.playing());QCOMPARE(requested,(QList<int>{1,2,3}));
    f.facade.setMode(Mode::SingleLoop);f.facade.previous();QTRY_VERIFY(f.facade.playing()&&f.facade.seekable());QCOMPARE(f.facade.current().onlineMetadata.value("page").toInt(),1);QVERIFY(f.facade.selectOnlinePart(3));QTRY_VERIFY(f.facade.playing()&&f.facade.seekable());f.facade.seek(2800);QTRY_COMPARE_WITH_TIMEOUT(f.facade.current().onlineMetadata.value("page").toInt(),1,5000);QCOMPARE(f.facade.current().id,video.id);QVERIFY(f.facade.shutdown());
}

void PhaseGTests::collectionSearchPagesWithinTheCurrentBiliFolderAndUser(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *engine=qmlEngine(shell.window());Transport transport;BilibiliBridge bili(&library,shell.facade(),nullptr,nullptr,&transport);
    engine->rootContext()->setContextProperty("bilibili",&bili);const auto cleanup=qScopeGuard([&]{engine->rootContext()->setContextProperty("bilibili",shell.bilibiliBridge());});
    QVERIFY(shell.appBridge()->navigate(4));auto *root=shell.window()->contentItem();QTRY_VERIFY(visual(root,"discoverPage"));auto *page=visual(root,"discoverPage");QVERIFY(QMetaObject::invokeMethod(page,"chooseSource",Q_ARG(QVariant,4)));
    transport.responses={biliPage(1,20,true)};QVERIFY(QMetaObject::invokeMethod(page,"showPage",Q_ARG(QVariant,"playlist"),Q_ARG(QVariant,"7"),Q_ARG(QVariant,false)));QTRY_COMPARE(bili.songs().size(),20);auto *list=visual(root,"onlineSongs");QVERIFY(list);
    transport.responses={biliPage(21,20,true),biliPage(41,5,false)};QVERIFY(QMetaObject::invokeMethod(page,"filterCollection",Q_ARG(QVariant,"Generated 45")));QTRY_COMPARE_WITH_TIMEOUT(bili.songs().size(),45,5000);QTRY_COMPARE(list->property("count").toInt(),1);QVERIFY(visual(root,"onlineSong-BVmock45"));QCOMPARE(bili.currentPlaylist().value("id").toString(),QString("7"));
    QVERIFY(QMetaObject::invokeMethod(page,"filterCollection",Q_ARG(QVariant,"")));QTRY_COMPARE(list->property("count").toInt(),45);QVERIFY(!bili.hasMore());
    transport.responses={biliNav(),R"({"code":0,"data":{"numPages":1,"result":[{"mid":99,"uname":"Synthetic user","videos":2}]}})"};QVERIFY(QMetaObject::invokeMethod(page,"showPage",Q_ARG(QVariant,"search-users"),Q_ARG(QVariant,"Mock"),Q_ARG(QVariant,false)));QTRY_COMPARE(bili.searchResults().size(),1);
    transport.responses={R"({"code":0,"data":{"list":{"vlist":[{"bvid":"BVone","title":"First upload","length":"2:03","author":"Synthetic user"},{"bvid":"BVtwo","title":"Second upload","length":"1:03","author":"Synthetic user"}]},"page":{"count":2}}})"};QVERIFY(QMetaObject::invokeMethod(page,"showPage",Q_ARG(QVariant,"user"),Q_ARG(QVariant,"99"),Q_ARG(QVariant,false)));QTRY_COMPARE(list->property("count").toInt(),2);QVERIFY(visual(root,"onlineCollectionSearchControl")->isVisible());QVERIFY(!visual(root,"pinOnlinePlaylist")->isVisible());
    QVERIFY(QMetaObject::invokeMethod(page,"filterCollection",Q_ARG(QVariant,"Second")));QTRY_COMPARE(list->property("count").toInt(),1);QCOMPARE(bili.songs().size(),2);QCOMPARE(bili.currentPlaylist().value("id").toString(),QString("99"));
    transport.responses={biliPage(1,20,true)};QVERIFY(QMetaObject::invokeMethod(page,"showPage",Q_ARG(QVariant,"playlist"),Q_ARG(QVariant,"8"),Q_ARG(QVariant,false)));QTRY_COMPARE(bili.songs().size(),20);transport.responses={R"({"code":-403})"};QVERIFY(QMetaObject::invokeMethod(page,"filterCollection",Q_ARG(QVariant,"absent")));QTRY_VERIFY(!bili.catalogLoading());QTest::qWait(150);const auto requests=transport.urls.size();QTest::qWait(350);QCOMPARE(transport.urls.size(),requests);
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}

void PhaseGTests::queueEditorAndEmptyFooterRespectControlsAndAudio(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));
    const auto wave=syntheticWave(dir.path());QVector<qint64> ids;
    for(int i=0;i<8;++i){Track t;t.file=wave;t.hash=QString("queue-ui-%1").arg(i);t.title=QString("Long title ").repeated(18);t.artist="Synthetic artist";t.duration=12000;ids.append(library.insert(t));QVERIFY(ids.last()>0);}
    const auto playlist=library.createPlaylist("Queue origin");QVERIFY(library.addToPlaylist(playlist,ids));
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();QVERIFY(QTest::qWaitForWindowExposed(w));
    auto *bar=visual(root,"playerBar"),*metadata=visual(root,"playerMetadata"),*panel=visual(root,"nextQueuePanel");QVERIFY(bar&&metadata&&panel);
    for(int width:{1240,1000,800,600,547,414}){
        w->resize(width,790);QTest::qWait(100);QVERIFY(!metadata->isVisible());QVERIFY(!panel->isVisible());QVERIFY(!visual(root,"compactQueueButton")->isVisible());
        if(width>=547){auto *volume=visual(root,"volumeSlider");QVERIFY(volume&&volume->isVisible());QVERIFY(qAbs(volume->mapToScene(QPointF(0,volume->height()/2)).y()-bar->mapToScene(QPointF(0,bar->height()/2)).y())<.5);}
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QVERIFY(!w->property("lyricsOpen").toBool());
    }
    w->resize(1000,790);QVERIFY(shell.libraryBridge()->openPlaylist(QString::number(playlist)));shell.libraryBridge()->play(QString::number(ids[0]));QTRY_VERIFY(shell.facade()->playing());QTRY_VERIFY(metadata->isVisible());
    auto *media=shell.facade()->backend()->media();QSignalSpy sources(media,&QMediaPlayer::sourceChanged),attempts(shell.facade(),&PlaybackFacade::playbackAttemptStarted);
    for(int width:{547,600,800,1000,1240}){w->resize(width,790);QTest::qWait(100);auto *hover=visual(root,"playerMetadataHover"),*elapsed=visual(root,"playbackState");QVERIFY(hover&&elapsed);QVERIFY(hover->mapRectToScene(QRectF(QPointF(),hover->size())).right()<=elapsed->mapToScene(QPointF()).x()-3.5);}
    auto capture=[](QQuickWindow *window,const QString &name){const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);window->grabWindow().save(QDir(folder).filePath(name+".png"));}};
    auto tap=[](QQuickItem *item,Qt::MouseButton button=Qt::LeftButton){if(!item||!item->isVisible())return false;QCoreApplication::processEvents();QTest::qWait(20);item->window()->grabWindow();const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();qInfo()<<"queue tap"<<item->objectName()<<item->isVisible()<<item->isEnabled()<<item->size()<<point;QTest::mouseMove(item->window(),point);QTest::mouseClick(item->window(),button,Qt::NoModifier,point);return true;};
    capture(w,"queue-footer-wide");panel->setProperty("panelExpanded",true);QTest::qWait(50);auto *row=visual(panel,"upcomingDelegate");QVERIFY(tap(row,Qt::RightButton));
    auto *context=panel->findChild<QObject *>("upcomingContextMenu");QVERIFY(context);QTRY_VERIFY(context->property("opened").toBool());
    auto *down=qobject_cast<QQuickItem *>(panel->findChild<QObject *>("upcomingMoveDown"));QVERIFY(down&&down->isEnabled());capture(w,"queue-context-wide");QVERIFY(tap(down));QTRY_COMPARE(shell.upcomingModel()->firstTrackKey(),QString::number(ids[2]));
    QTRY_VERIFY(!context->property("visible").toBool());QVERIFY(tap(visual(panel,"upcomingEditButton")));auto *edit=panel->findChild<QObject *>("upcomingEditMenu");QVERIFY(edit);QTRY_VERIFY(edit->property("opened").toBool());
    capture(w,"queue-edit-wide-before-multiselect");auto *begin=panel->findChild<QObject *>("upcomingBeginSelection");QSignalSpy triggered(begin,SIGNAL(triggered()));QVERIFY(triggered.isValid());QVERIFY(tap(qobject_cast<QQuickItem *>(begin)));capture(w,"queue-edit-wide-after-click");QTRY_COMPARE(triggered.size(),1);QTRY_VERIFY(panel->property("selectionMode").toBool());QTRY_VERIFY(!edit->property("visible").toBool());
    for(auto id:{ids[2],ids[1]}){auto *box=visual(panel,"upcomingSelect-"+QString::number(id));QVERIFY(box&&box->isVisible());QVERIFY(tap(box));QTRY_VERIFY(box->property("checked").toBool());}
    capture(w,"queue-multiselect-wide");QVERIFY(tap(visual(panel,"upcomingDeleteSelected")));QVERIFY(panel->property("selectionMode").toBool());QVERIFY(!edit->property("visible").toBool());
    QTRY_VERIFY(!shell.facade()->queue().items.contains(ids[1]));QVERIFY(!shell.facade()->queue().items.contains(ids[2]));QCOMPARE(shell.facade()->queue().items,(QVector<qint64>{ids[0],ids[3],ids[4],ids[5],ids[6],ids[7]}));QCOMPARE(shell.facade()->current().id,ids[0]);QVERIFY(shell.facade()->playing());
    const auto wanted=shell.upcomingModel()->firstTrackKey();QVERIFY(shell.upcomingModel()->locateTrack(wanted));QTRY_COMPARE(shell.libraryBridge()->playlistId(),QString::number(playlist));QTRY_COMPARE(shell.libraryModel()->selectedId(),wanted);
    panel->setProperty("panelExpanded",true);QVERIFY(tap(visual(panel,"upcomingEditButton")));QTRY_VERIFY(edit->property("opened").toBool());QVERIFY(tap(qobject_cast<QQuickItem *>(panel->findChild<QObject *>("upcomingClear"))));QTRY_COMPARE(shell.upcomingModel()->count(),0);QVERIFY(!metadata->isVisible());QVERIFY(!panel->isVisible());QCOMPARE(sources.size(),0);QCOMPARE(attempts.size(),0);QCOMPARE(shell.facade()->current().id,ids[0]);
    capture(w,"queue-empty-wide");w->resize(414,790);QTest::qWait(120);capture(w,"queue-empty-compact");QVERIFY(!visual(root,"compactQueueButton")->isVisible());QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());
    auto *mini=visual(root,"miniPlayer");QVERIFY(mini);QVERIFY(!visual(mini,"miniQueueButton")->isVisible());QVERIFY(QMetaObject::invokeMethod(mini,"openQueue"));auto *popup=mini->findChild<QObject *>("miniQueuePopup");QVERIFY(popup);QVERIFY(!popup->property("visible").toBool());capture(w,"queue-empty-mini");
    QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(!w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());
    QCOMPARE(sources.size(),0);QCOMPARE(attempts.size(),0);QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}
void PhaseGTests::queueMenusStayUsableInCompactAndMiniWindows(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto wave=syntheticWave(dir.path());QVector<qint64> ids;
    for(int i=0;i<6;++i){Track t;t.file=wave;t.hash=QString("mini-queue-edit-%1").arg(i);t.title=QString("Queued %1").arg(i);t.duration=12000;ids.append(library.insert(t));}
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();QVERIFY(QTest::qWaitForWindowExposed(w));w->resize(414,790);shell.facade()->play(ids,ids[0]);QTRY_COMPARE(shell.upcomingModel()->count(),6);
    auto capture=[](QQuickWindow *window,const QString &name){const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);window->grabWindow().save(QDir(folder).filePath(name+".png"));}};
    auto tap=[](QQuickItem *item,Qt::MouseButton button=Qt::LeftButton){if(!item||!item->isVisible())return false;QCoreApplication::processEvents();QTest::qWait(30);item->window()->grabWindow();const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();QTest::mouseMove(item->window(),point);QTest::mouseClick(item->window(),button,Qt::NoModifier,point);return true;};
    QTRY_COMPARE(visual(root,"playerBar")->property("compactProgress").toReal(),1.);
    auto *compact=visual(root,"compactQueueButton");QVERIFY(compact);QTRY_VERIFY(compact->isVisible());QVERIFY(tap(compact));auto *narrow=w->findChild<QObject *>("narrowQueuePopup");QVERIFY(narrow);QTRY_VERIFY(narrow->property("opened").toBool());
    auto *surface=narrow->property("contentItem").value<QQuickItem *>();QVERIFY(surface);auto *rows=visual(surface,"upcomingRows");QVERIFY(rows);auto *panel=rows->parentItem();QVERIFY(tap(visual(panel,"upcomingEditButton")));auto *editor=panel->findChild<QObject *>("upcomingEditMenu");QVERIFY(editor);QTRY_VERIFY(editor->property("opened").toBool());capture(w,"queue-edit-compact");
    QVERIFY(tap(qobject_cast<QQuickItem *>(panel->findChild<QObject *>("upcomingBeginSelection"))));QTRY_VERIFY(panel->property("selectionMode").toBool());QTRY_VERIFY(!editor->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(narrow,"close"));
    QSignalSpy sources(shell.facade()->backend()->media(),&QMediaPlayer::sourceChanged),attempts(shell.facade(),&PlaybackFacade::playbackAttemptStarted);
    QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());auto *mini=visual(root,"miniPlayer");QVERIFY(mini);
    const QPoint hover(200,28);QEnterEvent enter(hover,hover,w->mapToGlobal(hover));QCoreApplication::sendEvent(w,&enter);QTest::mouseMove(w,QPoint(199,28));QTest::mouseMove(w,hover);
    auto *queueButton=visual(mini,"miniQueueButton");QVERIFY(queueButton);QTRY_VERIFY(queueButton->isVisible());QVERIFY(tap(queueButton));auto *popup=mini->findChild<QObject *>("miniQueuePopup");QVERIFY(popup);QTRY_VERIFY(popup->property("opened").toBool());
    surface=popup->property("contentItem").value<QQuickItem *>();QVERIFY(surface);panel=visual(surface,"miniUpcomingPanel");QVERIFY(panel);auto *row=visual(panel,"upcomingDelegate");QVERIFY(tap(row,Qt::RightButton));auto *menu=panel->findChild<QObject *>("upcomingContextMenu");QVERIFY(menu);QTRY_VERIFY(menu->property("opened").toBool());
    auto *move=qobject_cast<QQuickItem *>(panel->findChild<QObject *>("upcomingMoveDown"));QVERIFY(move);QTest::mouseMove(move->window(),move->mapToScene(QPointF(move->width()/2,move->height()/2)).toPoint());QTest::qWait(200);QVERIFY(popup->property("visible").toBool());capture(surface->window(),"queue-context-mini");QVERIFY(tap(move));QTRY_COMPARE(shell.upcomingModel()->firstTrackKey(),QString::number(ids[2]));QTRY_VERIFY(!menu->property("visible").toBool());
    QVERIFY(tap(visual(panel,"upcomingEditButton")));editor=panel->findChild<QObject *>("upcomingEditMenu");QVERIFY(editor);QTRY_VERIFY(editor->property("opened").toBool());QVERIFY(tap(qobject_cast<QQuickItem *>(panel->findChild<QObject *>("upcomingBeginSelection"))));QTRY_VERIFY(panel->property("selectionMode").toBool());QTRY_VERIFY(!editor->property("visible").toBool());
    QVERIFY(tap(visual(panel,"upcomingSelectAll")));QCOMPARE(visual(panel,"upcomingSelectionCount")->property("text").toString(),QString("已选 5"));capture(surface->window(),"queue-selected-mini");
    QVERIFY(tap(visual(panel,"upcomingDeleteSelected")));QTRY_COMPARE(shell.upcomingModel()->count(),0);QTRY_VERIFY(!popup->property("visible").toBool());QVERIFY(!queueButton->isVisible());
    QCOMPARE(shell.facade()->current().id,ids[0]);QVERIFY(shell.facade()->playing());QCOMPARE(sources.size(),0);QCOMPARE(attempts.size(),0);
    QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(!w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());QVERIFY(!visual(root,"playerMetadata")->isVisible());QVERIFY(!visual(root,"compactQueueButton")->isVisible());
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}
void PhaseGTests::queueEditingOffersDirectActionsAndConsistentExits(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));const auto wave=syntheticWave(dir.path());QVector<qint64> ids;
    for(int i=0;i<6;++i){Track track;track.file=wave;track.hash=QString("queue-editor-exits-%1").arg(i);track.title=QString("Queue editor song %1").arg(i);track.duration=12000;ids.append(library.insert(track));QVERIFY(ids.last()>0);}
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();QVERIFY(QTest::qWaitForWindowExposed(w));shell.facade()->play(ids,ids[0]);QTRY_VERIFY(shell.facade()->playing());shell.facade()->toggle();QTRY_VERIFY(!shell.facade()->playing());QTRY_COMPARE(shell.upcomingModel()->count(),6);
    QSignalSpy sources(shell.facade()->backend()->media(),&QMediaPlayer::sourceChanged),attempts(shell.facade(),&PlaybackFacade::playbackAttemptStarted);
    auto tap=[](QQuickItem *item,Qt::MouseButton button=Qt::LeftButton){if(!item||!item->isVisible()||!item->isEnabled()||!QTest::qWaitForWindowExposed(item->window()))return false;QCoreApplication::processEvents();item->window()->grabWindow();const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();QCursor::setPos(item->window()->mapToGlobal(point));QTest::mouseMove(item->window(),point);QCoreApplication::processEvents();item->window()->grabWindow();const QPointer<QQuickWindow> window=item->window();const auto target=item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();QTest::mousePress(window,button,Qt::NoModifier,target);if(window)QTest::mouseRelease(window,button,Qt::NoModifier,target);return true;};
    auto selected=[](QQuickItem *panel){const auto value=panel->property("selectedKeys");return value.metaType()==QMetaType::fromType<QJSValue>()?value.value<QJSValue>().property("length").toInt():value.toStringList().size();};
    auto shot=[](QQuickWindow *window,const QString &name){const auto folder=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!folder.isEmpty()){QDir().mkpath(folder);window->grabWindow().save(QDir(folder).filePath(name+".png"));}};
    auto enterSelection=[&](QQuickItem *panel){auto *button=visual(panel,"upcomingEditButton");QSignalSpy clicked(button,SIGNAL(clicked()));if(!tap(button)){qInfo()<<"editor tap unavailable"<<panel->isVisible()<<panel->property("popupActive");return false;}auto *menu=panel->findChild<QObject *>("upcomingEditMenu");if(!menu||!QTest::qWaitFor([&]{return menu->property("opened").toBool();})){qInfo()<<"editor failed to open"<<clicked.size()<<panel->isVisible()<<panel->property("selectionMode")<<menu->property("visible");shot(panel->window(),"queue-editor-reentry-failure");return false;}if(!tap(qobject_cast<QQuickItem *>(panel->findChild<QObject *>("upcomingBeginSelection")))){qInfo()<<"selection menu tap unavailable";return false;}const bool ready=QTest::qWaitFor([&]{return panel->property("selectionMode").toBool()&&!menu->property("visible").toBool();});if(!ready){qInfo()<<"selection did not start"<<panel->isVisible()<<panel->property("selectionMode")<<menu->property("visible");shot(menu->property("contentItem").value<QQuickItem *>()->window(),"queue-editor-menu-miss");}return ready;};
    for(int width:{1000,414}){
        w->resize(width,790);QTRY_COMPARE(visual(root,"playerBar")->property("compactProgress").toReal(),width==414?1.:0.);
        auto *popup=w->findChild<QObject *>("narrowQueuePopup");QVERIFY(popup);QQuickItem *panel=nullptr;
        if(width==414){QVERIFY(tap(visual(root,"compactQueueButton")));QTRY_VERIFY(popup->property("opened").toBool());auto *surface=popup->property("contentItem").value<QQuickItem *>();QVERIFY(surface);panel=visual(surface,"upcomingRows")->parentItem();}
        else{panel=visual(root,"nextQueuePanel");QVERIFY(panel);panel->setProperty("panelExpanded",true);}
        QVERIFY(enterSelection(panel));auto *tools=visual(panel,"upcomingSelectionTools");auto *remove=visual(panel,"upcomingDeleteSelected");QVERIFY(tools&&tools->isVisible());QVERIFY(remove&&!remove->isEnabled());QCOMPARE(visual(panel,"upcomingEditButton")->property("text").toString(),QString("完成编辑"));
        QVERIFY(tap(visual(panel,"nextQueueHeading")));QCOMPARE(shell.facade()->queue().items,ids);QVERIFY(panel->property("selectionMode").toBool());
        auto *box=visual(panel,"upcomingSelect-"+QString::number(ids[1]));QVERIFY(box);QCOMPARE(visual(box,"upcomingSelectionIndicator")->width(),18.);QVERIFY(tap(box));QTRY_COMPARE(selected(panel),1);QVERIFY(remove->isEnabled());
        QVERIFY(tap(visual(panel,"upcomingEditButton")));QVERIFY(!panel->property("selectionMode").toBool());QCOMPARE(selected(panel),0);QVERIFY(!tools->isVisible());QVERIFY(width==414?popup->property("visible").toBool():panel->property("panelExpanded").toBool());
        QVERIFY(enterSelection(panel));QVERIFY(tap(visual(panel,"upcomingSelectAll")));QCOMPARE(selected(panel),5);QVERIFY(tap(visual(panel,"upcomingSelectAll")));QCOMPARE(selected(panel),0);
        QTest::keyClick(panel->window(),Qt::Key_Escape);QTRY_VERIFY(!panel->property("selectionMode").toBool());QVERIFY(width==414?popup->property("visible").toBool():panel->property("panelExpanded").toBool());
        QVERIFY(enterSelection(panel));QVERIFY(tap(visual(panel,"upcomingSelectAll")));shot(panel->window(),QString("queue-editor-tools-%1").arg(width));QVERIFY(tap(visual(panel,"upcomingToggle")));QTRY_VERIFY(width==414?!popup->property("visible").toBool():!panel->property("panelExpanded").toBool());QVERIFY(!panel->property("selectionMode").toBool());QCOMPARE(selected(panel),0);
        if(width==414){QVERIFY(tap(visual(root,"compactQueueButton")));QTRY_VERIFY(popup->property("opened").toBool());}else panel->setProperty("panelExpanded",true);
        QVERIFY(!panel->property("selectionMode").toBool());QTest::keyClick(panel->window(),Qt::Key_Escape);QTRY_VERIFY(width==414?!popup->property("visible").toBool():!panel->property("panelExpanded").toBool());
        if(width==414){QVERIFY(tap(visual(root,"compactQueueButton")));QTRY_VERIFY(popup->property("opened").toBool());}else panel->setProperty("panelExpanded",true);
        QVERIFY(enterSelection(panel));if(width==414){auto *surface=popup->property("contentItem").value<QQuickItem *>();QVERIFY(surface->mapToScene(QPointF(0,surface->height())).y()<=visual(root,"playerBar")->mapToScene(QPointF()).y()-8);}QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,QPoint(100,100));QTRY_VERIFY(width==414?!popup->property("visible").toBool():!panel->property("panelExpanded").toBool());QVERIFY(!panel->property("selectionMode").toBool());
    }
    QVERIFY(shell.settingsBridge()->setPreference("fontSize",20));QVERIFY(QMetaObject::invokeMethod(w,"toggleMiniPlayer"));QTRY_VERIFY(w->property("miniMode").toBool());QTRY_VERIFY(!w->property("miniSwitching").toBool());
    auto *mini=visual(root,"miniPlayer");QVERIFY(mini);const auto screen=w->screen()->geometry();w->setPosition(screen.left()+80,screen.top()+8);const QPoint hover(200,28);QEnterEvent enter(hover,hover,w->mapToGlobal(hover));QCoreApplication::sendEvent(w,&enter);QTest::mouseMove(w,hover);
    auto *queueButton=visual(mini,"miniQueueButton");QVERIFY(tap(queueButton));auto *popup=mini->findChild<QObject *>("miniQueuePopup");QVERIFY(popup);QTRY_VERIFY(popup->property("opened").toBool());auto *surface=popup->property("contentItem").value<QQuickItem *>();QVERIFY(surface);auto *panel=visual(surface,"miniUpcomingPanel");QVERIFY(panel);
    QVERIFY(enterSelection(panel));QTRY_COMPARE(visual(panel,"upcomingRows")->height(),280.);QCOMPARE(qRound(surface->height()),34+40+5*56+4);QVERIFY(surface->mapToGlobal(QPointF()).y()>=screen.top());
    auto *all=visual(panel,"upcomingSelectAll"),*count=visual(panel,"upcomingSelectionCount"),*remove=visual(panel,"upcomingDeleteSelected");QVERIFY(all&&count&&remove);QVERIFY(all->mapToScene(QPointF(all->width(),0)).x()<=count->mapToScene(QPointF()).x());
    QVERIFY(count->mapToScene(QPointF(count->width(),0)).x()<=remove->mapToScene(QPointF()).x());QVERIFY(tap(all));QCOMPARE(selected(panel),5);shot(surface->window(),"queue-editor-mini-font20");
    QTest::keyClick(surface->window(),Qt::Key_Escape);QTRY_VERIFY(!panel->property("selectionMode").toBool());QVERIFY(popup->property("visible").toBool());QCOMPARE(selected(panel),0);QTRY_COMPARE(visual(panel,"upcomingRows")->height(),280.);
    QVERIFY(tap(visual(panel,"upcomingEditButton")));auto *menu=panel->findChild<QObject *>("upcomingEditMenu");QVERIFY(menu);QTRY_VERIFY(menu->property("opened").toBool());QTest::keyClick(menu->property("contentItem").value<QQuickItem *>()->window(),Qt::Key_Escape);QTRY_VERIFY(!menu->property("visible").toBool());QVERIFY(popup->property("visible").toBool());
    QVERIFY(enterSelection(panel));QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,QPoint(20,28));QTRY_VERIFY(!popup->property("visible").toBool());QVERIFY(!panel->property("selectionMode").toBool());
    QVERIFY(shell.facade()->editUpcoming({ids[1]}));QTRY_COMPARE(shell.upcomingModel()->count(),1);QVERIFY(QMetaObject::invokeMethod(mini,"openQueue"));QTRY_VERIFY(popup->property("opened").toBool());QVERIFY(tap(visual(panel,"upcomingDelegate"),Qt::RightButton));auto *context=panel->findChild<QObject *>("upcomingContextMenu");QVERIFY(context);QTRY_VERIFY(context->property("opened").toBool());
    auto *menuSurface=context->property("contentItem").value<QQuickItem *>();QVERIFY(menuSurface);QVERIFY(menuSurface->window()!=surface->window());QVERIFY(context->property("height").toReal()>surface->height());shot(menuSurface->window(),"queue-editor-single-song-context");QTest::keyClick(menuSurface->window(),Qt::Key_Escape);QTRY_VERIFY(!context->property("visible").toBool());QVERIFY(popup->property("visible").toBool());QVERIFY(tap(visual(panel,"upcomingToggle")));QTRY_VERIFY(!popup->property("visible").toBool());
    QCOMPARE(shell.facade()->current().id,ids[0]);QVERIFY(!shell.facade()->playing());QCOMPARE(sources.size(),0);QCOMPARE(attempts.size(),0);QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}
void PhaseGTests::queuedCloudLocationUsesPinsAndReportsInaccessibleCollections(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));
    const auto wave=syntheticWave(dir.path());Track t;t.file=wave;t.hash="cloud-locate-local";t.title="Current local audio";t.duration=12000;const auto local=library.insert(t);QVERIFY(local>0);
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();QVERIFY(QTest::qWaitForWindowExposed(w));shell.facade()->play({local},local);QTRY_VERIFY(shell.facade()->playing());
    Transport transport;BilibiliBridge mock(&library,shell.facade(),shell.onlineArtworkCache(),nullptr,&transport);
    auto *context=QQmlEngine::contextForObject(w)->engine()->rootContext();auto *original=context->contextProperty("bilibili").value<QObject *>();context->setContextProperty("bilibili",&mock);const auto restore=qScopeGuard([&]{context->setContextProperty("bilibili",original);});
    QObject::disconnect(shell.libraryBridge(),&LibraryBridge::cloudPlaylistRequested,nullptr,nullptr);
    connect(shell.libraryBridge(),&LibraryBridge::cloudPlaylistRequested,&mock,[&](const QString &provider,const QString &id){if(provider=="bilibili")mock.openPlaylist(id);});
    connect(&mock,&BilibiliBridge::songsChanged,shell.libraryBridge(),[&]{shell.libraryBridge()->setCloudSongs("bilibili",mock.currentPlaylist(),mock.songs());});
    Track remote;remote.id=5000000000321LL;remote.hash="bilibili:BVqueueLocate";remote.title="Unavailable collection item";remote.onlineMetadata["queueOrigin"]=QJsonObject{{"provider","bilibili"},{"kind","playlist"},{"id","8123"}};
    QVERIFY(shell.facade()->enqueueTracks({remote},true));QTRY_COMPARE(shell.upcomingModel()->firstTrackKey(),QString::number(remote.id));
    const auto pinned=shell.libraryBridge()->pinCloudPlaylist({{"provider","bilibili"},{"id","8123"},{"name","Pinned source"}});QVERIFY(pinned!="0");
    QSignalSpy sources(shell.facade()->backend()->media(),&QMediaPlayer::sourceChanged),attempts(shell.facade(),&PlaybackFacade::playbackAttemptStarted);
    transport.responses={R"({"code":-403,"message":"收藏夹不可访问"})"};QVERIFY(shell.upcomingModel()->locateTrack(QString::number(remote.id)));QTRY_COMPARE(shell.libraryBridge()->playlistId(),pinned);QTRY_VERIFY(!mock.catalogLoading());
    QTRY_COMPARE(shell.feedbackBridge()->current().value("source").toString(),QString("Queue"));QCOMPARE(shell.appBridge()->page(),0);QVERIFY(shell.facade()->playing());
    QVERIFY(shell.libraryBridge()->deletePlaylist(pinned));transport.responses={R"({"code":-403,"message":"收藏夹不可访问"})"};QVERIFY(shell.upcomingModel()->locateTrack(QString::number(remote.id)));QTRY_COMPARE(shell.appBridge()->page(),4);
    auto *page=visual(w->contentItem(),"discoverPage");QVERIFY(page);QTRY_COMPARE(page->property("sourceIndex").toInt(),4);QTRY_VERIFY(page->property("queueLocateKey").toString().isEmpty());QCOMPARE(shell.facade()->current().id,local);QVERIFY(shell.facade()->playing());
    // A successful unpinned location must select the actual remote identifier,
    // and pinning the same collection must then select it in the sidebar page.
    transport.responses={R"({"code":0,"data":{"info":{"title":"Available folder","media_count":1},"has_more":false,"medias":[{"bvid":"BVqueueAvailable","title":"Located video","duration":12,"upper":{"name":"Mock author"},"cover":""}]}})"};
    mock.showPage("playlist","9456",true);QTRY_COMPARE(mock.songs().size(),1);const auto availableRow=mock.songs().first().toMap();
    Track available;available.id=availableRow.value("internalId").toLongLong();QVERIFY(available.id>0);available.hash="bilibili:BVqueueAvailable";available.title="Located video";
    available.onlineMetadata=QJsonObject::fromVariantMap(availableRow);available.onlineMetadata["queueOrigin"]=QJsonObject{{"provider","bilibili"},{"kind","playlist"},{"id","9456"}};
    QVERIFY(shell.facade()->enqueueTracks({available},true));QTRY_COMPARE(shell.upcomingModel()->firstTrackKey(),QString::number(available.id));
    QVERIFY(shell.upcomingModel()->locateTrack(QString::number(available.id)));QTRY_COMPARE(shell.appBridge()->page(),4);QTRY_COMPARE(page->property("selectedSongKey").toString(),QString("bilibili:BVqueueAvailable"));
    const auto availablePin=shell.libraryBridge()->pinCloudPlaylist({{"provider","bilibili"},{"id","9456"},{"name","Available folder"}});QVERIFY(availablePin!="0");
    QVERIFY(shell.upcomingModel()->locateTrack(QString::number(available.id)));QTRY_COMPARE(shell.libraryBridge()->playlistId(),availablePin);QTRY_COMPARE(shell.libraryModel()->selectedId(),QString::number(available.id));QCOMPARE(shell.appBridge()->page(),0);
    QCOMPARE(sources.size(),0);QCOMPARE(attempts.size(),0);QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}

void PhaseGTests::backgroundingRetainsArtworkWithoutDefaultFrames(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));
    QImage red(320,180,QImage::Format_RGB32);red.fill(Qt::red);const auto cover=dir.filePath("retained.png");QVERIFY(red.save(cover));
    const auto wave=syntheticWave(dir.path());Track local;local.file=wave;local.hash="retained-local";local.title="Retained cover";local.cover=cover;local.duration=12000;local.id=library.insert(local);QVERIFY(local.id);
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *facade=shell.facade();auto *root=w->contentItem();
    facade->play({local.id},local.id);QTRY_VERIFY(facade->playing());facade->toggle();QTRY_VERIFY(!facade->playing());
    auto retain=[&](QQuickItem *art){
        if(!art||!QTest::qWaitFor([&]{return art->property("status").toInt()==1;}))return false;
        const auto url=art->property("displayedUrl").toUrl();const auto slot=art->property("activeSlot").toInt();
        w->showMinimized();QTest::qWait(600);
        const bool unchanged=art->property("displayedUrl").toUrl()==url&&art->property("activeSlot").toInt()==slot&&art->property("status").toInt()==1;
        bool allReady=true;int frames=0;const auto observed=connect(w,&QQuickWindow::frameSwapped,w,[&]{++frames;allReady&=art->property("status").toInt()==1&&art->property("displayedUrl").toUrl()==url;});
        w->showNormal();const bool exposed=QTest::qWaitForWindowExposed(w);w->requestUpdate();const bool painted=QTest::qWaitFor([&]{return frames>0;});const auto shot=w->grabWindow();disconnect(observed);
        if(!unchanged||!exposed||!painted||!allReady||shot.isNull())return false;
        const auto center=art->mapToScene(QPointF(art->width()/2,art->height()/2));const auto ratio=qreal(shot.width())/w->width();return shot.pixelColor(qRound(center.x()*ratio),qRound(center.y()*ratio)).red()>220;
    };
    QVERIFY(retain(visual(root,"bottomCover")));
    QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_COMPARE(w->property("playerProgress").toReal(),qreal(1));QVERIFY(retain(visual(root,"currentArtwork")));
    QObject::disconnect(facade,&PlaybackFacade::onlineSourceRequested,nullptr,nullptr);
    QVERIFY(shell.settingsBridge()->setPreference("onlineVideoMode",1));Track online=local;online.id=5000000000789LL;online.hash="bilibili:BVretained";online.file.clear();facade->playOnline({online},online.id);
    QTRY_COMPARE(shell.playbackBridge()->trackId(),QString::number(online.id));QVERIFY(retain(visual(root,"currentArtwork")));
    QVERIFY(shell.settingsBridge()->setPreference("onlineVideoMode",0));auto *loading=visual(root,"onlineAudioCover");QVERIFY(loading);QTRY_VERIFY(loading->isVisible());QVERIFY(retain(loading));
    const auto loadingUrl=loading->property("displayedUrl").toUrl();loading->setVisible(false);QTest::qWait(50);QCOMPARE(loading->property("displayedUrl").toUrl(),loadingUrl);QCOMPARE(loading->property("status").toInt(),1);loading->setVisible(true);QVERIFY(retain(loading));QCOMPARE(loading->property("radius").toReal(),qreal(0));
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}

void PhaseGTests::startupVideoCoverAndClosingMetadataKeepTheirBounds(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto file=syntheticVideo(dir.path());QVERIFY(!file.isEmpty());QImage image(640,360,QImage::Format_RGB32);image.fill(Qt::red);const auto cover=dir.filePath("cover.png");QVERIFY(image.save(cover));const QString id="5000000000099";
    const QJsonObject track{{"id",id},{"hash","bilibili:BVstartup"},{"title",QString(100,'W')},{"artist",QString(70,'M')},{"duration",3000},{"cover",cover},{"metadata",QJsonObject{{"bvid","BVstartup"},{"cid","99"}}}};
    QVERIFY(library.setSetting("onlinePlayback",QJsonObject{{"tracks",QJsonArray{track}},{"current",id},{"position",0}}));QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *facade=shell.facade();disconnect(facade,&PlaybackFacade::onlineSourceRequested,shell.bilibiliBridge(),nullptr);connect(facade,&PlaybackFacade::onlineSourceRequested,facade,[&](qint64 key){facade->setOnlineSource(key,QUrl::fromLocalFile(file));});
    auto *w=shell.window();w->resize(1240,790);auto *root=w->contentItem();QVERIFY(!facade->playing());QTRY_VERIFY(shell.upcomingModel()->count()>0);QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_VERIFY(w->property("playerProgress").toReal()>=.999);auto *preview=visual(root,"startupVideoPreview");QVERIFY(preview);QTRY_VERIFY(preview->isVisible());QTRY_COMPARE(preview->property("status").toInt(),1);QCOMPARE(preview->property("radius").toReal(),qreal(0));QVERIFY(qAbs(preview->width()/preview->height()-16./9)<.001);
    auto *output=visual(root,"activeVideoOutput");QVERIFY(output);auto *sink=qobject_cast<QVideoSink *>(output->property("videoSink").value<QObject *>());QVERIFY(sink);QVERIFY(!sink->videoFrame().isValid());QTest::qWait(100);const auto pixelAt=[&](QQuickItem *item){const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));const auto shot=w->grabWindow();const auto scale=qreal(shot.width())/w->width();return shot.pixelColor(qRound(point.x()*scale),qRound(point.y()*scale));};QVERIFY(pixelAt(preview).red()>220);
    facade->toggle();QTRY_VERIFY_WITH_TIMEOUT(sink->videoFrame().isValid(),6000);QTRY_VERIFY(!preview->isVisible());QTest::qWait(80);const auto decoded=sink->videoFrame().toImage();QVERIFY(!decoded.isNull());QVERIFY(decoded.pixelColor(decoded.width()/2,decoded.height()/2).blue()>180);
    QTRY_COMPARE(output->opacity(),qreal(1));if(qEnvironmentVariable("QT_QUICK_BACKEND")=="software")qInfo()<<"Native video screen pixels NOT RUN on Qt Quick software backend; decoded frame, sink, visibility and cover handoff verified";else QTRY_VERIFY(pixelAt(output).blue()>180);facade->toggle();QTRY_VERIFY(!facade->playing());
    QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));bool sampled=false;for(int i=0;i<50&&w->property("metadataTravelling").toBool();++i){auto *flight=visual(root,"sharedTitleFlight");const auto progress=w->property("coverProgress").toReal();if(progress>.97&&progress<1){sampled=true;const auto right=flight->x()+flight->width()*flight->property("flightScale").toReal();const auto control=visual(root,"currentFavoriteButton")->mapToScene(QPointF()).x();QVERIFY2(right<control-8,qPrintable(QString("flight right %1; controls %2").arg(right).arg(control)));}QTest::qWait(10);}QVERIFY(sampled);QTRY_VERIFY(!w->property("metadataTravelling").toBool());
    QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_VERIFY(w->property("playerProgress").toReal()>=.999);QVERIFY(!preview->isVisible());QVERIFY(w->property("initialVideoPreviewId").toString().isEmpty());QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
    // A newly selected, paused video after an empty startup is not this special case.
    QTemporaryDir empty;Library fresh(empty.filePath("data"));QVERIFY(fresh.open());QmlShell next(&fresh,nullptr,true);QVERIFY(next.load());disconnect(next.facade(),&PlaybackFacade::onlineSourceRequested,next.bilibiliBridge(),nullptr);Track selected;selected.id=id.toLongLong();selected.hash="bilibili:BVnew";selected.title="New selection";next.facade()->playOnline({selected},selected.id);QTRY_VERIFY(next.upcomingModel()->count()>0);QVERIFY(QMetaObject::invokeMethod(next.window(),"showLyrics"));QTRY_VERIFY(visual(next.window()->contentItem(),"startupVideoPreview"));QVERIFY(!visual(next.window()->contentItem(),"startupVideoPreview")->isVisible());QVERIFY(next.shutdown());
}

void PhaseGTests::historyEntriesAndFallbackNavigationStayReachable(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());Track sample;sample.id=5000000000077LL;sample.hash="bilibili:BVhistoryUI";sample.title="Generated video history";sample.artist="Synthetic author";QVERIFY(library.recordPlaybackHistory(sample));sample.id=77;sample.hash="local-generated";sample.title="Generated local history";QVERIFY(library.recordPlaybackHistory(sample));QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();w->resize(1240,790);auto *root=w->contentItem();auto *settings=shell.settingsBridge();QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"settingsEntryMode-1"));QVERIFY(!visual(root,"openLegalNotices"));
    for(int width:{1240,414}){w->resize(width,790);QTest::qWait(380);for(auto key:{"settingsEntryMode","historyEntryMode"})for(int mode:{0,1}){auto *button=visual(root,QString(key)+"-"+QString::number(mode));QVERIFY(button&&button->isVisible());QVERIFY(QMetaObject::invokeMethod(button,"clicked"));QCOMPARE(settings->values().value(key).toInt(),mode);}}
    w->resize(1240,790);QTest::qWait(380);QTRY_VERIFY(visual(root,"sidebarSettingsButton")->isVisible());QVERIFY(visual(root,"sidebarHistoryButton")->x()<visual(root,"sidebarSettingsButton")->x());auto *editor=w->findChild<QObject *>("sidebarEditor");QVERIFY(editor);QCOMPARE(editor->property("sourceRows").value<QJSValue>().toVariant().toList().size(),5);
    auto rows=settings->values().value("sidebarSections").toList();for(auto &value:rows){auto row=value.toMap();row["visible"]=false;value=row;}QVERIFY(settings->saveSidebarSections(rows));QTRY_VERIFY(!visual(root,"sidebarSettingsButton")->isVisible());QVERIFY(!visual(root,"sidebarHistoryButton")->isVisible());
    auto *brand=visual(root,"brandFallbackArea");QVERIFY(brand&&brand->isEnabled());const auto point=brand->mapToScene(QPointF(brand->width()/2,brand->height()/2)).toPoint();QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,point);auto *menu=w->findChild<QObject *>("brandFallbackMenu");QVERIFY(menu);QTRY_VERIFY(menu->property("visible").toBool());auto *history=w->findChild<QObject *>("fallbackHistory"),*setting=w->findChild<QObject *>("fallbackSettings");QVERIFY(history&&setting);QVERIFY(QMetaObject::invokeMethod(history,"triggered"));QTRY_COMPARE(shell.appBridge()->page(),5);QTRY_VERIFY(visual(root,"historyList"));QVERIFY(QMetaObject::invokeMethod(menu,"close"));QTRY_VERIFY(!menu->property("visible").toBool());QTRY_COMPARE(visual(root,"historyList")->property("count").toInt(),2);
    const auto shots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!shots.isEmpty()){QDir().mkpath(shots);for(int width:{1240,414}){w->resize(width,790);QTest::qWait(380);w->grabWindow().save(QDir(shots).filePath(QString("history-%1.png").arg(width)));}w->resize(1240,790);QTest::qWait(380);}

    auto first=rows[0].toMap();first["visible"]=true;rows[0]=first;QVERIFY(settings->saveSidebarSections(rows));QTRY_COMPARE(w->property("sidebarSections").value<QJSValue>().toVariant().toList().size(),1);QTRY_VERIFY(!menu->property("visible").toBool());QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,point);QTest::qWait(120);QVERIFY(!menu->property("visible").toBool());
    QVERIFY(settings->setPreference("historyEntryMode",0));for(auto &value:rows){auto row=value.toMap();if(row.value("id").toInt()==6)row["visible"]=true;value=row;}QVERIFY(settings->saveSidebarSections(rows));QTRY_VERIFY(visual(root,"navigation6"));QVERIFY(QMetaObject::invokeMethod(visual(root,"navigation6"),"clicked"));QTRY_COMPARE(shell.appBridge()->page(),5);
    QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"openVersionLog"));QVERIFY(QMetaObject::invokeMethod(visual(root,"openVersionLog"),"clicked"));QTRY_VERIFY(visual(root,"openLegalNotices"));auto *legal=visual(root,"openLegalNotices"),*back=visual(root,"versionLogBack");QVERIFY(back);QVERIFY(legal->mapToScene(QPointF(legal->width(),0)).x()<back->mapToScene(QPointF()).x());
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}
void PhaseGTests::completedPlaybackHistoryPreservesSourcesAndDoesNotLogRestores(){
    Fixture f;QVERIFY(f.ok);Track local;local.file=syntheticWave(f.dir.filePath("audio"));local.hash="history-generated";local.title="Generated matched song";local.artist="Synthetic";local.duration=12000;local.id=f.library.insert(local);QVERIFY(local.id);
    f.facade.play({local.id},local.id);QTRY_VERIFY(f.facade.playing()&&f.facade.position()>250);Track cloud=local;cloud.id=1000000000042LL;cloud.hash="netease:42";cloud.onlineMetadata={{"id","42"},{"url","https://secret.example.test/signed"},{"Cookie","private"}};
    connect(&f.facade,&PlaybackFacade::onlineSourceRequested,&f.facade,[&](qint64 id){f.facade.setOnlineSource(id,QUrl::fromLocalFile(local.file));});f.facade.playOnline({cloud},cloud.id);QTRY_VERIFY(f.facade.playing()&&f.facade.position()>250);QCOMPARE(f.library.playbackHistory().size(),1);QVERIFY(f.facade.shutdown());
    auto rows=f.library.playbackHistory();QCOMPARE(rows.size(),2);QCOMPARE(rows[0].toMap().value("source").toString(),QString("云端 · 网易云音乐"));QCOMPARE(rows[1].toMap().value("source").toString(),QString("本地"));QCOMPARE(rows[0].toMap().value("endedAt").toString().size(),16);QVERIFY(!QJsonDocument(QJsonArray::fromVariantList(rows)).toJson().contains("secret.example"));QVERIFY(!rows[0].toMap().value("metadata").toMap().contains("Cookie"));QCOMPARE(f.library.playbackHistory("云端").size(),1);
    {PlaybackFacade restored(&f.library,nullptr,true);restored.restore(true);QVERIFY(!restored.playing());QVERIFY(restored.shutdown());}QCOMPARE(f.library.playbackHistory().size(),2);
    QStringList hashes{"qqmusic:test","qishui:test","kugou:test","bilibili:BVtest"};QStringList names{"QQ 音乐","汽水音乐","酷狗音乐","哔哩哔哩"};for(int i=0;i<hashes.size();++i){cloud.hash=hashes[i];QVERIFY(f.library.recordPlaybackHistory(cloud));QVERIFY(f.library.playbackHistory().first().toMap().value("source").toString().contains(names[i]));}QCOMPARE(f.library.playbackHistory().size(),6);QCOMPARE(f.library.playbackHistory("Generated").size(),6);
}
void PhaseGTests::videoAutoExpansionCanBeEnabledAfterEarlierPlayback(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto file=syntheticVideo(dir.path());QVERIFY(!file.isEmpty());Track video;video.file=file;video.hash="generated-expand";video.title="Generated video";video.duration=3000;const auto id=library.insert(video);QVERIFY(id);QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *f=shell.facade();
    f->play({id},id);QTRY_VERIFY(f->playing());QTest::qWait(150);QVERIFY(!w->property("lyricsOpen").toBool());QVERIFY(shell.settingsBridge()->setPreference("autoExpandVideo",true));f->toggle();QTRY_VERIFY(!f->playing());f->toggle();QTRY_VERIFY_WITH_TIMEOUT(w->property("lyricsOpen").toBool(),3000);QTRY_VERIFY(w->property("playerProgress").toReal()>=.999);
    QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QTRY_VERIFY(w->property("playerProgress").toReal()<=.001);f->play({id},id);QTRY_VERIFY(f->playing());QTest::qWait(120);QVERIFY(!w->property("lyricsOpen").toBool());QVERIFY(shell.settingsBridge()->setPreference("autoExpandVideo",false));QVERIFY(shell.settingsBridge()->setPreference("autoExpandVideo",true));f->toggle();QTRY_VERIFY(!f->playing());f->toggle();QTRY_VERIFY(w->property("lyricsOpen").toBool());QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}

void PhaseGTests::historyTableSortsAllRowsAndShowsPlatformLogos(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const QStringList sources{"local:","netease:","qqmusic:","qishui:","kugou:","bilibili:"};
    for(int i=0;i<125;++i){Track track;track.id=5000000000000LL+i+1;track.hash=sources[i%6]+QString::number(i);track.title=QString("Song %1").arg(124-i);track.artist=QString("Artist %1").arg(i%8);QVERIFY(library.recordPlaybackHistory(track));}
    const auto asc=library.playbackHistory("",0,"title",false),next=library.playbackHistory("",100,"title",false);QCOMPARE(asc.size(),100);QCOMPARE(next.size(),25);QCOMPARE(asc.first().toMap().value("title").toString(),QString("Song 0"));QCOMPARE(next.first().toMap().value("title").toString(),QString("Song 100"));QCOMPARE(library.playbackHistory("",0,"title",true).first().toMap().value("title").toString(),QString("Song 124"));QCOMPARE(library.playbackHistory("Song 124",0,"title",true).size(),1);
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();w->resize(1240,790);QVERIFY(shell.appBridge()->navigate(5));auto *root=w->contentItem();QTRY_VERIFY(visual(root,"historyPage"));auto *page=visual(root,"historyPage");auto visibleRows=[&]{const auto value=page->property("rows");return value.metaType()==QMetaType::fromType<QJSValue>()?value.value<QJSValue>().toVariant().toList():value.toList();};auto first=[&]{const auto rows=visibleRows();return rows.isEmpty()?QVariantMap{}:rows.first().toMap();};
    QTRY_COMPARE(visual(root,"historyList")->property("count").toInt(),100);QTest::qWait(120);
    auto click=[&](const QString &key){auto *heading=visual(root,"historyHeading-"+key);QVERIFY(heading);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,heading->mapToScene(QPointF(heading->width()/2,heading->height()/2)).toPoint());};
    click("title");QCOMPARE(first().value("title").toString(),QString("Song 0"));QVERIFY(!page->property("descending").toBool());click("title");QCOMPARE(first().value("title").toString(),QString("Song 124"));
    click("artist");QCOMPARE(first().value("artist").toString(),QString("Artist 0"));click("artist");QCOMPARE(first().value("artist").toString(),QString("Artist 7"));
    click("endedAt");const auto latest=first().value("endedUtc").toString();click("endedAt");QVERIFY(first().value("endedUtc").toString()<=latest);QCOMPARE(first().value("endedAt").toString().size(),16);
    click("platform");const auto firstPlatform=first().value("platform").toString();click("platform");QVERIFY(first().value("platform").toString()!=firstPlatform);
    for(const auto &name:QStringList{"网易云音乐","QQ 音乐","汽水音乐","酷狗音乐","哔哩哔哩"}){
        page->setProperty("query",name);QVERIFY(QMetaObject::invokeMethod(page,"reload"));QTRY_VERIFY(first().value("source").toString().contains(name));const auto historyId=first().value("historyId").toString();QTRY_VERIFY(visual(root,"historyPlatformLogo-"+historyId));auto *logo=visual(root,"historyPlatformLogo-"+historyId);QVERIFY(logo->isVisible());QTRY_COMPARE(logo->property("status").toInt(),1);QCOMPARE(logo->width(),qreal(24));if(name=="酷狗音乐")QCOMPARE(logo->property("sourceClipRect").toRectF(),QRectF(0,0,160,160));
    }
    page->setProperty("query",QString("本地"));QVERIFY(QMetaObject::invokeMethod(page,"reload"));QVERIFY(!first().value("cloud").toBool());
    page->setProperty("query",QString());QVERIFY(QMetaObject::invokeMethod(page,"sortBy",Q_ARG(QVariant,"title")));QTRY_COMPARE(first().value("title").toString(),QString("Song 0"));auto *list=visual(root,"historyList");list->setProperty("contentY",list->property("contentHeight").toReal()-list->height());QTRY_COMPARE(list->property("count").toInt(),125);QCOMPARE(visibleRows().last().toMap().value("title").toString(),QString("Song 124"));
    const auto shots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!shots.isEmpty()){QDir().mkpath(shots);QVERIFY(QMetaObject::invokeMethod(page,"reload"));QTest::qWait(120);w->grabWindow().save(QDir(shots).filePath("history-table-1240.png"));}
    w->resize(414,790);QTest::qWait(380);auto *viewport=visual(root,"historyTableViewport");QVERIFY(viewport);QCOMPARE(viewport->property("contentWidth").toReal(),viewport->width());QVERIFY(!viewport->property("interactive").toBool());QCOMPARE(viewport->property("contentX").toReal(),qreal(0));
    for(const auto &key:QStringList{"title","endedAt"}){auto *heading=visual(root,"historyHeading-"+key);QVERIFY(heading&&heading->isVisible());QVERIFY(heading->width()>0);QVERIFY(heading->mapToScene(QPointF(heading->width(),0)).x()<=w->width());}
    for(const auto &key:QStringList{"artist","platform"}){auto *heading=visual(root,"historyHeading-"+key);QVERIFY(heading&&!heading->isVisible());QCOMPARE(heading->width(),qreal(0));}if(!shots.isEmpty())w->grabWindow().save(QDir(shots).filePath("history-table-414.png"));
    page->setProperty("query",QString("not-in-generated-history"));QVERIFY(QMetaObject::invokeMethod(page,"reload"));QTRY_COMPARE(list->property("count").toInt(),0);auto *empty=visual(root,"historyEmptyState");QVERIFY(empty&&empty->isVisible());QVERIFY(qAbs(empty->mapToScene(QPointF(empty->width()/2,0)).x()-w->width()/2)<2);
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}

void PhaseGTests::onlineVideoVisibilityRequestsOnlyTheNeededTracksAndKeepsAudioCache(){
    Fixture f;QVERIFY(f.ok);QVector<QByteArray> files;QVector<QJsonObject> media;
    for(int i=0;i<2;++i){
        const auto path=f.dir.filePath(i==0?"picture.mp4":"sound.mp4");QProcess encoder;QStringList args{"-v","error","-f","lavfi","-i",i==0?"color=c=blue:s=160x90:r=10":"anullsrc=r=96000:cl=stereo","-t","12"};args.append(i==0?QStringList{"-c:v","libx264","-pix_fmt","yuv420p","-an"}:QStringList{"-c:a","flac","-strict","-2","-sample_fmt","s32","-vn"});args.append({"-movflags","+dash+global_sidx","-frag_duration","1000000","-y",path});encoder.start(toolPath("ffmpeg"),args);QVERIFY(encoder.waitForFinished(15000));QVERIFY2(encoder.exitCode()==0,encoder.readAllStandardError().constData());QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto bytes=file.readAll();QJsonObject info{{"id",i==0?80:30251},{"bandwidth",192000},{"codecs",i==0?"avc1.64000a":"fLaC"},{"width",160},{"height",90}};
        for(qint64 at=0;at+8<=bytes.size();){const auto size=qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData()+at));if(size<8)break;if(bytes.mid(at+4,4)=="sidx"){info["SegmentBase"]=QJsonObject{{"Initialization",QString("0-%1").arg(at-1)},{"indexRange",QString("%1-%2").arg(at).arg(at+size-1)}};break;}at+=size;}QVERIFY(info.contains("SegmentBase"));QByteArray padding(2*1024*1024+8,'\0');qToBigEndian(quint32(padding.size()),reinterpret_cast<uchar *>(padding.data()));padding.replace(4,4,"free");bytes+=padding;files.append(bytes);media.append(info);
    }
    QTcpServer origin;QVERIFY(origin.listen(QHostAddress::LocalHost));int fullRequests[2]={0,0};qint64 sent[2]={0,0};
    connect(&origin,&QTcpServer::newConnection,&origin,[&]{while(origin.hasPendingConnections()){
        auto *socket=origin.nextPendingConnection();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{if(socket->property("served").toBool()){socket->readAll();return;}socket->setProperty("served",true);const auto request=socket->readAll();const int index=request.contains("/picture")?0:1;const auto bytes=files[index];qint64 start=0,end=bytes.size()-1;bool ranged=false;for(const auto &line:request.split('\n'))if(line.toLower().startsWith("range: bytes=")){const auto range=line.trimmed().mid(13).split('-');start=range[0].toLongLong();if(range.size()>1&&!range[1].isEmpty())end=qMin(end,range[1].toLongLong());ranged=true;}QByteArray header=ranged?"HTTP/1.1 206 Partial Content\r\n":"HTTP/1.1 200 OK\r\n";header+="Content-Type: video/mp4\r\nAccept-Ranges: bytes\r\nContent-Length: "+QByteArray::number(end-start+1)+"\r\nConnection: close\r\n";if(ranged)header+="Content-Range: bytes "+QByteArray::number(start)+"-"+QByteArray::number(end)+"/"+QByteArray::number(bytes.size())+"\r\n";socket->write(header+"\r\n");if(ranged){socket->write(bytes.mid(start,end-start+1));socket->disconnectFromHost();return;}++fullRequests[index];socket->write(bytes.left(65536));sent[index]+=qMin<qint64>(65536,bytes.size());auto *timer=new QTimer(socket);timer->setInterval(20);connect(timer,&QTimer::timeout,socket,[&,socket,timer,bytes,index,offset=qint64(65536)]()mutable{if(socket->state()!=QAbstractSocket::ConnectedState){timer->stop();return;}const auto chunk=bytes.mid(offset,8192);socket->write(chunk);sent[index]+=chunk.size();offset+=chunk.size();if(offset>=bytes.size()){timer->stop();socket->disconnectFromHost();}});timer->start();});
    }});
    for(int i=0;i<2;++i){const auto url=QString("http://127.0.0.1:%1/%2").arg(origin.serverPort()).arg(i==0?"picture":"sound");media[i]["baseUrl"]=url;}
    BilibiliBridge bridge(&f.library,&f.facade,nullptr,nullptr,&f.transport);f.facade.setOnlineVideoVisible(false);QVideoSink sink;f.facade.setVideoSink(&sink);Track track;track.id=5000000000701LL;track.hash="bilibili:BVbackground";track.title="Generated DASH";track.duration=12000;
    const auto view=R"({"code":0,"data":{"pages":[{"cid":701,"page":1,"part":"Generated part","duration":12},{"cid":702,"page":2,"part":"Second part","duration":12}]}})";
    const auto source=QJsonDocument(QJsonObject{{"code",0},{"data",QJsonObject{{"timelength",12000},{"dash",QJsonObject{{"video",QJsonArray{media[0]}},{"audio",QJsonArray{media[1]}}}}}}}).toJson();f.transport.responses={view,biliNav(),source};QSignalSpy failures(&f.facade,&PlaybackFacade::playbackFailure);f.facade.playOnline({track},track.id);QTRY_VERIFY_WITH_TIMEOUT(f.facade.playing()&&f.facade.seekable(),8000);QCOMPARE(fullRequests[0],0);QCOMPARE(fullRequests[1],1);QVERIFY(!sink.videoFrame().isValid());QCOMPARE(f.facade.current().onlineMetadata.value("cid").toString(),QString("701"));
    auto onlinePlayer=[&](PlaybackFacade &facade){for(auto *player:facade.findChildren<QMediaPlayer *>())if(player!=facade.backend()->media())return player;return static_cast<QMediaPlayer *>(nullptr);};auto *initialPlayer=onlinePlayer(f.facade);QVERIFY(initialPlayer&&initialPlayer->hasAudio()&&!initialPlayer->hasVideo());QVERIFY(initialPlayer->audioBufferOutput());QSignalSpy decoded(initialPlayer->audioBufferOutput(),&QAudioBufferOutput::audioBufferReceived);QTRY_VERIFY(decoded.size()>0);QCOMPARE(decoded.first().first().value<QAudioBuffer>().format().sampleRate(),96000);
    f.facade.seek(1500);f.facade.toggle();QTRY_VERIFY(!f.facade.playing());const auto apiRequests=f.transport.urls.size();f.facade.setOnlineVideoVisible(true);QTRY_VERIFY_WITH_TIMEOUT(f.facade.seekable()&&f.facade.position()>=1500,8000);QVERIFY(!f.facade.playing());QTRY_VERIFY(sink.videoFrame().isValid());QCOMPARE(fullRequests[0],1);QCOMPARE(fullRequests[1],1);QCOMPARE(f.transport.urls.size(),apiRequests);
    f.facade.setOnlineVideoVisible(false);QTRY_VERIFY(f.facade.seekable()&&f.facade.position()>=1500);QVERIFY(!f.facade.playing());QTest::qWait(120);const auto stoppedBytes=sent[0],audioBytes=sent[1];QTest::qWait(180);QCOMPARE(sent[0],stoppedBytes);QVERIFY(sent[1]>audioBytes);QVERIFY(stoppedBytes<files[0].size());QCOMPARE(fullRequests[1],1);
    f.facade.toggle();QTRY_VERIFY(f.facade.playing());QVERIFY(f.library.setSetting("onlineVideoMode",1));f.facade.setOnlineVideoVisible(true);QTest::qWait(150);QCOMPARE(fullRequests[0],1);QVERIFY(f.facade.playing());QVERIFY(!f.facade.snapshot().videoFrameReady);QVERIFY(f.library.setSetting("onlineVideoMode",0));QTRY_COMPARE(fullRequests[0],2);QTRY_VERIFY_WITH_TIMEOUT(f.facade.playing()&&f.facade.seekable()&&sink.videoFrame().isValid(),8000);QVERIFY(f.facade.position()>=1500);QCOMPARE(fullRequests[1],1);QCOMPARE(f.facade.queue().items,QVector<qint64>{track.id});QCOMPARE(failures.size(),0);QCOMPARE(f.transport.urls.size(),apiRequests);
    const auto beforePartVideo=fullRequests[0];QVERIFY(f.library.setSetting("onlineVideoMode",1));f.transport.responses={view,source};QVERIFY(bridge.selectPart(2));QTRY_VERIFY_WITH_TIMEOUT(f.facade.playing()&&f.facade.seekable(),8000);QCOMPARE(bridge.currentPart(),2);QCOMPARE(f.facade.current().onlineMetadata.value("cid").toString(),QString("702"));QVERIFY(!onlinePlayer(f.facade)->hasVideo());QCOMPARE(fullRequests[0],beforePartVideo);QCOMPARE(fullRequests[1],2);QVERIFY(f.facade.shutdown());QVERIFY(f.library.setSetting("onlineVideoMode",0));
    const auto skip=qgetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");qunsetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");const auto restore=qScopeGuard([skip]{if(skip.isNull())qunsetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");else qputenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS",skip);});
    PlaybackFacade native(&f.library,nullptr,false);native.setVolume(0);QVideoSink nativeSink;native.setVideoSink(&nativeSink);QSignalSpy nativeErrors(&native,&PlaybackFacade::playbackFailure);auto nativeMedia=media;for(auto &entry:nativeMedia)entry["urls"]=QJsonArray{entry.value("baseUrl")};
    connect(&native,&PlaybackFacade::onlineSourceRequested,&native,[&](qint64 id){native.setOnlineDashSource(id,nativeMedia[0],nativeMedia[1]);});native.playOnline({track},track.id);QTRY_VERIFY_WITH_TIMEOUT(nativeSink.videoFrame().isValid()&&native.playing()&&native.seekable(),8000);QVERIFY(onlinePlayer(native)->audioOutput());native.seek(1500);const auto nativeAudioRequests=fullRequests[1];
    native.setOnlineVideoVisible(false);QTRY_VERIFY_WITH_TIMEOUT(native.playing()&&native.seekable()&&!onlinePlayer(native)->hasVideo(),8000);QVERIFY(native.position()>=1500);native.setOnlineVideoVisible(true);QTRY_VERIFY_WITH_TIMEOUT(native.playing()&&native.seekable()&&nativeSink.videoFrame().isValid(),8000);QCOMPARE(fullRequests[1],nativeAudioRequests);QCOMPARE(nativeErrors.size(),0);QVERIFY(native.shutdown());
}
void PhaseGTests::audioOnlyRejectsMergedSourcesAndKeepsTheVideoPreference(){
    Fixture f;QVERIFY(f.ok);BilibiliBridge bridge(&f.library,&f.facade,nullptr,nullptr,&f.transport);QVERIFY(f.library.setSetting("onlineVideoMode",1));Track track;track.id=5000000000702LL;track.hash="bilibili:BVmerged";track.title="Generated merged video";QSignalSpy failed(&f.facade,&PlaybackFacade::playbackFailure);
    f.transport.responses={R"({"code":0,"data":{"pages":[{"cid":702}]}})",biliNav(),R"({"code":0,"data":{"durl":[{"url":"https://not-requested.example.test/merged.mp4"}]}})"};f.facade.playOnline({track},track.id);QTRY_COMPARE(failed.size(),1);QVERIFY(!f.facade.playing());QVERIFY(bridge.status().contains("独立音轨"));QVERIFY(!f.facade.findChild<MediaPreload *>());QCOMPARE(f.library.setting("onlineVideoMode").toInt(),1);QVERIFY(f.facade.shutdown());
}
void PhaseGTests::onlineVideoPreferenceKeepsLocalVideoAndPartControls(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QmlShell shell(&library,nullptr,true);QVERIFY(shell.settingsBridge()->setPreference("onlineVideoMode",1));QVERIFY(shell.settingsBridge()->setPreference("autoExpandVideo",true));QVERIFY(!shell.settingsBridge()->setPreference("onlineVideoMode",2));QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();w->resize(1240,790);QTest::qWait(380);QVERIFY(shell.appBridge()->navigate(3));QTRY_VERIFY(visual(root,"onlineVideoMode-1"));QTRY_VERIFY(visual(root,"onlineVideoMode-1")->isVisible());w->resize(414,790);QTest::qWait(380);QVERIFY(!visual(root,"onlineVideoModeSetting")->isVisible());QVERIFY(visual(root,"onlineVideoMode-0")->isVisible());QVERIFY(visual(root,"onlineVideoMode-1")->isVisible());QVERIFY(QMetaObject::invokeMethod(visual(root,"onlineVideoMode-0"),"clicked"));QCOMPARE(shell.settingsBridge()->values().value("onlineVideoMode").toInt(),0);QVERIFY(QMetaObject::invokeMethod(visual(root,"onlineVideoMode-1"),"clicked"));QCOMPARE(shell.settingsBridge()->values().value("onlineVideoMode").toInt(),1);
    auto *facade=shell.facade();disconnect(facade,&PlaybackFacade::onlineSourceRequested,shell.bilibiliBridge(),nullptr);const auto wave=syntheticWave(dir.filePath("audio"));QImage thumbnail(400,300,QImage::Format_RGB32);thumbnail.fill(Qt::red);const auto coverPath=dir.filePath("audio-cover.png");QVERIFY(thumbnail.save(coverPath));connect(facade,&PlaybackFacade::onlineSourceRequested,facade,[&](qint64 id){facade->setOnlineSource(id,QUrl::fromLocalFile(wave));});Track cloud;cloud.id=5000000000703LL;cloud.hash="bilibili:BVaudioUI";cloud.title="Generated audio-only video";cloud.artist="Synthetic author";cloud.cover=coverPath;cloud.duration=12000;cloud.onlineMetadata={{"pages",QJsonArray{QJsonObject{{"cid","703"},{"page",1},{"part","One"},{"duration",12}},QJsonObject{{"cid","704"},{"page",2},{"part","Two"},{"duration",12}}}},{"cid","703"},{"page",1}};facade->playOnline({cloud},cloud.id);QTRY_VERIFY(facade->playing());QTRY_VERIFY(w->property("lyricsOpen").toBool());QTRY_VERIFY(w->property("playerProgress").toReal()>=.999);auto *picture=visual(root,"activeVideoOutput"),*cover=visual(root,"currentArtwork"),*surface=visual(root,"videoPlaybackSurface");QVERIFY(picture&&cover&&surface);QVERIFY(!picture->isVisible());QVERIFY(cover->isVisible());QVERIFY(qAbs(cover->width()/cover->height()-16./9)<.001);QVERIFY(!surface->isVisible());QCOMPARE(visual(root,"videoAudioMessage")->property("text").toString(),QString("云端视频音轨，请欣赏"));QTest::mouseMove(w,QPoint(180,180));QTRY_VERIFY(visual(root,"compactVideoPartsEntry")->isEnabled());
    w->showMinimized();QTRY_VERIFY(!facade->onlineVideoVisible());w->showNormal();QTRY_VERIFY(facade->onlineVideoVisible());
    const auto shots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!shots.isEmpty()){QDir().mkpath(shots);for(int width:{1240,414}){w->resize(width,790);QTest::qWait(380);w->grabWindow().save(QDir(shots).filePath(QString("audio-only-%1.png").arg(width)));}}
    QVERIFY(shell.bilibiliBridge()->selectPart(2));QTRY_COMPARE(shell.bilibiliBridge()->currentPart(),2);QTRY_VERIFY(facade->playing());QVERIFY(!picture->isVisible());
    QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QTRY_VERIFY(w->property("playerProgress").toReal()<=.001);QVERIFY(shell.appBridge()->navigate(4));QTRY_VERIFY(visual(root,"discoverPage"));QVERIFY(QMetaObject::invokeMethod(visual(root,"discoverPage"),"chooseSource",Q_ARG(QVariant,4)));auto *quality=w->findChild<QObject *>("onlineQuality-80"),*hires=w->findChild<QObject *>("bilibiliAudio-hires");QVERIFY(quality&&hires);QVERIFY(!quality->property("enabled").toBool());QVERIFY(hires->property("enabled").toBool());
    const auto video=syntheticVideo(dir.path());QVERIFY(!video.isEmpty());Track local;local.file=video;local.hash="local-still-video";local.title="Generated local video";local.duration=3000;const auto id=library.insert(local);QVERIFY(id);facade->play({id},id);QTRY_VERIFY(facade->playing());QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_VERIFY(picture->isVisible());auto *sink=qobject_cast<QVideoSink *>(picture->property("videoSink").value<QObject *>());QVERIFY(sink);QTRY_VERIFY_WITH_TIMEOUT(sink->videoFrame().isValid(),6000);QVERIFY(!cover->isVisible());QCOMPARE(library.setting("onlineVideoMode").toInt(),1);QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}

void PhaseGTests::localVideoAudioAndLyricsModesUseCoversAndSubtitles(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto video=syntheticVideo(dir.path());QVERIFY(!video.isEmpty());auto imported=Importer::importOne(library.root(),video);QVERIFY2(imported.error.isEmpty(),qPrintable(imported.error));QCOMPARE(imported.track.coverOrigin,QString("video-frame"));QImage frame(library.absolute(imported.track.cover));QVERIFY(!frame.isNull());QVERIFY(frame.pixelColor(frame.width()/2,frame.height()/2).blue()>220);
    imported.track.cover.clear();imported.track.coverOrigin="none";const auto id=library.insert(imported.track);QVERIFY(id);const auto repair=Importer::repairArtwork(library.root(),library.track(id));QCOMPARE(repair.origin,QString("video-frame"));QVERIFY2(library.applyArtworkRepair(repair),qPrintable(library.lastError()));QVERIFY(!library.track(id).cover.isEmpty());QVERIFY(library.saveLyrics(id,"[00:00.00]First local lyric\n[00:01.00]Second local lyric\n"));
    QImage sidecar(320,240,QImage::Format_RGB32);sidecar.fill(Qt::red);QVERIFY(sidecar.save(dir.filePath("generated.png")));Library other(dir.filePath("sidecar-data"));QVERIFY(other.open());const auto withCover=Importer::importOne(other.root(),video);QVERIFY(withCover.error.isEmpty());QCOMPARE(withCover.track.coverOrigin,QString("sidecar"));QImage preferred(other.absolute(withCover.track.cover));QVERIFY(preferred.pixelColor(20,20).red()>220);
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();w->resize(1240,790);auto *root=w->contentItem();auto *facade=shell.facade();auto *settings=shell.settingsBridge();facade->play({id},id);QTRY_VERIFY(facade->playing()&&facade->seekable());QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_VERIFY(w->property("playerProgress").toReal()>=.999);auto *picture=visual(root,"activeVideoOutput");QVERIFY(picture);auto *sink=qobject_cast<QVideoSink *>(picture->property("videoSink").value<QObject *>());QVERIFY(sink);QTRY_VERIFY(sink->videoFrame().isValid());facade->toggle();QTRY_VERIFY(!facade->playing());facade->seek(1500);QTRY_COMPARE(shell.lyricsModel()->currentIndex(),1);auto *caption=visual(root,"videoLyricCaption");QVERIFY(caption);QVERIFY(!caption->isVisible());
    QVERIFY(settings->setPreference("showLocalVideoLyrics",true));QTRY_VERIFY(caption->isVisible());QCOMPARE(caption->property("text").toString(),QString("Second local lyric"));auto *blur=visual(root,"videoLyricBlur"),*shade=visual(root,"videoLyricSoftwareShade");QVERIFY(blur&&shade);const bool softwareCaption=caption->property("software").toBool();QCOMPARE(blur->isVisible(),!softwareCaption);QCOMPARE(shade->isVisible(),softwareCaption);QTest::qWait(200);const auto screenshot=w->grabWindow();const auto ratio=qreal(screenshot.width())/w->width();auto sample=[&](qreal x){const auto p=caption->mapToScene(QPointF(caption->width()*x,caption->height()/2));return screenshot.pixelColor(qRound(p.x()*ratio),qRound(p.y()*ratio));};if(softwareCaption){auto shadeGrab=shade->grabToImage();QVERIFY(shadeGrab);QTRY_VERIFY(!shadeGrab->image().isNull());const auto shadeImage=shadeGrab->image();QVERIFY(shadeImage.pixelColor(shadeImage.width()/4,shadeImage.height()/2).alpha()>shadeImage.pixelColor(qMax(0,shadeImage.width()/100),shadeImage.height()/2).alpha()+8);qInfo()<<"Software lyric shade verified by alpha falloff; GPU Gaussian blur NOT RUN in this backend";}else QVERIFY2(sample(.25).blue()<sample(.01).blue()-8,qPrintable(QString("blur center %1 edge %2").arg(sample(.25).name()).arg(sample(.01).name())));
    const auto shots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!shots.isEmpty()){QDir().mkpath(shots);screenshot.save(QDir(shots).filePath("local-video-lyrics-1240.png"));}
    QVERIFY(settings->setPreference("localVideoMode",1));QTRY_VERIFY(!picture->isVisible());QTRY_COMPARE(facade->backend()->media()->activeVideoTrack(),-1);QVERIFY(!facade->playing());QVERIFY(facade->position()>=1450);auto *art=visual(root,"currentArtwork"),*message=visual(root,"videoAudioMessage");QVERIFY(art&&message);QVERIFY(art->isVisible());QVERIFY(qAbs(art->width()/art->height()-16./9)<.001);auto *lyrics=visual(root,"lyricsPanel");QVERIFY(lyrics&&lyrics->isVisible());QVERIFY(!message->isVisible());QVERIFY(!caption->isVisible());
    QVERIFY(settings->setPreference("showLocalVideoLyrics",false));QTRY_VERIFY(!lyrics->isVisible());QVERIFY(message->isVisible());QCOMPARE(message->property("text").toString(),QString("本地视频音轨，请欣赏"));QVERIFY(qAbs(art->x()+art->width()/2-art->parentItem()->width()/2)<1);
    if(!shots.isEmpty()){QTest::qWait(100);w->grabWindow().save(QDir(shots).filePath("local-audio-message-1240.png"));}
    QVERIFY(settings->setPreference("localVideoMode",0));QTRY_VERIFY(picture->isVisible());QTRY_COMPARE(facade->backend()->media()->activeVideoTrack(),0);QTRY_VERIFY(sink->videoFrame().isValid());QVERIFY(!facade->playing());QVERIFY(facade->position()>=1450);QVERIFY(!caption->isVisible());QVERIFY(visual(root,"videoLyricsSearch")->isVisible());QVERIFY(QMetaObject::invokeMethod(visual(root,"videoLyricsSearch"),"clicked"));QTRY_COMPARE(shell.lyricsSearchBridge()->targetTrackKey(),QString::number(id));auto *workspace=visual(root,"lyricsWorkspace");QVERIFY(workspace);QTRY_VERIFY(workspace->property("opened").toBool());QVERIFY(QMetaObject::invokeMethod(workspace,"close"));QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_VERIFY(w->property("playerProgress").toReal()>=.999);w->resize(414,790);QTest::qWait(380);QVERIFY(visual(root,"compactVideoLyricsSearch")->isVisible());QVERIFY(settings->setPreference("showLocalVideoLyrics",true));QTRY_VERIFY(caption->isVisible());if(!shots.isEmpty())w->grabWindow().save(QDir(shots).filePath("local-video-lyrics-414.png"));
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}

void PhaseGTests::regressionLayoutsUseAvailableTextSpace(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));QVERIFY(library.setSetting("autoExpandVideo",false));
    QVector<qint64> ids;
    for(int sample=0;sample<3;++sample){
        Track track;track.file=sample==2?syntheticVideo(dir.filePath("video")):syntheticWave(dir.filePath(QString("audio-%1").arg(sample)));QVERIFY(!track.file.isEmpty());
        track.title=sample==0?"MP3 With Embedded Cover":sample==1?QString::fromUtf8("短标题"):"Red 4x3 Video";track.artist=sample==1?QString::fromUtf8("阿短"):"Synthetic Artist Very Long Artist Name";track.hash=QString("layout-regression-%1").arg(sample);track.duration=sample==2?3000:12000;ids.append(library.insert(track));QVERIFY(ids.last());
    }
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();w->raise();w->requestActivate();QVERIFY(QTest::qWaitForWindowExposed(w));
    auto paint=[](QQuickWindow *window){
        // Each request must complete a fresh frame. An initially stale lineCount
        // must not satisfy an assertion before the new font has been painted.
        for(int frame=0;frame<2;++frame){QSignalSpy painted(window,&QQuickWindow::frameSwapped);window->requestUpdate();if(!QTest::qWaitFor([&]{return !painted.isEmpty();},3000))return false;}
        return !window->grabWindow().isNull();
    };
    const auto shots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!shots.isEmpty())QDir().mkpath(shots);
    for(int sample=0;sample<ids.size();++sample){
        shell.facade()->play({ids[sample]},ids[sample]);QTRY_VERIFY(shell.facade()->playing());shell.facade()->toggle();QTRY_VERIFY(!shell.facade()->playing());
        int fontRound=0;
        for(int size:{15,20,15}){
            if(size==15&&fontRound>0)QVERIFY(shell.settingsBridge()->resetFont());else QVERIFY(shell.settingsBridge()->setPreference("fontSize",size));++fontRound;
            for(int width:{547,600,800,1000,1240}){
                w->resize(width,790);auto *title=visual(root,"currentTitle"),*artist=visual(root,"currentArtist"),*button=visual(root,"currentFavoriteButton"),*center=visual(root,"playerCenter");QVERIFY(title&&artist&&button&&center);
                QTRY_COMPARE(title->property("font").value<QFont>().pixelSize(),size);QVERIFY(paint(w));
                if(!shots.isEmpty()&&(width==547||width==1000))w->grabWindow().save(QDir(shots).filePath(QString("metadata-%1-font%2-pass%3-%4.png").arg(sample).arg(size).arg(fontRound).arg(width)));
                const auto titleBox=title->mapRectToScene(QRectF(QPointF(),title->size()));const auto boundary=qMin(button->mapToScene(QPointF()).x()-22,center->mapToScene(QPointF()).x()-4)-7;
                QVERIFY(titleBox.right()<=boundary+.1);
                if(title->property("truncated").toBool())QVERIFY2(titleBox.right()>=boundary-2,qPrintable(QString("sample=%1 font=%2 width=%3 titleRight=%4 boundary=%5").arg(sample).arg(size).arg(width).arg(titleBox.right()).arg(boundary)));
                if(sample==1)QVERIFY2(!title->property("truncated").toBool(),qPrintable(QString("Three-character title font=%1 width=%2 available=%3 natural=%4").arg(size).arg(width).arg(title->width()).arg(title->implicitWidth())));
                QVERIFY(artist->mapToScene(QPointF(artist->width(),0)).x()<=center->mapToScene(QPointF()).x()-22+.1);
                auto *cover=visual(root,"bottomCover");QVERIFY(cover);QVERIFY(qAbs(cover->width()/cover->height()-(sample==2?4./3:1.))<.01);
            }
        }
    }
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}

void PhaseGTests::regressionHistoryAndSettingsWaitForPaintedFonts(){
    QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(library.setSetting("qml.reduceMotion",true));
    QmlShell shell(&library,nullptr,true);QVERIFY(shell.load());auto *w=shell.window();auto *root=w->contentItem();w->raise();w->requestActivate();QVERIFY(QTest::qWaitForWindowExposed(w));
    const auto shots=qEnvironmentVariable("RELAXER_PHASE_G_SCREENSHOTS");if(!shots.isEmpty())QDir().mkpath(shots);
    auto paint=[](QQuickWindow *window){
        // Each request must complete a fresh frame. An initially stale lineCount
        // must not satisfy an assertion before the new font has been painted.
        for(int frame=0;frame<2;++frame){QSignalSpy painted(window,&QQuickWindow::frameSwapped);window->update();if(!QTest::qWaitFor([&]{return !painted.isEmpty();},3000))return false;}
        return !window->grabWindow().isNull();
    };
    QVERIFY(shell.appBridge()->navigate(5));w->resize(414,790);QVERIFY(paint(w));auto *search=visual(root,"historySearchButton"),*heading=visual(root,"historyHeading");QVERIFY(search&&heading);QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,search->mapToScene(QPointF(search->width()/2,search->height()/2)).toPoint());
    QTRY_COMPARE(heading->opacity(),0.);if(!shots.isEmpty())w->grabWindow().save(QDir(shots).filePath("history-search-414.png"));QTest::keyClick(w,Qt::Key_Escape);QTRY_COMPARE(heading->opacity(),1.);
    QVERIFY(shell.appBridge()->navigate(3));QTest::qWait(160);
    std::function<QQuickItem *(QQuickItem *,const QString &)> textItem=[&](QQuickItem *item,const QString &text)->QQuickItem *{if(item->isVisible()&&item->property("text").toString()==text)return item;for(auto *child:item->childItems())if(auto *found=textItem(child,text))return found;return nullptr;};
    for(int size:{18,19,20})for(int width:{1240,1000}){
        QVERIFY(shell.settingsBridge()->setPreference("fontSize",size));w->resize(width,790);QVERIFY(paint(w));
        for(const auto &text:QStringList{QString::fromUtf8("主题与颜色"),QString::fromUtf8("界面与交互"),QString::fromUtf8("字体与应用区域"),QString::fromUtf8("音乐资料库侧栏")}){
            auto *label=textItem(root,text);QVERIFY(label);
            QTRY_COMPARE(label->property("font").value<QFont>().pixelSize(),qRound(label->property("basePixelSize").toReal()*size/15.));
            auto *scroll=visual(root,"settingsScroll");QVERIFY(scroll);auto *flick=scroll->property("contentItem").value<QQuickItem *>();QVERIFY(flick);const auto box=label->mapRectToItem(flick,QRectF(QPointF(),label->size()));
            if(box.top()<0||box.bottom()>flick->height())flick->setProperty("contentY",qBound(0.,flick->property("contentY").toReal()+box.top()-12,qMax(0.,flick->property("contentHeight").toReal()-flick->height())));
            QVERIFY(paint(w));
            if(!shots.isEmpty())w->grabWindow().save(QDir(shots).filePath(QString("settings-font%1-%2-%3.png").arg(size).arg(width).arg(text)));
            QCOMPARE(label->property("lineCount").toInt(),1);QVERIFY(!label->property("truncated").toBool());QVERIFY(label->property("contentWidth").toReal()<=label->width()+.1);
        }
    }
    QVERIFY2(shell.warnings().isEmpty(),qPrintable(shell.warnings().join('\n')));QVERIFY(shell.shutdown());
}

void PhaseGTests::displayedOnlineCoversAreReusedFromDiskAfterRestart() {
    QTemporaryDir data;
    QVERIFY(data.isValid());
    QByteArray images[2];
    for(int index=0;index<2;++index){
        QImage cover(index?240:180,180,QImage::Format_RGB32);
        cover.fill(index?Qt::blue:Qt::red);
        QBuffer output(&images[index]);
        QVERIFY(output.open(QIODevice::WriteOnly));
        QVERIFY(cover.save(&output,"PNG"));
    }

    QTcpServer origin;
    QVERIFY(origin.listen(QHostAddress::LocalHost));
    bool available=true;
    int requests=0;
    connect(&origin,&QTcpServer::newConnection,&origin,[&]{
        while(origin.hasPendingConnections()){
            auto *socket=origin.nextPendingConnection();
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket,request=QByteArray()]()mutable{
                request+=socket->readAll();
                if(!request.contains("\r\n\r\n"))return;
                ++requests;
                const auto bytes=available?images[request.startsWith("GET /video.png")?1:0]:QByteArray();
                const QByteArray status=available?"200 OK":"503 Service Unavailable";
                socket->write("HTTP/1.1 "+status+"\r\nContent-Type: image/png\r\nContent-Length: "+QByteArray::number(bytes.size())+"\r\nConnection: close\r\n\r\n"+bytes);
                socket->disconnectFromHost();
            });
        }
    });

    const QUrl urls[]={QUrl(QString("http://127.0.0.1:%1/song.png?size=small").arg(origin.serverPort())),
                       QUrl(QString("http://127.0.0.1:%1/video.png").arg(origin.serverPort()))};
    for(int run=0;run<2;++run){
        Library library(data.filePath("data"));
        QVERIFY(library.open());
        QmlShell shell(&library,nullptr,true);
        QVERIFY(shell.load());
        QQmlComponent component(qmlEngine(shell.window()),QUrl("qrc:/qml/components/Artwork.qml"));
        QVERIFY2(component.isReady(),qPrintable(component.errorString()));
        for(int index=0;index<2;++index){
            // Changing the decode size prevents an in-memory pixmap from
            // satisfying the second shell's image request.
            std::unique_ptr<QObject> object(component.createWithInitialProperties({
                {"file",urls[index]},{"width",120},{"height",90},{"videoCover",index==1},
                {"resolution",run==0?96:144},{"animated",false},
            }));
            auto *artwork=qobject_cast<QQuickItem *>(object.get());
            QVERIFY2(artwork,qPrintable(component.errorString()));
            artwork->setParentItem(shell.window()->contentItem());
            QTRY_COMPARE(artwork->property("status").toInt(),1);
            const auto key=QCryptographicHash::hash(urls[index].toEncoded(),QCryptographicHash::Sha256).toHex();
            QFile stored(QDir(DiskCache::online(&library)).filePath("artwork-"+key+".img"));
            QVERIFY(stored.open(QIODevice::ReadOnly));
            QCOMPARE(stored.readAll(),images[index]);
        }
        if(run==0){
            QVERIFY(requests>=2);
            available=false;
        }
        QVERIFY(shell.shutdown());
        if(run==0)requests=0;
        else QCOMPARE(requests,0);
    }
}

QTEST_MAIN(PhaseGTests)
#include "phase-g-tests.moc"
