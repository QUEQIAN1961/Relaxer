#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
namespace relaxer {
struct OriginalCredit {
    bool found = false;
    QString title, artist;
};
OriginalCredit originalCredit(const QString &title);
QString toSimplified(const QString &text);
bool containsTraditional(const QString &text);
bool lyricsAreChinese(const QString &text);
QByteArray simplifyTraditionalLyrics(const QByteArray &data);
bool lyricsIncludeTranslation(const QByteArray &data);
QByteArray sourceLyricsForTranslation(const QByteArray &data);
QStringList lyricTexts(const QByteArray &data);
QByteArray attachTranslations(const QByteArray &data, const QStringList &translations);
QString translationLanguagePair(const QString &sample);
bool isMidiSuffix(const QString &suffix);
bool playsWithoutFfmpeg(const QString &suffix);
bool needsTranscodedPlayback(const QString &suffix);
QString storedAudioSuffix(const QString &suffix);
} // namespace relaxer
