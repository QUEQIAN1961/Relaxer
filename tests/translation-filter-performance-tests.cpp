#include "import-fixture.h"
#include "lyricslookup.h"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

namespace relaxer {
class LyricsLookupDiagnostics {
public:
    static void reset(LyricsLookup &dialog){dialog.m_filterMetrics={};}
    static void status(LyricsLookup &dialog,int row){dialog.setRowStatus(row,"Synthetic status transition");}
    static QJsonObject work(const LyricsLookup &dialog){
        const auto &m=dialog.m_filterMetrics;
        return {{"full_passes",double(m.fullPasses)},{"row_visits",double(m.rowVisits)},{"assessments",double(m.assessments)},
            {"file_reads",double(m.fileReads)},{"bytes_read",double(m.bytesRead)},{"json_keys",double(m.jsonKeys)}};
    }
};
}
using namespace relaxer;
class TranslationFilterPerformanceTests : public QObject {
    Q_OBJECT
private slots:
    void thousandSongsAndTwoHundredStatusTransitions() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        QVector<qint64> ids;ids.reserve(1000);
        for(int i=0;i<1000;++i){
            Track t;t.hash="perf-"+QString::number(i);t.title="Synthetic "+QString::number(i);t.artist="Fixture";t.file="Music/fixture.mid";
            const auto id=library.insert(t);QVERIFY(id>0);QVERIFY(library.saveLyrics(id,"[00:01.00]Hello my friend\n[00:02.00]This is a synthetic song"));ids<<id;
        }
        auto dialog=std::make_unique<LyricsLookup>(&library,ids,nullptr,QUrl("http://127.0.0.1:9/api"));
        auto *filter=dialog->findChild<QCheckBox *>("lyricsMissingTranslationOnly");auto *refresh=dialog->findChild<QPushButton *>("lyricsRefreshTranslationFilter");QVERIFY(filter);QVERIFY(refresh);
        QJsonObject result{{"songs",1000},{"status_updates",200},{"measurement","actual local synthetic SQLite and LRC files"}};
        auto measure=[&](const QString &name,auto action){LyricsLookupDiagnostics::reset(*dialog);QElapsedTimer timer;timer.start();action();auto data=LyricsLookupDiagnostics::work(*dialog);data["elapsed_ms"]=double(timer.nsecsElapsed())/1e6;result[name]=data;return data;};
        measure("enable",[&]{filter->setChecked(true);});
        measure("refresh",[&]{refresh->click();});
        measure("repeat_refresh",[&]{refresh->click();});
        const auto updates=measure("status_updates_work",[&]{for(int i=0;i<200;++i)LyricsLookupDiagnostics::status(*dialog,i);});
        auto *rows=dialog->findChild<QListWidget *>("lyricsTracks");QVERIFY(rows);QCOMPARE(rows->count(),1000);
        for(int i=0;i<rows->count();++i){QVERIFY(!rows->item(i)->isHidden());QCOMPARE(rows->item(i)->data(Qt::UserRole+4).toString(),QStringLiteral("待翻译"));}
        QElapsedTimer closing;closing.start();dialog.reset();result["close_elapsed_ms"]=double(closing.nsecsElapsed())/1e6;
        const auto output=qEnvironmentVariable("RELAXER_FILTER_BENCHMARK_OUTPUT");
        if(!output.isEmpty()){QFile file(output);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(result).toJson(QJsonDocument::Indented));}
        qInfo().noquote()<<QJsonDocument(result).toJson(QJsonDocument::Compact);
        if(!qEnvironmentVariableIsSet("RELAXER_FILTER_BENCHMARK_BASELINE")){
            QVERIFY2(updates["row_visits"].toDouble()<=400,"Single-row status updates must not rescan the thousand-song library");
            QVERIFY(updates["assessments"].toDouble()<=400);QCOMPARE(updates["file_reads"].toInt(),0);
        }
    }
};
QTEST_MAIN(TranslationFilterPerformanceTests)
#include "translation-filter-performance-tests.moc"
