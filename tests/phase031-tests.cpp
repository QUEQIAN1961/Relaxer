#include "lyricslookup.h"
#include "browser-translation.h"
#include "lyrics-document-reader.h"
#include "lyric-text.h"
#include "lyrics.h"
#include "lyric-remote.h"
#include "window.h"
#include "lookup-drive.h"
#include <QCheckBox>
#include <QDateTime>
#include <QDataStream>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkReply>
#include <QPushButton>
#include <QScrollBar>
#include <QTextDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QtTest>
#include <algorithm>
using namespace relaxer;
class Phase031Server : public QTcpServer {
public:
    int requests = 0;
    bool hold = false;
    QList<QUrl> urls;
    QList<QPointer<QTcpSocket>> held;
    bool relaxerOnly = true;
    std::function<QPair<int, QByteArray>(const QUrl &)> reply = [](const QUrl &) { return qMakePair(404, QByteArray("{}")); };
    Phase031Server() {
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                auto bytes = socket->property("buffer").toByteArray() + socket->readAll();
                socket->setProperty("buffer", bytes);
                if (!bytes.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
                socket->setProperty("answered", true);
                if (relaxerOnly && !bytes.contains("User-Agent: Relaxer/") && !bytes.contains("User-Agent: Mozilla/")) {
                    socket->disconnectFromHost();
                    return;
                }
                const QUrl url = QUrl::fromEncoded("http://localhost" + bytes.split(' ').value(1));
                if (url.path().isEmpty() || url.path() == "/") {
                    socket->disconnectFromHost();
                    return;
                }
                urls << url;
                ++requests;
                if (hold) {
                    held << socket;
                    return;
                }
                const auto response = reply(url);
                send(socket, response.first, response.second);
            });
        });
    }
    static void send(QTcpSocket *socket, int code, const QByteArray &body) {
        if (!socket) return;
        socket->write("HTTP/1.1 " + QByteArray::number(code) + " Result\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }
    QUrl endpoint(const QString &path = "/api") const { return QUrl(QString("http://127.0.0.1:%1%2").arg(serverPort()).arg(path)); }
};
class Phase031Tests : public QObject {
    Q_OBJECT
    static Track seed(Library &library, const QString &title, const QString &hash, const QString &artist = "歌手甲") {
        Track track;
        track.title = title;
        track.hash = hash;
        track.artist = artist;
        track.album = "测试专辑";
        track.file = "Music/" + hash + ".wav";
        track.duration = 10000;
        track.coverOrigin = "none";
        QFile file(library.absolute(track.file));
        if (!file.open(QIODevice::WriteOnly)) return {};
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData("RIFF", 4);
        out << quint32(160036);
        out.writeRawData("WAVEfmt ", 8);
        out << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
        out.writeRawData("data", 4);
        out << quint32(160000);
        out.writeRawData(QByteArray(160000, '\0').constData(), 160000);
        file.close();
        track.id = library.insert(track);
        return library.track(track.id);
    }
    static QByteArray synced(const QString &line, const QString &line2) {
        return QString("[00:00.00]%1\n[00:02.00]%2").arg(line, line2).toUtf8();
    }
    static QJsonObject record(const QString &title, const QString &artist, double seconds = 10) {
        return {{"trackName", title}, {"artistName", artist}, {"albumName", "测试专辑"}, {"duration", seconds}, {"syncedLyrics", QString::fromUtf8(synced("hello line", "second line"))}, {"plainLyrics", "hello line\nsecond line"}};
    }
    static int rowForId(QWidget &dialog,qint64 id) {
        auto *rows=dialog.findChild<QListWidget *>("lyricsTracks");
        for(int r=0;r<rows->count();++r)if(rows->item(r)->data(Qt::UserRole).toLongLong()==id)return r;
        return -1;
    }
    static QString statusAt(QWidget &dialog, int row) {
        return dialog.findChild<QListWidget *>("lyricsTracks")->item(row)->data(Qt::UserRole + 2).toString();
    }
private slots:
    void cancelledBrowserValidationCannotFailANewerTask(){
        QTemporaryDir dir;BrowserTranslation browser(dir.path());QSignalSpy done(&browser,&BrowserTranslation::completed);
        browser.translate(1,"bing",QString(4001,'a'),"en");browser.cancel();QCoreApplication::processEvents();QCOMPARE(done.size(),0);
        browser.translate(2,"bing",QString(4001,'b'),"en");browser.translate(3,"baidu",QString(4001,'c'),"en");
        QTRY_COMPARE(done.size(),1);QCOMPARE(done.front()[0].toULongLong(),quint64(3));QVERIFY(done.front()[2].toString().contains("4000"));
        QVERIFY(!QDir(dir.filePath("browser-translation")).exists());
    }
    void browserTranslationInfrastructureRemainsIsolatedFromUserWorkflow(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        QVERIFY(library.setSetting("lyricBrowserProvider","baidu"));QVERIFY(library.setSetting("translateLyrics",true));
        LyricsLookup dialog(&library,{},nullptr,QUrl("http://127.0.0.1:9/api"));
        QVERIFY(!dialog.findChild<QComboBox *>("lyricsTranslationProvider"));
        QVERIFY(!dialog.findChild<QCheckBox *>("lyricsAutoTranslate"));
        QVERIFY(!dialog.findChild<QPushButton *>("lyricsShowTranslationBrowser"));
        QVERIFY(!dialog.findChild<QPushButton *>("lyricsTranslationResume"));
        QVERIFY(dialog.findChild<QPushButton *>("lyricsTranslate")->text().contains("AI"));
        QVERIFY(dialog.findChild<QLabel *>("lyricsCloudTranslationNotice"));
        QCOMPARE(library.setting("lyricBrowserProvider").toString(),QString("baidu"));
        QCOMPARE(library.setting("translateLyrics").toBool(),true);
        const QString source="a & b + c / # ? 百分之100%";const auto url=BrowserTranslation::pageUrl("baidu",source,"ja");
        QCOMPARE(url.host(),QString("fanyi.baidu.com"));QCOMPARE(QUrlQuery(url).queryItemValue("query",QUrl::FullyDecoded),source);QCOMPARE(QUrlQuery(url).queryItemValue("lang"),QString("jp2zh"));
        QCOMPARE(BrowserTranslation::pageUrl("bing",source,"en").host(),QString("www.bing.com"));
        QVERIFY(!QDir(dir.filePath("data/browser-translation")).exists());
    }
    void bilingualPreviewHasSeparatePanelsAndAlignedRows_data(){
        QTest::addColumn<bool>("dark");QTest::newRow("light")<<false;QTest::newRow("dark")<<true;
    }
    void bilingualPreviewHasSeparatePanelsAndAlignedRows(){
        QFETCH(bool,dark);
        struct RestorePalette{QPalette value;QString style;~RestorePalette(){qApp->setStyleSheet(style);qApp->setPalette(value);}}restore{qApp->palette(),qApp->styleSheet()};
        auto palette=qApp->palette();palette.setColor(QPalette::Base,dark?QColor("#19191e"):QColor("#ffffff"));palette.setColor(QPalette::Window,palette.color(QPalette::Base));palette.setColor(QPalette::Text,dark?Qt::white:Qt::black);palette.setColor(QPalette::WindowText,palette.color(QPalette::Text));palette.setColor(QPalette::Mid,dark?QColor("#34343c"):QColor("#e9e7eb"));qApp->setPalette(palette);
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library,"原文与译文对照","bilingual-preview");
        QStringList source,translated;
        for(int i=0;i<80;++i){source<<QString("[%1:%2.00]original lyric %3").arg(i/60,2,10,QChar('0')).arg(i%60,2,10,QChar('0')).arg(i);translated<<QString("第 %1 行译文").arg(i);}
        const auto original=source.join('\n').toUtf8();QVERIFY(library.saveLyrics(track.id,attachTranslations(original,translated)));
        QVERIFY(library.setSetting("theme",dark?1:0));Window host(&library);
        auto candidate=record(track.title,track.artist);candidate["syncedLyrics"]=QString::fromUtf8(original);QVERIFY(library.saveLookupCache(track.id,QJsonArray{candidate},false));
        LyricsLookup dialog(&library,{track.id},&host,QUrl("http://127.0.0.1:9/api"),{},true);dialog.resize(1200,760);dialog.show();dialog.findChild<QListWidget *>("lyricsTracks")->setCurrentRow(0);QTest::qWait(60);
        auto *left=dialog.findChild<QPlainTextEdit *>("lyricsPreview"),*right=dialog.findChild<QPlainTextEdit *>("lyricsTranslationPreview");QVERIFY(left&&right);
        QVERIFY(left->toPlainText().contains("original lyric 0"));QVERIFY(!left->toPlainText().contains("第 0 行译文"));QVERIFY(right->toPlainText().contains("第 0 行译文"));QVERIFY(!right->toPlainText().contains("original lyric"));
        QCOMPARE(left->document()->blockCount(),right->document()->blockCount());QVERIFY(left->isVisible()&&right->isVisible());
        auto *candidatePanel=dialog.findChild<QFrame *>("lyricsCandidatePanel"),*previewPanel=dialog.findChild<QFrame *>("lyricsPreviewPanel");QVERIFY(candidatePanel&&previewPanel);QVERIFY(!candidatePanel->isAncestorOf(previewPanel));QVERIFY(previewPanel->height()<=240);
        left->verticalScrollBar()->setValue(12);QCOMPARE(right->verticalScrollBar()->value(),left->verticalScrollBar()->value());
        right->verticalScrollBar()->setValue(4);QCOMPARE(left->verticalScrollBar()->value(),4);
        const auto output=qEnvironmentVariable("RELAXER_QA_DIR");if(!output.isEmpty()){QDir().mkpath(output);QVERIFY(dialog.grab().save(output+(dark?"/lyrics-dark.png":"/lyrics-light.png")));}
    }
    void lyricsSearchNeverStartsAutomaticCloudTranslation(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        const auto track=seed(library,"Search Without Translation","no-auto-translation");
        QVERIFY(library.setSetting("translateLyrics",true));
        Phase031Server search;QVERIFY(search.listen(QHostAddress::LocalHost));
        search.reply=[&](const QUrl &url){if(url.path().endsWith("/get"))return qMakePair(404,QByteArray("{}"));return qMakePair(200,QJsonDocument(QJsonArray{record(track.title,track.artist)}).toJson());};
        Phase031Server retiredWeb;retiredWeb.relaxerOnly=false;QVERIFY(retiredWeb.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&library,{track.id},nullptr,search.endpoint(),{},true);
        dialog.setProperty("lyricTranslateUrl",retiredWeb.endpoint("/translate"));dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(!library.track(track.id).lyrics.isEmpty(),8000);
        QTRY_VERIFY_WITH_TIMEOUT(statusAt(dialog,0).startsWith("已保存"),3000);
        QCOMPARE(retiredWeb.requests,0);
        const auto saved=LyricsDocumentReader::readFile(library.absolute(library.track(track.id).lyrics));
        QVERIFY(saved.opened&&saved.complete);QCOMPARE(saved.bytes,record(track.title,track.artist)["syncedLyrics"].toString().toUtf8());
        QVERIFY(!dialog.findChild<QCheckBox *>("lyricsAutoTranslate"));
    }    void initTestCase() { qApp->setQuitOnLastWindowClosed(false); qApp->setStyle("Fusion"); }
    void sourceLyricsDropTranslationLines() {
        const auto timed = QByteArray("[00:00.00]hello\n[00:00.00]你好\n[00:02.00]world\n[00:02.00]世界\n");
        QCOMPARE(sourceLyricsForTranslation(timed), QByteArray("[00:00.00]hello\n[00:02.00]world\n"));
        const auto plain = QByteArray("[re:relaxer-bilingual]\nhello\n你好\nworld\n世界");
        QCOMPARE(QString::fromUtf8(sourceLyricsForTranslation(plain)), QString("hello\nworld"));
        QCOMPARE(lyricTexts(sourceLyricsForTranslation(timed)), QStringList({"hello", "world"}));
    }
    void openingDoesNotSearchUntilClicked() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto track = seed(library, "未点击", "idle");
        Phase031Server server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&library, {track.id}, nullptr, server.endpoint());
        dialog.show();
        QTest::qWait(250);
        QCOMPARE(server.requests, 0);
        QCOMPARE(statusAt(dialog, 0), QString("等待查找"));
        QVERIFY(dialog.findChild<QPushButton *>("lyricsStart")->text() == "搜索歌词");
        relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(server.requests > 0, 3000);
    }
    void confirmCancelSendsNothingAndAcceptSearchesVisible() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto first = seed(library, "甲歌", "a");
        const auto second = seed(library, "乙歌", "b");
        Phase031Server server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.reply = [](const QUrl &url) { return qMakePair(url.path().endsWith("/get") ? 404 : 200, url.path().endsWith("/get") ? QByteArray("{}") : QByteArray("[]")); };
        LyricsLookup dialog(&library, {first.id, second.id}, nullptr, server.endpoint());
        dialog.show();
        QTimer::singleShot(0, &dialog, [] {
            for (auto *widget : QApplication::topLevelWidgets())
                if (widget->objectName() == "lyricsSearchAllConfirm")
                    widget->findChild<QAbstractButton *>("lyricsSearchAllCancel")->click();
        });
        dialog.findChild<QPushButton *>("lyricsStart")->click();
        QTest::qWait(200);
        QCOMPARE(server.requests, 0);
        QCOMPARE(statusAt(dialog, 0), QString("等待查找"));
        QCOMPARE(statusAt(dialog, 1), QString("等待查找"));
        relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(server.requests > 0, 3000);
        // ffprobe completion and per-track dispatch are asynchronous. The first
        // request does not imply that both tracks have already reached the server.
        const auto seen = [&](const QString &title) {
            return std::any_of(server.urls.begin(), server.urls.end(), [&](const QUrl &url) {
                return QUrlQuery(url).queryItemValue("track_name", QUrl::FullyDecoded) == title;
            });
        };
        QTRY_VERIFY_WITH_TIMEOUT(seen(first.title) && seen(second.title), 3000);
    }
    void oneSelectionSearchesOnlyThatSong() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto first = seed(library, "只搜这首", "only");
        const auto second = seed(library, "不要这首", "other");
        Phase031Server server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.hold = true;
        LyricsLookup dialog(&library, {first.id, second.id}, nullptr, server.endpoint());
        dialog.show();
        auto *tracks = dialog.findChild<QListWidget *>("lyricsTracks");
        tracks->setCurrentRow(0);
        dialog.findChild<QPushButton *>("lyricsStart")->click();
        QTRY_VERIFY_WITH_TIMEOUT(server.requests > 0, 3000);
        for (const auto &url : server.urls) QVERIFY(!QUrlQuery(url).queryItemValue("track_name", QUrl::FullyDecoded).contains(second.title));
        QCOMPARE(statusAt(dialog, 1), QString("等待查找"));
        dialog.findChild<QPushButton *>("lyricsStop")->click();
    }
    void missingFilterKeepsSessionSnapshotAndPinsExplicitSongs() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ready = seed(library, "已有", "ready");
        const auto missing = seed(library, "缺失", "missing");
        QVERIFY(library.saveLyrics(ready.id, synced("本地", "歌词")));
        LyricsLookup hidden(&library, {ready.id}, nullptr, QUrl("http://127.0.0.1:9/api"));
        hidden.show();
        QVERIFY(hidden.findChild<QCheckBox *>("lyricsMissingOnly")->isChecked());
        QVERIFY(hidden.findChild<QListWidget *>("lyricsTracks")->item(0)->isHidden());
        QCOMPARE(hidden.findChild<QLabel *>("lyricsMissingEmpty")->text(), QString("当前没有缺失歌词的歌曲，可取消勾选以查看全部歌曲。"));
        QVERIFY(hidden.findChild<QLabel *>("lyricsMissingEmpty")->isVisible());
        hidden.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);
        LyricsLookup again(&library, {ready.id}, nullptr, QUrl("http://127.0.0.1:9/api"));
        again.show();
        QVERIFY(again.findChild<QCheckBox *>("lyricsMissingOnly")->isChecked());
        LyricsLookup pinned(&library, {ready.id, missing.id}, nullptr, QUrl("http://127.0.0.1:9/api"), {}, true);
        pinned.show();
        QVERIFY(pinned.findChild<QCheckBox *>("lyricsMissingOnly")->isChecked());
        QVERIFY(!pinned.findChild<QListWidget *>("lyricsTracks")->item(0)->isHidden());
        QVERIFY(!pinned.findChild<QListWidget *>("lyricsTracks")->item(1)->isHidden());
    }
    void existingLyricsAreNotAutoReplacedAndFailureKeepsOffset() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto track = seed(library, "保留歌词", "keep");
        const auto old = synced("旧歌词", "第二行");
        QVERIFY(library.saveLyrics(track.id, old));
        QVERIFY(library.setLyricOffset(track.id, 800));
        Phase031Server server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.reply = [&](const QUrl &url) {
            if (url.path().endsWith("/get")) return qMakePair(404, QByteArray("{}"));
            return qMakePair(200, QJsonDocument(QJsonArray{record(track.title, track.artist)}).toJson());
        };
        LyricsLookup dialog(&library, {track.id}, nullptr, server.endpoint());
        dialog.show();
        dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);
        dialog.findChild<QCheckBox *>("lyricsReplace")->setChecked(true);
        relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(statusAt(dialog, 0).startsWith("需选择"), 8000);
        QFile file(library.absolute(library.track(track.id).lyrics));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), old);
        QCOMPARE(library.lyricOffset(track.id), 800);
        dialog.close();
        Phase031Server fail;
        QVERIFY(fail.listen(QHostAddress::LocalHost));
        fail.reply = [](const QUrl &) { return qMakePair(404, QByteArray("{}")); };
        LyricsLookup again(&library, {track.id}, nullptr, fail.endpoint());
        again.show();
        again.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);
        relaxerClickSearch(&again);
        QTRY_VERIFY_WITH_TIMEOUT(again.findChild<QPushButton *>("lyricsStart")->isEnabled(), 8000);
        QFile kept(library.absolute(library.track(track.id).lyrics));
        QVERIFY(kept.open(QIODevice::ReadOnly));
        QCOMPARE(kept.readAll(), old);
        QCOMPARE(library.lyricOffset(track.id), 800);
    }
    void freshNoResultCacheIsSearchedAgain() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto track = seed(library, "空缓存", "empty-cache");
        QVERIFY(library.saveLookupCache(track.id, {}, true));
        Phase031Server server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.reply = [](const QUrl &url) { return qMakePair(url.path().endsWith("/get") ? 404 : 200, url.path().endsWith("/get") ? QByteArray("{}") : QByteArray("[]")); };
        LyricsLookup dialog(&library, {track.id}, nullptr, server.endpoint());
        dialog.show();
        relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(server.requests > 0, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(statusAt(dialog, 0).contains("数据库无结果"), 8000);
    }
    void parenthesisQueryIsSentBeforeSlowRankingReturns() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto track = seed(library, "Clear (Shawn Wasabi Remix)", "clear", "Pusher,Mothica");
        Phase031Server server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.hold = true;
        LyricsLookup dialog(&library, {track.id}, nullptr, server.endpoint());
        dialog.setProperty("lyricScoreDelayMs", 4000);
        dialog.show();
        relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(server.held.size() >= 3, 4000);
        QVERIFY(!server.urls.isEmpty());
        const auto weak = QJsonDocument(QJsonArray{QJsonObject{{"trackName", "其他"}, {"artistName", "别人"}, {"albumName", ""}, {"duration", 80}, {"syncedLyrics", "[00:00.00]弱\n[00:02.00]候选"}, {"plainLyrics", "弱\n候选"}}}).toJson();
        Phase031Server::send(server.held.takeFirst(), 200, weak);
        QTRY_VERIFY_WITH_TIMEOUT(std::any_of(server.urls.begin(), server.urls.end(), [](const QUrl &url) { return QUrlQuery(url).queryItemValue("track_name", QUrl::FullyDecoded) == "Clear"; }), 800);
        dialog.findChild<QPushButton *>("lyricsStop")->click();
    }
    void missingAIConfigurationDoesNotFallBackToLegacyWebTranslation(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        const auto track=seed(library,"未配置 AI","no-ai-config");const auto original=synced("hello","world");QVERIFY(library.saveLyrics(track.id,original));
        QVERIFY(library.setSetting("translateLyrics",true));
        Phase031Server retiredWeb;retiredWeb.relaxerOnly=false;QVERIFY(retiredWeb.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&library,{track.id},nullptr,QUrl("http://127.0.0.1:9/api"));
        dialog.setProperty("lyricTranslateUrl",retiredWeb.endpoint("/translate"));dialog.show();
        dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);dialog.findChild<QListWidget *>("lyricsTracks")->setCurrentRow(0);
        dialog.findChild<QPushButton *>("lyricsNeedsTranslation")->click();
        QVERIFY(dialog.findChild<QPushButton *>("lyricsNeedsTranslation")->isChecked());
        dialog.findChild<QPushButton *>("lyricsTranslate")->click();
        QVERIFY(dialog.findChild<QLabel *>("lyricsStatus")->text().contains("AI 服务不可用"));
        QCOMPARE(retiredWeb.requests,0);
        const auto saved=LyricsDocumentReader::readFile(library.absolute(library.track(track.id).lyrics));QVERIFY(saved.opened);QCOMPARE(saved.bytes,original);
    }    void wholeSongTranslationRejectsMultipleSelectedTracks(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        const auto first=seed(library,"多选甲","multi-translation-a"),second=seed(library,"多选乙","multi-translation-b");
        QVERIFY(library.saveLyrics(first.id,synced("hello","world")));QVERIFY(library.saveLyrics(second.id,synced("goodbye","later")));
        Phase031Server retiredWeb;retiredWeb.relaxerOnly=false;QVERIFY(retiredWeb.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&library,{first.id,second.id},nullptr,QUrl("http://127.0.0.1:9/api"),{},true);
        dialog.setProperty("lyricTranslateUrl",retiredWeb.endpoint("/translate"));dialog.show();
        dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);dialog.findChild<QPushButton *>("lyricsMultiSelect")->click();
        dialog.findChild<QPushButton *>("lyricsSelectAll")->click();dialog.findChild<QPushButton *>("lyricsTranslate")->click();
        QVERIFY(!dialog.findChild<QPushButton *>("lyricsTranslate")->isEnabled());
        QVERIFY(dialog.findChild<QPushButton *>("lyricsTranslate")->toolTip().contains("一首"));
        QCOMPARE(dialog.findChild<QListWidget *>("lyricsTracks")->selectedItems().size(),2);
        QCOMPARE(retiredWeb.requests,0);
    }    void localLyricsAreCompleteAndStablyGroupedLast() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        const auto a=seed(library,"已有甲","local-a"),b=seed(library,"缺失甲","missing-a"),c=seed(library,"已有乙","local-b"),d=seed(library,"缺失乙","missing-b");
        QVERIFY(library.saveLyrics(a.id,synced("原文甲","原文乙")));QVERIFY(library.saveLyrics(c.id,synced("原文丙","原文丁")));
        LyricsLookup dialog(&library,{a.id,b.id,c.id,d.id},nullptr,QUrl("http://127.0.0.1:9/api"));dialog.show();
        auto *rows=dialog.findChild<QListWidget *>("lyricsTracks");
        QCOMPARE(rowForId(dialog,b.id),0);QCOMPARE(rowForId(dialog,d.id),1);QCOMPARE(rowForId(dialog,a.id),2);QCOMPARE(rowForId(dialog,c.id),3);
        QCOMPARE(rows->item(0)->data(Qt::UserRole+1).toInt(),0); // Waiting has its own neutral clock, never the failure cross.
        for(int r:{2,3}){QCOMPARE(rows->item(r)->data(Qt::UserRole+1).toInt(),2);QVERIFY(rows->item(r)->isHidden());}
        dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);
        for(int r:{2,3}){QVERIFY(!rows->item(r)->isHidden());QVERIFY(statusAt(dialog,r).startsWith("已有"));}
    }
    void rowStatePreview() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        const auto ready=seed(library,"导入时自带歌词","preview-ready"),waiting=seed(library,"尚未搜索的歌曲","preview-waiting"),candidate=seed(library,"找到歌词，等待选择","preview-candidate");
        QVERIFY(library.saveLyrics(ready.id,synced("本地歌词","第二行")));QVERIFY(library.saveLookupCache(candidate.id,QJsonArray{record(candidate.title,"其他歌手",20)},false));
        LyricsLookup dialog(&library,{ready.id,waiting.id,candidate.id},nullptr,QUrl("http://127.0.0.1:9/api"));dialog.show();dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);QTest::qWait(40);
        const auto directory=qEnvironmentVariable("RELAXER_QA_DIR");if(!directory.isEmpty()){QDir().mkpath(directory);QVERIFY(dialog.grab().save(directory+"/lookup-row-states.png"));}
    }
    void cachedCandidatesDoNotSuppressExplicitSearchAndSurviveFailure() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library,"Natural","cache-search","Imagine Dragons");
        auto old=record(track.title,"旧候选歌手",180);QVERIFY(library.saveLookupCache(track.id,QJsonArray{old},false));
        Phase031Server server;QVERIFY(server.listen(QHostAddress::LocalHost));
        server.reply=[](const QUrl &url){return qMakePair(url.path().endsWith("/get")?404:200,url.path().endsWith("/get")?QByteArray("{}"):QByteArray("[]"));};
        LyricsLookup dialog(&library,{track.id},nullptr,server.endpoint());dialog.show();
        auto *rows=dialog.findChild<QListWidget *>("lyricsTracks");QCOMPARE(rows->item(0)->data(Qt::UserRole+1).toInt(),3);QCOMPARE(server.requests,0);
        relaxerClickSearch(&dialog);QTRY_VERIFY_WITH_TIMEOUT(server.requests>0,3000);QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled(),8000);
        QVERIFY(statusAt(dialog,0).startsWith("需选择"));QCOMPARE(rows->item(0)->data(Qt::UserRole+1).toInt(),3);QVERIFY(!library.lookupCache(track.id).noResult);QVERIFY(!library.lookupCache(track.id).records.isEmpty());
        rows->setCurrentRow(0);QCOMPARE(dialog.findChild<QListWidget *>("lyricsVersions")->count(),1);
        server.reply=[&](const QUrl &url){return qMakePair(url.path().endsWith("/get")?404:200,url.path().endsWith("/get")?QByteArray("{}"):QJsonDocument(QJsonArray{record(track.title,track.artist)}).toJson());};
        const auto before=server.requests;dialog.findChild<QPushButton *>("lyricsStart")->click();QTRY_VERIFY_WITH_TIMEOUT(server.requests>before,3000);QTRY_VERIFY_WITH_TIMEOUT(!library.track(track.id).lyrics.isEmpty(),8000);
        QCOMPARE(rows->item(0)->data(Qt::UserRole+1).toInt(),2);
    }
    void unavailableAITranslationRetainsCompletedLyricsState() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library,"外语原文","translation-state");const auto original=synced("hello","world");QVERIFY(library.saveLyrics(track.id,original));
        Phase031Server retiredWeb;retiredWeb.relaxerOnly=false;QVERIFY(retiredWeb.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&library,{track.id},nullptr,QUrl("http://127.0.0.1:9/api"));dialog.setProperty("lyricTranslateUrl",retiredWeb.endpoint());dialog.show();dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);
        auto *rows=dialog.findChild<QListWidget *>("lyricsTracks");QCOMPARE(rows->item(0)->data(Qt::UserRole+1).toInt(),2);rows->setCurrentRow(0);
        dialog.findChild<QPushButton *>("lyricsNeedsTranslation")->click();QVERIFY(dialog.findChild<QPushButton *>("lyricsNeedsTranslation")->isChecked());
        dialog.findChild<QPushButton *>("lyricsTranslate")->click();
        QVERIFY(dialog.findChild<QLabel *>("lyricsStatus")->text().contains("AI 服务不可用"));QCOMPARE(rows->item(0)->data(Qt::UserRole+1).toInt(),2);QCOMPARE(retiredWeb.requests,0);
        const auto saved=LyricsDocumentReader::readFile(library.absolute(library.track(track.id).lyrics));QVERIFY(saved.opened);QCOMPARE(saved.bytes,original);
    }
    void stoppedSearchKeepsRankedCandidatesActionable() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library,"有候选后停止","stop-candidate");
        Phase031Server server;server.hold=true;QVERIFY(server.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&library,{track.id},nullptr,server.endpoint());dialog.show();relaxerClickSearch(&dialog);QTRY_VERIFY_WITH_TIMEOUT(server.held.size()>=3,3000);
        // Finish one request with a nonconfident candidate, leaving other network requests in flight.
        Phase031Server::send(server.held.takeFirst(),200,QJsonDocument(QJsonArray{record(track.title,"其他歌手",18)}).toJson());
        auto *rows=dialog.findChild<QListWidget *>("lyricsTracks");rows->setCurrentRow(0);QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QListWidget *>("lyricsVersions")->count()>0,3000);
        dialog.findChild<QPushButton *>("lyricsStop")->click();QCOMPARE(rows->item(0)->data(Qt::UserRole+1).toInt(),3);QVERIFY(dialog.findChild<QPushButton *>("lyricsApply")->isEnabled());
        dialog.findChild<QPushButton *>("lyricsApply")->click();QVERIFY(!library.track(track.id).lyrics.isEmpty());
    }
    void saturatedPrimaryDoesNotBlockAnIndependentFallback() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVector<qint64> ids;for(int i=0;i<6;++i)ids<<seed(library,QString("并发%1").arg(i),QString("parallel-%1").arg(i)).id;
        Phase031Server primary,fallback;primary.hold=true;QVERIFY(primary.listen(QHostAddress::LocalHost));QVERIFY(fallback.listen(QHostAddress::LocalHost));fallback.reply=[](const QUrl &){return qMakePair(200,QByteArray("{\"lyrics\":\"fallback first line\nsecond line\"}"));};
        auto url=fallback.endpoint("/v1");url.setHost("localhost");LyricsLookup dialog(&library,ids,nullptr,primary.endpoint(),url);dialog.setProperty("lyricsRequestTimeoutMs",10000);dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(fallback.requests>0,2500); // Must not wait for the first source's 10 second timeout.
        dialog.findChild<QPushButton *>("lyricsStop")->click();
    }
    void providerMetadataMustComeFromTheResultNotTheQuery() {
        const QJsonObject song{{"name","另一个版本"},{"id",42},{"duration",10000},{"artists",QJsonArray{QJsonObject{{"name","实际歌手"}}}}};
        const auto result=lyric_remote::consume(2,"search",QJsonDocument(QJsonObject{{"result",QJsonObject{{"songs",QJsonArray{song}}}}}).toJson(),"查询标题","查询歌手",10000);
        QVERIFY(result.again);QCOMPARE(result.matched["trackName"].toString(),QString("另一个版本"));QCOMPARE(result.matched["artistName"].toString(),QString("实际歌手"));QVERIFY(!result.matched["manualOnly"].toBool());
        const auto unknown=lyric_remote::consume(2,"search",QByteArray("{\"result\":{\"songs\":[{\"id\":42}]}}"),"查询标题","查询歌手",10000);QVERIFY(unknown.matched["manualOnly"].toBool());
    }
    void providedSongTagsFindCandidatesWithoutEditing() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        const QList<QPair<QString,QString>> songs{{"Natural","Imagine Dragons"},{"室内系的TrackMaker(YUNOMI)（翻自 nicamoq）","Hanser"},{"Something Just Like This (Alesso Remix)","The Chainsmokers/Coldplay/Alesso"},{"Frontier (Cargo Remix)","Cargo/Vinai/SCNDL"},{"Legends Never Die (Alan Walker Remix)","Against the Current/Alan Walker/Mako"},{"Empty Love","Lulleaux/Kid Princess"},{"Shelter (Piano version)","Porter Robinson/Madeon"},{"Counting Stars","Simply Three"},{"Xenogenesis","TheFatRat"},{"Xenogenesis","TheFatRat"},{"Home (Blaze U Remix)","ThimLife/Blaze U/Bibiane Z"},{"フラレガイガール","春茶"},{QString::fromUtf8("I\u00a0Love\u00a0The\u00a0Sky"),"蔡恩雨"}};
        QVector<qint64> ids;for(int i=0;i<songs.size();++i)ids<<seed(library,songs[i].first,QString("provided-%1").arg(i),songs[i].second).id;
        Phase031Server server;QVERIFY(server.listen(QHostAddress::LocalHost));server.reply=[&](const QUrl &url){if(url.path().endsWith("/get"))return qMakePair(404,QByteArray("{}"));const auto title=QUrlQuery(url).queryItemValue("track_name",QUrl::FullyDecoded);for(const auto &song:songs)if(title==LyricsLookup::cleanTitle(song.first))return qMakePair(200,QJsonDocument(QJsonArray{record(title,song.second,20)}).toJson());return qMakePair(200,QByteArray("[]"));};
        LyricsLookup dialog(&library,ids,nullptr,server.endpoint());dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(server.requests>0,3000);
        const auto allReady=[&]{for(auto id:ids)if(!statusAt(dialog,rowForId(dialog,id)).startsWith("需选择"))return false;return true;};
        const auto stateDetails=[&]{QStringList states;for(auto id:ids)states<<library.track(id).title+": "+statusAt(dialog,rowForId(dialog,id));return states.join(" | ")+QString("; requests=%1; running=%2").arg(server.requests).arg(!dialog.findChild<QPushButton *>("lyricsStart")->isEnabled());};
        QTRY_VERIFY2_WITH_TIMEOUT(allReady(),qPrintable(stateDetails()),8000);
        QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled(),3000);
        auto *rows=dialog.findChild<QListWidget *>("lyricsTracks");for(auto id:ids){const auto row=rowForId(dialog,id);QVERIFY(statusAt(dialog,row).startsWith("需选择"));QCOMPARE(rows->item(row)->data(Qt::UserRole+1).toInt(),3);rows->setCurrentRow(row);QVERIFY(dialog.findChild<QListWidget *>("lyricsVersions")->count()>0);}
    }
    void unchangedManualUsesTheSameNormalizedQueryPlan_data() {
        QTest::addColumn<bool>("reverseResponses");
        QTest::newRow("forward-responses")<<false;
        QTest::newRow("reverse-responses")<<true;
    }
    void unchangedManualUsesTheSameNormalizedQueryPlan() {
        QFETCH(bool,reverseResponses);
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        const auto track=seed(library,QString::fromUtf8("I\u00a0Love\u00a0The\u00a0Sky"),"query-parity","蔡恩雨");
        Phase031Server server;QVERIFY(server.listen(QHostAddress::LocalHost));
        server.reply=[](const QUrl &url){return qMakePair(url.path().endsWith("/get")?404:200,url.path().endsWith("/get")?QByteArray("{}"):QByteArray("[]"));};
        LyricsLookup dialog(&library,{track.id},nullptr,server.endpoint());dialog.show();
        auto *network=dialog.findChild<QNetworkAccessManager *>();QVERIFY(network);
        QStringList initial;
        for(int pass=0;pass<2;++pass){
            server.hold=true;
            if(pass==0)relaxerClickSearch(&dialog);
            else {dialog.findChild<QListWidget *>("lyricsTracks")->setCurrentRow(0);dialog.findChild<QPushButton *>("lyricsManualSearch")->click();}
            QTRY_COMPARE_WITH_TIMEOUT(server.held.size(),3,3000);
            // Concurrent connections can reach the server in any order. With no
            // replies completed, QObject child order preserves request creation
            // order, so check the submitted plan instead of network arrival order.
            QList<QUrl> submitted;
            for(auto *reply:network->findChildren<QNetworkReply *>(QString(),Qt::FindDirectChildrenOnly))
                if(reply->property("lookupTrackId").toLongLong()==track.id&&!reply->property("lookupSlotReleased").toBool())submitted<<reply->request().url();
            QCOMPARE(submitted.size(),3);
            QVERIFY(submitted.first().path().endsWith("/search"));
            const QUrlQuery first(submitted.first());
            QCOMPARE(first.queryItemValue("track_name"),QString("I Love The Sky"));
            QCOMPARE(first.queryItemValue("artist_name"),track.artist);
            QVERIFY(!first.hasQueryItem("album_name"));QVERIFY(!first.hasQueryItem("duration"));
            QVERIFY(submitted.at(1).path().endsWith("/get"));
            server.hold=false;
            auto held=server.held;server.held.clear();
            if(reverseResponses)std::reverse(held.begin(),held.end());
            for(const auto &socket:held){
                QVERIFY(socket);
                const auto url=QUrl::fromEncoded("http://localhost"+socket->property("buffer").toByteArray().split(' ').value(1));
                const auto response=server.reply(url);Phase031Server::send(socket,response.first,response.second);
            }
            QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled(),3000);
            QStringList plan;for(const auto &url:server.urls)plan<<url.toString();plan.sort();
            QVERIFY(!plan.isEmpty());
            if(pass==0)initial=plan;else QCOMPARE(initial,plan);
            server.urls.clear();
        }
    }
    void firstPassErrorsAutomaticallyRecoverOneSongAtATime() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVector<qint64> ids;
        for(int i=0;i<3;++i)ids<<seed(library,QString("自动复查%1").arg(i),QString("recovery-%1").arg(i)).id;
        Phase031Server server;QVERIFY(server.listen(QHostAddress::LocalHost));QElapsedTimer elapsed;elapsed.start();
        server.reply=[&](const QUrl &url){
            if(elapsed.elapsed()<350)return qMakePair(503,QByteArray("{}"));
            if(url.path().endsWith("/get"))return qMakePair(404,QByteArray("{}"));
            const auto title=QUrlQuery(url).queryItemValue("track_name");
            return qMakePair(200,title.isEmpty()?QByteArray("[]"):QJsonDocument(QJsonArray{record(title,"待核对歌手",20)}).toJson());
        };
        LyricsLookup dialog(&library,ids,nullptr,server.endpoint());dialog.setProperty("lyricsSearchBudgetMs",120);dialog.setProperty("lyricsRetryDelayMs",500);
        QSet<qint64> recovered;bool serial=true;
        auto *network=dialog.findChild<QNetworkAccessManager *>();
        connect(network,&QNetworkAccessManager::finished,&dialog,[&](QNetworkReply *reply){
            if(reply->property("lookupSearchPass").toInt()!=1)return;
            recovered.insert(reply->property("lookupTrackId").toLongLong());QSet<qint64> active;
            for(auto *other:network->findChildren<QNetworkReply *>())if(other->property("lookupSearchPass").toInt()==1&&!other->property("lookupSlotReleased").toBool())active.insert(other->property("lookupTrackId").toLongLong());
            serial&=active.size()<=1;
        });
        dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled(),6000);
        QCOMPARE(recovered.size(),3);QVERIFY(serial);
        for(auto id:ids){QVERIFY(statusAt(dialog,rowForId(dialog,id)).startsWith("需选择"));QVERIFY(!library.lookupCache(id).records.isEmpty());QVERIFY(!library.lookupCache(id).noResult);}
    }
    void recoveryIsBoundedAndNetworkFailureIsNotDatabaseAbsence() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library,"持续异常","bounded-recovery");
        Phase031Server server;QVERIFY(server.listen(QHostAddress::LocalHost));server.reply=[](const QUrl &){return qMakePair(503,QByteArray("{}"));};
        LyricsLookup dialog(&library,{track.id},nullptr,server.endpoint());dialog.setProperty("lyricsSearchBudgetMs",80);dialog.setProperty("lyricsRetryDelayMs",20);
        QSet<int> passes;connect(dialog.findChild<QNetworkAccessManager *>(),&QNetworkAccessManager::finished,&dialog,[&](QNetworkReply *reply){passes.insert(reply->property("lookupSearchPass").toInt());});
        dialog.show();relaxerClickSearch(&dialog);QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled(),3000);
        QCOMPARE(passes,QSet<int>({0,1}));QVERIFY(statusAt(dialog,0).startsWith("等待重试"));QCOMPARE(dialog.findChild<QListWidget *>("lyricsTracks")->item(0)->data(Qt::UserRole+1).toInt(),0);
        QVERIFY(!library.lookupCache(track.id).noResult);const int requests=server.requests;QTest::qWait(450);QCOMPARE(server.requests,requests);
    }
    void stopCancelsDelayedRecoveryWithoutLateNetworkOrWrites() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto track=seed(library,"停止复查","stop-recovery");
        Phase031Server server;QVERIFY(server.listen(QHostAddress::LocalHost));server.reply=[](const QUrl &){return qMakePair(503,QByteArray("{}"));};
        LyricsLookup dialog(&library,{track.id},nullptr,server.endpoint());dialog.setProperty("lyricsSearchBudgetMs",80);dialog.setProperty("lyricsRetryDelayMs",500);dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(statusAt(dialog,0).startsWith("正在重试"),2000);dialog.findChild<QPushButton *>("lyricsStop")->click();const int before=server.requests;
        QTest::qWait(700);QCOMPARE(server.requests,before);QVERIFY(library.track(track.id).lyrics.isEmpty());QVERIFY(!library.lookupCache(track.id).noResult);
    }
    void domesticSourcesKeepValidResultsAfterTheFirstHit() {
        for(int kind:{2,3,4}){
            QJsonArray songs;
            for(int i=0;i<12;++i){
                QJsonObject song;
                if(kind==2)song={{"id",i?i:0},{"name",i==2?"目标版本":"其他版本"},{"duration",10000},{"artists",QJsonArray{QJsonObject{{"name",i==2?"目标歌手":"其他歌手"}}}}};
                if(kind==3)song={{"mid",i?QString::number(i):QString()},{"name",i==2?"目标版本":"其他版本"},{"interval",10},{"singer",QJsonArray{QJsonObject{{"name",i==2?"目标歌手":"其他歌手"}}}}};
                if(kind==4)song={{"id",i},{"accesskey",i?"test-key":""},{"song",i==2?"目标版本":"其他版本"},{"singer",i==2?"目标歌手":"其他歌手"},{"duration",10000}};
                songs<<song;if(i==1)songs<<song; // Invalid first row and a duplicate must not hide the next version.
            }
            QJsonObject payload;
            if(kind==2)payload={{"result",QJsonObject{{"songs",songs}}},{"code",200}};
            if(kind==3)payload={{"data",QJsonObject{{"song",QJsonObject{{"list",songs}}}}},{"code",0}};
            if(kind==4)payload={{"candidates",songs},{"status",200}};
            const auto follow=lyric_remote::consume(kind,"search",QJsonDocument(payload).toJson(),"查询标题","查询歌手",10000);
            QVERIFY(follow.again);QVERIFY(!follow.failed);QCOMPARE(follow.remaining.size(),7);QCOMPARE(follow.matched["trackName"].toString(),QString("其他版本"));
            QCOMPARE(follow.remaining.first().matched["trackName"].toString(),QString("目标版本"));QCOMPARE(follow.remaining.first().matched["artistName"].toString(),QString("目标歌手"));
            QCOMPARE(follow.remaining.first().durationMs,qint64(10000));QSet<QUrl> urls{follow.step.url};for(const auto &detail:follow.remaining){QVERIFY(!urls.contains(detail.step.url));urls.insert(detail.step.url);}
        }
    }
    void providerChallengeAndRejectionAreNotEmptySearchResults() {
        const auto challenge=lyric_remote::consume(2,"search",QByteArray("{\"result\":\"encrypted challenge\",\"code\":200}"),"歌名","歌手",10000);QVERIFY(challenge.failed);
        QVERIFY(lyric_remote::consume(2,"search",QByteArray("{\"code\":429}"),"歌名","歌手",10000).failed);
        QVERIFY(lyric_remote::consume(3,"search",QByteArray("{\"code\":100}"),"歌名","歌手",10000).failed);
        QVERIFY(lyric_remote::consume(4,"search",QByteArray("{\"status\":403}"),"歌名","歌手",10000).failed);
        QVERIFY(!lyric_remote::consume(2,"search",QByteArray("{\"result\":{\"songCount\":0},\"code\":200}"),"歌名","歌手",10000).failed);
    }
    void unavailableAIServiceDoesNotLeaveAHiddenBrowserTask(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        const auto track=seed(library,"没有隐藏请求","no-hidden-browser-task");const auto original=synced("hello","world");QVERIFY(library.saveLyrics(track.id,original));
        Phase031Server retiredWeb;retiredWeb.relaxerOnly=false;QVERIFY(retiredWeb.listen(QHostAddress::LocalHost));
        LyricsLookup dialog(&library,{track.id},nullptr,QUrl("http://127.0.0.1:9/api"));dialog.setProperty("lyricTranslateUrl",retiredWeb.endpoint("/translate"));dialog.show();
        dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);dialog.findChild<QListWidget *>("lyricsTracks")->setCurrentRow(0);
        dialog.findChild<QPushButton *>("lyricsNeedsTranslation")->click();QVERIFY(dialog.findChild<QPushButton *>("lyricsNeedsTranslation")->isChecked());
        dialog.findChild<QPushButton *>("lyricsTranslate")->click();QVERIFY(dialog.findChild<QLabel *>("lyricsStatus")->text().contains("AI 服务不可用"));
        QCOMPARE(retiredWeb.requests,0);dialog.close();QVERIFY(!dialog.isVisible());
        for(auto *widget:QApplication::topLevelWidgets())QVERIFY(widget->objectName()!="lyricsTranslateCloseConfirm");
        const auto saved=LyricsDocumentReader::readFile(library.absolute(library.track(track.id).lyrics));QVERIFY(saved.opened);QCOMPARE(saved.bytes,original);
    }
};
QTEST_MAIN(Phase031Tests)
#include "phase031-tests.moc"
