#include "integration-fixture.h"
#include <QSqlQuery>
using namespace relaxer;
using namespace integration_fixture;
using J=TranslationJudgment;
namespace {
QPushButton *button(LyricsLookup &d,const char *name){return d.findChild<QPushButton *>(name);}
QListWidgetItem *item(LyricsLookup &d,qint64 id){auto *list=d.findChild<QListWidget *>("lyricsTracks");for(int n=0;n<list->count();++n)if(list->item(n)->data(Qt::UserRole).toLongLong()==id)return list->item(n);return nullptr;}
void all(LyricsLookup &d){d.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);d.findChild<QCheckBox *>("lyricsMissingTranslationOnly")->setChecked(false);}
QString badge(LyricsLookup &d,qint64 id){return item(d,id)->data(Qt::UserRole+4).toString();}
bool green(LyricsLookup &d,qint64 id){return item(d,id)->data(Qt::UserRole+6).toBool();}
const QByteArray source="[00:01.00]Hello my friend\n[00:02.00]This is a synthetic song";
QByteArray response(){return completion(QString::fromUtf8(translationJson({{0,QStringLiteral("朋友你好")},{1,QStringLiteral("这是一首合成歌曲")}})));}
}
namespace relaxer {
class LyricsLookupDiagnostics {
  public:
    static void activateAi(LyricsLookup &d){d.translateSelected();}
    static TranslationSummary preview(const LyricsLookup &d){return d.aiSummary();}
    static void audit(LyricsLookup &d){d.startTranslationCacheAudit();}
};
}
class TranslationWorkflowTests : public QObject {
    Q_OBJECT
  private slots:
    void transitionButtons_data(){
        QTest::addColumn<int>("initial");QTest::addColumn<bool>("needs");QTest::addColumn<int>("expected");
        QTest::newRow("unknown-no")<<int(J::Unknown)<<false<<int(J::NoTranslationNeeded);
        QTest::newRow("no-undo")<<int(J::NoTranslationNeeded)<<false<<int(J::Unknown);
        QTest::newRow("unknown-needs")<<int(J::Unknown)<<true<<int(J::NeedsTranslation);
        QTest::newRow("needs-undo")<<int(J::NeedsTranslation)<<true<<int(J::Unknown);
        QTest::newRow("no-to-needs")<<int(J::NoTranslationNeeded)<<true<<int(J::NeedsTranslation);
        QTest::newRow("needs-to-no")<<int(J::NeedsTranslation)<<false<<int(J::NoTranslationNeeded);
    }
    void transitionButtons(){
        QFETCH(int,initial);QFETCH(bool,needs);QFETCH(int,expected);QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);QCOMPARE(ids.size(),3);const auto id=ids[0];QVERIFY(lib.saveLyrics(id,source));QVERIFY(lib.setTranslationJudgment(id,J(initial)));
        {LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"));all(d);select(d,id);
         button(d,needs?"lyricsNeedsTranslation":"lyricsNoTranslationNeeded")->click();
         QCOMPARE(lib.translationJudgment(id),J(expected));
         QCOMPARE(button(d,"lyricsNeedsTranslation")->isChecked(),J(expected)==J::NeedsTranslation);
         QCOMPARE(button(d,"lyricsNoTranslationNeeded")->isChecked(),J(expected)==J::NoTranslationNeeded);
         QCOMPARE(green(d,id),J(expected)==J::NoTranslationNeeded);
         QCOMPARE(bytes(lib.absolute(lib.track(id).lyrics)),source);QVERIFY(!lib.isInstrumental(id));}
        {Library reopened(dir.path());QVERIFY(reopened.open());QCOMPARE(reopened.translationJudgment(id),J(expected));}
    }
    void missingFilterAndUndoFromAll(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);const auto a=ids[0],b=ids[1];QVERIFY(lib.saveLyrics(a,source));
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"));auto *filter=d.findChild<QCheckBox *>("lyricsMissingTranslationOnly");filter->setChecked(true);select(d,a);
        QVERIFY(!item(d,a)->isHidden());QCOMPARE(badge(d,a),QString("待判断"));QVERIFY(!green(d,a));
        QVERIFY(d.findChild<QWidget *>("lyricsSourcePanel")->isHidden());QVERIFY(d.findChild<QFrame *>("lyricsCandidatePanel")->isHidden());
        QCOMPARE(d.findChild<QPlainTextEdit *>("lyricsPreview")->toPlainText().contains("Hello my friend"),true);
        button(d,"lyricsFindOrReplace")->click();QVERIFY(!d.findChild<QWidget *>("lyricsSourcePanel")->isHidden());QVERIFY(!d.findChild<QFrame *>("lyricsCandidatePanel")->isHidden());
        button(d,"lyricsFindOrReplace")->click();button(d,"lyricsNoTranslationNeeded")->click();QVERIFY(item(d,a)->isHidden());QVERIFY(green(d,a));QCOMPARE(badge(d,a),QString("无需翻译"));
        all(d);QVERIFY(!item(d,a)->isHidden());select(d,a);button(d,"lyricsNoTranslationNeeded")->click();QCOMPARE(lib.translationJudgment(a),J::Unknown);QVERIFY(!green(d,a));
        select(d,b);button(d,"lyricsNeedsTranslation")->click();filter->setChecked(true);QVERIFY(!item(d,b)->isHidden());QCOMPARE(badge(d,b),QString("需要翻译"));QVERIFY(!green(d,b));
        QVERIFY(d.findChild<QLabel *>("lyricsClassificationSummary")->text().contains("没有歌词"));
    }
    void realBilingualCompletionAndReversalPreserveBytes(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);const auto id=ids[0];
        const auto bilingual=QString("[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]朋友你好").toUtf8();QVERIFY(lib.saveLyricsPreservingText(id,bilingual));
        for(int n=0;n<2;++n){LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"));all(d);select(d,id);QVERIFY(green(d,id));QCOMPARE(badge(d,id),QString("翻译完成"));
            button(d,"lyricsNoTranslationNeeded")->click();QCOMPARE(badge(d,id),QString("无需翻译"));button(d,"lyricsNoTranslationNeeded")->click();QCOMPARE(badge(d,id),QString("翻译完成"));QVERIFY(green(d,id));QCOMPARE(bytes(lib.absolute(lib.track(id).lyrics)),bilingual);}
    }
    void sortAndFiltersUseTrackIdentity(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);QVERIFY(lib.saveLyrics(ids[0],source));QVERIFY(lib.saveLyrics(ids[1],source));
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"));all(d);select(d,ids[0]);button(d,"lyricsNeedsTranslation")->click();
        d.findChild<QListWidget *>("lyricsTracks")->sortItems(Qt::DescendingOrder);select(d,ids[1]);button(d,"lyricsNoTranslationNeeded")->click();
        QCOMPARE(lib.translationJudgment(ids[0]),J::NeedsTranslation);QCOMPARE(lib.translationJudgment(ids[1]),J::NoTranslationNeeded);QCOMPARE(lib.translationJudgment(ids[2]),J::Unknown);
        d.findChild<QCheckBox *>("lyricsMissingTranslationOnly")->setChecked(true);QVERIFY(!item(d,ids[0])->isHidden());QVERIFY(item(d,ids[1])->isHidden());all(d);select(d,ids[0]);QVERIFY(button(d,"lyricsNeedsTranslation")->isChecked());
    }
    void rejectedWriteRestoresButtonsAndDoesNotComplete(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);QVERIFY(lib.saveLyrics(ids[0],source));
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"));all(d);select(d,ids[0]);
        QSqlQuery q(import_fixture::database(lib));QVERIFY(q.exec("CREATE TRIGGER deny_judgment BEFORE INSERT ON settings WHEN NEW.key LIKE 'lyrics.translationJudgment.%' BEGIN SELECT RAISE(ABORT,'denied'); END"));
        button(d,"lyricsNoTranslationNeeded")->click();QCOMPARE(lib.translationJudgment(ids[0]),J::Unknown);QVERIFY(!button(d,"lyricsNoTranslationNeeded")->isChecked());QVERIFY(!green(d,ids[0]));QVERIFY(d.findChild<QLabel *>("lyricsStatus")->text().contains("分类未保存"));
        QVERIFY(q.exec("DROP TRIGGER deny_judgment"));button(d,"lyricsNoTranslationNeeded")->click();QVERIFY(green(d,ids[0]));QCOMPARE(bytes(lib.absolute(lib.track(ids[0]).lyrics)),source);
    }
    void strictGateAndSingleExplicitRequest(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);const auto a=ids[0],b=ids[1],c=ids[2];QVERIFY(lib.saveLyrics(a,source));
        QVERIFY(lib.saveLyricsPreservingText(c,QString("[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]朋友你好").toUtf8()));
        TranslationHttpMock paid;QVERIFY(paid.listen());paid.setHold(true);paid.enqueue(response());
        AiRuntime runtime(&lib,DeepSeekProvider::MockEndpoint{paid.baseUrl()},std::make_unique<FakeCredentialStore>());QVERIFY(configureRuntime(runtime));
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"),{},true,&runtime);all(d);select(d,a);auto *ai=button(d,"lyricsTranslate");
        QVERIFY(!ai->isEnabled());LyricsLookupDiagnostics::activateAi(d);QCOMPARE(paid.requestCount(),0);
        button(d,"lyricsNoTranslationNeeded")->click();QVERIFY(!ai->isEnabled());LyricsLookupDiagnostics::activateAi(d);QCOMPARE(paid.requestCount(),0);
        button(d,"lyricsNeedsTranslation")->click();QVERIFY(ai->isEnabled());
        button(d,"lyricsMultiSelect")->click();QVERIFY(!ai->isEnabled());LyricsLookupDiagnostics::activateAi(d);QCOMPARE(paid.requestCount(),0);
        item(d,b)->setSelected(true);LyricsLookupDiagnostics::activateAi(d);QCOMPARE(paid.requestCount(),0);button(d,"lyricsMultiSelect")->click();
        select(d,b);button(d,"lyricsNeedsTranslation")->click();QVERIFY(!ai->isEnabled());QVERIFY(ai->toolTip().contains("没有可用原歌词"));LyricsLookupDiagnostics::activateAi(d);QCOMPARE(paid.requestCount(),0);
        select(d,c);button(d,"lyricsNeedsTranslation")->click();QVERIFY(!ai->isEnabled());LyricsLookupDiagnostics::activateAi(d);QCOMPARE(paid.requestCount(),0);
        select(d,a);QVERIFY(ai->isEnabled());consent(d);QTRY_COMPARE(paid.heldCount(),1);QVERIFY(!ai->isEnabled());QVERIFY(!button(d,"lyricsNoTranslationNeeded")->isEnabled());LyricsLookupDiagnostics::activateAi(d);QCOMPARE(paid.requestCount(),1);
        QVERIFY(paid.releaseHeld());auto *save=button(d,"lyricsSaveTranslation");QTRY_VERIFY(save->isEnabled());QVERIFY(!ai->isEnabled());QVERIFY(!green(d,a));QCOMPARE(badge(d,a),QString("翻译未保存"));QCOMPARE(bytes(lib.absolute(lib.track(a).lyrics)),source);
        LyricsLookupDiagnostics::activateAi(d);QCOMPARE(paid.requestCount(),1);d.findChild<QCheckBox *>("lyricsMissingTranslationOnly")->setChecked(true);QVERIFY(!item(d,a)->isHidden());save->click();
        QVERIFY(green(d,a));QCOMPARE(badge(d,a),QString("翻译完成"));QVERIFY(item(d,a)->isHidden());QVERIFY(!save->isEnabled());
        QVERIFY(bytes(lib.absolute(lib.track(a).lyrics)).contains("relaxer-bilingual"));QCOMPARE(lib.translationJudgment(a),J::NeedsTranslation);QVERIFY(lib.track(b).lyrics.isEmpty());QCOMPARE(paid.requestCount(),1);
        all(d);select(d,a);QVERIFY(!ai->isEnabled());QCOMPARE(paid.requestCount(),1);
    }
    void saveFailureRetainsPreviewAndCanRetry(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);const auto id=ids[0];QVERIFY(lib.saveLyrics(id,source));
        TranslationHttpMock paid;QVERIFY(paid.listen());paid.enqueue(response());AiRuntime runtime(&lib,DeepSeekProvider::MockEndpoint{paid.baseUrl()},std::make_unique<FakeCredentialStore>());QVERIFY(configureRuntime(runtime));
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"),{},true,&runtime);all(d);select(d,id);consent(d);auto *save=button(d,"lyricsSaveTranslation");QTRY_VERIFY(save->isEnabled());
        const auto text=d.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText();const auto task=LyricsLookupDiagnostics::preview(d).taskId;
        QSqlQuery q(import_fixture::database(lib));QVERIFY(q.exec("CREATE TRIGGER deny_lyrics BEFORE UPDATE OF lyrics ON tracks BEGIN SELECT RAISE(ABORT,'denied'); END"));save->click();
        QVERIFY(save->isEnabled());QVERIFY(!green(d,id));QCOMPARE(LyricsLookupDiagnostics::preview(d).taskId,task);QCOMPARE(d.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText(),text);QCOMPARE(bytes(lib.absolute(lib.track(id).lyrics)),source);QCOMPARE(paid.requestCount(),1);
        QVERIFY(q.exec("DROP TRIGGER deny_lyrics"));save->click();QVERIFY(!save->isEnabled());QVERIFY(green(d,id));QCOMPARE(paid.requestCount(),1);
    }
    void classificationPreviewProtection_data(){
        QTest::addColumn<bool>("unknown");QTest::addColumn<bool>("discard");
        QTest::newRow("no-cancel")<<false<<false;QTest::newRow("no-discard")<<false<<true;
        QTest::newRow("unknown-cancel")<<true<<false;QTest::newRow("unknown-discard")<<true<<true;
    }
    void classificationPreviewProtection(){
        QFETCH(bool,unknown);QFETCH(bool,discard);QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);const auto id=ids[0];QVERIFY(lib.saveLyrics(id,source));
        TranslationHttpMock paid;QVERIFY(paid.listen());paid.enqueue(response());AiRuntime runtime(&lib,DeepSeekProvider::MockEndpoint{paid.baseUrl()},std::make_unique<FakeCredentialStore>());QVERIFY(configureRuntime(runtime));
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"),{},true,&runtime);all(d);select(d,id);consent(d);QTRY_VERIFY(button(d,"lyricsSaveTranslation")->isEnabled());const auto task=LyricsLookupDiagnostics::preview(d).taskId;const auto text=d.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText();
        int prompts=0;QTimer::singleShot(0,&d,[&]{auto *box=d.findChild<QMessageBox *>("lyricsUnsavedTranslationClassificationConfirm");QVERIFY(box);++prompts;QCOMPARE(box->defaultButton()->objectName(),QString("lyricsKeepClassificationPreview"));
            d.close();select(d,ids[1]);QCOMPARE(d.findChildren<QMessageBox *>().size(),1);
            box->findChild<QPushButton *>(discard?"lyricsDiscardClassificationPreview":"lyricsKeepClassificationPreview")->click();});
        button(d,unknown?"lyricsNeedsTranslation":"lyricsNoTranslationNeeded")->click();QCOMPARE(prompts,1);QCOMPARE(paid.requestCount(),1);QCOMPARE(bytes(lib.absolute(lib.track(id).lyrics)),source);
        if(discard){QCOMPARE(lib.translationJudgment(id),unknown?J::Unknown:J::NoTranslationNeeded);QVERIFY(!button(d,"lyricsSaveTranslation")->isEnabled());QVERIFY(!LyricsLookupDiagnostics::preview(d).hasPreview());}
        else {QCOMPARE(lib.translationJudgment(id),J::NeedsTranslation);QCOMPARE(LyricsLookupDiagnostics::preview(d).taskId,task);QCOMPARE(d.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText(),text);QVERIFY(button(d,"lyricsSaveTranslation")->isEnabled());button(d,"lyricsSaveTranslation")->click();QVERIFY(green(d,id));}
        QCOMPARE(paid.requestCount(),1);
    }
    void classificationFailureDoesNotDiscardPaidPreview(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);const auto id=ids[0];QVERIFY(lib.saveLyrics(id,source));
        TranslationHttpMock paid;QVERIFY(paid.listen());paid.enqueue(response());AiRuntime runtime(&lib,DeepSeekProvider::MockEndpoint{paid.baseUrl()},std::make_unique<FakeCredentialStore>());QVERIFY(configureRuntime(runtime));
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"),{},true,&runtime);all(d);select(d,id);consent(d);QTRY_VERIFY(button(d,"lyricsSaveTranslation")->isEnabled());const auto task=LyricsLookupDiagnostics::preview(d).taskId;
        QSqlQuery q(import_fixture::database(lib));QVERIFY(q.exec("CREATE TRIGGER deny_judgment BEFORE INSERT ON settings WHEN NEW.key LIKE 'lyrics.translationJudgment.%' BEGIN SELECT RAISE(ABORT,'denied'); END"));
        QTimer::singleShot(0,&d,[&]{d.findChild<QMessageBox *>("lyricsUnsavedTranslationClassificationConfirm")->findChild<QPushButton *>("lyricsDiscardClassificationPreview")->click();});button(d,"lyricsNoTranslationNeeded")->click();
        QCOMPARE(lib.translationJudgment(id),J::NeedsTranslation);QCOMPARE(LyricsLookupDiagnostics::preview(d).taskId,task);QVERIFY(button(d,"lyricsSaveTranslation")->isEnabled());QVERIFY(!green(d,id));QCOMPARE(paid.requestCount(),1);QCOMPARE(bytes(lib.absolute(lib.track(id).lyrics)),source);
    }
    void searchAndPassiveRefreshKeepPreview(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);const auto id=ids[0];QVERIFY(lib.saveLyrics(id,source));
        TranslationHttpMock paid;QVERIFY(paid.listen());paid.enqueue(response());AiRuntime runtime(&lib,DeepSeekProvider::MockEndpoint{paid.baseUrl()},std::make_unique<FakeCredentialStore>());QVERIFY(configureRuntime(runtime));
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"),{},true,&runtime);all(d);select(d,id);consent(d);QTRY_VERIFY(button(d,"lyricsSaveTranslation")->isEnabled());const auto task=LyricsLookupDiagnostics::preview(d).taskId;const auto text=d.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText();
        button(d,"lyricsStart")->click();button(d,"lyricsManualSearch")->click();d.findChild<QCheckBox *>("lyricsMissingTranslationOnly")->setChecked(true);button(d,"lyricsRefreshTranslationFilter")->click();d.findChild<QListWidget *>("lyricsTracks")->clearSelection();
        QCOMPARE(LyricsLookupDiagnostics::preview(d).taskId,task);QVERIFY(LyricsLookupDiagnostics::preview(d).hasPreview());QCOMPARE(d.findChild<QPlainTextEdit *>("lyricsTranslationPreview")->toPlainText(),text);QVERIFY(button(d,"lyricsSaveTranslation")->isEnabled());QCOMPARE(paid.requestCount(),1);
    }
    void changedJudgmentDuringConsentDoesNotSend(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);const auto id=ids[0];QVERIFY(lib.saveLyrics(id,source));TranslationHttpMock paid;QVERIFY(paid.listen());
        AiRuntime runtime(&lib,DeepSeekProvider::MockEndpoint{paid.baseUrl()},std::make_unique<FakeCredentialStore>());QVERIFY(configureRuntime(runtime));LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"),{},true,&runtime);all(d);select(d,id);button(d,"lyricsNeedsTranslation")->click();
        QTimer::singleShot(0,&d,[&]{QVERIFY(!button(d,"lyricsNoTranslationNeeded")->isEnabled());QVERIFY(lib.setTranslationJudgment(id,J::NoTranslationNeeded));d.findChild<QMessageBox *>("lyricsAiCloudConsent")->findChild<QPushButton *>("lyricsAiConsentSend")->click();});button(d,"lyricsTranslate")->click();
        QCoreApplication::processEvents();QCOMPARE(paid.requestCount(),0);QVERIFY(!button(d,"lyricsSaveTranslation")->isEnabled());QVERIFY(!button(d,"lyricsTranslate")->isEnabled());QCOMPARE(bytes(lib.absolute(lib.track(id).lyrics)),source);
    }
    void externalSameStampChangeInvalidatesAutomaticCompletion(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);const auto id=ids[0];const auto bilingual=QString("[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]朋友你好").toUtf8();QVERIFY(lib.saveLyricsPreservingText(id,bilingual));
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"));all(d);select(d,id);QVERIFY(green(d,id));const auto path=lib.absolute(lib.track(id).lyrics);const auto stamp=QFileInfo(path).lastModified();QByteArray changed=source;changed+=QByteArray(bilingual.size()-changed.size(),' ');QCOMPARE(changed.size(),bilingual.size());
        QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly|QIODevice::Truncate));QCOMPARE(f.write(changed),qint64(changed.size()));QVERIFY(f.setFileTime(stamp,QFileDevice::FileModificationTime));f.close();
        LyricsLookupDiagnostics::audit(d);QTRY_VERIFY(!green(d,id));QCOMPARE(lib.translationJudgment(id),J::Unknown);QCOMPARE(bytes(path),changed);
    }
    void markedSavedTranslationMustContainValidPairs_data(){
        QTest::addColumn<QByteArray>("data");QTest::addColumn<bool>("complete");
        QTest::newRow("chinese-pairs")<<QString("[re:relaxer-bilingual]\n[00:01.00]你好朋友\n[00:01.00]朋友你好").toUtf8()<<true;
        QTest::newRow("chinese-pairs-credit")<<QString("[re:relaxer-bilingual]\n[00:00.00]作词：测试作者\n[00:01.00]你好朋友\n[00:01.00]朋友你好").toUtf8()<<true;
        QTest::newRow("marker-only")<<QByteArray("[re:relaxer-bilingual]")<<false;
        QTest::newRow("missing-partner")<<QString("[re:relaxer-bilingual]\n[00:01.00]你好朋友").toUtf8()<<false;
        QTest::newRow("different-time")<<QString("[re:relaxer-bilingual]\n[00:01.00]你好朋友\n[00:02.00]朋友你好").toUtf8()<<false;
        QTest::newRow("extra-unpaired-body")<<QString("[re:relaxer-bilingual]\n[00:01.00]你好朋友\n[00:01.00]朋友你好\n额外原文").toUtf8()<<false;
    }
    void markedSavedTranslationMustContainValidPairs(){
        QFETCH(QByteArray,data);QFETCH(bool,complete);QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);QVERIFY(lib.saveLyricsPreservingText(ids[0],source));
        QFile external(lib.absolute(lib.track(ids[0]).lyrics));QVERIFY(external.open(QIODevice::WriteOnly|QIODevice::Truncate));QCOMPARE(external.write(data),qint64(data.size()));external.close();
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"));all(d);select(d,ids[0]);QCOMPARE(green(d,ids[0]),complete);QCOMPARE(lib.translationJudgment(ids[0]),J::Unknown);QCOMPARE(bytes(lib.absolute(lib.track(ids[0]).lyrics)),data);
    }
    void serviceAlsoRechecksClassificationAndRefusesDuplicateTasks(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);const auto id=ids[0];QVERIFY(lib.saveLyrics(id,source));
        TranslationHttpMock paid;QVERIFY(paid.listen());paid.enqueue(response());AiRuntime runtime(&lib,DeepSeekProvider::MockEndpoint{paid.baseUrl()},std::make_unique<FakeCredentialStore>());QVERIFY(configureRuntime(runtime));TranslationCoordinator coordinator(&lib,&runtime);
        QString error;QCOMPARE(coordinator.prepareAi(id,&error),quint64(0));QVERIFY(!error.isEmpty());QVERIFY(lib.setTranslationJudgment(id,J::NoTranslationNeeded));QCOMPARE(coordinator.prepareAi(id),quint64(0));
        QVERIFY(lib.setTranslationJudgment(id,J::NeedsTranslation));auto task=coordinator.prepareAi(id);QVERIFY(task);QCOMPARE(coordinator.prepareAi(id),quint64(0));QVERIFY(lib.setTranslationJudgment(id,J::Unknown));QVERIFY(!coordinator.start(task));QCOMPARE(paid.requestCount(),0);
        QVERIFY(lib.setTranslationJudgment(id,J::NeedsTranslation));task=coordinator.prepareAi(id);QVERIFY(coordinator.start(task));QVERIFY(lib.setTranslationJudgment(id,J::NoTranslationNeeded));QTRY_COMPARE(coordinator.summary(task).state,TranslationState::Invalidated);QCOMPARE(paid.requestCount(),0);
        QVERIFY(lib.setTranslationJudgment(id,J::NeedsTranslation));task=coordinator.prepareAi(id);QVERIFY(coordinator.start(task));QTRY_VERIFY(coordinator.summary(task).hasPreview());QCOMPARE(paid.requestCount(),1);QCOMPARE(coordinator.prepareAi(id),quint64(0));QCOMPARE(bytes(lib.absolute(lib.track(id).lyrics)),source);QVERIFY(coordinator.discard(task));
    }
    // D2-06: the Widgets filter consumes the shared decision; a "missing" row is
    // visible under the translation-only filter and a "completed" row is green.
    void filterMatchesSharedClassification(){
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto ids=seed(lib);QCOMPARE(ids.size(),3);
        // Foreign body -> needs translation; marked bilingual -> translated; empty -> no source.
        QVERIFY(lib.saveLyrics(ids[0],source));
        QVERIFY(lib.saveLyrics(ids[1],QString("[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]朋友你好\n[00:02.00]Goodbye friend\n[00:02.00]朋友再见\n").toUtf8()));
        LyricsLookup d(&lib,ids,nullptr,QUrl("http://127.0.0.1:9/api"));all(d);
        d.findChild<QCheckBox *>("lyricsMissingTranslationOnly")->setChecked(true);
        // ids[0] foreign -> shown in filter, badge 待判断, not green
        QVERIFY(!item(d,ids[0])->isHidden());QCOMPARE(badge(d,ids[0]),QString("待判断"));QVERIFY(!green(d,ids[0]));
        // ids[1] marked bilingual -> hidden in filter, green, 翻译完成
        QVERIFY(item(d,ids[1])->isHidden());QVERIFY(green(d,ids[1]));QCOMPARE(badge(d,ids[1]),QString("翻译完成"));
        // ids[2] no lyrics -> not missing, not green, badge empty
        QVERIFY(item(d,ids[2])->isHidden());QVERIFY(!green(d,ids[2]));
        QCOMPARE(badge(d,ids[2]),QString());
    }
};
QTEST_MAIN(TranslationWorkflowTests)
#include "translation-workflow-tests.moc"
