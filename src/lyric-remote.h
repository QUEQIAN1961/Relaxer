#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QtGlobal>
#include <QVector>
namespace relaxer::lyric_remote {
struct Step {
    QUrl url;
    QList<QPair<QByteArray, QByteArray>> headers;
};
struct Detail {
    Step step;
    QJsonObject matched;
    qint64 durationMs = -1;
};
struct Follow {
    bool again = false;
    bool failed = false;
    Step step;
    QJsonArray records;
    QJsonObject matched;
    QString failure;
    qint64 durationMs = -1;
    QVector<Detail> remaining;
};
Step begin(int kind, const QString &title, const QString &artist, qint64 durationMs);
Follow consume(int kind, const QByteArray &phase, const QByteArray &body, const QString &title,
               const QString &artist, qint64 durationMs);
} // namespace relaxer::lyric_remote
