#include "ui-bridge/qml-shell.h"
#include <QCommandLineParser>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <QSaveFile>
#include <QImage>
#include <QSignalSpy>
#include <QtTest>
#include <cmath>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif
using namespace relaxer;
namespace {
bool saveJson(const QString &path,const QJsonObject &object) {
    QSaveFile file(path); const auto bytes=QJsonDocument(object).toJson();
    return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&file.commit();
}
QQuickItem *visual(QQuickItem *item,const QString &name) {
    if(item->objectName()==name)return item;
    for(auto *child:item->childItems())if(auto *found=visual(child,name))return found;
    return nullptr;
}
int delegates(QQuickItem *item,const QString &name="trackDelegate") {
    int result=item->objectName()==name?1:0;
    for(auto *child:item->childItems())result+=delegates(child,name);
    return result;
}
double frame(QQuickWindow *window) {
    QSignalSpy swapped(window,&QQuickWindow::frameSwapped); QElapsedTimer clock;clock.start();
    window->update();if(swapped.isEmpty()&&!swapped.wait(5000))return -1;
    return clock.nsecsElapsed()/1000000.;
}
QJsonObject memory() {
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);
    if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),sizeof(counters)))
        return {{"workingSetBytes",double(counters.WorkingSetSize)},{"privateBytes",double(counters.PrivateUsage)},
                {"peakWorkingSetBytes",double(counters.PeakWorkingSetSize)}};
#endif
    return {{"available",false}};
}
bool prepare(const QString &root,int count) {
    if(QFileInfo::exists(root))return false;
    Library library(root);if(!library.open())return false;
    QFile audio(library.absolute("Music/fixture.wav"));if(!audio.open(QIODevice::WriteOnly))return false;
    QDataStream out(&audio);out.setByteOrder(QDataStream::LittleEndian);
    const int samples=8000*10;
    out.writeRawData("RIFF",4);out<<quint32(samples*2+36);out.writeRawData("WAVEfmt ",8);
    out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);
    out.writeRawData("data",4);out<<quint32(samples*2);
    for(int i=0;i<samples;++i)out<<qint16(1000*(std::sin(i*2*3.141592653589793*220/8000)+std::sin(i*2*3.141592653589793*330/8000)));
    audio.close();if(out.status()!=QDataStream::Ok)return false;
    auto db=QSqlDatabase::database(QSqlDatabase::connectionNames().last());if(!db.transaction())return false;
    library.deferChanges(true);
    for(int i=0;i<count;++i) {
        Track track;track.hash=QString("perf-%1").arg(i);track.file="Music/fixture.wav";
        track.title=QString("歌曲 日本語 テスト Track %1 — 长标题用于桌面省略测试").arg(i,5,10,QChar('0'));
        track.artist=i%2?"Artist B":"Artist A";track.album=QString("Album %1").arg(i%11);track.duration=10000;
        if(!library.insert(track)){db.rollback();library.deferChanges(false);return false;}
    }
    const bool committed=db.commit();library.deferChanges(false);
    const auto list=library.createPlaylist("合成测试 Synthetic playlist");
    QVector<qint64> subset;const auto tracks=library.tracks();for(int i=0;i<qMin(100,int(tracks.size()));++i)subset.append(tracks[i].id);
    if(list&&!subset.isEmpty()&&!library.addToPlaylist(list,subset))return false;

    if(!library.setSetting("volume",0))return false;
    if(count){
        QByteArray lyrics;for(int i=0;i<80;++i)lyrics+=QString("[%1:%2.00]Hello moon %3 中文 日本語 テスト English\n").arg(i/60,2,10,QChar('0')).arg(i%60,2,10,QChar('0')).arg(i).toUtf8();
        QImage image(256,256,QImage::Format_RGB32);image.fill(QColor("#b62e50"));
        const auto art=QDir(root).filePath("fixture-cover.png");if(!image.save(art))return false;
        if(!library.saveLyrics(tracks.last().id,lyrics)||!library.updateArtwork(tracks.last().id,art))return false;
    }
    return committed&&saveJson(QDir(root).filePath(".c4c5-fixture.json"),{{"synthetic",true},{"count",count}});
}
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    QCommandLineParser parser;parser.addHelpOption();
    parser.addOptions({{"data","Owned synthetic fixture root","path"},{"count","0/1000/10000","number"},
        {"output","Result JSON","file"},{"prepare","Create a new isolated fixture then exit"},{"screenshots","Save three window sizes","directory"}});
    parser.process(app);bool valid=false;const int count=parser.value("count").toInt(&valid);
    const auto root=parser.value("data");
    if(!valid||(count!=0&&count!=1000&&count!=10000)||root.isEmpty())return 64;
    if(parser.isSet("prepare"))return prepare(root,count)?0:65;
    QFile marker(QDir(root).filePath(".c4c5-fixture.json"));if(!marker.open(QIODevice::ReadOnly))return 66;
    const auto fixture=QJsonDocument::fromJson(marker.readAll()).object();
    if(!fixture["synthetic"].toBool()||fixture["count"].toInt(-1)!=count||!parser.isSet("output"))return 66;
    QElapsedTimer clock;clock.start();Library library(root);if(!library.open()||library.trackCount()!=count)return 2;
    const double openMs=clock.nsecsElapsed()/1000000.;clock.restart();
    QmlShell shell(&library,nullptr,true);const double shellInitMs=clock.nsecsElapsed()/1000000.;
    if(!shell.load())return 10;
    auto *window=shell.window();QSignalSpy first(window,&QQuickWindow::frameSwapped);
    if(!QTest::qWaitForWindowExposed(window,5000)||(first.isEmpty()&&!first.wait(5000)))return 12;
    const double firstFrameMs=clock.nsecsElapsed()/1000000.;
    auto *list=visual(window->contentItem(),"trackListView");if(!list||list->property("count").toInt()!=count)return 13;
    int maxDelegates=delegates(window->contentItem());
    if((count>0&&maxDelegates<1)||maxDelegates>100)return 14;
    QJsonObject result{{"tracks",count},{"databaseOpenMs",openMs},{"shellAndModelInitMs",shellInitMs},
        {"firstFrameFromShellInitMs",firstFrameMs},{"firstScreenVisibleDelegates",maxDelegates},
        {"graphicsApi",int(window->rendererInterface()->graphicsApi())},{"qpa",QGuiApplication::platformName()},
        {"quickBackend",qEnvironmentVariable("QT_QUICK_BACKEND")},{"windowWidth",window->width()},{"windowHeight",window->height()},
        {"devicePixelRatio",window->devicePixelRatio()},{"cachePolicy","Fresh process; OS caches not flushed; synthetic fixture prepared immediately beforehand"}};
    auto *model=shell.libraryModel();if(count)model->setSelectedId(model->idAt(count/2));const auto selection=model->selectedId();
    clock.restart();model->setFilterText("Artist A");result["filterModelMs"]=clock.nsecsElapsed()/1000000.;
    if(frame(window)<0||model->count()!=(count+1)/2)return 15;
    result["filterUntilNextFrameMs"]=clock.nsecsElapsed()/1000000.;
    model->setFilterText({});clock.restart();model->setSortKey(1);result["sortModelMs"]=clock.nsecsElapsed()/1000000.;
    if(frame(window)<0)return 16;result["sortUntilNextFrameMs"]=clock.nsecsElapsed()/1000000.;
    QJsonArray navigation;
    for(int i=0;i<12;++i) {
        shell.appBridge()->navigate(i%3);const auto ms=frame(window);if(ms<0)return 17;navigation.append(ms);
    }
    shell.appBridge()->navigate(0);if(frame(window)<0||model->selectedId()!=selection)return 18;
    QJsonArray scroll;
    const auto maximum=qMax(0.,list->property("contentHeight").toDouble()-list->height());
    for(int i=0;i<40;++i) {
        list->setProperty("contentY",maximum*i/39);const auto ms=frame(window);if(ms<0)return 19;scroll.append(ms);
        maxDelegates=qMax(maxDelegates,delegates(window->contentItem()));
    }
    result["navigationNextFrameMs"]=navigation;result["programmaticScrollNextFrameMs"]=scroll;
    result["maxVisibleAndCachedDelegates"]=maxDelegates;if(maxDelegates>100)return 20;
    QTest::qWait(500);result["steadyMemoryBeforeLegacyBenchmark"]=memory();
    clock.restart();for(int i=0;i<20;++i)if(library.trackCount()!=count)return 21;
    result["countQueryMeanNs"]=double(clock.nsecsElapsed()/20);
    clock.restart();qsizetype payload=0;
    for(int i=0;i<5;++i) {
        const auto tracks=library.tracks();if(tracks.size()!=count)return 22;
        payload=tracks.capacity()*qsizetype(sizeof(Track));
        for(const auto &track:tracks)for(const auto &s:{track.hash,track.file,track.title,track.artist,track.album,track.cover,track.lyrics,track.coverOrigin,track.importedAt})payload+=s.capacity()*qsizetype(sizeof(QChar));
    }
    result["legacyMaterializedCountMeanNs"]=double(clock.nsecsElapsed()/5);
    result["legacyRetainedPayloadLowerBoundBytes"]=double(payload);result["newCountMaterializedTracks"]=0;
    if(count>0) {
        clock.restart();shell.libraryBridge()->playAll();result["playCommandMs"]=clock.nsecsElapsed()/1000000.;
        QElapsedTimer ready;ready.start();
        while((shell.queueModel()->count()!=count||!shell.playbackBridge()->playing())&&ready.elapsed()<5000)QTest::qWait(5);
        if(shell.queueModel()->count()!=count||!shell.playbackBridge()->playing())return 25;
        result["queueAndPlaybackReadyMs"]=clock.nsecsElapsed()/1000000.;
        QSignalSpy queueResets(shell.queueModel(),&QAbstractItemModel::modelReset);
        QSignalSpy trackResets(shell.libraryModel(),&QAbstractItemModel::modelReset);
        QSignalSpy playlistResets(shell.playlistModel(),&QAbstractItemModel::modelReset);
        QTest::qWait(1100);
        if(shell.facade()->position()<500||!queueResets.isEmpty()||!trackResets.isEmpty()||!playlistResets.isEmpty())return 26;
        result["playingPositionMs"]=double(shell.facade()->position());result["queueCountWhilePlaying"]=shell.queueModel()->count();
        result["modelResetsDuringPlayback"]=queueResets.size()+trackResets.size()+playlistResets.size();
        result["memoryWhilePlaying"]=memory();
        list->setProperty("contentY",0.);if(frame(window)<0)return 19;
    }
    if(parser.isSet("screenshots")) {
        const auto directory=parser.value("screenshots");QDir().mkpath(directory);
        for(const auto &size:{QSize(900,600),QSize(1180,760),QSize(1600,900)}) {
            window->resize(size);if(frame(window)<0||!window->grabWindow().save(QDir(directory).filePath(QString("%1x%2.png").arg(size.width()).arg(size.height()))))return 23;
        }
        shell.appBridge()->navigate(2);if(frame(window)<0||!window->grabWindow().save(QDir(directory).filePath("queue.png")))return 23;
        shell.appBridge()->navigate(1);if(frame(window)<0||!window->grabWindow().save(QDir(directory).filePath("playlists.png")))return 23;
        const auto lists=library.playlists();if(!lists.isEmpty()) {
            clock.restart();if(!shell.libraryBridge()->openPlaylist(QString::number(lists.first().id)))return 27;
            shell.appBridge()->navigate(0);if(frame(window)<0)return 27;
            result["playlistOpenUntilNextFrameMs"]=clock.nsecsElapsed()/1000000.;
            result["playlistVisibleTracks"]=shell.libraryModel()->count();
            if(!window->grabWindow().save(QDir(directory).filePath("playlist-tracks.png")))return 23;
        }
    }

    // Exercise the actual lyric surface, not a blank startup window.
    if(!shell.libraryBridge()->openPlaylist("0"))return 30;
    shell.appBridge()->navigate(0);clock.restart();
    if(!QMetaObject::invokeMethod(window,"showLyrics")||frame(window)<0)return 30;
    result["lyricsPageUntilNextFrameMs"]=clock.nsecsElapsed()/1000000.;
    if(count){
        QElapsedTimer wait;wait.start();while(shell.lyricsModel()->count()!=80&&wait.elapsed()<5000)QTest::qWait(5);
        if(shell.lyricsModel()->count()!=80)return 31;
    }
    auto *lyrics=visual(window->contentItem(),"lyricsList");if(!lyrics||lyrics->width()<250)return 32;
    result["normalLyricsRows"]=shell.lyricsModel()->count();result["lyricsViewportWidth"]=lyrics->width();
    QJsonArray fullscreen;const auto beforeBounds=window->geometry();const auto queueBefore=shell.facade()->queue().items;
    for(int i=0;i<12;++i){clock.restart();QMetaObject::invokeMethod(window,"toggleFullscreen");if(frame(window)<0)return 33;fullscreen.append(clock.nsecsElapsed()/1000000.);}
    QTest::qWait(100);if(window->geometry()!=beforeBounds||shell.facade()->queue().items!=queueBefore)return 34;
    result["fullscreenNextFrameMs"]=fullscreen;result["fullscreenGeometryRestored"]=true;
    const auto shots=parser.value("screenshots");
    if(!shots.isEmpty())for(const auto &size:{QSize(900,600),QSize(1180,760),QSize(1600,900)}){
        window->resize(size);if(frame(window)<0||!window->grabWindow().save(QDir(shots).filePath(QString("lyrics-%1x%2.png").arg(size.width()).arg(size.height()))))return 35;
    }
    if(count){
        shell.playbackBridge()->pause();QSignalSpy lyricResets(shell.lyricsModel(),&QAbstractItemModel::modelReset);
        QByteArray longDocument;for(int i=0;i<5000;++i)longDocument+=QString("[%1:%2.00]Long lyric %3 中文 日本語 English — wrapped text for the real lyric page\n").arg(i/60,2,10,QChar('0')).arg(i%60,2,10,QChar('0')).arg(i).toUtf8();
        clock.restart();if(!library.saveLyrics(shell.facade()->current().id,longDocument))return 36;
        QElapsedTimer wait;wait.start();while(shell.lyricsModel()->count()!=5000&&wait.elapsed()<5000)QTest::qWait(5);
        if(shell.lyricsModel()->count()!=5000||frame(window)<0)return 37;
        result["longLyricsRows"]=5000;result["longLyricsReadModelFrameMs"]=clock.nsecsElapsed()/1000000.;
        const int resetsBefore=lyricResets.size();QJsonArray lyricScroll;
        const auto maximum=qMax(0.,lyrics->property("contentHeight").toDouble()-lyrics->height());int maxLyrics=0;
        for(int i=0;i<40;++i){lyrics->setProperty("contentY",maximum*i/39);const auto ms=frame(window);if(ms<0)return 38;lyricScroll.append(ms);maxLyrics=qMax(maxLyrics,delegates(window->contentItem(),"lyricDelegate"));}
        if(maxLyrics>100)return 39;result["longLyricsMaxDelegates"]=maxLyrics;result["lyricsScrollNextFrameMs"]=lyricScroll;
        QJsonArray sync;
        for(int i=0;i<120;++i){clock.restart();emit shell.facade()->positionChanged(i*41111,100000000);sync.append(double(clock.nsecsElapsed()));}
        result["longLyricsAdapterPositionNs"]=sync;result["lyricsResetsDuringPositionUpdates"]=lyricResets.size()-resetsBefore;
        if(lyricResets.size()!=resetsBefore)return 40;
        result["memoryWithLongLyrics"]=memory();
        if(!shots.isEmpty()&&!window->grabWindow().save(QDir(shots).filePath("long-lyrics.png")))return 35;
        QJsonArray transitions;result["memoryBeforeRapidTrackChanges"]=memory();
        for(int i=0;i<20;++i){clock.restart();shell.facade()->next();if(frame(window)<0)return 41;transitions.append(clock.nsecsElapsed()/1000000.);}
        result["rapidTrackChangeNextFrameMs"]=transitions;result["memoryAfterRapidTrackChanges"]=memory();
    }
    shell.appBridge()->navigate(3);if(frame(window)<0)return 42;
    auto *settings=visual(window->contentItem(),"settingsPage");if(!settings)return 42;
    settings->setProperty("section",0);if(frame(window)<0)return 42;
    clock.restart();shell.settingsBridge()->edit("reduceMotion",true);if(!shell.settingsBridge()->save()||frame(window)<0)return 43;
    result["settingsSaveUntilNextFrameMs"]=clock.nsecsElapsed()/1000000.;
    if(!shots.isEmpty()&&!window->grabWindow().save(QDir(shots).filePath("settings.png")))return 35;
    settings->setProperty("section",2);if(frame(window)<0)return 42;
    if(!shots.isEmpty()&&!window->grabWindow().save(QDir(shots).filePath("translation-ai.png")))return 35;
    const auto incoming=QDir(root).filePath("incoming");QDir().mkpath(incoming);
    for(int i=0;i<10;++i){QFile file(QDir(incoming).filePath(QString("导入 %1.mid").arg(i)));if(!file.open(QIODevice::WriteOnly))return 44;file.write(QByteArray::fromHex("4d546864000000060000000100604d54726b0000000400ff2f00")+QByteArray::number(i));}
    settings->setProperty("section",1);if(frame(window)<0)return 42;
    const auto importQueue=shell.facade()->queue().items;clock.restart();
    if(!shell.importBridge()->start({QUrl::fromLocalFile(incoming)}))return 45;
    QJsonArray importFrames;QElapsedTimer wait;wait.start();
    while(shell.importBridge()->busy()&&wait.elapsed()<30000){const auto ms=frame(window);if(ms<0)return 45;importFrames.append(ms);QTest::qWait(2);}
    if(shell.importBridge()->busy()||library.trackCount()!=count+10||shell.libraryModel()->count()!=count+10||shell.facade()->queue().items!=importQueue)return 46;
    result["importTenFilesMs"]=clock.nsecsElapsed()/1000000.;result["importFrameResponseMs"]=importFrames;
    result["importResult"]=QJsonObject::fromVariantMap(shell.importBridge()->result());result["tracksAfterImport"]=double(library.trackCount());
    if(!shots.isEmpty()&&!window->grabWindow().save(QDir(shots).filePath("import.png")))return 35;
    result["warnings"]=QJsonArray::fromStringList(shell.warnings());if(!shell.warnings().isEmpty())return 24;
    result["databaseConnections"]=QSqlDatabase::connectionNames().size();result["playbackInstances"]=shell.findChildren<Playback *>().size();
    result["method"]="Fresh measurement process after separate fixture preparation; real Qt first frame and next-frame response to 40 requested scroll steps. These response times are not real FPS or a physical display smoothness claim. Memory sampled before legacy materialization benchmark; payload is retained capacity lower bound, not total allocator traffic.";
    result["shutdownSaved"]=shell.shutdown();if(!result["shutdownSaved"].toBool())return 8;
    return saveJson(parser.value("output"),result)?0:9;
}
