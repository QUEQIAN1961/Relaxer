#include "lyrics-document-reader.h"

#include <QFile>

namespace relaxer {

LyricsDocumentReader::FileResult LyricsDocumentReader::readFile(const QString &path, qint64 maximumBytes,
                                                                  OversizePolicy policy) {
    FileResult result;
    if (maximumBytes < 0) {
        result.error = QStringLiteral("歌词文件读取上限无效");
        return result;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
    result.opened = true;

    if (file.size() > maximumBytes) {
        result.tooLarge = true;
        if (policy == OversizePolicy::Reject) {
            result.error = QStringLiteral("歌词文件超过 4 MB");
            return result;
        }
    }

    result.bytes = policy == OversizePolicy::ReadPrefix ? file.read(maximumBytes) : file.readAll();
    if (file.error() != QFileDevice::NoError)
        result.error = file.errorString();
    result.complete = file.error() == QFileDevice::NoError && file.atEnd();
    return result;
}

LyricsDocumentReader::DocumentResult LyricsDocumentReader::readDocument(const QString &path, qint64 maximumBytes,
                                                                          OversizePolicy policy) {
    DocumentResult result;
    result.file = readFile(path, maximumBytes, policy);
    if (result.file.opened && (policy == OversizePolicy::ReadPrefix || !result.file.tooLarge))
        result.lyrics = Lyrics::parse(result.file.bytes);
    return result;
}

} // namespace relaxer
