#pragma once
#include "model.h"
#include "lyrics.h"
#include <QByteArray>
#include <QJsonObject>

namespace relaxer {
// Stateless matching calculations. No network, storage, UI or task ownership.
class LyricMatchPolicy final {
  public:
    static QString normalized(const QString &text);
    static QString cleanTitle(const QString &text);
    static bool knownArtist(const QString &artist);
    static bool sameArtist(const QString &a,const QString &b);
    static QString primaryArtist(const QString &artist);
    static QString versionMark(const QString &title);
    static QString baseTitle(QString title);
    static Track searchTrack(const Track &track);
    static bool confident(const Track &track,const QJsonObject &record);
    static double matchScore(const Track &track,const QJsonObject &record);
    // Keep the original short-circuit order: reject raw HTML before parsing.
    // A successful raw check leaves reason untouched; the final check clears it.
    static bool rawLyricsAllowed(const QByteArray &data,QString *reason=nullptr);
    // Parsing remains with the existing caller/parser. Only read its value here.
    static bool credibleLyrics(const Lyrics &parsed,qint64 duration,QString *reason=nullptr);
};
} // namespace relaxer
