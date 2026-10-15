#pragma once
#include <QString>
#include <QVector>
namespace relaxer {
struct LyricLine {
    qint64 time;
    QString text;
};
class Lyrics {
  public:
    QVector<LyricLine> lines;
    bool timed = false;
    bool instrumental = false;
    bool bilingual = false;
    static Lyrics parse(const QByteArray &data);
    int indexAt(qint64 ms) const;
};
} // namespace relaxer
