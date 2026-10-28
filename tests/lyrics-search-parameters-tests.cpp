#include "search-parameters-fixture.h"
#include "lyrics-replacement-fixture.h"
#include "translation-fixture.h"
#include "c4c5-ui-fixture.h"
#include <QQuickItem>
#include <limits>
using namespace search_fixture;
namespace relaxer {
class LyricsSearchParameterDiagnostics {
public:
    static QVector<LyricsSearchRequest> requests(const LyricsSearchCoordinator &c){return c.m_queue;}
};
}
namespace {
QByteArray bytes(Library &l,qint64 id){return LyricsDocumentReader::readFile(l.absolute(l.track(id).lyrics)).bytes;}
bool click(QQuickWindow *w,QQuickItem *item){if(!item||!item->isVisible()||!item->isEnabled())return false;QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());return true;}
void type(QQuickWindow *w,QQuickItem *item,const QString &text){item->forceActiveFocus();QTest::keyClick(w,Qt::Key_A,Qt::ControlModifier);for(auto c:text.toLatin1())QTest::keyClick(w,c);}
bool shot(QmlShell &shell,QString name){const auto root=qEnvironmentVariable("RELAXER_D204_EVIDENCE");if(root.isEmpty())return true;QDir().mkpath(root);return shell.window()->grabWindow().save(root+"/"+name+".png");}
}
class SearchParametersTests final:public QObject {
    Q_OBJECT
private slots:
    void parameters_data(){
        QTest::addColumn<QString>("title");QTest::addColumn<QString>("artist");QTest::addColumn<QString>("album");QTest::addColumn<QString>("fragment");QTest::addColumn<double>("duration");QTest::addColumn<bool>("valid");
        QTest::newRow("P01-default")<<"Song"<<"Artist"<<"Album"<<""<<60000.<<true;
        QTest::newRow("P02-custom-title")<<"New title (Live)"<<"Artist"<<"Album"<<""<<60000.<<true;
        QTest::newRow("P03-custom-artist")<<"Song"<<"New Artist"<<"Album"<<""<<60000.<<true;
        QTest::newRow("P04-no-album")<<"Song"<<"Artist"<<""<<""<<60000.<<true;
        QTest::newRow("P05-no-duration")<<"Song"<<"Artist"<<"Album"<<""<<0.<<true;
        QTest::newRow("P06-Unicode")<<QStringLiteral("繁體 歌曲 日本語 한국어")<<QStringLiteral("歌手")<<QStringLiteral("专辑")<<""<<60000.<<true;
        QTest::newRow("P07-special")<<"A&B + C? # / 100%"<<"AC/DC"<<"x+y & #"<<"line + ?"<<60000.<<true;
        QTest::newRow("P08-empty")<<""<<"Artist"<<""<<""<<0.<<false;
        QTest::newRow("P08-only-query")<<""<<""<<""<<QStringLiteral("我記得的歌詞")<<0.<<true;
        QTest::newRow("P08-title-only")<<"Only title"<<""<<""<<""<<0.<<true;
        QTest::newRow("P08-too-long")<<QString(513,'x')<<""<<""<<""<<0.<<false;
        QTest::newRow("P08-control")<<QString("Song\nHidden")<<""<<""<<""<<0.<<false;
        QTest::newRow("P08-invalid-duration")<<"Song"<<""<<""<<""<<std::numeric_limits<double>::quiet_NaN()<<false;
    }
    void parameters(){
        QFETCH(QString,title);QFETCH(QString,artist);QFETCH(QString,album);QFETCH(QString,fragment);QFETCH(double,duration);QFETCH(bool,valid);
        Fixture f;QVERIFY(f.ok);f.sources({"lrclib"});QSignalSpy feedback(f.bridge.get(),&LyricsSearchBridge::searchFeedback);
        QCOMPARE(f.bridge->searchDetailed(title,artist,album,fragment,true,false,false,duration),valid);
        if(!valid){QCOMPARE(f.net.urls.size(),0);QCOMPARE(feedback.size(),1);QVERIFY(!f.bridge->status().isEmpty());return;}
        QTRY_VERIFY_WITH_TIMEOUT(!f.bridge->busy(),4000);QVERIFY(!f.net.urls.isEmpty());QCOMPARE(f.net.called(),QSet<QString>{"lrclib"});
        bool seenTitle=title.isEmpty(),seenArtist=artist.isEmpty(),seenAlbum=album.isEmpty(),seenFragment=fragment.isEmpty(),seenDuration=duration==0;
        for(const auto &url:f.net.urls){const QUrlQuery q(url);seenTitle|=q.queryItemValue("track_name",QUrl::FullyDecoded)==title;seenArtist|=q.queryItemValue("artist_name",QUrl::FullyDecoded)==artist;
            seenAlbum|=q.queryItemValue("album_name",QUrl::FullyDecoded)==album;seenFragment|=q.queryItemValue("q",QUrl::FullyDecoded)==fragment;
            if(duration==0)QVERIFY(!q.hasQueryItem("duration"));else seenDuration|=q.queryItemValue("duration").toDouble()==duration/1000.;}
        QVERIFY(seenTitle);QVERIFY(seenArtist);QVERIFY(seenAlbum);QVERIFY(seenFragment);QVERIFY(seenDuration);
        QTRY_COMPARE(feedback.size(),1);QCOMPARE(feedback[0][0].toString(),QString("Information"));
    }
    void sourceCalls_data(){
        QTest::addColumn<QStringList>("sources");
        QTest::newRow("S01-A-LRCLIB")<<QStringList{"lrclib"};QTest::newRow("S02-B-ovh")<<QStringList{"ovh"};
        QTest::newRow("S02-netease")<<QStringList{"netease"};QTest::newRow("S02-qq")<<QStringList{"qq"};QTest::newRow("S02-kugou")<<QStringList{"kugou"};
        QTest::newRow("S03-A+B")<<QStringList{"lrclib","ovh"};QTest::newRow("S04-none")<<QStringList{};
    }
    void sourceCalls(){
        QFETCH(QStringList,sources);Fixture f;QVERIFY(f.ok);f.sources(sources);QSignalSpy feedback(f.bridge.get(),&LyricsSearchBridge::searchFeedback);
        if(sources.isEmpty()){QVERIFY(!f.bridge->search(true));QCOMPARE(f.net.urls.size(),0);QVERIFY(f.bridge->status().contains("来源"));QCOMPARE(feedback.size(),1);return;}
        f.net.respond=[&](Reply *r){answerProvider(r,f.track);};
        QVERIFY(f.bridge->search(true));QTRY_VERIFY_WITH_TIMEOUT(!f.bridge->busy(),4000);
        QCOMPARE(f.net.called(),QSet<QString>(sources.begin(),sources.end()));QVERIFY(f.lib.track(f.track.id).lyrics.isEmpty());
        QVERIFY(f.bridge->rowCount()>0);
        if(sources.size()==1&&sources[0]!="lrclib"&&sources[0]!="ovh"){
            bool search=false,detail=false;for(const auto &url:f.net.urls){search|=url.path().contains("search");detail|=url.path().contains("lyric")||url.path().contains("download");}
            QVERIFY(search);QVERIFY(detail);
        }
    }
    void sourceOutcomes_data(){
        QTest::addColumn<QString>("mode");QTest::addColumn<QString>("category");
        QTest::newRow("S05-empty")<<"empty"<<"Information";
        QTest::newRow("S06-network")<<"error"<<"Error";
        QTest::newRow("S06-invalid-response")<<"invalid"<<"Error";
        QTest::newRow("S06-auth")<<"auth"<<"Error";
        QTest::newRow("S06-limit")<<"rate"<<"Error";
        QTest::newRow("S06-timeout")<<"timeout"<<"Error";
        QTest::newRow("S07-partial")<<"partial"<<"Warning";
    }
    void sourceOutcomes(){
        QFETCH(QString,mode);QFETCH(QString,category);Fixture f;QVERIFY(f.ok);f.sources(mode=="partial"?QStringList{"lrclib","ovh"}:QStringList{"lrclib"});
        f.net.respond=[&](Reply *r){
            if(mode=="timeout")return;
            if(mode=="empty")r->answer(200,"[]");
            else if(mode=="invalid")r->answer(200,"not JSON");
            else if(mode=="auth")r->answer(403,"{}");
            else if(mode=="rate")r->answer(429,"{}");
            else if(mode=="partial"&&provider(r->url())=="lrclib")r->answer(200,json(QJsonArray{record(f.track)}));
            else r->answer(0,{},QNetworkReply::ConnectionRefusedError);
        };
        QSignalSpy feedback(f.bridge.get(),&LyricsSearchBridge::searchFeedback);
        if(mode=="timeout") {auto req=f.request();LyricsSearchOptions opts;opts.budgetMs=40;opts.requestTimeoutMs=20;
            QSignalSpy events(f.service.get(),&LyricsSearchCoordinator::taskFinished);QVERIFY(f.service->start({req},opts));QTRY_VERIFY(!f.service->summary().running);QTRY_COMPARE(events.size(),1);
            const auto e=qvariant_cast<LyricsSearchEvent>(events[0][0]);QCOMPARE(e.state,LyricsSearchState::Failed);QVERIFY(e.status.contains("超时")||e.status.contains("预算"));return;}
        QVERIFY(f.bridge->search(true));QTRY_VERIFY_WITH_TIMEOUT(!f.bridge->busy(),4000);QTRY_COMPARE(feedback.size(),1);QCOMPARE(feedback[0][0].toString(),category);
        if(mode=="partial"){QVERIFY(f.bridge->rowCount()>0);QVERIFY(feedback[0][4].toString().contains("Lyrics.ovh"));QVERIFY(!f.bridge->selectedKey().isEmpty());}
        if(mode!="empty")QVERIFY(!f.lib.lookupCache(f.track.id).noResult);
    }
    void staleSearchCancellationAndRetry(){
        Fixture f;QVERIFY(f.ok);f.sources({"lrclib"});f.net.respond={};QSignalSpy feedback(f.bridge.get(),&LyricsSearchBridge::searchFeedback);
        QVERIFY(f.bridge->searchDetailed("Old", "Artist","","",true));QTRY_VERIFY(!f.net.replies.isEmpty());const auto held=f.net.replies;
        f.bridge->parametersEdited();QVERIFY(!f.bridge->busy());QVERIFY(f.bridge->selectedKey().isEmpty());QCOMPARE(feedback.size(),1);QCOMPARE(feedback[0][0].toString(),QString("Cancellation"));
        f.sources({"ovh"});f.net.respond=[](Reply *r){r->answer(200,"{\"lyrics\":\"Fresh first line\\nFresh second line\"}");};
        QVERIFY(f.bridge->searchDetailed("New", "Artist","","",true));QTRY_VERIFY_WITH_TIMEOUT(!f.bridge->busy(),4000);QTRY_VERIFY(f.bridge->rowCount()>0);
        const auto key=f.bridge->selectedKey();const auto status=f.bridge->status();
        for(const auto &reply:held)if(reply){QVERIFY(reply->aborted);reply->answer(200,json(QJsonArray{record(f.track,"STALE")}));}
        QTest::qWait(30);QCOMPARE(f.bridge->selectedKey(),key);QCOMPARE(f.bridge->status(),status);QVERIFY(f.bridge->previewText().contains("Fresh"));QVERIFY(!f.bridge->previewText().contains("STALE"));
        f.bridge->restoreSearchDefaults();QCOMPARE(f.bridge->enabledSources().size(),5);QVERIFY(f.bridge->selectedKey().isEmpty());
        const auto defaults=f.bridge->defaultQuery();QCOMPARE(defaults["title"].toString(),f.track.title);
        f.net.urls.clear();f.sources({"lrclib"});f.net.respond=[](Reply *r){r->answer(200,"[]");};QVERIFY(f.bridge->search(true));QTRY_VERIFY(!f.bridge->busy());
        QCOMPARE(QUrlQuery(f.net.urls.first()).queryItemValue("track_name"),f.track.title);
    }
    void explicitRetryKeepsCompatibleCandidatesAfterBridgeReset_data(){
        QTest::addColumn<bool>("networkFailure");QTest::newRow("empty")<<false;QTest::newRow("failure")<<true;
    }
    void explicitRetryKeepsCompatibleCandidatesAfterBridgeReset(){
        QFETCH(bool,networkFailure);
        Fixture f;QVERIFY(f.ok);f.sources({"lrclib"});
        f.net.respond=[&](Reply *r){r->answer(200,json(QJsonArray{record(f.track,"Retained preview")}));};
        QVERIFY(f.bridge->searchDetailed("Temporary","Artist","Album","",true));QTRY_VERIFY(!f.bridge->busy());
        QCOMPARE(f.bridge->rowCount(),1);const auto preview=f.bridge->previewText();QVERIFY(!preview.isEmpty());
        f.net.respond=[&](Reply *r){if(networkFailure)r->answer(503,"{}");else r->answer(r->url().path().endsWith("/get")?404:200,r->url().path().endsWith("/get")?"{}":"[]");};
        QSignalSpy feedback(f.bridge.get(),&LyricsSearchBridge::searchFeedback);
        const auto sent=f.net.urls.size();
        QVERIFY(f.bridge->searchDetailed("Temporary","Artist","Album","",true));QTRY_VERIFY(!f.bridge->busy());
        QVERIFY(f.net.urls.size()>sent);QCOMPARE(f.bridge->rowCount(),1);QCOMPARE(f.bridge->previewText(),preview);
        QTRY_COMPARE(feedback.size(),1);QCOMPARE(feedback[0][0].toString(),networkFailure?QString("Warning"):QString("Information"));
        QVERIFY(f.lib.track(f.track.id).lyrics.isEmpty());QVERIFY(!f.lib.lookupCache(f.track.id).noResult);
        QVERIFY(f.bridge->searchDetailed("Changed","Artist","Album","",true));QTRY_VERIFY(!f.bridge->busy());
        QCOMPARE(f.bridge->rowCount(),0);
    }
    void cacheIdentityAndUnknownOrigin(){
        Fixture f;QVERIFY(f.ok);auto a=record(f.track);a["source"]="Lyrics.ovh";auto unknown=record(f.track,"Unknown");
        QVERIFY(f.lib.saveLookupCache(f.track.id,{a,unknown},false));f.service->loadCached({f.track.id});
        QVERIFY(f.bridge->setTargetTrack("0"));QVERIFY(f.bridge->setTargetTrack(QString::number(f.track.id)));QCOMPARE(f.bridge->rowCount(),2);
        QVERIFY(f.bridge->data(f.bridge->index(1),LyricsSearchBridge::Source).toString().contains("来源未知"));QCOMPARE(f.net.urls.size(),0);
        f.sources({"lrclib"});f.service->loadCached({f.track.id},QStringList{"lrclib"});QVERIFY(f.service->candidates(f.track.id).records.isEmpty());
        f.net.respond=[&](Reply *r){r->answer(200,json(QJsonArray{record(f.track)}));};QVERIFY(f.bridge->searchDetailed("Temporary", "Artist","Album","",true));QTRY_VERIFY(!f.bridge->busy());
        QVERIFY(!f.lib.lookupCache(f.track.id).records.first().toObject()["lookupContext"].toString().isEmpty());
        f.service->invalidateCandidates(f.track.id);f.service->loadCached({f.track.id},QStringList{"lrclib"});QVERIFY(f.service->candidates(f.track.id).records.isEmpty());
        f.bridge->restoreSearchDefaults();f.sources({"lrclib"});QVERIFY(f.bridge->search(true));QTRY_VERIFY(!f.bridge->busy());
        f.service->invalidateCandidates(f.track.id);f.service->loadCached({f.track.id},QStringList{"lrclib"});QCOMPARE(f.service->candidates(f.track.id).records.size(),1);
        QVERIFY(f.service->candidates(f.track.id).records.first().toObject()["cacheOrigin"].toBool());
        f.sources({"ovh"});f.service->loadCached({f.track.id},QStringList{"ovh"});QVERIFY(f.service->candidates(f.track.id).records.isEmpty());
    }
    void matchingAndStableFilteredSelection(){
        Fixture f;QVERIFY(f.ok);auto correct=record(f.track),wrongArtist=correct,live=correct,longer=correct;
        wrongArtist["artistName"]="Someone else";live["trackName"]="Song (Live)";longer["duration"]=400;
        QVERIFY(LyricsSearchCoordinator::score(f.track,correct)>LyricsSearchCoordinator::score(f.track,wrongArtist));
        QVERIFY(LyricsSearchCoordinator::score(f.track,correct)>LyricsSearchCoordinator::score(f.track,live));
        QVERIFY(LyricsSearchCoordinator::score(f.track,correct)>LyricsSearchCoordinator::score(f.track,longer));
        f.sources({"lrclib","ovh"});f.net.respond=[&](Reply *r){if(provider(r->url())=="ovh")r->answer(200,"{\"lyrics\":\"Another first line\\nAnother second line\"}");else r->answer(200,json(QJsonArray{correct,correct,wrongArtist,live,longer}));};
        QVERIFY(f.bridge->search(true));QTRY_VERIFY(!f.bridge->busy());QCOMPARE(f.bridge->rowCount(),5); // duplicate within one provider collapsed, ovh remains distinct
        const auto original=f.bridge->selectedKey();f.bridge->setCandidateSource("Lyrics.ovh");QVERIFY(!f.bridge->selectedKey().isEmpty());QVERIFY(f.bridge->selectedKey()!=original);
        int visible=0;QString visibleKey;for(int row=0;row<f.bridge->rowCount();++row)if(f.bridge->data(f.bridge->index(row),LyricsSearchBridge::CandidateVisible).toBool()){++visible;visibleKey=f.bridge->data(f.bridge->index(row),LyricsSearchBridge::CandidateKey).toString();}
        QCOMPARE(visible,1);QCOMPARE(f.bridge->recommendedKey(),visibleKey);
        QVERIFY(!f.bridge->saveSelected(original,true));QVERIFY(f.bridge->saveSelected(f.bridge->selectedKey(),true));QVERIFY(bytes(f.lib,f.track.id).contains("Another"));
        f.bridge->setCandidateSource("");
    }
    void targetSwitchRejectsLateResultsAndSelection(){
        Fixture f;QVERIFY(f.ok);const auto b=search_fixture::seed(f.lib,"two","Second");f.sources({"lrclib"});f.net.respond={};
        QVERIFY(f.bridge->search(true));QTRY_VERIFY(!f.net.replies.isEmpty());const auto old=f.net.replies;
        QVERIFY(f.bridge->setTargetTrack(QString::number(b.id)));QVERIFY(!f.bridge->busy());
        f.net.respond=[&](Reply *r){r->answer(200,json(QJsonArray{record(b,"SECOND")}));};QVERIFY(f.bridge->search(true));QTRY_VERIFY(!f.bridge->busy());
        const auto key=f.bridge->selectedKey();for(const auto &reply:old)if(reply)reply->answer(200,json(QJsonArray{record(f.track,"STALE")}));
        QTest::qWait(20);QCOMPARE(f.bridge->selectedKey(),key);QVERIFY(f.bridge->previewText().contains("SECOND"));QVERIFY(f.lib.track(f.track.id).lyrics.isEmpty());
        f.sources({"ovh"});QVERIFY(!f.bridge->saveSelected(key,true));QVERIFY(f.lib.track(b.id).lyrics.isEmpty());
    }
    void frozenBatchAndOfflinePriority(){
        Fixture f;QVERIFY(f.ok);const auto b=search_fixture::seed(f.lib,"two","Second");QVERIFY(f.bridge->setTargetTracks({QString::number(f.track.id),QString::number(b.id)},false,true));f.sources({"lrclib"});
        f.net.respond={};f.bridge->setSearchOptions(true,true);QVERIFY(f.bridge->searchTracks({QString::number(f.track.id),QString::number(b.id)},true));
        QVERIFY(!f.bridge->setSourceEnabled("ovh",true));f.bridge->setSearchOptions(false,false);f.bridge->parametersEdited();
        const auto frozen=LyricsSearchParameterDiagnostics::requests(*f.service);QCOMPARE(frozen.size(),2);
        for(const auto &request:frozen){QVERIFY(request.replace);QVERIFY(request.requireTimed);QCOMPARE(*request.enabledSources,QStringList{"lrclib"});QCOMPARE(request.match.title,request.original.title);}
        f.net.respond=[](Reply *r){r->answer(200,"[]");};const auto held=f.net.replies;for(auto reply:held)if(reply&&!reply->isFinished())reply->answer(200,"[]");
        QTRY_VERIFY_WITH_TIMEOUT(!f.bridge->busy(),4000);QCOMPARE(f.net.called(),QSet<QString>{"lrclib"});QVERIFY(f.bridge->searchContext().contains("滚动 是"));QVERIFY(f.bridge->searchContext().contains("重新匹配 是"));
        QSet<QString> titles;for(const auto &url:f.net.urls){const auto value=QUrlQuery(url).queryItemValue("track_name");if(!value.isEmpty())titles.insert(value);}QVERIFY(titles.contains("Song"));QVERIFY(titles.contains("Second"));
        f.net.urls.clear();QFile local(f.lib.absolute("Music/one.lrc"));QVERIFY(local.open(QIODevice::WriteOnly));local.write(replacement_fixture::mono());local.close();
        auto req=f.request();req.manual=false;req.replace=false;QVERIFY(f.service->start({req}));QTRY_VERIFY(!f.service->summary().running);QCOMPARE(f.net.urls.size(),0);QCOMPARE(f.service->summary().saved,1);
    }
    void acceptingCandidateWhileSearchingReportsSave(){
        Fixture f;QVERIFY(f.ok);QVERIFY(f.bridge->setTargetTracks({QString::number(f.track.id)},true,true));f.sources({"lrclib"});
        bool answered=false;f.net.respond=[&](Reply *r){if(answered)return;answered=true;auto candidate=record(f.track);candidate["manualOnly"]=true;r->answer(200,json(QJsonArray{candidate}));};
        QSignalSpy feedback(f.bridge.get(),&LyricsSearchBridge::searchFeedback),saved(f.bridge.get(),&LyricsSearchBridge::saved);
        QVERIFY(f.bridge->searchTargets(true));QTRY_VERIFY(f.bridge->rowCount()>0);QVERIFY(f.bridge->busy());
        QVERIFY(f.bridge->saveSelected(f.bridge->selectedKey(),true));QCOMPARE(saved.size(),1);QTRY_VERIFY(!f.bridge->busy());QTRY_COMPARE(feedback.size(),1);
        QCOMPARE(feedback[0][0].toString(),QString("Information"));QVERIFY(f.bridge->status().contains("已保存"));QVERIFY(!f.lib.track(f.track.id).lyrics.isEmpty());
    }
    void draftsAndTranslationPreviewSurviveSearch(){
        Fixture f;QVERIFY(f.ok);QVERIFY(f.lib.saveLyricsPreservingText(f.track.id,replacement_fixture::bilingual()));
        QSignalSpy conflict(f.bridge.get(),&LyricsSearchBridge::replacementRequested);QVERIFY(!f.bridge->saveText("Draft first\nDraft second",true));QCOMPARE(conflict.size(),1);const auto draft=f.bridge->draftText();f.bridge->cancelReplacement(conflict[0][0].toString());
        TranslationHttpMock http;QVERIFY(http.listen());AiRuntime runtime(&f.lib,DeepSeekProvider::MockEndpoint{http.baseUrl()},std::make_unique<FakeCredentialStore>());QVERIFY(configureRuntime(runtime));
        TranslationCoordinator translation(&f.lib,&runtime);f.bridge->setTranslationCoordinator(&translation);
        QVERIFY(f.lib.saveLyricsPreservingText(f.track.id,replacement_fixture::mono()));QVERIFY(f.lib.setTranslationJudgment(f.track.id,TranslationJudgment::NeedsTranslation));
        http.enqueue(completion(QString::fromUtf8(translationJson({{0,QStringLiteral("世界你好")},{1,QStringLiteral("走回家")}}))));
        const auto task=translation.prepareAi(f.track.id);QVERIFY(task);QVERIFY(translation.start(task));QTRY_VERIFY(translation.summary(task).hasPreview());const auto preview=translation.summary(task).translations;
        f.sources({"lrclib"});f.net.respond=[&](Reply *r){r->answer(200,json(QJsonArray{record(f.track,"Different")}));};QVERIFY(f.bridge->search(true));QTRY_VERIFY(!f.bridge->busy());
        QCOMPARE(f.bridge->draftText(),draft);QCOMPARE(translation.summary(task).translations,preview);QVERIFY(translation.summary(task).hasPreview());
        const auto count=conflict.size();QVERIFY(!f.bridge->saveSelected(f.bridge->selectedKey(),true));QCOMPARE(conflict.size(),count+1);f.bridge->cancelReplacement(conflict.last()[0].toString());
        QVERIFY(translation.summary(task).hasPreview());QCOMPARE(bytes(f.lib,f.track.id),replacement_fixture::mono());translation.shutdown();f.bridge->setTranslationCoordinator(nullptr);
    }
    void actualQmlParametersSourcesRetryAndEscape(){
        QTemporaryDir dir;Library lib(dir.filePath("data"));QVERIFY(lib.open());const auto a=seed(lib),b=search_fixture::seed(lib,"two","Second");Network net;
        QmlShell shell(&lib,nullptr,true,nullptr,{}, {},&net);QVERIFY(shell.load());auto *w=shell.window();w->requestActivate();QVERIFY(QTest::qWaitForWindowActive(w));
        auto *bridge=shell.lyricsSearchBridge();QVERIFY(bridge->setTargetTracks({QString::number(a.id),QString::number(b.id)},false,true));
        auto *ws=c4c5_fixture::visual(w->contentItem(),"lyricsWorkspace");QVERIFY(ws);QVERIFY(QMetaObject::invokeMethod(ws,"open"));
        QVERIFY(click(w,c4c5_fixture::visual(w->contentItem(),"lyricsSearchSettingsButton")));auto *dialog=w->findChild<QObject *>("lyricsSearchSettingsDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(shot(shell,"qml-sources-before"));
        for(const auto &id:QStringList{"ovh","netease","qq","kugou"}){
            QTest::qWait(30);auto *control=c4c5_fixture::visual(w->contentItem(),"lyricsSource-"+id);QVERIFY(control);
            QTRY_VERIFY(control->width()>100&&control->height()>10);QVERIFY(click(w,control));QTRY_VERIFY(!bridge->enabledSources().contains(id));
        }
        QCOMPARE(bridge->enabledSources(),QStringList{"lrclib"});QVERIFY(shot(shell,"qml-sources"));QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!dialog->property("visible").toBool());QVERIFY(ws->isVisible());
        auto *title=c4c5_fixture::visual(w->contentItem(),"lyricsSearchTitle");QVERIFY(title);type(w,title,"From QML");
        type(w,c4c5_fixture::visual(w->contentItem(),"lyricsSearchArtist"),"QML Artist");
        net.respond=[](Reply *r){r->answer(0,{},QNetworkReply::ConnectionRefusedError);};QVERIFY(click(w,c4c5_fixture::visual(w->contentItem(),"searchLyricsButton")));QTRY_VERIFY(!bridge->busy());QTRY_VERIFY(shell.feedbackBridge()->visible());QCOMPARE(net.called(),QSet<QString>{"lrclib"});
        QCOMPARE(QUrlQuery(net.urls.first()).queryItemValue("track_name"),QString("From QML"));QCOMPARE(QUrlQuery(net.urls.first()).queryItemValue("artist_name"),QString("QML Artist"));
        type(w,title,"Retry QML");net.urls.clear();net.respond={};QVERIFY(click(w,c4c5_fixture::visual(w->contentItem(),"searchLyricsButton")));QTRY_VERIFY(bridge->busy());QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!bridge->busy());QVERIFY(ws->isVisible());
        QVERIFY(!w->property("popupOpen").toBool());QVERIFY(shell.feedbackBridge()->current()["category"].toString()!="Cancellation");
        net.respond=[&](Reply *r){const auto q=QUrlQuery(r->url());const auto target=q.queryItemValue("track_name")=="Second"?b:a;r->answer(200,json(QJsonArray{record(target)}));};
        QMetaObject::invokeMethod(ws,"restoreSearchDefaults");for(const auto &id:QStringList{"ovh","netease","qq","kugou"})QVERIFY(bridge->setSourceEnabled(id,false));
        QVERIFY(click(w,c4c5_fixture::visual(w->contentItem(),"batchLyricsSearchButton")));QTRY_VERIFY(!bridge->busy());QTRY_VERIFY(bridge->rowCount()>0);QVERIFY(shot(shell,"qml-candidates"));
        QVERIFY(click(w,c4c5_fixture::visual(w->contentItem(),"lyricsApplyButton")));QTRY_COMPARE(bridge->targetTrackKey(),QString::number(b.id));QVERIFY(!lib.track(a.id).lyrics.isEmpty());QVERIFY(lib.track(b.id).lyrics.isEmpty());
        QVERIFY(click(w,c4c5_fixture::visual(w->contentItem(),"lyricsApplyButton")));QTRY_VERIFY(!lib.track(b.id).lyrics.isEmpty());QVERIFY(shell.warnings().isEmpty());
        QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!ws->isVisible());QVERIFY(shell.shutdown());
    }
};
QTEST_MAIN(SearchParametersTests)
#include "lyrics-search-parameters-tests.moc"
