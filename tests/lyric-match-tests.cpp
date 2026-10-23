#include "domain/lyric-match.h"
#include "lyric-match-fixture.h"
#include <QtTest>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <future>
#include <vector>

using namespace relaxer;
namespace f=lyric_match_fixture;
class LyricMatchTests:public QObject {
    Q_OBJECT
    static bool credible(const QJsonObject &row,QString *reason) {
        const auto bytes=f::lyrics(row);
        return LyricMatchPolicy::rawLyricsAllowed(bytes,reason)&&LyricMatchPolicy::credibleLyrics(Lyrics::parse(bytes),f::track(row["track"].toObject()).duration,reason);
    }
  private slots:
    void explicitPrimaryArtist_data(){
        QTest::addColumn<QString>("artist");QTest::addColumn<QString>("candidate");QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("album");QTest::addColumn<double>("duration");QTest::addColumn<bool>("accepted");
        auto row=[](const char *name,QString artist,QString candidate,QString title="Song",QString album="Album",double duration=200.,bool accepted=false){
            QTest::newRow(name)<<artist<<candidate<<title<<album<<duration<<accepted;
        };
        row("featuring","Artist feat. Guest","Artist","Song","Album",200.,true);
        row("ft","Artist ft. Guest","Artist","Song","Album",200.8,true);
        row("with","Artist with Guest","Artist","Song","Album",200.,true);
        row("chinese","歌手 合作 嘉宾","歌手","Song","Album",200.,true);
        row("guest-only","Artist feat. Guest","Guest");row("swapped-order","Guest feat. Artist","Artist");
        row("same-title-wrong-album","Artist feat. Guest","Artist","Song","Other Album");
        row("wrong-title","Artist feat. Guest","Artist","Other Song");row("duration-conflict","Artist feat. Guest","Artist","Song","Album",203.);
        row("live","Artist feat. Guest","Artist","Song (Live)");row("remix","Artist feat. Guest","Artist","Song (Remix)");row("cover","Artist feat. Guest","Artist","Song (Cover)");
        row("wrong-singer","Artist feat. Guest","Wrong");row("empty-singer","","Artist");
        row("comma-ambiguous","Artist, Guest","Artist");row("band-comma","Earth, Wind & Fire","Earth");
        row("band-ampersand","Simon & Garfunkel","Simon");row("band-slash","AC/DC","AC");
    }
    void explicitPrimaryArtist(){
        QFETCH(QString,artist);QFETCH(QString,candidate);QFETCH(QString,title);QFETCH(QString,album);QFETCH(double,duration);QFETCH(bool,accepted);
        Track t;t.title="Song";t.artist=artist;t.album="Album";t.duration=200000;
        QJsonObject r{{"trackName",title},{"artistName",candidate},{"albumName",album},{"duration",duration}};
        QCOMPARE(LyricMatchPolicy::confident(t,r),accepted);
        if(accepted){t.title="Song (Live)";r["trackName"]="Song (Live)";QVERIFY(!LyricMatchPolicy::confident(t,r));}
    }
    void initTestCase(){QCOMPARE(f::root()["baseCommit"].toString(),QString("3a0dbd9cc67f6524c93ddf68b55ac2b3b403b4ec"));QCOMPARE(f::root()["cases"].toArray().size(),104);}
    void frozenBehavior_data(){QTest::addColumn<QJsonObject>("row");for(const auto &v:f::root()["cases"].toArray()){const auto row=v.toObject();QTest::newRow(qPrintable(row["id"].toString()))<<row;}}
    void frozenBehavior(){
        QFETCH(QJsonObject,row);const auto t=f::track(row["track"].toObject());const auto r=row["record"].toObject();const auto e=row["expected"].toObject();
        QCOMPARE(LyricMatchPolicy::normalized(t.title),e["normalized"].toString());
        QCOMPARE(LyricMatchPolicy::cleanTitle(t.title),e["cleanTitle"].toString());
        const auto clean=LyricMatchPolicy::searchTrack(t);QCOMPARE(f::metadata(clean),e["searchTrack"].toObject());
        QCOMPARE(clean.id,t.id);QCOMPARE(clean.hash,t.hash);QCOMPARE(clean.cover,t.cover);QCOMPARE(clean.lyrics,t.lyrics);QCOMPARE(clean.plays,t.plays);QCOMPARE(clean.heard,t.heard);QCOMPARE(clean.favorite,t.favorite);QCOMPARE(clean.coverOrigin,t.coverOrigin);QCOMPARE(clean.importedAt,t.importedAt);
        QCOMPARE(LyricMatchPolicy::matchScore(t,r),e["score"].toString().toDouble());
        QCOMPARE(LyricMatchPolicy::confident(t,r),e["confident"].toBool());
        QString reason="sentinel";QCOMPARE(credible(row,&reason),e["credible"].toBool());QCOMPARE(reason,e["rejection"].toString());
        QCOMPARE(credible(row,nullptr),e["credible"].toBool());
    }
    void artistAndVersionHelpers(){
        QVERIFY(LyricMatchPolicy::sameArtist("A / B","B & A"));QVERIFY(!LyricMatchPolicy::sameArtist("A/B","A"));
        QVERIFY(LyricMatchPolicy::sameArtist("FrogMonster 蛙蛙","FrogMonster"));
        QVERIFY(!LyricMatchPolicy::knownArtist("未知歌手"));QVERIFY(LyricMatchPolicy::knownArtist("未知歌手 "));
        QCOMPARE(LyricMatchPolicy::baseTitle("Song (Live)"),QString("Song"));
        QCOMPARE(LyricMatchPolicy::versionMark("Song Live Remix Remix"),QString("live|remix"));
    }
    void rawRejectionPrecedesParsing(){
        QString reason="sentinel";QVERIFY(!LyricMatchPolicy::rawLyricsAllowed("<DiV>one\ntwo",&reason));QCOMPARE(reason,QString("内容像网页，需人工确认"));
        reason="sentinel";QVERIFY(LyricMatchPolicy::rawLyricsAllowed("one\ntwo",&reason));QCOMPARE(reason,QString("sentinel"));
        Lyrics parsed;parsed.lines={{-1,"one"},{-1,"two"}};QVERIFY(LyricMatchPolicy::credibleLyrics(parsed,0,&reason));QVERIFY(reason.isEmpty());
    }
    void frozenRanking_data(){QTest::addColumn<QJsonObject>("set");for(const auto &v:f::root()["rankSets"].toArray()){const auto set=v.toObject();QTest::newRow(qPrintable(set["id"].toString()))<<set;}}
    void frozenRanking(){
        QFETCH(QJsonObject,set);const auto t=f::track(set["track"].toObject());
        struct Row{QJsonObject record;double score;};QVector<Row> rows;
        for(const auto &r:f::records(set["records"].toArray()))rows.push_back({r,LyricMatchPolicy::matchScore(t,r)});
        std::stable_sort(rows.begin(),rows.end(),[](const Row &a,const Row &b){return a.score>b.score;});
        QJsonArray result;for(const auto &row:rows)result.append(row.record);QCOMPARE(result,set["expectedOrder"].toArray());
    }
    void concurrentResultsRemainIdentical(){
        const auto rows=f::root()["cases"].toArray();std::vector<std::future<bool>> workers;
        for(int worker=0;worker<8;++worker)workers.push_back(std::async(std::launch::async,[rows]{
            for(int pass=0;pass<3;++pass)for(const auto &v:rows){const auto row=v.toObject();const auto e=row["expected"].toObject();const auto t=f::track(row["track"].toObject());const auto r=row["record"].toObject();QString reason="sentinel";
                if(LyricMatchPolicy::matchScore(t,r)!=e["score"].toString().toDouble()||LyricMatchPolicy::confident(t,r)!=e["confident"].toBool()||credible(row,&reason)!=e["credible"].toBool()||reason!=e["rejection"].toString())return false;
            }return true;
        }));
        for(auto &worker:workers)QVERIFY(worker.get());
    }
    void largeCandidatePerformance(){
        const auto rows=f::many();const auto t=f::performanceTrack();QElapsedTimer timer;timer.start();double checksum=0;
        for(const auto &r:rows)checksum+=LyricMatchPolicy::matchScore(t,r);
        QVERIFY(std::isfinite(checksum));QCOMPARE(rows.size(),2000);
        qInfo().noquote()<<QString("B3 pure policy candidates=%1 elapsed_ms=%2; real B2 cached-rank baseline_ms=%3 (timing informational)").arg(rows.size()).arg(timer.nsecsElapsed()/1000000.).arg(f::root()["performance"].toObject()["milliseconds"].toDouble());
    }
};
QTEST_GUILESS_MAIN(LyricMatchTests)
#include "lyric-match-tests.moc"
