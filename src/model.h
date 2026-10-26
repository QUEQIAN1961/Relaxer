#pragma once
#include <QMetaType>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTextBoundaryFinder>
#include <QVector>
namespace relaxer {
struct Track {
    qint64 id = 0;
    QString hash, file, title, artist, album, cover, lyrics;
    qint64 duration = 0, plays = 0, heard = 0;
    bool favorite = false;
    QString coverOrigin;
    QString importedAt;
};
struct Playlist {
    qint64 id = 0;
    QString name, cover;
    int count = 0;
    qint64 plays = 0;
    QStringList tags;
    QString description;
};
inline QStringList playlistTagOptions() {
    return {"华语", "流行", "摇滚", "民谣", "电子", "另类/独立", "轻音乐", "综艺", "影视原声", "ACG"};
}
// Count user-perceived characters, including emoji and combining sequences.
inline int characterCount(const QString &text) {
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    int count = 0;
    while (finder.toNextBoundary() >= 0) ++count;
    return count;
}
inline int characterPrefixLength(const QString &text, int limit) {
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    int position = 0;
    for (int i = 0; i < limit; ++i) {
        const int next = finder.toNextBoundary();
        if (next < 0) return text.size();
        position = next;
    }
    return position;
}
struct Session {
    qint64 id = 0, track = 0, heard = 0, duration = 0;
    bool sought = false, counted = false;
};
struct ImportResult {
    Track track;
    QString source, error;
};
// Value-only file identity. Captured before work and rechecked at commit; no open
// QObject or database handle crosses the worker boundary.
struct ArtworkFileVersion {
    QString path, canonical;
    qint64 size=-1;
    QDateTime modified, metadataChanged, created;
    bool operator==(const ArtworkFileVersion &other) const {
        return path==other.path && canonical==other.canonical && size==other.size &&
            modified==other.modified && metadataChanged==other.metadataChanged && created==other.created;
    }
};
enum class ArtworkOutcome { Recovered, NoSource, InvalidImage, ReadFailed, Cancelled };
struct ArtworkResult {
    qint64 id = 0;
    QString hash, previousCover, cover, origin, error, trackFile;
    ArtworkFileVersion sourceVersion, previousVersion;
    QByteArray png;
    ArtworkOutcome outcome=ArtworkOutcome::ReadFailed;
    bool previousUsable=false;
};
inline QString durationText(qint64 ms) {
    auto s = qMax<qint64>(0, ms / 1000);
    if (s >= 3600)
        return QString("%1:%2:%3")
            .arg(s / 3600)
            .arg((s / 60) % 60, 2, 10, QChar('0'))
            .arg(s % 60, 2, 10, QChar('0'));
    return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}
} // namespace relaxer
Q_DECLARE_METATYPE(relaxer::Track)
Q_DECLARE_METATYPE(relaxer::ImportResult)
Q_DECLARE_METATYPE(relaxer::ArtworkResult)
