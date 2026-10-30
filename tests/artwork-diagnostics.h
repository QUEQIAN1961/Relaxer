#pragma once
#include "artwork-recovery-fixture.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QSqlDriver>
#include <QSqlError>
#include <QThread>
#include <sqlite3.h>

// Test-only observations. Keep error reads before any diagnostic SQL, and buffer
// in memory until the tested operation has returned.
namespace artwork_diagnostics {
inline sqlite3 *native(const relaxer::Library &library) {
    auto db=import_fixture::database(library);
    auto handle=db.driver()?db.driver()->handle():QVariant();
    return handle.isValid()?*static_cast<sqlite3 **>(handle.data()):nullptr;
}
inline QJsonObject databaseState(const relaxer::Library &library,const QString &stage,qint64 elapsed=0) {
    auto *connection=native(library);
    const int code=connection?sqlite3_errcode(connection):-1;
    const int extended=connection?sqlite3_extended_errcode(connection):-1;
    const auto message=connection?QString::fromUtf8(sqlite3_errmsg(connection)):QString("No native connection");
    auto db=import_fixture::database(library);
    const auto path=db.databaseName();
    return {{"stage",stage},{"elapsedMs",double(elapsed)},{"utcMs",double(QDateTime::currentMSecsSinceEpoch())},
        {"sqlitePrimaryCode",code&255},{"sqliteExtendedCode",extended},{"sqliteMessage",message},
        {"sqliteBusy",(code&255)==SQLITE_BUSY},{"sqliteLocked",(code&255)==SQLITE_LOCKED},
        {"sqliteIOError",(code&255)==SQLITE_IOERR},{"autocommit",connection?sqlite3_get_autocommit(connection):-1},
        {"transactionState",connection?sqlite3_txn_state(connection,nullptr):-1},
        {"connectionName",db.connectionName()},{"connectionOpen",db.isOpen()},{"connectionValid",db.isValid()},
        {"qtError",db.lastError().text()},{"qtNativeError",db.lastError().nativeErrorCode()},
        {"libraryError",library.lastError()},{"databasePath",path},{"databaseExists",QFileInfo::exists(path)},
        {"databaseDirectoryExists",QFileInfo(QFileInfo(path).absolutePath()).isDir()},
        {"pid",double(QCoreApplication::applicationPid())},
        {"thread",QString::number(quintptr(QThread::currentThreadId()),16)}};
}
class SqlTrace {
    sqlite3 *connection;
    QJsonArray &events;
    QElapsedTimer clock;
    static int observe(unsigned kind,void *context,void *statement,void *detail) {
        auto &self=*static_cast<SqlTrace *>(context);
        auto *sql=sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
        QJsonObject event{{"phase",kind==SQLITE_TRACE_STMT?"statement":"profile"},
            {"sql",QString::fromUtf8(sql?sql:"")},{"elapsedMs",double(self.clock.elapsed())}};
        if(kind==SQLITE_TRACE_PROFILE&&detail)event["durationNs"]=double(*static_cast<sqlite3_int64 *>(detail));
        self.events.append(event);return 0;
    }
public:
    SqlTrace(sqlite3 *db,QJsonArray &values):connection(db),events(values) {
        clock.start();if(connection)sqlite3_trace_v2(connection,SQLITE_TRACE_STMT|SQLITE_TRACE_PROFILE,observe,this);
    }
    ~SqlTrace(){if(connection)sqlite3_trace_v2(connection,0,nullptr,nullptr);}
};
inline relaxer::Track legacy(relaxer::Library &library,const QString &source,QJsonObject &result) {
    QElapsedTimer timer;timer.start();QJsonArray stages,statements;
    SqlTrace trace(native(library),statements);
    auto capture=[&](const QString &stage){stages.append(databaseState(library,stage,timer.elapsed()));};
    capture("before-import");
    auto imported=relaxer::Importer::importOne(library.root(),source);
    capture("after-import");result["importError"]=imported.error;
    auto finish=[&](const QString &failure,const relaxer::Track &track=relaxer::Track{}) {
        result["failureStage"]=failure;result["stages"]=stages;result["sqlTrace"]=statements;
        result["elapsedMs"]=double(timer.elapsed());result["returnedID"]=double(track.id);
        result["sourcePath"]=source;result["sourceExists"]=QFileInfo::exists(source);
        return track;
    };
    if(!imported.error.isEmpty())return finish("import");
    auto track=imported.track;track.id=0;track.cover="Artwork/"+track.hash+".png";track.coverOrigin.clear();
    const auto cache=artwork_fixture::png(Qt::blue);
    if(!artwork_fixture::write(library.absolute(track.cover),cache))return finish("cache-write");
    capture("before-insert");track.id=library.insert(track);capture("after-insert");
    if(!track.id)return finish("insert");
    auto stored=library.track(track.id);capture("after-readback");
    return finish(stored.id?QString():QString("readback"),stored);
}
}
