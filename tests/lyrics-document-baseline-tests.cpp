#include "lyrics.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QStringConverter>
#include <QtTest>

using namespace relaxer;

namespace {
constexpr qint64 kLegacyReadLimit = 4 * 1024 * 1024;

QByteArray legacyCompleteRead(const QString &path, bool *accepted = nullptr) {
    QFile file(path);
    const bool ok = file.open(QIODevice::ReadOnly) && file.size() <= kLegacyReadLimit;
    if (accepted)
        *accepted = ok;
    return ok ? file.readAll() : QByteArray();
}

QByteArray legacyPlaybackRead(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.read(kLegacyReadLimit) : QByteArray();
}

bool writeFile(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
} // namespace

class LyricsDocumentBaselineTests final : public QObject {
    Q_OBJECT

  private slots:
    void timedDuplicatesOffsetAndStableOrder() {
        const auto parsed = Lyrics::parse(
            "[ar:artist]\r\n[offset:120]\r\n[00:01.00][00:01.00][00:03.250]same\r\n"
            "[00:02.00]next\r\n\r\n");
        QVERIFY(parsed.timed);
        QVERIFY(!parsed.instrumental);
        QCOMPARE(parsed.lines.size(), 4);
        QCOMPARE(parsed.lines[0].time, qint64(1120));
        QCOMPARE(parsed.lines[1].time, qint64(1120));
        QCOMPARE(parsed.lines[2].time, qint64(2120));
        QCOMPARE(parsed.lines[3].time, qint64(3370));
        QCOMPARE(parsed.lines[0].text, QString("same"));
        QCOMPARE(parsed.lines[1].text, QString("same"));
        QCOMPARE(parsed.lines[2].text, QString("next"));
        QCOMPARE(parsed.lines[3].text, QString("same"));
    }

    void plainTextBlankLinesAndMixedInput() {
        const auto plain = Lyrics::parse("\n  first line  \r\n\r\n second line \n");
        QVERIFY(!plain.timed);
        QCOMPARE(plain.lines.size(), 2);
        QCOMPARE(plain.lines[0].time, qint64(-1));
        QCOMPARE(plain.lines[0].text, QString("first line"));
        QCOMPARE(plain.lines[1].text, QString("second line"));

        // B4 discards untimed lines once at least one timestamp is present.
        const auto mixed = Lyrics::parse("plain line\n[00:02]timed line\n");
        QVERIFY(mixed.timed);
        QCOMPARE(mixed.lines.size(), 1);
        QCOMPARE(mixed.lines[0].text, QString("timed line"));
    }

    void bilingualAndMultilingualRows() {
        const auto parsed = Lyrics::parse(
            "relaxer-bilingual\n[00:01.00]原文\n[00:01.00]translation\n"
            "[00:02.00]こんにちは\n[00:03.00]English\n");
        QVERIFY(parsed.timed);
        QVERIFY(parsed.bilingual);
        QCOMPARE(parsed.lines.size(), 4);
        QCOMPARE(parsed.lines[0].text, QString("原文"));
        QCOMPARE(parsed.lines[1].text, QString("translation"));
        QCOMPARE(parsed.lines[2].text, QString("こんにちは"));
        QCOMPARE(parsed.lines[3].text, QString("English"));
        QCOMPARE(parsed.lines[0].time, parsed.lines[1].time);
    }

    void utf8BomAndUtf16() {
        const QByteArray utf8Bom("\xef\xbb\xbf", 3);
        const auto utf8 = Lyrics::parse(utf8Bom + QByteArray("[00:01]中文 lyric"));
        QCOMPARE(utf8.lines.size(), 1);
        QCOMPARE(utf8.lines[0].text, QString("中文 lyric"));

        QStringEncoder le(QStringConverter::Utf16LE);
        const auto utf16le = Lyrics::parse(QByteArray("\xff\xfe", 2) + le(QString("[00:02]日本語")));
        QCOMPARE(utf16le.lines.size(), 1);
        QCOMPARE(utf16le.lines[0].time, qint64(2000));
        QCOMPARE(utf16le.lines[0].text, QString("日本語"));

        QStringEncoder be(QStringConverter::Utf16BE);
        const auto utf16be = Lyrics::parse(QByteArray("\xfe\xff", 2) + be(QString("[00:03]English")));
        QCOMPARE(utf16be.lines.size(), 1);
        QCOMPARE(utf16be.lines[0].time, qint64(3000));
        QCOMPARE(utf16be.lines[0].text, QString("English"));
    }

    void invalidUtf8FallsBackToLocal8Bit() {
        QByteArray legacyBytes;
        legacyBytes.append(char(0xe9));
        legacyBytes.append('A'); // A complete invalid UTF-8 sequence exercises the B4 fallback.
        const QByteArray input = QByteArray("[00:01]") + legacyBytes;
        const auto parsed = Lyrics::parse(input);
        QString expected = QString::fromLocal8Bit(input);
        expected.remove("[00:01]");
        QCOMPARE(parsed.lines.size(), 1);
        QCOMPARE(parsed.lines[0].text, expected);
    }

    void malformedAndLargeButRepresentableTimes() {
        const auto malformed = Lyrics::parse("[ar:artist]\n[ab:cd]literal\n");
        QVERIFY(!malformed.timed);
        QCOMPARE(malformed.lines.size(), 1);
        QCOMPARE(malformed.lines[0].text, QString("[ab:cd]literal"));

        const auto large = Lyrics::parse("[999999:99.999]late\n");
        QVERIFY(large.timed);
        QCOMPARE(large.lines.size(), 1);
        QCOMPARE(large.lines[0].time, qint64(60000039999LL));

        const auto empty = Lyrics::parse(QByteArray());
        QVERIFY(empty.lines.isEmpty());
        QVERIFY(!empty.timed);
        QVERIFY(!empty.instrumental);
    }

    void unicodePathAndMissingFileSemantics() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QString::fromUtf8("歌词 special (1).lrc"));
        QVERIFY(writeFile(path, QByteArray("[00:01]path lyric\n")));
        bool accepted = false;
        const auto bytes = legacyCompleteRead(path, &accepted);
        QVERIFY(accepted);
        const auto parsed = Lyrics::parse(bytes);
        QCOMPARE(parsed.lines.size(), 1);
        QCOMPARE(parsed.lines[0].text, QString("path lyric"));

        const auto missing = dir.filePath("missing.lrc");
        const auto missingBytes = legacyCompleteRead(missing, &accepted);
        QVERIFY(!accepted);
        QVERIFY(missingBytes.isEmpty());
        QVERIFY(Lyrics::parse(missingBytes).lines.isEmpty());
    }

    void legacyOversizePoliciesDifferByCallSite() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("large.lrc");
        QByteArray payload("[00:01]kept\n");
        payload.resize(int(kLegacyReadLimit));
        payload += "[00:02]outside prefix\n";
        QVERIFY(writeFile(path, payload));

        bool accepted = true;
        QVERIFY(legacyCompleteRead(path, &accepted).isEmpty());
        QVERIFY(!accepted);

        const auto prefix = legacyPlaybackRead(path);
        QCOMPARE(prefix.size(), int(kLegacyReadLimit));
        const auto parsed = Lyrics::parse(prefix);
        QVERIFY(parsed.timed);
        QCOMPARE(parsed.lines.size(), 1);
        QCOMPARE(parsed.lines[0].time, qint64(1000));
        QCOMPARE(parsed.lines[0].text, QString("kept"));
    }
};

QTEST_GUILESS_MAIN(LyricsDocumentBaselineTests)
#include "lyrics-document-baseline-tests.moc"
