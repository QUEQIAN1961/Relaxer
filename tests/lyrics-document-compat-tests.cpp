#include "library.h"
#include "lyrics-document-reader.h"
#include "lyrics-replacement-policy.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace relaxer;

namespace {
bool writeFile(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
} // namespace

class LyricsDocumentCompatibilityTests final : public QObject {
    Q_OBJECT

  private slots:
    void libraryUpdateKeepsItsSaveAndInstrumentalContract() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        Track track;
        track.hash = "lyrics-document-import";
        track.file = "Music/fixture.mp3";
        track.title = "文档导入";
        track.artist = "测试歌手";
        const auto id = library.insert(track);
        QVERIFY(id > 0);

        const auto source = dir.filePath(QString::fromUtf8("选择的歌词.lrc"));
        const QByteArray original("[00:01]原文\n[00:02]translation\n");
        QVERIFY(writeFile(source, original));
        QVERIFY(library.updateLyrics(id, source));

        const auto storedTrack = library.track(id);
        QVERIFY(!storedTrack.lyrics.isEmpty());
        const auto storedPath = library.absolute(storedTrack.lyrics);
        const auto document = LyricsDocumentReader::readDocument(storedPath);
        QVERIFY(document.file.opened);
        QCOMPARE(document.file.bytes, original);
        QVERIFY(document.lyrics.timed);
        QCOMPARE(document.lyrics.lines.size(), 2);
        QVERIFY(!library.isInstrumental(id));

        // D2-01 turned this compatible entry point into a guarded save: an
        // unattended call must refuse to overwrite different existing lyrics and
        // must report why. Overwriting requires an explicit confirmation, which
        // the shared service turns into the same checked write used below (the
        // service itself is exercised by relaxer_lyrics_replacement_integration).
        const auto instrumentalSource = dir.filePath("instrumental.txt");
        const QByteArray candidate = QStringLiteral("纯音乐，请欣赏").toUtf8();
        QVERIFY(writeFile(instrumentalSource, candidate));
        QVERIFY(!library.updateLyrics(id, instrumentalSource));
        QVERIFY(!library.lastError().isEmpty());
        QCOMPARE(library.track(id).lyrics, storedTrack.lyrics);
        QCOMPARE(LyricsDocumentReader::readDocument(storedPath).file.bytes, original);

        const auto policy = LyricsReplacementPolicy::analyze(original, candidate);
        QVERIFY(policy.valid);
        QVERIFY(policy.confirmationRequired);
        const auto snapshot = library.lyricsSaveSnapshot(id);
        QVERIFY(snapshot.valid);
        QVERIFY(library.saveLyricsChecked(id, policy.content, snapshot, true));
        const auto instrumental = LyricsDocumentReader::readDocument(library.absolute(library.track(id).lyrics));
        QVERIFY(instrumental.lyrics.instrumental);
        QVERIFY(library.isInstrumental(id));
    }

    void missingOrOversizeSourceDoesNotReplaceSavedLyrics() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        Track track;
        track.hash = "lyrics-document-failure";
        track.file = "Music/failure.mp3";
        track.title = "失败路径";
        const auto id = library.insert(track);
        QVERIFY(id > 0);
        const QByteArray original("[00:01]keep\n");
        QVERIFY(library.saveLyrics(id, original));
        const auto previousPath = library.track(id).lyrics;
        const auto previousFile = library.absolute(previousPath);

        QVERIFY(!library.updateLyrics(id, dir.filePath("missing.lrc")));
        QCOMPARE(library.track(id).lyrics, previousPath);
        auto saved = LyricsDocumentReader::readFile(previousFile);
        QVERIFY(saved.opened);
        QCOMPARE(saved.bytes, original);

        const auto large = dir.filePath("oversize.lrc");
        QByteArray oversized("[00:01]large\n");
        oversized.resize(int(LyricsDocumentReader::MaximumBytes + 1));
        QVERIFY(writeFile(large, oversized));
        QVERIFY(!library.updateLyrics(id, large));
        QCOMPARE(library.track(id).lyrics, previousPath);
        saved = LyricsDocumentReader::readFile(previousFile);
        QCOMPARE(saved.bytes, original);
    }
};

QTEST_GUILESS_MAIN(LyricsDocumentCompatibilityTests)
#include "lyrics-document-compat-tests.moc"
