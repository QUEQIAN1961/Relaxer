#include "import-fixture.h"
#include <QDir>
#include <QSqlDriver>
#include <QTemporaryDir>
#include <QtTest>
#include <sqlite3.h>

using namespace relaxer;
namespace {
Track seed(Library &library) {
    Track track;track.hash="lyrics-storage-fixture";track.title="Synthetic";track.file="Music/synthetic.mid";
    track.id=library.insert(track);return library.track(track.id);
}
QByteArray read(const QString &path) {QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};return file.readAll();}
struct Failure {
    QString path;QByteArray next;
    bool afterFileCommit=false,blockRestore=false,explicitTransaction=false;
    int commitCalls=0;
};
sqlite3 *handle(const Library &library) {
    auto value=import_fixture::database(library).driver()->handle();
    if(!value.isValid()||QByteArray(value.typeName())!="sqlite3*")return nullptr;
    return *static_cast<sqlite3 **>(value.data());
}
bool installFailure(Library &library,Failure &state,const QString &trigger) {
    auto *db=handle(library);if(!db)return false;
    const auto callback=+[](sqlite3_context *context,int,sqlite3_value **){
        auto &fault=*static_cast<Failure *>(sqlite3_user_data(context));
        fault.afterFileCommit=read(fault.path)==fault.next;
        if(fault.blockRestore){QFile::remove(fault.path);QDir().mkdir(fault.path);}
        sqlite3_result_error(context,"R4 injected SQLite update failure",-1);
    };
    if(sqlite3_create_function_v2(db,"r4_fail_lyrics",0,SQLITE_UTF8,&state,callback,nullptr,nullptr,nullptr)!=SQLITE_OK)return false;
    QSqlQuery query(import_fixture::database(library));return query.exec(trigger);
}
}

class LyricsPersistenceTests : public QObject {
    Q_OBJECT
private slots:
    void normalAndPreservingPathsKeepExistingEncodingSemantics() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library);QVERIFY(track.id);
        const auto text=QStringLiteral("[00:01.00]她在遠方").toUtf8();QVERIFY(library.saveLyrics(track.id,text));
        QCOMPARE(read(library.absolute(library.track(track.id).lyrics)),QStringLiteral("[00:01.00]她在远方").toUtf8());
        QVERIFY(library.saveLyricsPreservingText(track.id,text));QCOMPARE(read(library.absolute(library.track(track.id).lyrics)),text);
        QVERIFY(library.saveLyrics(track.id,QStringLiteral("纯音乐，请欣赏").toUtf8()));QVERIFY(library.isInstrumental(track.id));
    }
    void sqliteFailureRestoresExistingFile() {
        QTemporaryDir dir;Failure fault;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library);
        const QByteArray original="Original source";QVERIFY(library.saveLyrics(track.id,original));fault.path=library.absolute(library.track(track.id).lyrics);fault.next="New source";
        QVERIFY(installFailure(library,fault,"CREATE TRIGGER r4_fail BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT r4_fail_lyrics(); END"));
        QVERIFY(!library.saveLyrics(track.id,fault.next));QVERIFY(fault.afterFileCommit);QCOMPARE(read(fault.path),original);
        QVERIFY(library.lastError().contains("R4 injected"));QVERIFY(!library.isInstrumental(track.id));
    }
    void sqliteFailureRemovesNewFileAndKeepsOldReference() {
        QTemporaryDir dir;Failure fault;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library);
        fault.path=library.absolute(QString("Lyrics/track-%1.lrc").arg(track.id));fault.next="New source";QVERIFY(!QFileInfo::exists(fault.path));
        QVERIFY(installFailure(library,fault,"CREATE TRIGGER r4_fail BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT r4_fail_lyrics(); END"));
        QVERIFY(!library.saveLyrics(track.id,fault.next));QVERIFY(fault.afterFileCommit);QVERIFY(!QFileInfo::exists(fault.path));QVERIFY(library.track(track.id).lyrics.isEmpty());
    }
    void instrumentalFailureRollsBackBothDatabaseUpdates() {
        QTemporaryDir dir;Failure fault;Library library(dir.filePath("data"));QVERIFY(library.open());auto track=seed(library);
        const auto oldPath=QString("Lyrics/original-import.lrc");QFile old(library.absolute(oldPath));QVERIFY(old.open(QIODevice::WriteOnly));old.write("Existing import");old.close();
        QSqlQuery query(import_fixture::database(library));query.prepare("UPDATE tracks SET lyrics=? WHERE id=?");query.addBindValue(oldPath);query.addBindValue(track.id);QVERIFY(query.exec());
        fault.path=library.absolute(QString("Lyrics/track-%1.lrc").arg(track.id));fault.next=QStringLiteral("纯音乐，请欣赏").toUtf8();
        QVERIFY(installFailure(library,fault,"CREATE TRIGGER r4_fail BEFORE INSERT ON lyrics_playback BEGIN SELECT r4_fail_lyrics(); END"));
        QVERIFY(!library.saveLyrics(track.id,fault.next));QVERIFY(fault.afterFileCommit);QCOMPARE(library.track(track.id).lyrics,oldPath);
        QVERIFY(!library.isInstrumental(track.id));QVERIFY(!QFileInfo::exists(fault.path));QCOMPARE(read(library.absolute(oldPath)),QByteArray("Existing import"));
    }
    void actualSqliteCommitFailureRestoresFileAndTransaction() {
        QTemporaryDir dir;Failure fault;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library);
        const QByteArray original="Before commit failure";QVERIFY(library.saveLyrics(track.id,original));fault.path=library.absolute(library.track(track.id).lyrics);fault.next="After commit failure";
        auto *db=handle(library);QVERIFY(db);
        sqlite3_commit_hook(db,+[](void *data){auto &state=*static_cast<Failure *>(data);++state.commitCalls;state.afterFileCommit=read(state.path)==state.next;return 1;},&fault);
        QVERIFY(!library.saveLyrics(track.id,fault.next));sqlite3_commit_hook(db,nullptr,nullptr);
        QCOMPARE(fault.commitCalls,1);QVERIFY(fault.afterFileCommit);QCOMPARE(read(fault.path),original);
        QVERIFY(library.saveLyrics(track.id,"Retry after rejected commit"));QCOMPARE(read(fault.path),QByteArray("Retry after rejected commit"));
    }
    void restorationFailureIsReportedAsUnrecovered() {
        QTemporaryDir dir;Failure fault;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library);
        QVERIFY(library.saveLyrics(track.id,"Previous"));fault.path=library.absolute(library.track(track.id).lyrics);fault.next="New source";fault.blockRestore=true;
        QVERIFY(installFailure(library,fault,"CREATE TRIGGER r4_fail BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT r4_fail_lyrics(); END"));
        QVERIFY(!library.saveLyrics(track.id,fault.next));QVERIFY(fault.afterFileCommit);QVERIFY(QFileInfo(fault.path).isDir());
        QVERIFY2(library.lastError().contains(QStringLiteral("无法恢复")),qPrintable(library.lastError()));
        QVERIFY(QDir().rmdir(fault.path));
    }
    void fileCommitFailureLeavesDatabaseAndOriginalUntouched() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library);
        const auto path=library.absolute(QString("Lyrics/track-%1.lrc").arg(track.id));QVERIFY(QDir().mkdir(path));
        QVERIFY(!library.saveLyrics(track.id,"Blocked destination"));QVERIFY(QFileInfo(path).isDir());QVERIFY(library.track(track.id).lyrics.isEmpty());
        QVERIFY(!library.lastError().isEmpty());QVERIFY(QDir().rmdir(path));
    }
};
QTEST_GUILESS_MAIN(LyricsPersistenceTests)
#include "lyrics-persistence-tests.moc"
