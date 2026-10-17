#include "import-fixture.h"
#include "lyricslookup.h"
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

namespace relaxer {
class LyricsLookupDiagnostics {
public:
    static void reset(LyricsLookup &dialog){dialog.m_filterMetrics={};}
    static void status(LyricsLookup &dialog,int row){dialog.setRowStatus(row,"Synthetic status transition");}
    static void audit(LyricsLookup &dialog){dialog.startTranslationCacheAudit();}
    static void requestAudit(LyricsLookup &dialog){dialog.requestTranslationCacheAudit();}
    static bool auditing(const LyricsLookup &dialog){return dialog.m_translationAuditInFlight;}
    static QJsonObject work(const LyricsLookup &dialog){
        const auto &m=dialog.m_filterMetrics;
        return {{"full_passes",double(m.fullPasses)},{"row_visits",double(m.rowVisits)},{"assessments",double(m.assessments)},
            {"file_reads",double(m.fileReads)},{"bytes_read",double(m.bytesRead)},{"json_keys",double(m.jsonKeys)},
            {"digest_computations",double(m.digestComputations)},{"digest_bytes",double(m.digestBytes)},
            {"audit_passes",double(m.auditPasses)},{"audit_files",double(m.auditFiles)}};
    }
};
}
using namespace relaxer;
class TranslationFilterPerformanceTests : public QObject {
    Q_OBJECT
private slots:
    void thousandSongsAndTwoHundredStatusTransitions_data() {
        QTest::addColumn<int>("songs");
        QTest::newRow("100")<<100;
        QTest::newRow("500")<<500;
        QTest::newRow("1000")<<1000;
    }
    void thousandSongsAndTwoHundredStatusTransitions() {
        QFETCH(int,songs);
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        QVector<qint64> ids;ids.reserve(songs);
        const QByteArray sourceLyrics="[00:01.00]Hello my friend\n[00:02.00]This is a synthetic song";
        for(int i=0;i<songs;++i){
            Track t;t.hash="perf-"+QString::number(i);t.title="Synthetic "+QString::number(i);t.artist="Fixture";t.file="Music/fixture.mid";
            const auto id=library.insert(t);QVERIFY(id>0);QVERIFY(library.saveLyrics(id,sourceLyrics));ids<<id;
        }
        auto dialog=std::make_unique<LyricsLookup>(&library,ids,nullptr,QUrl("http://127.0.0.1:9/api"));
        auto *filter=dialog->findChild<QCheckBox *>("lyricsMissingTranslationOnly");auto *refresh=dialog->findChild<QPushButton *>("lyricsRefreshTranslationFilter");QVERIFY(filter);QVERIFY(refresh);
        QJsonObject result{{"songs",songs},{"status_updates",200},{"measurement","actual local synthetic SQLite and LRC files"}};
        auto measure=[&](const QString &name,auto action){LyricsLookupDiagnostics::reset(*dialog);QElapsedTimer timer;timer.start();action();auto data=LyricsLookupDiagnostics::work(*dialog);data["elapsed_ms"]=double(timer.nsecsElapsed())/1e6;result[name]=data;return data;};
        measure("enable",[&]{filter->setChecked(true);});
        measure("refresh",[&]{refresh->click();});
        measure("repeat_refresh",[&]{refresh->click();});
        const auto updates=measure("status_updates_work",[&]{for(int i=0;i<200;++i)LyricsLookupDiagnostics::status(*dialog,i%songs);});
        LyricsLookupDiagnostics::reset(*dialog);QElapsedTimer auditClock;auditClock.start();int guiHeartbeats=0;QTimer heartbeat;
        heartbeat.setInterval(1);connect(&heartbeat,&QTimer::timeout,&heartbeat,[&]{++guiHeartbeats;});heartbeat.start();
        for(int i=0;i<20;++i)LyricsLookupDiagnostics::requestAudit(*dialog);
        QTRY_VERIFY_WITH_TIMEOUT(LyricsLookupDiagnostics::work(*dialog)["audit_passes"].toInt()>0,5000);
        QTRY_VERIFY_WITH_TIMEOUT(!LyricsLookupDiagnostics::auditing(*dialog),5000);heartbeat.stop();
        auto auditWork=LyricsLookupDiagnostics::work(*dialog);auditWork["elapsed_ms"]=double(auditClock.nsecsElapsed())/1e6;auditWork["gui_heartbeat_events"]=guiHeartbeats;result["activation_audit"]=auditWork;
        auto *rows=dialog->findChild<QListWidget *>("lyricsTracks");QVERIFY(rows);QCOMPARE(rows->count(),songs);
        for(int i=0;i<rows->count();++i){QVERIFY(!rows->item(i)->isHidden());QCOMPARE(rows->item(i)->data(Qt::UserRole+4).toString(),QStringLiteral("待翻译"));}
        const QByteArray translated=QStringLiteral("你好世界").toUtf8();
        for(const auto id:ids){
            const auto path=library.absolute(library.track(id).lyrics);const auto stamp=QFileInfo(path).lastModified();
            QByteArray bytes=translated;bytes+=QByteArray(sourceLyrics.size()-translated.size(),' ');QCOMPARE(bytes.size(),sourceLyrics.size());
            QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));QCOMPARE(file.write(bytes),qint64(bytes.size()));
            QVERIFY(file.setFileTime(stamp,QFileDevice::FileModificationTime));file.close();
        }
        LyricsLookupDiagnostics::reset(*dialog);auditClock.restart();guiHeartbeats=0;heartbeat.start();
        for(int i=0;i<20;++i)LyricsLookupDiagnostics::requestAudit(*dialog);
        QTRY_VERIFY_WITH_TIMEOUT(LyricsLookupDiagnostics::work(*dialog)["audit_passes"].toInt()>0,5000);
        QTRY_VERIFY_WITH_TIMEOUT(!LyricsLookupDiagnostics::auditing(*dialog),5000);heartbeat.stop();
        auto changedWork=LyricsLookupDiagnostics::work(*dialog);changedWork["elapsed_ms"]=double(auditClock.nsecsElapsed())/1e6;changedWork["gui_heartbeat_events"]=guiHeartbeats;result["content_change_reassessment"]=changedWork;
        for(int i=0;i<rows->count();++i)QVERIFY(rows->item(i)->isHidden());
        QElapsedTimer closing;closing.start();dialog.reset();result["close_elapsed_ms"]=double(closing.nsecsElapsed())/1e6;
        const auto output=qEnvironmentVariable("RELAXER_FILTER_BENCHMARK_OUTPUT");
        if(!output.isEmpty()){QFile file(output);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(result).toJson(QJsonDocument::Indented));}
        qInfo().noquote()<<QJsonDocument(result).toJson(QJsonDocument::Compact);
        if(!qEnvironmentVariableIsSet("RELAXER_FILTER_BENCHMARK_BASELINE")){
            QVERIFY2(updates["row_visits"].toDouble()<=400,"Single-row status updates must not rescan the thousand-song library");
            QVERIFY(updates["assessments"].toDouble()<=400);QCOMPARE(updates["file_reads"].toInt(),0);QCOMPARE(updates["digest_computations"].toInt(),0);
            QCOMPARE(auditWork["audit_files"].toInt(),songs);QCOMPARE(auditWork["digest_computations"].toInt(),songs);
            QCOMPARE(auditWork["audit_passes"].toInt(),1);
            QVERIFY2(guiHeartbeats>0,"The GUI event loop must continue processing while cache verification runs");
            QCOMPARE(changedWork["full_passes"].toInt(),0);QCOMPARE(changedWork["row_visits"].toInt(),songs);
            QCOMPARE(changedWork["file_reads"].toInt(),songs);QCOMPARE(changedWork["digest_computations"].toInt(),songs);
            QCOMPARE(changedWork["audit_passes"].toInt(),1);QCOMPARE(changedWork["audit_files"].toInt(),songs);
            QVERIFY2(changedWork["gui_heartbeat_events"].toInt()>0,"The GUI event loop must remain responsive during changed-file audits");
        }
    }
};
QTEST_MAIN(TranslationFilterPerformanceTests)
#include "translation-filter-performance-tests.moc"
