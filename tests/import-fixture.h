#pragma once
#include "importer.h"
#include "library.h"
#include <QDataStream>
#include <QFile>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace import_fixture {
inline bool midi(const QString &path, int variation = 0) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QByteArray bytes = QByteArray::fromHex("4d546864000000060000000100604d54726b0000000400ff2f00");
    // Distinct, harmless trailing fixture bytes exercise existing byte-hash identity.
    bytes.append(QByteArray::number(variation));
    return file.write(bytes) == bytes.size();
}
inline bool wave(const QString &path) {
    QProcess ff;
    ff.start(relaxer::toolPath("ffmpeg"), {"-v","error","-f","lavfi","-i","sine=frequency=440:duration=0.2",
        "-metadata","title=Fixture Title","-metadata","artist=Fixture Artist","-metadata","album=Fixture Album","-y",path});
    return ff.waitForFinished(20000) && ff.exitCode() == 0;
}
inline QSqlDatabase database(const relaxer::Library &library) {
    for (const auto &name : QSqlDatabase::connectionNames()) {
        auto db = QSqlDatabase::database(name);
        if (db.databaseName() == library.absolute("relaxer.sqlite")) return db;
    }
    return {};
}
inline QStringList schema(const relaxer::Library &library) {
    QSqlQuery query(database(library));
    query.exec("SELECT name||':'||sql FROM sqlite_master WHERE sql IS NOT NULL ORDER BY name");
    QStringList result;
    while(query.next()) result << query.value(0).toString();
    return result;
}
}
