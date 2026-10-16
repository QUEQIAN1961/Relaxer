#include "library.h"
#ifdef RELAXER_SETTINGS_FACADE
#include "storage/settings-store.h"
#include "storage/appearance-store.h"
#include "appearance-image.h"
using SettingsAccess=relaxer::SettingsStore;
#else
#include "appearance.h"
// Baseline adapter: every operation calls the actual accepted B3 Library.
struct SettingsAccess {
    relaxer::Library *library;
    explicit SettingsAccess(relaxer::Library *value):library(value){}
    QJsonValue get(const QString &key,const QJsonValue &fallback={})const{return library->setting(key,fallback);}
    bool set(const QString &key,const QJsonValue &value){return library->setSetting(key,value);}
    bool setMany(const QJsonObject &values){return library->setSettings(values);}
};
#endif
#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QSqlError>
#include <QTemporaryDir>
using namespace relaxer;
namespace {
QSqlDatabase databaseFor(const Library &library){
    for(const auto &name:QSqlDatabase::connectionNames()){
        auto db=QSqlDatabase::database(name);
        if(db.databaseName()==library.absolute("relaxer.sqlite"))return db;
    }
    return {};
}
QByteArray rawSetting(const Library &library,const QString &key){
    QSqlQuery query(databaseFor(library));query.prepare("SELECT value FROM settings WHERE key=?");query.addBindValue(key);
    return query.exec()&&query.next()?query.value(0).toByteArray():QByteArray();
}
bool insertRaw(const Library &library,const QString &key,const QByteArray &value){
    QSqlQuery query(databaseFor(library));query.prepare("INSERT OR REPLACE INTO settings(key,value) VALUES(?,?)");query.addBindValue(key);query.addBindValue(value.isEmpty()?QStringLiteral(""):QString::fromUtf8(value));return query.exec();
}
QStringList images(const Library &library){return QDir(library.absolute("Appearance")).entryList({"*.png"},QDir::Files,QDir::Name);}
QJsonObject extension(){return {{"future",QJsonArray{QJsonObject{{"nested",QJsonArray{true,QJsonValue(),3.25,"文字"}}},7}},{"flag",false}};}
bool image(const QString &path,const QColor &color){QImage pixels(24,16,QImage::Format_ARGB32);pixels.fill(color);return pixels.save(path);}
}
class SettingsAppearanceTests:public QObject {
    Q_OBJECT
  private slots:
    void newLibraryAndExistingTheme_data(){QTest::addColumn<int>("theme");for(int value:{0,1,2,3})QTest::newRow(qPrintable(QString::number(value)))<<value;}
    void newLibraryAndExistingTheme(){
        QFETCH(int,theme);QTemporaryDir dir;const auto root=dir.filePath("data");
        {Library library(root);QVERIFY(library.open());SettingsAccess settings(&library);QCOMPARE(settings.get("theme").toInt(),2);QCOMPARE(rawSetting(library,"theme"),QByteArray("[2]"));QVERIFY(settings.set("theme",theme));}
        {Library library(root);QVERIFY(library.open());SettingsAccess settings(&library);QCOMPARE(settings.get("theme").toInt(),theme);QCOMPARE(rawSetting(library,"theme"),QJsonDocument(QJsonArray{theme}).toJson(QJsonDocument::Compact));}
    }
    void legacyEncoding_data(){
        QTest::addColumn<QByteArray>("bytes");QTest::addColumn<QJsonValue>("expected");
        QTest::newRow("integer")<<QByteArray("[1]")<<QJsonValue(1);
        QTest::newRow("boolean")<<QByteArray("[false]")<<QJsonValue(false);
        QTest::newRow("string")<<QByteArray("[\"baidu\"]")<<QJsonValue("baidu");
        QTest::newRow("null")<<QByteArray("[null]")<<QJsonValue(QJsonValue::Null);
        QTest::newRow("nested")<<QJsonDocument(QJsonArray{extension()}).toJson(QJsonDocument::Compact)<<QJsonValue(extension());
        QTest::newRow("multi-element-first")<<QByteArray("[2,99]")<<QJsonValue(2);
        for(const auto &value:{QByteArray("[]"),QByteArray("{\"theme\":2}"),QByteArray("broken"),QByteArray("2"),QByteArray()})QTest::newRow(("fallback-"+value).constData())<<value<<QJsonValue("fallback");
    }
    void legacyEncoding(){
        QFETCH(QByteArray,bytes);QFETCH(QJsonValue,expected);QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QVERIFY(insertRaw(library,"legacy",bytes));
        SettingsAccess settings(&library);QCOMPARE(settings.get("legacy","fallback"),expected);QCOMPARE(rawSetting(library,"legacy"),bytes);QCOMPARE(settings.get("absent",extension()),QJsonValue(extension()));
    }
    void unknownSettingsAndTypesSurviveReopen(){
        QTemporaryDir dir;const auto root=dir.filePath("data");
        const QJsonObject values{{"unknown",extension()},{"volume",37},{"translateLyrics",false},{"lyricBrowserProvider","bing"},{"array",QJsonArray{2,"x",extension()}},{"null",QJsonValue(QJsonValue::Null)},{"",true},{"quote'键",-9.5}};
        {Library library(root);QVERIFY(library.open());SettingsAccess settings(&library);QVERIFY(settings.setMany(values));QVERIFY(settings.set("theme",1));QVERIFY(settings.set("undefined",QJsonValue(QJsonValue::Undefined)));QCOMPARE(rawSetting(library,"undefined"),QByteArray("[null]"));}
        {Library library(root);QVERIFY(library.open());SettingsAccess settings(&library);for(auto it=values.begin();it!=values.end();++it){QCOMPARE(settings.get(it.key()),it.value());QCOMPARE(rawSetting(library,it.key()),QJsonDocument(QJsonArray{it.value()}).toJson(QJsonDocument::Compact));}}
    }
    void notificationAndConnectionContract(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());const auto connections=QSqlDatabase::connectionNames();SettingsAccess settings(&library);QSignalSpy changed(&library,&Library::changed),stats(&library,&Library::statsChanged);
        QVERIFY(settings.set("theme",1));QCOMPARE(settings.get("theme").toInt(),1);QVERIFY(settings.setMany({{"theme",2},{"volume",17}}));QVERIFY(settings.setMany({}));
        QCOMPARE(changed.size(),0);QCOMPARE(stats.size(),0);QCOMPARE(QSqlDatabase::connectionNames(),connections);
        library.deferChanges(true);library.deferChanges(true);Track track;track.hash="notify";track.file="Music/temp.wav";track.title="temporary";QVERIFY(library.insert(track)>0);QVERIFY(settings.set("theme",0));QCOMPARE(changed.size(),0);
        int visibleAtNotify=-1;connect(&library,&Library::changed,this,[&]{visibleAtNotify=settings.get("theme").toInt();});
        library.deferChanges(false);QCOMPARE(changed.size(),0);library.deferChanges(false);QCOMPARE(changed.size(),1);QCOMPARE(visibleAtNotify,0);QCOMPARE(stats.size(),0);
    }
    void failedWritesAndBatchRollback(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());SettingsAccess settings(&library);QVERIFY(settings.setMany({{"aa-first",4},{"zz-fail",5},{"unknown",extension()}}));QSignalSpy spy(&library,&Library::changed);
        QSqlQuery query(databaseFor(library));QVERIFY(query.exec("CREATE TEMP TRIGGER reject_setting BEFORE INSERT ON settings WHEN NEW.key='zz-fail' BEGIN SELECT RAISE(ABORT,'b4 injected failure'); END"));
        QVERIFY(!settings.setMany({{"aa-first",40},{"zz-fail",50}}));QCOMPARE(settings.get("aa-first").toInt(),4);QCOMPARE(settings.get("zz-fail").toInt(),5);QCOMPARE(settings.get("unknown"),QJsonValue(extension()));QVERIFY(!library.lastError().isEmpty());
        QVERIFY(!settings.set("zz-fail",99));QCOMPARE(rawSetting(library,"zz-fail"),QByteArray("[5]"));QCOMPARE(spy.size(),0);
        QVERIFY(query.exec("DROP TRIGGER reject_setting"));QVERIFY(settings.set("zz-fail",99));
    }
    void oldMinimalDatabaseOpensWithoutOverwritingTheme(){
        QTemporaryDir dir;const auto root=dir.filePath("data");QVERIFY(QDir().mkpath(root));const QString name="b4-legacy";
        {auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(QDir(root).filePath("relaxer.sqlite"));QVERIFY(db.open());QSqlQuery query(db);QVERIFY(query.exec("CREATE TABLE settings(key TEXT PRIMARY KEY,value TEXT NOT NULL)"));QVERIFY(query.exec("INSERT INTO settings VALUES('theme','[1]')"));QVERIFY(query.exec("INSERT INTO settings VALUES('future','[{\"nested\":[1,true,null]}]')"));db.close();}
        QSqlDatabase::removeDatabase(name);Library library(root);QVERIFY(library.open());SettingsAccess settings(&library);QCOMPARE(settings.get("theme").toInt(),1);QCOMPARE(rawSetting(library,"future"),QByteArray("[{\"nested\":[1,true,null]}]"));
    }
    void historyFilteringIsReadOnly(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());auto valid=AppearanceStore::defaults();valid["id"]="one";valid["name"]="旧外观";valid["extension"]=extension();auto duplicate=valid;duplicate["name"]="duplicate";
        const QJsonObject raw{{"items",QJsonArray{QJsonObject(),valid,duplicate,QJsonObject{{"id","bad"},{"mode","unknown"}}}},{"active","one"},{"legacyUnknownWrapper",extension()}};
        QVERIFY(library.setSetting("appearanceHistory",raw));const auto before=rawSetting(library,"appearanceHistory");AppearanceStore store(&library);
        QCOMPARE(store.state(),QJsonObject({{"items",QJsonArray{valid}},{"active","one"}}));QCOMPARE(store.active(),valid);QCOMPARE(rawSetting(library,"appearanceHistory"),before);
        auto draft=store.draft(valid);QCOMPARE(draft["extension"],QJsonValue(extension()));draft["right"]="#111111";QCOMPARE(store.active(),valid);QCOMPARE(rawSetting(library,"appearanceHistory"),before);
    }
    void paletteHistorySaveRenameOverwriteSwitchRemove(){
        QTemporaryDir dir;const auto root=dir.filePath("data");QString first,second;QJsonObject finalState;
        {Library library(root);QVERIFY(library.open());AppearanceStore store(&library);auto theme=AppearanceStore::defaults();theme["extension"]=extension();theme["left"]="#AABBCC";theme["blurLeft"]=99;theme["blurRight"]=-1;theme["blurExpanded"]=52;QSignalSpy spy(&library,&Library::changed);
         QVERIFY(store.save(theme,{}));auto a=store.active();first=a["id"].toString();QVERIFY(!first.isEmpty());QCOMPARE(a["name"].toString(),QString("主题 1"));QCOMPARE(a["left"].toString(),QString("#aabbcc"));QCOMPARE(a["blurLeft"].toDouble(),50.);QCOMPARE(a["blurRight"].toDouble(),0.);QCOMPARE(a["blurExpanded"].toDouble(),50.);QCOMPARE(library.setting("theme").toInt(),3);
         QVERIFY(store.rename(first,"  测试外观  "));QVERIFY(!store.rename(first,"  "));QCOMPARE(store.active()["name"].toString(),QString("测试外观"));
         theme=store.draft(store.active());theme["right"]="#123456";QVERIFY(store.save(theme,first));QCOMPARE(store.active()["id"].toString(),first);QCOMPARE(store.active()["name"].toString(),QString("测试外观"));QCOMPARE(store.active()["extension"],QJsonValue(extension()));QCOMPARE(store.state()["items"].toArray().size(),1);
         QVERIFY(store.save(AppearanceStore::defaults(),{}));second=store.active()["id"].toString();QVERIFY(first!=second);QVERIFY(store.use(first));QCOMPARE(store.active()["right"].toString(),QString("#123456"));QVERIFY(!store.use("missing"));QVERIFY(!store.remove("missing"));QVERIFY(!store.save(theme,"missing"));
         QVERIFY(store.remove(second));QCOMPARE(library.setting("theme").toInt(),3);QCOMPARE(spy.size(),0);finalState=store.state();}
        for(int reopen=0;reopen<3;++reopen){Library library(root);QVERIFY(library.open());AppearanceStore store(&library);QCOMPARE(store.state(),finalState);QCOMPARE(store.active()["extension"],QJsonValue(extension()));}
        {Library library(root);QVERIFY(library.open());AppearanceStore store(&library);QVERIFY(store.remove(first));QCOMPARE(library.setting("theme").toInt(),0);QCOMPARE(store.active(),AppearanceStore::defaults());QCOMPARE(store.state()["active"].toString(),QString());}
    }
    void imageReferencesDraftAndReopen(){
        QTemporaryDir dir;const auto root=dir.filePath("data"),path=dir.filePath("image.png");QVERIFY(image(path,QColor(80,110,140,180)));QByteArray stored;QString relative;QJsonObject saved;
        {Library library(root);QVERIFY(library.open());AppearanceStore store(&library);auto theme=AppearanceStore::defaults();theme["mode"]="image";theme["shareLR"]=true;theme["shareFooter"]=false;theme["sourceLeft"]=path;theme["sourceFooter"]=path;theme["blurLeft"]=2.5;theme["extension"]=extension();QVERIFY(store.save(theme,{}));saved=store.active();relative=saved["imageLeft"].toString();QCOMPARE(saved["imageRight"].toString(),relative);QCOMPARE(saved["imageFooter"].toString(),relative);QCOMPARE(images(library).size(),1);QVERIFY(!saved.contains("sourceLeft"));QCOMPARE(saved["blurLeft"].toDouble(),2.5);
         QFile file(library.absolute(relative));QVERIFY(file.open(QIODevice::ReadOnly));stored=file.readAll();QCOMPARE(relative,QString("Appearance/")+QString::fromLatin1(QCryptographicHash::hash(stored,QCryptographicHash::Sha256).toHex())+".png");
         const auto snapshot=rawSetting(library,"appearanceHistory");auto draft=store.draft(saved);QCOMPARE(QFileInfo(draft["sourceLeft"].toString()).canonicalFilePath(),QFileInfo(library.absolute(relative)).canonicalFilePath());draft["blurLeft"]=49.5;QCOMPARE(rawSetting(library,"appearanceHistory"),snapshot);
         QVERIFY(store.save(store.draft(saved),saved["id"].toString()));QCOMPARE(images(library).size(),1);}
        {Library library(root);QVERIFY(library.open());AppearanceStore store(&library);QCOMPARE(store.active(),saved);QVERIFY(!store.imagePath(saved,"left").isEmpty());auto bad=saved;bad["imageLeft"]="Appearance/../image.png";QVERIFY(store.imagePath(bad,"left").isEmpty());bad["imageLeft"]=path;QVERIFY(store.imagePath(bad,"left").isEmpty());QVERIFY(store.remove(saved["id"].toString()));QVERIFY(QFile::exists(library.absolute(relative)));}
    }
    void failedAppearanceSavesPreserveRecordsAndFiles(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());AppearanceStore store(&library);QVERIFY(store.save(AppearanceStore::defaults(),{}));const auto before=rawSetting(library,"appearanceHistory");const auto id=store.active()["id"].toString();QString error;
        auto bad=AppearanceStore::defaults();bad["mode"]="bad";QVERIFY(!store.save(bad,id,&error));QCOMPARE(error,QString("请选择调色盘或图片模式。"));bad=AppearanceStore::defaults();bad["left"]="invalid";QVERIFY(!store.save(bad,id,&error));QCOMPARE(error,QString("颜色无效，请重新选择。"));QCOMPARE(rawSetting(library,"appearanceHistory"),before);
        const auto source=dir.filePath("new.png");QVERIFY(image(source,Qt::green));auto theme=AppearanceStore::defaults();theme["mode"]="image";theme["shareLR"]=true;theme["sourceLeft"]=source;
        QSqlQuery query(databaseFor(library));QVERIFY(query.exec("CREATE TEMP TRIGGER reject_appearance BEFORE INSERT ON settings WHEN NEW.key='theme' BEGIN SELECT RAISE(ABORT,'appearance failure'); END"));
        const auto oldImages=images(library);QVERIFY(!store.save(theme,id,&error));QVERIFY(!error.isEmpty());QCOMPARE(rawSetting(library,"appearanceHistory"),before);QCOMPARE(library.setting("theme").toInt(),3);QCOMPARE(images(library),oldImages);QVERIFY(QFile::exists(source));
    }
    void progressInitializationPreservesItemExtensions(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());auto theme=AppearanceStore::defaults();theme["id"]="legacy";theme["name"]="旧主题";theme["extension"]=extension();theme["customProgressColors"]=true;theme["progressFill"]="#102030";theme["progressTrack"]="#405060";QVERIFY(library.setSetting("appearanceHistory",QJsonObject{{"items",QJsonArray{theme}},{"active","legacy"}}));AppearanceStore store(&library);QVERIFY(store.initializeProgressColors());QCOMPARE(store.active()["extension"],QJsonValue(extension()));QCOMPARE(store.active()["progressFill"].toString(),QString("#102030"));const auto bytes=rawSetting(library,"appearanceHistory");QVERIFY(store.initializeProgressColors());QCOMPARE(rawSetting(library,"appearanceHistory"),bytes);
    }
};
QTEST_GUILESS_MAIN(SettingsAppearanceTests)
#include "settings-appearance-tests.moc"
