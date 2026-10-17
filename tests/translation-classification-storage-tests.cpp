#include "import-fixture.h"
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
using J=TranslationJudgment;
namespace {
qint64 seed(Library &lib,QString hash) {
    Track t;t.hash=hash;t.file="Music/"+hash+".mid";t.title=hash;t.artist="Artist";
    QDir().mkpath(lib.absolute("Music"));if(!import_fixture::midi(lib.absolute(t.file)))return 0;
    return lib.insert(t);
}
bool sql(const Library &lib,QString text){QSqlQuery q(import_fixture::database(lib));return q.exec(text);}
QString key(qint64 id){return QString("lyrics.translationJudgment.v1.%1").arg(id);}
}
class TranslationClassificationStorageTests : public QObject {
    Q_OBJECT
  private slots:
    void defaultsAndAllTransitions() {
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto id=seed(lib,"first");QVERIFY(id);
        QCOMPARE(lib.translationJudgment(id),J::Unknown);
        for(const auto judgment:{J::NoTranslationNeeded,J::Unknown,J::NeedsTranslation,J::Unknown,J::NoTranslationNeeded,J::NeedsTranslation,J::NoTranslationNeeded}){
            QVERIFY(lib.setTranslationJudgment(id,judgment));QCOMPARE(lib.translationJudgment(id),judgment);
            QVERIFY(!lib.isInstrumental(id));QVERIFY(lib.track(id).lyrics.isEmpty());
        }
        QCOMPARE(lib.setting(key(id)).toObject().value("state").toString(),QString("not-needed"));
        QVERIFY(lib.setTranslationJudgment(id,J::Unknown));QVERIFY(lib.setting(key(id),QJsonValue(QJsonValue::Undefined)).isUndefined());
    }
    void restartTwiceSchemaAndUnknownSettings() {
        QTemporaryDir dir;qint64 a,b;QStringList schema;QByteArray lyrics;
        const QJsonObject unknown{{"未来",QJsonArray{true,17,"keep"}}};
        {Library lib(dir.path());QVERIFY(lib.open());schema=import_fixture::schema(lib);a=seed(lib,"a");b=seed(lib,"b");QVERIFY(a&&b);
         QVERIFY(lib.setSetting("unknown-extension",unknown));QVERIFY(lib.favorite(a,true));
         lyrics=QString("[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]朋友你好").toUtf8();QVERIFY(lib.saveLyricsPreservingText(a,lyrics));
         QVERIFY(lib.setTranslationJudgment(a,J::NoTranslationNeeded));QVERIFY(lib.setTranslationJudgment(b,J::NeedsTranslation));}
        for(int n=0;n<2;++n){Library lib(dir.path());QVERIFY(lib.open());QCOMPARE(import_fixture::schema(lib),schema);
            QCOMPARE(lib.translationJudgment(a),J::NoTranslationNeeded);QCOMPARE(lib.translationJudgment(b),J::NeedsTranslation);
            QCOMPARE(lib.setting("unknown-extension").toObject(),unknown);QVERIFY(lib.track(a).favorite);
            QFile f(lib.absolute(lib.track(a).lyrics));QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),lyrics);}
        {Library lib(dir.path());QVERIFY(lib.open());QVERIFY(lib.setTranslationJudgment(a,J::Unknown));QFile f(lib.absolute(lib.track(a).lyrics));QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),lyrics);}
    }
    void invalidAndFutureStatesCannotCrossTracks() {
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto a=seed(lib,"a"),b=seed(lib,"b");
        QVERIFY(!lib.setTranslationJudgment(0,J::NeedsTranslation));QVERIFY(!lib.setTranslationJudgment(99999,J::NeedsTranslation));
        QVERIFY(!lib.setTranslationJudgment(a,static_cast<J>(999)));QCOMPARE(lib.translationJudgment(a),J::Unknown);
        QVERIFY(lib.setSetting(key(a),QJsonObject{{"state","future-state"},{"trackHash","a"}}));QCOMPARE(lib.translationJudgment(a),J::Unknown);
        QVERIFY(lib.setSetting(key(a),QJsonObject{{"state","needs"},{"trackHash","b"}}));QCOMPARE(lib.translationJudgment(a),J::Unknown);QCOMPARE(lib.translationJudgment(b),J::Unknown);
        QVERIFY(lib.setSetting(key(a),QJsonArray{true}));QCOMPARE(lib.translationJudgment(a),J::Unknown);
    }
    void deletionIdReuseDuplicatesAndPathChanges() {
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto a=seed(lib,"a");QVERIFY(a);
        QVERIFY(lib.setTranslationJudgment(a,J::NeedsTranslation));bool added=true;
        QCOMPARE(lib.insertImportedTrack(lib.track(a),&added),a);QVERIFY(!added);QCOMPARE(lib.translationJudgment(a),J::NeedsTranslation);
        QVERIFY(sql(lib,"UPDATE tracks SET file='Music/moved.mid' WHERE id="+QString::number(a)));QCOMPARE(lib.translationJudgment(a),J::NeedsTranslation);
        QVERIFY(lib.remove({a}));QVERIFY(lib.setting(key(a),QJsonValue(QJsonValue::Undefined)).isUndefined());QCOMPARE(lib.translationJudgment(a),J::Unknown);
        const auto b=seed(lib,"b");QVERIFY(b);QCOMPARE(lib.translationJudgment(b),J::Unknown);
        QVERIFY(lib.setTranslationJudgment(b,J::NoTranslationNeeded));QVERIFY(lib.removeManaged({b}));QVERIFY(lib.setting(key(b),QJsonValue(QJsonValue::Undefined)).isUndefined());
        const auto c=seed(lib,"b");QVERIFY(c);QCOMPARE(lib.translationJudgment(c),J::Unknown);
    }
    void sqlFailuresRemainAtomicAndRetryable() {
        QTemporaryDir dir;Library lib(dir.path());QVERIFY(lib.open());const auto id=seed(lib,"a");
        QVERIFY(lib.setTranslationJudgment(id,J::NeedsTranslation));
        QVERIFY(sql(lib,"CREATE TRIGGER reject_judgment BEFORE INSERT ON settings WHEN NEW.key LIKE 'lyrics.translationJudgment.%' BEGIN SELECT RAISE(ABORT,'classification denied'); END"));
        QVERIFY(!lib.setTranslationJudgment(id,J::NoTranslationNeeded));QCOMPARE(lib.translationJudgment(id),J::NeedsTranslation);
        QVERIFY(!lib.lastError().isEmpty());QVERIFY(sql(lib,"DROP TRIGGER reject_judgment"));QVERIFY(lib.setTranslationJudgment(id,J::NoTranslationNeeded));
        QVERIFY(sql(lib,"CREATE TRIGGER reject_delete BEFORE DELETE ON settings WHEN OLD.key LIKE 'lyrics.translationJudgment.%' BEGIN SELECT RAISE(ABORT,'delete denied'); END"));
        QVERIFY(!lib.setTranslationJudgment(id,J::Unknown));QCOMPARE(lib.translationJudgment(id),J::NoTranslationNeeded);
        QVERIFY(!lib.remove({id}));QVERIFY(lib.track(id).id);QCOMPARE(lib.translationJudgment(id),J::NoTranslationNeeded);
        QVERIFY(sql(lib,"DROP TRIGGER reject_delete"));QVERIFY(lib.setTranslationJudgment(id,J::Unknown));QCOMPARE(lib.translationJudgment(id),J::Unknown);
    }
};
QTEST_GUILESS_MAIN(TranslationClassificationStorageTests)
#include "translation-classification-storage-tests.moc"
