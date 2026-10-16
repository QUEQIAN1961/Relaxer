#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QStringList>
#include <QVector>

namespace relaxer::ai::lyrics {

struct SourceLine {
    int id = -1;
    int sourceRow = -1;
    qint64 timestampMs = -1;
    QStringList timestampPrefixes;
    QString text;
};

struct PreparedLyrics {
    QVector<SourceLine> lines;
    QByteArray sourceDigest;
    bool timed = false;
    bool instrumental = false;
    bool bilingual = false;
    QString error;
    bool ok() const { return error.isEmpty() && !lines.isEmpty(); }
};

struct ValidationResult {
    QStringList translations;
    QString error;
    bool ok() const { return error.isEmpty(); }
};

PreparedLyrics prepare(const QByteArray &source, const QString &localTitle = {}, const QString &localArtist = {});
QString systemPrompt();
QString blockedLinePlaceholder();
QJsonArray chatMessages(const QString &targetLanguage,
                        const QString &stylePrompt, const PreparedLyrics &lyrics);
ValidationResult validateResponse(const QString &content, const PreparedLyrics &lyrics);
QByteArray merge(const QByteArray &source, const PreparedLyrics &lyrics, const QStringList &translations,
                 QString *error = nullptr);
qint64 inputTokenUpperBound(const QJsonArray &messages);

} // namespace relaxer::ai::lyrics
