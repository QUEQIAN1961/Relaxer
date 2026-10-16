#pragma once

#include "lyrics.h"

#include <QByteArray>
#include <QString>

namespace relaxer {

// A synchronous, stateless reader for local lyric files. Callers continue to
// own path selection, persistence, task lifetime, and thread choice.
class LyricsDocumentReader final {
  public:
    static constexpr qint64 MaximumBytes = 4 * 1024 * 1024;

    enum class OversizePolicy { Reject, ReadPrefix };

    struct FileResult {
        QByteArray bytes;
        QString error;
        bool opened = false;
        bool tooLarge = false;
        bool complete = false;
    };

    struct DocumentResult {
        FileResult file;
        Lyrics lyrics;
    };

    static FileResult readFile(const QString &path, qint64 maximumBytes = MaximumBytes,
                               OversizePolicy policy = OversizePolicy::Reject);
    static DocumentResult readDocument(const QString &path, qint64 maximumBytes = MaximumBytes,
                                       OversizePolicy policy = OversizePolicy::Reject);
};

} // namespace relaxer
