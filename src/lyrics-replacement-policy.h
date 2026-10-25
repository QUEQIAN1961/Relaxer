#pragma once
#include <QByteArray>
#include <QString>

namespace relaxer {
enum class LyricsReplacementChoice { SafeSave, Replace, KeepExisting, Cancel };

// Pure value policy: no files, database, tasks, or frontend objects are touched.
struct LyricsReplacementDecision {
    bool valid = false;
    bool existingTranslation = false;
    bool equivalent = false;
    bool preservesTranslation = false;
    bool confirmationRequired = false;
    QString reason;
    QByteArray content;
};
class LyricsReplacementPolicy final {
public:
    static LyricsReplacementDecision analyze(const QByteArray &existing, const QByteArray &candidate);
};
}
