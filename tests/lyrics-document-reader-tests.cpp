#include "lyrics-document-reader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

using namespace relaxer;

namespace {
bool writeFile(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

void compareLyrics(const Lyrics &actual, const Lyrics &expected) {
    QCOMPARE(actual.timed, expected.timed);
    QCOMPARE(actual.instrumental, expected.instrumental);
    QCOMPARE(actual.bilingual, expected.bilingual);
    QCOMPARE(actual.lines.size(), expected.lines.size());
    for (qsizetype i = 0; i < actual.lines.size(); ++i) {
        QCOMPARE(actual.lines[i].time, expected.lines[i].time);
        QCOMPARE(actual.lines[i].text, expected.lines[i].text);
    }
}

struct CurrentDirectoryRestorer {
    QString previous = QDir::currentPath();
    ~CurrentDirectoryRestorer() { QDir::setCurrent(previous); }
};
} // namespace

class LyricsDocumentReaderTests final : public QObject {
    Q_OBJECT

  private slots:
    void readsAndParsesExistingFormats() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QString::fromUtf8("歌词 special (1).lrc"));
        const QByteArray bytes("[offset:-50]\r\n[00:01.00]原文\r\n[00:01.00]translation\r\n[00:02]こんにちは\r\n");
        QVERIFY(writeFile(path, bytes));

        const auto result = LyricsDocumentReader::readDocument(path);
        QVERIFY(result.file.opened);
        QVERIFY(!result.file.tooLarge);
        QVERIFY(result.file.complete);
        QCOMPARE(result.file.bytes, bytes);
        compareLyrics(result.lyrics, Lyrics::parse(bytes));
        QCOMPARE(result.lyrics.lines.size(), 3);
        QCOMPARE(result.lyrics.lines[0].time, qint64(950));
        QCOMPARE(result.lyrics.lines[1].time, qint64(950));

        CurrentDirectoryRestorer restore;
        QVERIFY(QDir::setCurrent(dir.path()));
        const auto relative = LyricsDocumentReader::readDocument(QString::fromUtf8("歌词 special (1).lrc"));
        QVERIFY(relative.file.opened);
        QCOMPARE(relative.file.bytes, bytes);
        compareLyrics(relative.lyrics, result.lyrics);
    }

    void encodingAndEmptyFileMatchFrozenParser() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QByteArray utf8Bom("\xef\xbb\xbf", 3);
        const QByteArray utf8 = utf8Bom + QByteArray("[00:01]中文");
        const auto utf8Path = dir.filePath("utf8.lrc");
        QVERIFY(writeFile(utf8Path, utf8));
        auto result = LyricsDocumentReader::readDocument(utf8Path);
        QCOMPARE(result.lyrics.lines.size(), 1);
        compareLyrics(result.lyrics, Lyrics::parse(utf8));

        QStringEncoder utf16(QStringConverter::Utf16LE);
        const QByteArray le = QByteArray("\xff\xfe", 2) + utf16(QString("[00:02]日本語"));
        const auto lePath = dir.filePath("utf16-le.lrc");
        QVERIFY(writeFile(lePath, le));
        result = LyricsDocumentReader::readDocument(lePath);
        compareLyrics(result.lyrics, Lyrics::parse(le));

        const auto emptyPath = dir.filePath("empty.lrc");
        QVERIFY(writeFile(emptyPath, {}));
        result = LyricsDocumentReader::readDocument(emptyPath);
        QVERIFY(result.file.opened);
        QVERIFY(result.file.complete);
        QVERIFY(result.file.bytes.isEmpty());
        QVERIFY(result.lyrics.lines.isEmpty());
    }

    void missingFileReportsFailureWithoutLyrics() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto result = LyricsDocumentReader::readDocument(dir.filePath("absent.lrc"));
        QVERIFY(!result.file.opened);
        QVERIFY(!result.file.error.isEmpty());
        QVERIFY(result.file.bytes.isEmpty());
        QVERIFY(result.lyrics.lines.isEmpty());
    }

    void rejectAndPrefixPoliciesRetainB4CallSiteSemantics() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("large.lrc");
        QByteArray bytes("[00:01]kept\n");
        bytes.resize(int(LyricsDocumentReader::MaximumBytes));
        bytes += "[00:02]outside prefix\n";
        QVERIFY(writeFile(path, bytes));

        const auto rejected = LyricsDocumentReader::readDocument(path);
        QVERIFY(rejected.file.opened);
        QVERIFY(rejected.file.tooLarge);
        QVERIFY(!rejected.file.error.isEmpty());
        QVERIFY(rejected.file.bytes.isEmpty());
        QVERIFY(rejected.lyrics.lines.isEmpty());

        const auto prefix = LyricsDocumentReader::readDocument(
            path, LyricsDocumentReader::MaximumBytes, LyricsDocumentReader::OversizePolicy::ReadPrefix);
        QVERIFY(prefix.file.opened);
        QVERIFY(prefix.file.tooLarge);
        QVERIFY(!prefix.file.complete);
        QCOMPARE(prefix.file.bytes.size(), int(LyricsDocumentReader::MaximumBytes));
        compareLyrics(prefix.lyrics, Lyrics::parse(prefix.file.bytes));
        QCOMPARE(prefix.lyrics.lines.size(), 1);
        QCOMPARE(prefix.lyrics.lines[0].time, qint64(1000));
        QCOMPARE(prefix.lyrics.lines[0].text, QString("kept"));
    }

    void independentConcurrentReadsAreConsistent() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("parallel.lrc");
        const QByteArray bytes("[00:01]first\n[00:02]second\n");
        QVERIFY(writeFile(path, bytes));
        constexpr int workers = 6;
        LyricsDocumentReader::DocumentResult results[workers];
        QThread *threads[workers]{};
        for (int i = 0; i < workers; ++i) {
            threads[i] = QThread::create([&, i] { results[i] = LyricsDocumentReader::readDocument(path); });
            QVERIFY(threads[i]);
            threads[i]->start();
        }
        for (auto *thread : threads) {
            QVERIFY(thread->wait(5000));
            delete thread;
        }
        for (const auto &result : results) {
            QVERIFY(result.file.opened);
            QCOMPARE(result.file.bytes, bytes);
            compareLyrics(result.lyrics, Lyrics::parse(bytes));
        }
    }
};

QTEST_GUILESS_MAIN(LyricsDocumentReaderTests)
#include "lyrics-document-reader-tests.moc"
