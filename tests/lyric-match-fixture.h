#pragma once
#include "model.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace lyric_match_fixture {
inline QJsonObject root() {
    QFile file(QString::fromUtf8(RELAXER_LYRIC_MATCH_FIXTURE));
    if(!file.open(QIODevice::ReadOnly))return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
inline relaxer::Track track(const QJsonObject &o) {
    relaxer::Track t;t.id=73;t.hash="baseline-hash";t.cover="cover-sentinel";t.lyrics="lyrics-sentinel";
    t.plays=11;t.heard=1234;t.favorite=true;t.coverOrigin="origin-sentinel";t.importedAt="2026-10-16";
    t.title=o["title"].toString();t.artist=o["artist"].toString();t.album=o["album"].toString();
    t.file=o["file"].toString();t.duration=qint64(o["duration"].toDouble());return t;
}
inline QJsonObject metadata(const relaxer::Track &t) {
    return {{"title",t.title},{"artist",t.artist},{"album",t.album},{"file",t.file},{"duration",double(t.duration)}};
}
inline QByteArray lyrics(const QJsonObject &row) {
    return QByteArray::fromBase64(row["lyricsBase64"].toString().toLatin1());
}
inline QVector<QJsonObject> records(const QJsonArray &array) {
    QVector<QJsonObject> out;for(const auto &v:array)out<<v.toObject();return out;
}
inline QJsonArray json(const QVector<QJsonObject> &rows) {
    QJsonArray out;for(const auto &row:rows)out.append(row);return out;
}
inline QVector<QJsonObject> many() {
    QVector<QJsonObject> out;for(int i=0;i<2000;++i)out<<QJsonObject{{"id",i},{"trackName",QString("Performance baseline song %1").arg(i%17)},{"artistName","Artist B & Artist A"},{"duration",200.0+(i%9)},{"syncedLyrics","[00:00.00]one\n[00:02.00]two"}};return out;
}
inline relaxer::Track performanceTrack() {
    relaxer::Track t;t.title="Performance baseline song";t.artist="Artist A / Artist B";t.album="Album";t.duration=200000;return t;
}
} // namespace lyric_match_fixture
