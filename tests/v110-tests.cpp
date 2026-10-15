#include "importer.h"
#include "lyric-text.h"
#include <QtTest>
using namespace relaxer;
class V110Tests : public QObject {
    Q_OBJECT
  private slots:
    void originalCredits() {
        const auto marked = originalCredit("夜曲 (原唱: 周杰伦)");
        QVERIFY(marked.found);
        QCOMPARE(marked.artist, QString("周杰伦"));
        QVERIFY(marked.title.contains("夜曲"));
        const auto cover = originalCredit("Song (cover: Adele)");
        QVERIFY(cover.found);
        QCOMPARE(cover.artist, QString("Adele"));
        QVERIFY(!originalCredit("普通歌名").found);
        QVERIFY(originalCredit("歌名 翻自 原歌手").found);
    }
    void traditionalToSimplified() {
        QCOMPARE(toSimplified(QString("國")), QString("国"));
        QVERIFY(containsTraditional(QString("國樂")));
        QVERIFY(!containsTraditional(QString("国乐")));
        QVERIFY(lyricsAreChinese("这是一首中文歌词"));
        QVERIFY(!lyricsAreChinese("hello from another language"));
        const auto simplified = simplifyTraditionalLyrics("[00:01.00]國\n[00:02.00]樂\n");
        const auto text = QString::fromUtf8(simplified);
        QVERIFY(text.contains("国"));
        QVERIFY(text.contains("乐"));
        QVERIFY(!text.contains("國"));
    }
    void bilingualAttachment() {
        const QByteArray original("[00:01.00]Hello\n[00:02.00]World\n");
        QVERIFY(!lyricsIncludeTranslation(original));
        const auto attached = attachTranslations(original, {"你好", "世界"});
        QVERIFY(lyricsIncludeTranslation(attached));
        QCOMPARE(translationLanguagePair("こんにちは"), QString("ja|zh-CN"));
        QCOMPARE(translationLanguagePair("안녕하세요"), QString("ko|zh-CN"));
        QCOMPARE(lyricTexts(original).size(), 2);
    }
    void audioFormats() {
        QVERIFY(isMidiSuffix("mid"));
        QVERIFY(isMidiSuffix("KAR"));
        QVERIFY(playsWithoutFfmpeg("mp3"));
        QVERIFY(playsWithoutFfmpeg("flac"));
        QVERIFY(playsWithoutFfmpeg("midi"));
        QVERIFY(!playsWithoutFfmpeg("ogg"));
        QVERIFY(needsTranscodedPlayback("opus"));
        QVERIFY(needsTranscodedPlayback("ape"));
        QVERIFY(needsTranscodedPlayback("wv"));
        QVERIFY(!needsTranscodedPlayback("wav"));
        QCOMPARE(storedAudioSuffix("ape"), QString("flac"));
        QCOMPARE(storedAudioSuffix("opus"), QString("flac"));
        QCOMPARE(storedAudioSuffix("midi"), QString("midi"));
        QCOMPARE(storedAudioSuffix("mp3"), QString("mp3"));
        const auto extensions = Importer::extensions();
        for (const char *suffix : {"mid", "midi", "wv", "tta", "mpc", "ac3", "amr", "mka", "m4b"})
            QVERIFY2(extensions.contains(QString::fromLatin1(suffix)), suffix);
    }
};
QTEST_APPLESS_MAIN(V110Tests)
#include "v110-tests.moc"
