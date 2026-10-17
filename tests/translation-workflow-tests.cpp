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
};
QTEST_MAIN(TranslationWorkflowTests)
#include "translation-workflow-tests.moc"
