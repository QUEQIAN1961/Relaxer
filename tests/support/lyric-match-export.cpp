#include "lyricslookup.h"
#include "lyric-text.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QTextStream>

using namespace relaxer;
static Track track(const QJsonObject &o) {
    Track t;t.id=73;t.hash="baseline-hash";t.cover="cover-sentinel";t.lyrics="lyrics-sentinel";
    t.plays=11;t.heard=1234;t.favorite=true;t.coverOrigin="origin-sentinel";t.importedAt="2026-10-16";
    t.title=o["title"].toString();t.artist=o["artist"].toString();t.album=o["album"].toString();
    t.file=o["file"].toString();t.duration=qint64(o["duration"].toDouble());return t;
}
static QJsonObject metadata(const Track &t) {
    return {{"title",t.title},{"artist",t.artist},{"album",t.album},{"file",t.file},{"duration",double(t.duration)}};
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if(argc!=3)return 2;
    QFile input(QString::fromLocal8Bit(argv[1]));if(!input.open(QIODevice::ReadOnly))return 3;
    auto root=QJsonDocument::fromJson(input.readAll()).object();
    QJsonArray frozen;
    for(const auto &v:root["cases"].toArray()) {
        auto row=v.toObject();const auto t=track(row["track"].toObject());const auto r=row["record"].toObject();
        QString reason="sentinel";const auto bytes=QByteArray::fromBase64(row["lyricsBase64"].toString().toLatin1());
        const auto credit=originalCredit(t.title);
        row["expected"]=QJsonObject{{"normalized",LyricsLookup::normalized(t.title)},
            {"cleanTitle",LyricsLookup::cleanTitle(t.title)},{"searchTrack",metadata(LyricsLookup::searchTrack(t))},
            {"score",QString::number(LyricsLookup::matchScore(t,r),'g',17)},
            {"confident",LyricsLookup::confident(t,r)},{"matchReason",LyricsLookup::matchReason(t,r)},
            {"credible",LyricsLookup::credibleLyrics(bytes,t,&reason)},{"rejection",reason},
            {"originalCredit",QJsonObject{{"found",credit.found},{"title",credit.title},{"artist",credit.artist}}}};
        frozen.append(row);
    }
    root["cases"]=frozen;
    QJsonArray ranks;
    for(const auto &v:root["rankSets"].toArray()) {
        auto row=v.toObject();QVector<QJsonObject> records;for(const auto &r:row["records"].toArray())records<<r.toObject();
        LyricsLookup::resetMatchScoreCount();const auto ranked=LyricsLookup::rankCandidates(track(row["track"].toObject()),records);
        QJsonArray expected;for(const auto &r:ranked)expected.append(r);
        row["expectedOrder"]=expected;row["expectedScoreCalls"]=double(LyricsLookup::matchScoreCount());ranks.append(row);
    }
    root["rankSets"]=ranks;
    Track t;t.title="Performance baseline song";t.artist="Artist A / Artist B";t.album="Album";t.duration=200000;
    QVector<QJsonObject> many;for(int i=0;i<2000;++i)many<<QJsonObject{{"id",i},{"trackName",QString("Performance baseline song %1").arg(i%17)},{"artistName","Artist B & Artist A"},{"duration",200.0+(i%9)},{"syncedLyrics","[00:00.00]one\n[00:02.00]two"}};
    LyricsLookup::resetMatchScoreCount();QElapsedTimer timer;timer.start();
    const auto ranked=LyricsLookup::rankCandidates(t,many);
    root["performance"]=QJsonObject{{"candidates",many.size()},{"scoreCalls",double(LyricsLookup::matchScoreCount())},{"milliseconds",double(timer.nsecsElapsed())/1000000.},{"ranked",ranked.size()}};
    root["baseCommit"]="3a0dbd9cc67f6524c93ddf68b55ac2b3b403b4ec";
    root["oracle"]="Actual unmodified B2 LyricsLookup symbols from verified Windows Release static libraries; no replacement implementation";
    QFile output(QString::fromLocal8Bit(argv[2]));if(!output.open(QIODevice::WriteOnly))return 4;
    output.write(QJsonDocument(root).toJson());
    QTextStream(stdout)<<"Frozen "<<frozen.size()<<" cases and "<<ranks.size()<<" rank sets; score calls="<<LyricsLookup::matchScoreCount()<<Qt::endl;
    return 0;
}
