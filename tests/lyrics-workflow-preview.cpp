// Explicit screenshot/layout probe using real widgets and loopback AI only.
#include "integration-fixture.h"
#include "window.h"
#include <QTextStream>
using namespace relaxer;
using namespace integration_fixture;
int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");if(app.arguments().size()!=2)return 64;
    const auto output=app.arguments()[1];QDir().mkpath(output);QTemporaryDir dir;
    Library lib(dir.path());if(!lib.open())return 65;
    const auto ids=seed(lib);if(ids.size()!=3)return 66;
    const QByteArray original="[00:01.00]Hello my friend\n[00:02.00]This is a synthetic song";
    for(auto id:ids)if(!lib.saveLyrics(id,original))return 67;
    TranslationHttpMock paid;if(!paid.listen())return 68;
    paid.enqueue(completion(QString::fromUtf8(translationJson({{0,QStringLiteral("朋友你好")},{1,QStringLiteral("这是一首合成歌曲")}}))));
    AiRuntime runtime(&lib,DeepSeekProvider::MockEndpoint{paid.baseUrl()},std::make_unique<FakeCredentialStore>());
    if(!configureRuntime(runtime))return 69;
    Window parent(&lib);
    LyricsLookup d(&lib,ids,&parent,QUrl("http://127.0.0.1:9/api"),{},true,&runtime);d.resize(1240,780);d.show();
    auto *filter=d.findChild<QCheckBox *>("lyricsMissingTranslationOnly");filter->setChecked(true);select(d,ids[0]);
    QStringList screenshots,layoutErrors;
    auto capture=[&](QString name){
        QTest::qWait(80);
        for(auto *button:d.findChildren<QAbstractButton *>())if(button->isVisible()){
            const QRect rect(button->mapTo(&d,QPoint()),button->size());
            if(!d.rect().contains(rect))layoutErrors<<name+":"+button->objectName();
        }
        if(!d.grab().save(QDir(output).filePath(name+".png")))layoutErrors<<"save:"+name;
        screenshots<<name;
    };
    capture("01-unknown");d.resize(860,560);capture("02-unknown-compact");d.resize(1240,780);
    d.findChild<QPushButton *>("lyricsNeedsTranslation")->click();capture("03-needs-translation");
    consent(d);auto *save=d.findChild<QPushButton *>("lyricsSaveTranslation");QElapsedTimer timer;timer.start();
    while(!save->isEnabled()&&timer.elapsed()<5000)QTest::qWait(10);
    if(!save->isEnabled())return 70;
    capture("04-unsaved-preview");save->click();filter->setChecked(false);select(d,ids[0]);capture("05-saved-translation");
    select(d,ids[1]);d.findChild<QPushButton *>("lyricsNoTranslationNeeded")->click();capture("06-no-translation-needed");
    filter->setChecked(true);select(d,ids[2]);d.findChild<QPushButton *>("lyricsFindOrReplace")->click();capture("07-original-search-entry");
    d.resize(860,560);capture("08-search-compact");d.close();
    const QJsonObject result{{"screenshots",QJsonArray::fromStringList(screenshots)},
        {"layoutErrors",QJsonArray::fromStringList(layoutErrors)},{"mockPaidRequests",paid.requestCount()},{"realPaidRequests",0}};
    QTextStream(stdout)<<QJsonDocument(result).toJson(QJsonDocument::Indented);
    return layoutErrors.isEmpty()&&paid.requestCount()==1?0:1;
}
