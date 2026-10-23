#include "domain/lyric-match.h"
#include "lyrics-search-coordinator.h"
#include "release-version.h"
#include "import-fixture.h"
#include <QFile>
#include <QJsonDocument>
#include <QSemaphore>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <QtTest>
#include <functional>
#include "async-pool-gate.h"

namespace relaxer {
class LyricsSearchDiagnostics {
public:
    using Job=LyricsSearchCoordinator::JobPtr;
    using Result=LyricsSearchCoordinator::RankResult;
    static Job job(LyricsSearchCoordinator &c,qint64 id){return c.m_jobs.value(id);}
    static bool captured(const Job &j){return j&&j->scoreInFlight&&!j->collected.isEmpty();}
    static bool ready(const Job &j){return j->readyValid;}
    static Result capture(const Job &j){
        LyricsSearchCoordinator::ScoreSnap s;s.payload=j->collected;s.original=j->original;s.match=j->match;
        s.id=j->original.id;s.hash=j->original.hash;s.fpO=LyricsSearchCoordinator::trackContext(j->original);s.fpM=LyricsSearchCoordinator::trackContext(j->match);
        s.generation=j->generation;s.epoch=j->collectionEpoch;s.timedRequired=j->input.requireTimed;
        return LyricsSearchCoordinator::computeRank(s);
    }
    static void deliver(LyricsSearchCoordinator &c,const Job &j,const Result &r){c.applyRanking(j,r);}
    static void fingerprintChange(const Job &j){j->match.title+=" changed";}
};
}

using namespace relaxer;
namespace {
class ControlledReply final : public QNetworkReply {
  public:
    explicit ControlledReply(const QNetworkRequest &request, QObject *parent) : QNetworkReply(parent) {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        open(QIODevice::ReadOnly);
    }
    bool aborted = false;
    void abort() override {
        aborted = true;
        setError(OperationCanceledError, "cancelled");
        setFinished(true);
        emit finished();
    }
    void respond(int status, QByteArray body, NetworkError error = NoError, bool duplicate = false) {
        bytes = std::move(body);
        offset = 0;
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (status == 429)
            setRawHeader("Retry-After", "30");
        if (error != NoError)
            setError(error, "controlled error");
        else if (status >= 400)
            setError(status >= 500 ? InternalServerError : ContentNotFoundError, "controlled HTTP error");
        setFinished(true);
        emit readyRead();
        emit downloadProgress(bytes.size(), bytes.size());
        emit finished();
        if (duplicate)
            emit finished();
    }
    qint64 bytesAvailable() const override { return bytes.size() - offset + QNetworkReply::bytesAvailable(); }

  protected:
    qint64 readData(char *data, qint64 maximum) override {
        const auto size = qMin(maximum, qint64(bytes.size() - offset));
        if (size <= 0)
            return -1;
        memcpy(data, bytes.constData() + offset, size);
        offset += size;
        return size;
    }

  private:
    QByteArray bytes;
    qint64 offset = 0;
};
class ControlledNetwork final : public QNetworkAccessManager {
  public:
    QList<QPointer<ControlledReply>> replies;
    QList<QUrl> urls;
    std::function<void(ControlledReply *)> responder;

  protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        auto *reply = new ControlledReply(request, this);
        replies.append(reply);
        urls.append(request.url());
        if (responder)
            QTimer::singleShot(0, reply, [this, reply] { responder(reply); });
        return reply;
    }
};
Track seed(Library &library, QString title = "Song", QString hash = "synthetic", QString artist = "Artist") {
    Track t;
    t.title = title;
    t.artist = artist;
    t.album = "Album";
    t.hash = hash;
    t.duration = 10000;
    t.file = "Music/" + hash + ".wav";
    t.coverOrigin = "none";
    t.id = library.insert(t);
    return library.track(t.id);
}
LyricsSearchRequest input(const Track &track, bool manual = false) {
    LyricsSearchRequest r;
    r.original = track;
    r.match = LyricMatchPolicy::searchTrack(track);
    r.replace = true;
    r.manual = manual;
    return r;
}
QJsonObject record(const Track &track, QString first = "First line", QString second = "Second line") {
    return {{"id", 1},
            {"trackName", track.title},
            {"artistName", track.artist},
            {"albumName", track.album},
            {"duration", track.duration / 1000.},
            {"syncedLyrics", "[00:01.00]" + first + "\n[00:02.00]" + second},
            {"plainLyrics", first + "\n" + second}};
}
QByteArray json(const QJsonArray &rows) {
    return QJsonDocument(rows).toJson(QJsonDocument::Compact);
}
QByteArray read(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
QVector<LyricsSearchSource> providers() {
    return {{"Primary", QUrl("http://primary.test/api"), 0, false},
            {"Secondary", QUrl("http://secondary.test/api"), 0, false}};
}
LyricsSearchOptions shortOptions() {
    LyricsSearchOptions o;
    o.budgetMs = 100;
    o.requestTimeoutMs = 40;
    o.retryDelayMs = 0;
    return o;
}
void emptyResponse(ControlledReply *reply) {
    reply->respond(reply->url().path().endsWith("/get") ? 404 : 200,
                   reply->url().path().endsWith("/get") ? QByteArray("{}") : QByteArray("[]"));
}
} // namespace
class LyricsSearchTests final : public QObject {
    Q_OBJECT
  private slots:
    void rankedFirstHitStopsFanout_data(){
        QTest::addColumn<bool>("holdOthers");QTest::newRow("immediate-all")<<false;QTest::newRow("cancel-slow-inflight")<<true;
    }
    void rankedFirstHitStopsFanout(){
        QFETCH(bool,holdOthers);QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l);
        ControlledNetwork net;net.responder=[&](ControlledReply *r){if(!holdOthers||r==net.replies.first())r->respond(200,json({record(t)}));};
        LyricsSearchCoordinator c(&l,nullptr,QUrl("http://primary.test/api"),{},&net,providers());
        QSignalSpy done(&c,&LyricsSearchCoordinator::finished);auto options=shortOptions();options.budgetMs=3000;options.requestTimeoutMs=1000;options.scoreDelayMs=100;
        c.start({input(t)},options);QCOMPARE(net.urls.size(),3);int aborted=0;
        for(const auto &reply:net.replies)connect(reply,&QNetworkReply::finished,this,[&,reply]{if(reply->aborted)++aborted;});
        QTRY_COMPARE(done.size(),1);QCOMPARE(c.summary().saved,1);QCOMPARE(c.summary().automaticAccepted,1);
        for(const auto &url:net.urls)qInfo().noquote()<<"P3 FIRST-HIT TRACE"<<url.host()<<url.path()<<url.query();
        qInfo()<<"P3 FIRST-HIT actual_requests="<<net.urls.size()<<"aborted="<<aborted;
        QCOMPARE(net.urls.size(),3);QCOMPARE(c.summary().activeRequests,0);QCOMPARE(aborted,holdOthers?2:0);
    }
    void automaticAcceptedRequiresPersistenceAndRetry(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l);
        QSqlDatabase database;
        for(const auto &name:QSqlDatabase::connectionNames())if(QSqlDatabase::database(name).databaseName()==l.absolute("relaxer.sqlite"))database=QSqlDatabase::database(name);
        QSqlQuery q(database);QVERIFY(q.exec("CREATE TRIGGER p3_reject BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT RAISE(ABORT,'P3 reject'); END"));
        ControlledNetwork net;net.responder=[&](ControlledReply *r){r->respond(200,json({record(t)}),QNetworkReply::NoError,true);};
        LyricsSearchCoordinator c(&l,nullptr,QUrl("http://mock.test/api"),{},&net);QSignalSpy done(&c,&LyricsSearchCoordinator::finished);
        c.start({input(t)});QTRY_COMPARE(done.size(),1);QCOMPARE(c.summary().saved,0);QCOMPARE(c.summary().automaticAccepted,0);QVERIFY(l.track(t.id).lyrics.isEmpty());
        QVERIFY(q.exec("DROP TRIGGER p3_reject"));c.start({input(t)});QTRY_COMPARE(done.size(),2);
        QCOMPARE(c.summary().saved,1);QCOMPARE(c.summary().automaticAccepted,1);QCOMPARE(c.summary().finished,1);
        c.noteManualSave();QCOMPARE(c.summary().automaticAccepted,1);QCOMPARE(c.summary().saved,2);
    }
    void headlessEventTraceAndCompletedNetworkBudget(){
        QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto t=seed(l);
        ControlledNetwork net;net.responder=[&](ControlledReply *r){QCOMPARE(r->request().rawHeader("User-Agent"),QByteArray(release::UserAgent));r->respond(200,json({record(t)}));};
        LyricsSearchCoordinator c(&l,nullptr,QUrl("http://mock.test/api"),{},&net);QSignalSpy events(&c,&LyricsSearchCoordinator::eventOccurred);
        QSignalSpy done(&c,&LyricsSearchCoordinator::finished);QSignalSpy tasks(&c,&LyricsSearchCoordinator::taskFinished);
        auto options=shortOptions();options.budgetMs=50;options.scoreDelayMs=100;
        c.start({input(t,true)},options);QTRY_COMPARE(done.size(),1);QCOMPARE(tasks.size(),1);
        const auto terminal=qvariant_cast<LyricsSearchEvent>(tasks.first().first());QVERIFY(!terminal.status.contains("预算"));
        int requests=0;for(const auto &args:events){const auto event=qvariant_cast<LyricsSearchEvent>(args.first());
            QCOMPARE(event.taskId,terminal.taskId);QCOMPARE(event.generation,terminal.generation);QCOMPARE(event.trackId,t.id);
            if(event.phase=="request"){++requests;qInfo().noquote()<<QString("TRACE task=%1 generation=%2 track=%3 source=%4 attempt=%5 url=%6").arg(event.taskId).arg(event.generation).arg(event.trackId).arg(event.source).arg(event.attempt).arg(event.url.toString());}
        }
        QCOMPARE(requests,net.urls.size());QCOMPARE(c.summary().automaticAccepted,0);QCOMPARE(c.summary().saved,0);
    }
    void domesticDetailsRunThroughProductionCoordinator_data() {
        QTest::addColumn<int>("kind");
        QTest::newRow("netease") << 2;
        QTest::newRow("qq") << 3;
        QTest::newRow("kugou") << 4;
    }
    void domesticDetailsRunThroughProductionCoordinator() {
        QFETCH(int, kind);
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        QJsonArray songs;
        for (int n = 1; n <= 3; ++n) {
            QJsonObject song;
            if (kind == 2)
                song = {{"id", n},
                        {"name", t.title},
                        {"duration", 10000},
                        {"artists", QJsonArray{QJsonObject{{"name", t.artist}}}}};
            if (kind == 3)
                song = {{"mid", QString("mid%1").arg(n)},
                        {"name", t.title},
                        {"interval", 10},
                        {"singer", QJsonArray{QJsonObject{{"name", t.artist}}}}};
            if (kind == 4)
                song = {{"id", n},
                        {"accesskey", "mock-key"},
                        {"song", t.title},
                        {"singer", t.artist},
                        {"duration", 10000}};
            songs.append(song);
            songs.append(song);
        }
        QJsonObject search;
        if (kind == 2)
            search = {{"result", QJsonObject{{"songs", songs}}}, {"code", 200}};
        if (kind == 3)
            search = {{"data", QJsonObject{{"song", QJsonObject{{"list", songs}}}}}, {"code", 0}};
        if (kind == 4)
            search = {{"candidates", songs}, {"status", 200}};
        const auto text = record(t)["syncedLyrics"].toString();
        QJsonObject lyric;
        if (kind == 2)
            lyric = {{"lrc", QJsonObject{{"lyric", text}}}};
        if (kind == 3)
            lyric = {{"lyric", text}};
        if (kind == 4)
            lyric = {{"content", QString::fromLatin1(text.toUtf8().toBase64())}};
        ControlledNetwork net;
        QSet<QUrl> detailUrls;
        net.responder = [&](ControlledReply *r) {
            const bool isDetail = r->url().path().contains("lyric") || r->url().path().endsWith("download");
            if (isDetail) {
                detailUrls.insert(r->url());
                r->respond(200, QJsonDocument(lyric).toJson(), QNetworkReply::NoError, true);
            } else
                r->respond(200, QJsonDocument(search).toJson());
        };
        const QVector<LyricsSearchSource> sources{{"Domestic", QUrl("http://source.test"), kind, false}};
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net, sources);
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start({input(t, true)});
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(detailUrls.size(), 3);
        QCOMPARE(c.candidates(t.id).records.size(), 3);
        QSet<QString> nativeIds;
        for (const auto &value : c.candidates(t.id).records) {
            const auto r = value.toObject();
            QCOMPARE(r["trackName"].toString(), t.title);
            QCOMPARE(r["artistName"].toString(), t.artist);
            nativeIds.insert(r["sourceRecordId"].toString());
        }
        QCOMPARE(nativeIds.size(), 3);
        QCOMPARE(c.summary().activeRequests, 0);
        QCOMPARE(c.summary().saved, 0);
        qInfo() << "domestic details" << kind << "requests" << c.summary().requests << "native IDs"
                << nativeIds;
    }
    void identicalCachedResultsStillCountAsFound() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        auto row = record(t);
        row["source"] = "LRCLIB";
        QVERIFY(l.saveLookupCache(t.id, {row}, false));
        ControlledNetwork net;
        net.responder = [&](ControlledReply *r) { r->respond(200, json({row})); };
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        c.loadCached({t.id});
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start({input(t, true)});
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(c.summary().candidateTracks, 1);
        QVERIFY(c.summary().requests > 0);
        QCOMPARE(c.summary().saved, 0);
    }
    void cancelledQueuedTracksFinishOnce() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        QVector<LyricsSearchRequest> batch;
        for (int n = 0; n < 12; ++n)
            batch.append(input(seed(l, QString::number(n), QString::number(n))));
        ControlledNetwork net;
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QSignalSpy tasks(&c, &LyricsSearchCoordinator::taskFinished);
        c.start(batch);
        c.cancelTrack(batch.last().original.id);
        c.stop();
        QTRY_COMPARE(tasks.size(), 12);
        QCOMPARE(c.summary().finished, 12);
        QCOMPARE(c.summary().cancelled, 12);
        QCOMPARE(c.summary().activeRequests, 0);
        QSet<quint64> identities;
        for (const auto &args : tasks) {
            const auto event = qvariant_cast<LyricsSearchEvent>(args.first());
            QVERIFY(!identities.contains(event.taskId));
            identities.insert(event.taskId);
        }
    }
    void distinctProviderRecordsMustNotCollapse() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        auto first = record(t), second = first;
        first["id"] = "alpha";
        second["id"] = "beta";
        first["albumName"] = "Edition A";
        second["albumName"] = "Edition B";
        ControlledNetwork net;
        net.responder = [&](ControlledReply *r) { r->respond(200, json({first, second})); };
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start({input(t, true)});
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(c.candidates(t.id).records.size(), 2);
    }
    void metadataPlan_data() {
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("artist");
        QTest::addColumn<QString>("expected");
        QTest::newRow("chinese-english")
            << QString("蔡恩雨 - I Love The Sky") << QString("蔡恩雨") << QString("I Love The Sky");
        QTest::newRow("japanese") << QString("春茶 - フラレガイガール") << QString("春茶")
                                  << QString("フラレガイガール");
        QTest::newRow("multi-remix") << QString("Home (Blaze U Remix)")
                                     << QString("ThimLife, Blaze U, Bibiane Z")
                                     << QString("Home (Blaze U Remix)");
        QTest::newRow("stage-name") << QString("TheFatRat - Xenogenesis") << QString("TheFatRat")
                                    << QString("Xenogenesis");
        QTest::newRow("korean") << QString("봄날") << QString("방탄소년단") << QString("봄날");
        QTest::newRow("traditional") << QString("無與倫比的美麗") << QString("蘇打綠")
                                     << QString("無與倫比的美麗");
        QTest::newRow("swapped-unknown-artist") << QString("Artist - Song") << QString() << QString("Song");
        QTest::newRow("reversed-known-metadata")
            << QString("Song - Artist") << QString("Artist") << QString("Song - Artist");
        for (const auto &version : QStringList{"Remix", "Live", "Acoustic", "Cover", "Instrumental",
                                               "Karaoke", "Explicit", "Radio Edit", "Sped Up", "Nightcore"})
            QTest::newRow(qPrintable(version))
                << "Song (" + version + ")" << QString("Artist") << "Song (" + version + ")";
        for (const auto &feature : QStringList{"feat.", "ft.", "featuring"})
            QTest::newRow(qPrintable(feature))
                << "Song " + feature + " Guest" << QString("A & B") << "Song " + feature + " Guest";
        QTest::newRow("fullwidth") << QString::fromUtf8("Ｓｏｎｇ（Ｌｉｖｅ）") << QString("Artist")
                                   << QString("Song(Live)");
        QTest::newRow("spaces") << QString("  Song   Title  ") << QString("Artist") << QString("Song Title");
        QTest::newRow("nbsp") << QString::fromUtf8("I\u00a0Love\u00a0The\u00a0Sky") << QString("蔡恩雨")
                              << QString("I Love The Sky");
        QTest::newRow("zero-width") << QString::fromUtf8("So\u200bng") << QString("Artist")
                                    << QString::fromUtf8("So\u200bng");
        QTest::newRow("combining") << QString::fromUtf8("Cafe\u0301") << QString("Artist")
                                   << QString::fromUtf8("Café");
        QTest::newRow("punctuation") << QString("Song, Part (Two)!") << QString("A/B")
                                     << QString("Song, Part (Two)!");
        QTest::newRow("missing-tags") << QString() << QString() << QString("synthetic");
    }
    void metadataPlan() {
        QFETCH(QString, title);
        QFETCH(QString, artist);
        QFETCH(QString, expected);
        Track track;
        track.id = 42;
        track.hash = "hash";
        track.file = "Music/synthetic.wav";
        track.title = title;
        track.artist = artist;
        track.duration = 10000;
        auto request = input(track);
        const auto sources = LyricsSearchCoordinator::defaultSources(QUrl("http://mock.test/api"));
        auto plan = LyricsSearchCoordinator::queryPlan(request, sources);
        QVERIFY(!plan.attempts.isEmpty());
        QCOMPARE(plan.attempts.first().title, expected);
        QVERIFY(!plan.attempts.first().exact);
        QCOMPARE(plan.request.original.title, title);
        QCOMPARE(plan.request.original.hash, track.hash);
        QCOMPARE(plan.request.original.id, track.id);
        auto manual = request;
        manual.manual = true;
        const auto second = LyricsSearchCoordinator::queryPlan(manual, sources);
        QCOMPARE(second.attempts.size(), plan.attempts.size());
        for (int n = 0; n < plan.attempts.size(); ++n) {
            QCOMPARE(second.attempts[n].title, plan.attempts[n].title);
            QCOMPARE(second.attempts[n].artist, plan.attempts[n].artist);
            QCOMPARE(second.attempts[n].exact, plan.attempts[n].exact);
        }
    }
    void sourceOrderBudgetAndExtendedRecall() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l, "Home (Blaze U Remix)");
        ControlledNetwork net;
        const auto sources = LyricsSearchCoordinator::defaultSources(QUrl("https://lrclib.net/api"));
        QCOMPARE(sources.size(), 5);
        QCOMPARE(sources[1].label, QString("Lyrics.ovh"));
        auto plan = LyricsSearchCoordinator::queryPlan(input(t), sources);
        QCOMPARE(plan.options.budgetMs, 30000);
        QCOMPARE(plan.options.requestTimeoutMs, 8000);
        QCOMPARE(plan.attempts[0].source, 0);
        QCOMPARE(plan.attempts[1].source, 2);
        QCOMPARE(plan.attempts[2].source, 3);
        QCOMPARE(plan.attempts[3].source, 4);
        QVERIFY(plan.attempts.last().source == 1);
        net.responder = [&](ControlledReply *r) {
            if (QUrlQuery(r->url()).queryItemValue("track_name") == "Home")
                r->respond(200, json({record(t)}));
            else
                emptyResponse(r);
        };
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start({input(t)});
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(c.summary().saved, 1);
        QCOMPARE(c.summary().automaticAccepted, 1);
        QVERIFY(c.summary().requests > 3);
        QVERIFY(read(l.absolute(l.track(t.id).lyrics)).contains("First line"));
        qInfo() << "extended recall requests" << c.summary().requests << "candidates"
                << c.summary().candidateTracks << "matched" << c.summary().matchedTracks;
    }
    void explicitStartOnlyAndEmptyCacheDoesNotSuppressNetwork() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        QVERIFY(l.saveLookupCache(t.id, {}, true));
        ControlledNetwork net;
        net.responder = emptyResponse;
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        c.loadCached({t.id});
        QCOMPARE(net.urls.size(), 0);
        QVERIFY(c.candidates(t.id).records.isEmpty());
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start({input(t)});
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(!net.urls.isEmpty());
        const auto sent = net.urls.size();
        c.start({input(t, true)});
        QTRY_COMPARE(done.size(), 2);
        QCOMPARE(net.urls.size(), 2 * sent);
        QCOMPARE(c.summary().saved, 0);
        QVERIFY(l.lookupCache(t.id).noResult);
    }
    void acceptanceGuards_data() {
        QTest::addColumn<QString>("scenario");
        for (const auto &s : QStringList{"trusted", "manual", "artist", "live", "remix", "cover",
                                         "instrumental-conflict", "html", "one-line", "timeline", "ambiguity",
                                         "existing-bilingual", "plain-text", "missing-artist"})
            QTest::newRow(qPrintable(s)) << s;
    }
    void acceptanceGuards() {
        QFETCH(QString, scenario);
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        auto t = seed(l);
        QByteArray before;
        if (scenario == "existing-bilingual") {
            QVERIFY(l.saveLyricsPreservingText(
                t.id, "[relaxer-bilingual:1]\n[00:01.00]Original\n[00:01.00]原译文"));
            t = l.track(t.id);
            before = read(l.absolute(t.lyrics));
        }
        if (scenario == "missing-artist") {
            t.artist.clear();
            for (const auto &name : QSqlDatabase::connectionNames()) {
                auto db = QSqlDatabase::database(name);
                if (db.databaseName() == l.absolute("relaxer.sqlite")) {
                    QSqlQuery q(db);
                    QVERIFY(q.exec("UPDATE tracks SET artist=''"));
                }
            }
            t = l.track(t.id);
        }
        auto row = record(t);
        QJsonArray rows;
        if (scenario == "artist")
            row["artistName"] = "Other";
        if (scenario == "live" || scenario == "remix" || scenario == "cover")
            row["trackName"] = t.title + " (" + scenario + ")";
        if (scenario == "html")
            row["syncedLyrics"] = "<html><div>error</div></html>";
        if (scenario == "one-line")
            row["syncedLyrics"] = "[00:01.00]single";
        if (scenario == "timeline")
            row["syncedLyrics"] = "[00:01.00]first\n[09:00.00]late";
        if (scenario == "plain-text") {
            row.remove("syncedLyrics");
            row["plainLyrics"] = "First\nSecond";
        }
        if (scenario == "ambiguity") {
            row["albumName"] = "Different A";
            auto other = row;
            other["id"] = 2;
            other["albumName"] = "Different B";
            other["syncedLyrics"] = "[00:01.00]different\n[00:02.00]text";
            rows.append(other);
        }
        if (scenario == "instrumental-conflict") {
            auto pure = row;
            pure["id"] = 3;
            pure["instrumental"] = true;
            pure.remove("syncedLyrics");
            pure.remove("plainLyrics");
            rows.append(pure);
        }
        rows.append(row);
        ControlledNetwork net;
        net.responder = [&](ControlledReply *r) { r->respond(200, json(rows)); };
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start({input(t, scenario == "manual")});
        QTRY_COMPARE(done.size(), 1);
        const bool accepted = scenario == "trusted" || scenario == "plain-text";
        QCOMPARE(c.summary().saved, accepted ? 1 : 0);
        QCOMPARE(c.summary().automaticAccepted, accepted ? 1 : 0);
        QCOMPARE(c.summary().candidateTracks, 1);
        QVERIFY(!c.candidates(t.id).records.isEmpty());
        if (!before.isEmpty())
            QCOMPARE(read(l.absolute(t.lyrics)), before);
        else if (!accepted)
            QVERIFY(l.track(t.id).lyrics.isEmpty());
    }
    void sourceFailures_data() {
        QTest::addColumn<int>("failure");
        for (int code : {429, 500, 404, 400, 200, 0, 413, 408})
            QTest::newRow(qPrintable(QString::number(code))) << code;
    }
    void sourceFailures() {
        QFETCH(int, failure);
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        ControlledNetwork net;
        net.responder = [&](ControlledReply *r) {
            if (r->url().host() == "secondary.test") {
                r->respond(200, json({record(t)}));
                return;
            }
            if (failure == 408)
                return;
            if (failure == 413) {
                r->respond(200, QByteArray(4 * 1024 * 1024 + 1, 'x'));
                return;
            }
            if (failure == 0) {
                r->respond(0, {}, QNetworkReply::ConnectionRefusedError);
                return;
            }
            r->respond(failure, failure == 200 ? QByteArray("invalid json") : QByteArray("{}"));
        };
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net, providers());
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start({input(t)}, shortOptions());
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(c.summary().saved, 1);
        QCOMPARE(c.summary().activeRequests, 0);
        QVERIFY(c.summary().requests < 30);
        QVERIFY(read(l.absolute(l.track(t.id).lyrics)).contains("First line"));
    }
    void allFailuresAreBoundedAndNeverNegativeCached() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        ControlledNetwork net;
        net.responder = [](ControlledReply *r) { r->respond(500, "{}"); };
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        QSignalSpy tasks(&c, &LyricsSearchCoordinator::taskFinished);
        c.start({input(t)}, shortOptions());
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(tasks.size(), 1);
        QCOMPARE(c.summary().saved, 0);
        QCOMPARE(c.summary().finished, 1);
        QVERIFY(!l.lookupCache(t.id).noResult);
        QVERIFY(c.summary().requests <= 16);
        QCOMPARE(c.summary().activeRequests, 0);
    }
    void duplicatesStableTiesAndCandidateRevision() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        auto first = record(t, "One", "Two"), second = record(t, "Three", "Four");
        second["id"] = 2;
        ControlledNetwork net;
        net.responder = [&](ControlledReply *r) {
            r->respond(200, json({first, first, second}), QNetworkReply::NoError, true);
        };
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        QSignalSpy taskDone(&c, &LyricsSearchCoordinator::taskFinished);
        c.start({input(t, true)});
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(taskDone.size(), 1);
        const auto snapshot = c.candidates(t.id);
        QCOMPARE(snapshot.records.size(), 2);
        QVERIFY(snapshot.revision > 0);
        QCOMPARE(snapshot.records[0].toObject()["id"].toInt(), 1);
        QCOMPARE(snapshot.records[1].toObject()["id"].toInt(), 2);
        QVERIFY(LyricsSearchCoordinator::candidateIdentity(snapshot.records[0].toObject()) !=
                LyricsSearchCoordinator::candidateIdentity(snapshot.records[1].toObject()));
        QCOMPARE(c.summary().activeRequests, 0);
        QCOMPARE(c.summary().saved, 0);
    }
    void staleRankAndABACancellation() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto a = seed(l, "A", "a"), b = seed(l, "B", "b");
        ControlledNetwork net;
        net.responder = [&](ControlledReply *r) { r->respond(200, json({record(a)})); };
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QSignalSpy events(&c, &LyricsSearchCoordinator::eventOccurred);
        auto options = shortOptions();
        options.scoreDelayMs = 60;
        options.budgetMs = 500;
        LyricsSearchCoordinator::resetRankRuns();
        c.start({input(a)}, options);
        QTRY_VERIFY(LyricsSearchCoordinator::rankRuns() > 0);
        const auto oldGeneration = c.summary().generation;
        c.stop();
        net.responder = [&](ControlledReply *r) { r->respond(200, json({record(b)})); };
        c.start({input(b)}, options);
        c.stop();
        net.responder = [&](ControlledReply *r) {
            r->respond(200, json({record(a, "New A", "New second")}));
        };
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start({input(a)}, options);
        QTRY_VERIFY(!c.summary().running);
        QTRY_VERIFY(!done.isEmpty());
        QVERIFY(c.summary().generation > oldGeneration);
        QVERIFY(l.track(b.id).lyrics.isEmpty());
        QVERIFY(read(l.absolute(l.track(a.id).lyrics)).contains("New A"));
        QSet<quint64> terminals;
        QSignalSpy terminal(&c, &LyricsSearchCoordinator::taskFinished);
        for (const auto &args : events) {
            const auto e = qvariant_cast<LyricsSearchEvent>(args[0]);
            if (e.phase == "finished" || e.phase == "cancelled") {
                QVERIFY(!terminals.contains(e.taskId));
                terminals.insert(e.taskId);
            }
        }
    }
    void staleRankingNeverPublishesBeforeFreshResult_data(){
        QTest::addColumn<QString>("kind");
        for(const auto *kind:{"ABA","cancelled","epoch","payload","fingerprint","current"})QTest::newRow(kind)<<QString::fromLatin1(kind);
    }
    void staleRankingNeverPublishesBeforeFreshResult(){
        QFETCH(QString,kind);QTemporaryDir d;Library l(d.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"A","a"),b=seed(l,"B","b");
        AsyncPoolGate gate(LyricsSearchCoordinator::computePool());QVERIFY(gate.entered());
        ControlledNetwork net;net.responder=[&](ControlledReply *r){r->respond(200,json({record(a,"Stale A","Stale second")}));};
        LyricsSearchCoordinator c(&l,nullptr,QUrl("http://mock.test/api"),{},&net);auto options=shortOptions();options.budgetMs=10000;
        c.start({input(a,true)},options);QTRY_VERIFY(LyricsSearchDiagnostics::captured(LyricsSearchDiagnostics::job(c,a.id)));
        const auto old=LyricsSearchDiagnostics::job(c,a.id);auto result=LyricsSearchDiagnostics::capture(old);
        if(kind=="ABA"){
            c.stop();c.start({input(b,true)},options);c.stop();
            net.responder=[&](ControlledReply *r){r->respond(200,json({record(a,"Fresh A","Fresh second")}));};
            c.start({input(a,true)},options);QTRY_VERIFY(LyricsSearchDiagnostics::captured(LyricsSearchDiagnostics::job(c,a.id)));
        }else if(kind=="cancelled")c.stop();
        else if(kind=="epoch")++result.epoch;
        else if(kind=="payload")result.payload.append(record(b));
        else if(kind=="fingerprint")LyricsSearchDiagnostics::fingerprintChange(old);
        else if(kind=="current"){
            // A separate valid SQL call changes the stored fingerprint while
            // the captured result still matches the in-memory job snapshot.
            QSqlQuery q(import_fixture::database(l));q.prepare("UPDATE tracks SET title=? WHERE id=?");q.addBindValue("DB changed");q.addBindValue(a.id);QVERIFY(q.exec());
        }
        QCoreApplication::sendPostedEvents();QCoreApplication::processEvents();
        QSignalSpy publications(&c,&LyricsSearchCoordinator::candidatesChanged),terminals(&c,&LyricsSearchCoordinator::taskFinished);
        const auto revision=c.candidates(a.id).revision;bool delivered=false;
        QTimer::singleShot(0,&c,[&]{LyricsSearchDiagnostics::deliver(c,old,result);delivered=true;});
        QTRY_VERIFY(delivered);QCoreApplication::sendPostedEvents();QCoreApplication::processEvents();
        QCOMPARE(publications.size(),0);QCOMPARE(c.candidates(a.id).revision,revision);
        QVERIFY(old->readyRanked.isEmpty()||kind=="current"); // precise apply boundary before current() rejection
        if(kind!="current")QVERIFY(!LyricsSearchDiagnostics::ready(old));
        QCOMPARE(terminals.size(),kind=="current"?1:0);QVERIFY(l.track(a.id).lyrics.isEmpty());QVERIFY(l.track(b.id).lyrics.isEmpty());
        if(kind=="ABA"){
            const auto fresh=LyricsSearchDiagnostics::job(c,a.id);const auto correct=LyricsSearchDiagnostics::capture(fresh);
            LyricsSearchDiagnostics::deliver(c,fresh,correct);QCoreApplication::sendPostedEvents();QCoreApplication::processEvents();
            QCOMPARE(publications.size(),1);QVERIFY(c.candidates(a.id).revision>revision);QVERIFY(c.candidates(a.id).records.first().toObject()["syncedLyrics"].toString().contains("Fresh A"));
        }
        c.stop();
    }
    void cancellationRejectsLateSuccessAndReleasesSlots() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        ControlledNetwork net;
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QSignalSpy tasks(&c, &LyricsSearchCoordinator::taskFinished);
        c.start({input(t)});
        QCOMPARE(net.replies.size(), 3);
        const auto held = net.replies;
        c.stop();
        for (const auto &reply : held)
            if (reply) {
                QVERIFY(reply->aborted);
                reply->respond(200, json({record(t)}));
            }
        QTRY_COMPARE(tasks.size(), 1);
        QVERIFY(l.track(t.id).lyrics.isEmpty());
        QVERIFY(c.candidates(t.id).records.isEmpty());
        QCOMPARE(c.summary().activeRequests, 0);
        net.responder = emptyResponse;
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start({input(t)});
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(c.summary().activeRequests, 0);
    }
    void completionCanRestartAndObserverCanDestroyService() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        ControlledNetwork net;
        net.responder = emptyResponse;
        auto *c = new LyricsSearchCoordinator(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QPointer<LyricsSearchCoordinator> guard(c);
        int completions = 0;
        connect(c, &LyricsSearchCoordinator::finished, &l, [&](const LyricsSearchSummary &) {
            if (++completions == 1)
                c->start({input(t, true)});
            else
                delete c;
        });
        c->start({input(t, true)});
        QTRY_VERIFY(!guard);
        QCOMPARE(completions, 2);
        auto *another = new LyricsSearchCoordinator(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QPointer<LyricsSearchCoordinator> second(another);
        connect(another, &LyricsSearchCoordinator::eventOccurred, &l,
                [&](const LyricsSearchEvent &) { delete another; });
        another->start({input(t)});
        QTRY_VERIFY(!second);
        QVERIFY(l.track(t.id).lyrics.isEmpty());
    }
    void frozenBatchRemovalAndHostCaps() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        QVector<LyricsSearchRequest> requests;
        for (int n = 0; n < 14; ++n)
            requests.append(input(seed(l, QString::number(n), QString::number(n))));
        ControlledNetwork net;
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start(requests);
        QCOMPARE(net.replies.size(), 6);
        QVERIFY(c.summary().activeTracks <= 10);
        QVERIFY(c.summary().activeRequests <= 6);
        QVERIFY(l.remove({requests.last().original.id}));
        const auto extra = seed(l, "Added later", "later");
        net.responder = emptyResponse;
        const auto held = net.replies;
        for (const auto &r : held)
            if (r)
                emptyResponse(r);
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(c.summary().requested, 14);
        QCOMPARE(c.summary().finished, 14);
        QVERIFY(c.summary().failed >= 1);
        QVERIFY(l.track(extra.id).lyrics.isEmpty());
        QVERIFY(c.candidates(extra.id).records.isEmpty());
        for (const auto &url : net.urls)
            QVERIFY(QUrlQuery(url).queryItemValue("track_name") != "Added later");
    }
    void snapshotAndSqlFailureProtectActualBytes() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        ControlledNetwork net;
        net.responder = [&](ControlledReply *r) { r->respond(200, json({record(t)})); };
        for (const auto &name : QSqlDatabase::connectionNames()) {
            auto db = QSqlDatabase::database(name);
            if (db.databaseName() == l.absolute("relaxer.sqlite")) {
                QSqlQuery q(db);
                QVERIFY(q.exec("CREATE TRIGGER fail_save BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT "
                               "RAISE(ABORT,'injected'); END"));
            }
        }
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        QSignalSpy done(&c, &LyricsSearchCoordinator::finished);
        c.start({input(t)});
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(c.summary().saved, 0);
        QCOMPARE(c.summary().automaticAccepted, 0);
        QVERIFY(l.track(t.id).lyrics.isEmpty());
        for (const auto &name : QSqlDatabase::connectionNames()) {
            auto db = QSqlDatabase::database(name);
            if (db.databaseName() == l.absolute("relaxer.sqlite")) {
                QSqlQuery q(db);
                QVERIFY(q.exec("DROP TRIGGER fail_save"));
            }
        }
        auto next = input(t);
        next.saveSnapshot = l.lyricsSaveSnapshot(t.id);
        QFile external(next.saveSnapshot.target.path);
        QVERIFY(external.open(QIODevice::WriteOnly));
        QCOMPARE(external.write("external protected bytes"), qint64(24));
        external.close();
        c.start({next});
        QTRY_COMPARE(done.size(), 2);
        QCOMPARE(c.summary().saved, 0);
        QCOMPARE(read(next.saveSnapshot.target.path), QByteArray("external protected bytes"));
        QCOMPARE(c.summary().automaticAccepted, 0);
    }
    void wrongThreadStartCannotTouchSqlOrTransport() {
        QTemporaryDir d;
        Library l(d.filePath("data"));
        QVERIFY(l.open());
        const auto t = seed(l);
        ControlledNetwork net;
        LyricsSearchCoordinator c(&l, nullptr, QUrl("http://mock.test/api"), {}, &net);
        quint64 result = 1;
        QThread *worker = QThread::create([&] { result = c.start({input(t)}); });
        worker->start();
        QVERIFY(worker->wait(3000));
        delete worker;
        QCOMPARE(result, quint64(0));
        QCOMPARE(net.urls.size(), 0);
        QVERIFY(l.track(t.id).lyrics.isEmpty());
    }
};
QTEST_GUILESS_MAIN(LyricsSearchTests)
#include "lyrics-search-coordinator-tests.moc"
