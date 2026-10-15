#include "browser-translation.h"
#include "lyric-remote.h"

#include <QDir>
#include <QJsonDocument>
#include <QPointer>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>

using namespace relaxer;

class RelaxerServicesTests : public QObject {
    Q_OBJECT

private slots:
    void browserTranslationHonorsQObjectLifetimeAndCancel() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        QPointer<BrowserTranslation> guarded;
        {
            QObject owner;
            auto *browser = new BrowserTranslation(directory.path(), &owner);
            guarded = browser;
            QCOMPARE(browser->parent(), &owner);

            browser->cancel();
            QVERIFY(!browser->waiting());
        }

        QVERIFY(guarded.isNull());
        QVERIFY(!QDir(directory.filePath("browser-translation")).exists());
    }

    void remoteLyricsSearchAndTimedTranslationParsing() {
        const auto request = lyric_remote::begin(2, "Test Song", "Test Artist", 183000);
        QCOMPARE(request.url.host(), QString("music.163.com"));
        QCOMPARE(QUrlQuery(request.url).queryItemValue("s"), QString("Test Song Test Artist"));

        const QByteArray searchBody = R"json({"code":200,"result":{"songs":[{"id":12345,"name":"Test Song","artists":[{"name":"Test Artist"}],"album":{"name":"Test Album"},"dt":183000}]}})json";
        const auto search = lyric_remote::consume(2, "search", searchBody, "Test Song", "Test Artist", 183000);
        QVERIFY(search.again);
        QVERIFY(!search.failed);
        QCOMPARE(search.step.url.host(), QString("music.163.com"));
        QCOMPARE(search.step.url.path(), QString("/api/song/lyric"));
        QCOMPARE(QUrlQuery(search.step.url).queryItemValue("id"), QString("12345"));
        QCOMPARE(search.matched.value("trackName").toString(), QString("Test Song"));
        QCOMPARE(search.matched.value("artistName").toString(), QString("Test Artist"));
        QCOMPARE(search.durationMs, qint64(183000));

        const QByteArray lyricBody = R"json({"lrc":{"lyric":"[00:01.00]hello\n[00:03.00]world"},"tlyric":{"lyric":"[00:01.00]你好\n[00:03.00]世界"}})json";
        const auto lyrics = lyric_remote::consume(2, "lyric", lyricBody, "Test Song", "Test Artist", 183000);
        QVERIFY(!lyrics.failed);
        QCOMPARE(lyrics.records.size(), 1);
        const auto record = lyrics.records.first().toObject();
        QCOMPARE(record.value("source").toString(), QString("网易云"));
        QCOMPARE(record.value("duration").toDouble(), 183.0);
        QCOMPARE(record.value("syncedLyrics").toString(),
                 QString("[00:01.00]hello\n[00:01.00]你好\n[00:03.00]world\n[00:03.00]世界"));
    }
};

QTEST_GUILESS_MAIN(RelaxerServicesTests)
#include "services-tests.moc"
