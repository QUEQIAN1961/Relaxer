#include "import-fixture.h"
#include "lyricslookup.h"
#include "translation-fixture.h"
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QtTest>

using namespace relaxer;
namespace relaxer {
class LibraryDiagnostics {
public:
    static void reset(Library &l){l.m_lyricsSaveMetrics={};}
    static LyricsSaveMetrics work(const Library &l){return l.m_lyricsSaveMetrics;}
};
class LyricsLookupDiagnostics {
public:
    static void select(LyricsLookup &d){d.m_missingOnly->setChecked(false);d.m_incomplete->setChecked(false);d.updateFilter();d.m_tracks->setCurrentRow(0);}
    static bool manual(LyricsLookup &d){return d.saveManual("[00:01.00]New first\n[00:02.00]New second","已保存测试");}
};
}
class LyricsSaveWorkTests final:public QObject {
    Q_OBJECT
private slots:
    void manualSaveValidationWork(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"Manual","p3-manual");
        QVERIFY(l.saveLyrics(t.id,"[00:01.00]Old first\n[00:02.00]Old second"));
        LyricsLookup dialog(&l,{t.id},nullptr,QUrl("http://127.0.0.1:9/api"));
        LyricsLookupDiagnostics::select(dialog);
        QTimer answer;answer.setInterval(1);connect(&answer,&QTimer::timeout,this,[]{
            if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                if(box->objectName()=="lyricsReplaceConfirm")box->done(QMessageBox::Yes);
        });answer.start();LibraryDiagnostics::reset(l);
        QVERIFY(LyricsLookupDiagnostics::manual(dialog));answer.stop();const auto work=LibraryDiagnostics::work(l);
        qInfo()<<"MANUAL validation_and_backup_file_reads="<<work.fileReads<<"digests="<<work.digestComputations;
        if(!qEnvironmentVariableIsSet("RELAXER_P3_MEASURE_BASELINE")){
            QCOMPARE(work.fileReads,quint64(4));QCOMPARE(work.digestComputations,quint64(3));
        }
    }
    void aiSaveValidationWork(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"AI","p3-ai");
        QVERIFY(l.saveLyrics(t.id,"[00:01.00]Hello world\n[00:02.00]Walk home"));
        QVERIFY(l.setTranslationJudgment(t.id,TranslationJudgment::NeedsTranslation));
        TranslationHttpMock http;QVERIFY(http.listen());
        AiRuntime runtime(&l,DeepSeekProvider::MockEndpoint{http.baseUrl()},std::make_unique<FakeCredentialStore>());
        QVERIFY(configureRuntime(runtime));TranslationCoordinator service(&l,&runtime);
        http.enqueue(completion(QString::fromUtf8(translationJson({{0,QStringLiteral("世界你好")},{1,QStringLiteral("走回家")}}))));
        const auto id=service.prepareAi(t.id);QVERIFY(service.start(id));
        QTRY_COMPARE(service.summary(id).state,TranslationState::CompletedPendingSave);
        LibraryDiagnostics::reset(l);QVERIFY(service.save(id));const auto work=LibraryDiagnostics::work(l);
        qInfo()<<"AI validation_and_backup_file_reads="<<work.fileReads<<"digests="<<work.digestComputations;
        if(!qEnvironmentVariableIsSet("RELAXER_P3_MEASURE_BASELINE")){
            QCOMPARE(work.fileReads,quint64(3));QCOMPARE(work.digestComputations,quint64(2));
        }
    }
};
QTEST_MAIN(LyricsSaveWorkTests)
#include "lyrics-save-work-tests.moc"
