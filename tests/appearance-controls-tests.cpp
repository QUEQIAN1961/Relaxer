#include "c4c5-ui-fixture.h"
#include "ui-bridge/ui-images.h"
#include <QApplication>
#include <QColorDialog>
#include <QUrlQuery>
using namespace c4c5_fixture;
namespace {
QRectF rect(QQuickItem *item) { return item->mapRectToScene(QRectF(QPointF(),item->size())); }
bool click(QQuickWindow *w,QQuickItem *item) {
    QTest::qWait(50);
    if(!item || !item->isVisible() || !QRectF(QPointF(),w->size()).contains(rect(item).center()))return false;
    QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,rect(item).center().toPoint());return true;
}
bool prepare(Fixture &f) {
    if(!f.ok)return false;
    f.shell->playbackBridge()->pause();auto *w=f.shell->window();w->showNormal();w->resize(1240,790);
    return QTest::qWaitForWindowExposed(w);
}
void appearance(Fixture &f,int mode) {
    f.shell->appBridge()->navigate(3);f.shell->window()->setProperty("settingsSection","appearance");
    f.shell->settingsBridge()->beginAppearance();f.shell->settingsBridge()->setAppearanceMode(mode);
}
void shot(QQuickWindow *w,const QString &name) {
    const auto folder=qEnvironmentVariable("RELAXER_APPEARANCE_EVIDENCE");
    if(!folder.isEmpty()){QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath(name+".png"));}
}
QString pattern(Fixture &f) {
    QImage image(640,640,QImage::Format_RGB32);
    for(int y=0;y<640;++y)for(int x=0;x<640;++x)image.setPixelColor(x,y,((x/12+y/12)%2)?QColor("#fbc735"):QColor("#214e98"));
    const auto path=f.dir.filePath("pattern.png");return image.save(path)?path:QString();
}
double detail(const QImage &image) {
    double sum=0;for(int y=0;y<image.height();++y)for(int x=1;x<image.width();++x)sum+=qAbs(qGray(image.pixel(x,y))-qGray(image.pixel(x-1,y)));
    return sum/(image.width()*image.height());
}
}
class AppearanceControlsTests final:public QObject {
    Q_OBJECT
private slots:
    void inlineColorsAndImageOptions() {
        Fixture f(2);QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *s=f.shell->settingsBridge();
        for(int mode:{3,4}) {
            appearance(f,mode);
            for(const char *key:{"customText","customProgressColors","customFooter"})QVERIFY(s->editAppearance(key,false));
            QVERIFY(s->editAppearance("rightInk","#c52649"));
            for(const QSize size:{QSize(1240,790),QSize(1000,660)}) {
                w->resize(size);QTest::qWait(100);
                for(const auto pair:{qMakePair("appearanceCustomText","appearanceColor-rightInk"),qMakePair("appearanceCustomProgressColors","appearanceColor-progressFill"),qMakePair("appearanceCustomFooter","appearanceColor-footer")}) {
                    auto *check=visual(root,pair.first),*swatch=visual(root,pair.second);QVERIFY(check && swatch);
                    QVERIFY(swatch->isVisible());QVERIFY(!swatch->isEnabled());QVERIFY(qAbs(rect(check).center().y()-rect(swatch).center().y())<1);
                    QVERIFY(rect(swatch).left()>rect(check).right());
                    QVERIFY(click(w,swatch));for(auto *widget:QApplication::topLevelWidgets())QVERIFY(!qobject_cast<QColorDialog *>(widget) || !widget->isVisible());
                    QVERIFY(click(w,check));QTRY_VERIFY(swatch->isEnabled());QVERIFY(click(w,check));QTRY_VERIFY(!swatch->isEnabled());
                }
                auto *share=visual(root,"appearanceShareLR"),*footer=visual(root,"appearanceShareFooter"),*bold=visual(root,"appearanceMutedBold");QVERIFY(share && footer && bold);
                if(mode==4){QVERIFY(share->isVisible());QCOMPARE(rect(share).top(),rect(footer).top());QCOMPARE(rect(share).top(),rect(bold).top());QVERIFY(rect(share).right()<rect(footer).left());QVERIFY(rect(footer).right()<rect(bold).left());QVERIFY(rect(bold).right()<w->width()-20);}
                shot(w,QString("controls-%1-%2").arg(mode).arg(size.width()));
            }
            QCOMPARE(s->appearanceDraft()["rightInk"].toString(),QString("#c52649"));
        }
        QVERIFY(f.shell->warnings().isEmpty());
    }
    void independentFooterAndExpandedBlur() {
        Fixture f(2);QVERIFY(prepare(f));appearance(f,4);auto *s=f.shell->settingsBridge();auto *w=f.shell->window();auto *root=w->contentItem();
        const auto image=pattern(f);QVERIFY(!image.isEmpty());
        QVERIFY(s->editAppearance("sourceLeft",image));QVERIFY(s->editAppearance("shareLR",true));QVERIFY(s->editAppearance("shareFooter",true));
        QVERIFY(s->editAppearance("blurRight",17.2));QVERIFY(s->saveAppearance());
        auto history=f.library.setting("appearanceHistory").toObject();auto items=history["items"].toArray();auto old=items[0].toObject();old.remove("blurFooter");items[0]=old;history["items"]=items;QVERIFY(f.library.setSetting("appearanceHistory",history));
        s->beginAppearance();QCOMPARE(s->appearanceDraft()["blurFooter"].toDouble(),17.2);QCOMPARE(s->palette()["blurFooter"].toDouble(),17.2);
        QVERIFY(s->editAppearance("blurFooter",6.4));QVERIFY(s->editAppearance("blurRight",41.));
        QTRY_VERIFY(visual(root,"footerBackground")->property("source").toUrl().query().contains("blur=6.4"));
        QTRY_VERIFY(visual(root,"pageBackground")->property("source").toUrl().query().contains("blur=41"));
        auto *scroll=visual(root,"appearanceScroll");auto *flick=scroll->property("contentItem").value<QObject *>();QVERIFY(flick);
        flick->setProperty("contentY",flick->property("contentHeight").toReal()-flick->property("height").toReal());QTest::qWait(100);
        auto *left=visual(root,"appearanceBlur-blurLeft"),*right=visual(root,"appearanceBlur-blurRight"),*footer=visual(root,"appearanceBlur-blurFooter"),*expanded=visual(root,"appearanceBlur-blurExpanded");QVERIFY(left && right && footer && expanded);
        QCOMPARE(rect(left).top(),rect(right).top());QCOMPARE(rect(footer).top(),rect(expanded).top());QVERIFY(rect(footer).top()>rect(left).bottom());QCOMPARE(rect(left).left(),rect(footer).left());QCOMPARE(rect(right).left(),rect(expanded).left());
        auto *input=qobject_cast<QQuickItem *>(footer->property("contentItem").value<QObject *>());QVERIFY(click(w,input));QTest::keyClick(w,Qt::Key_A,Qt::ControlModifier);
        for(char c:QByteArray("7.3"))QTest::keyClick(w,Qt::Key(c));QTest::keyClick(w,Qt::Key_Return);QTRY_COMPARE(s->appearanceDraft()["blurFooter"].toDouble(),7.3);shot(w,"blur-grid");
        QVERIFY(s->editAppearance("blurExpanded",0.));auto *backdrop=visual(root,"playerBackdrop");QVERIFY(backdrop);
        QTRY_VERIFY(backdrop->property("source").toUrl().query().contains("blur=0"));
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTest::qWait(600);QTRY_COMPARE(backdrop->property("status").toInt(),1);shot(w,"expanded-blur-0");
        QVERIFY(s->editAppearance("blurExpanded",35.));QTRY_VERIFY(backdrop->property("source").toUrl().query().contains("blur=35"));QTRY_COMPARE(backdrop->property("status").toInt(),1);QTest::qWait(250);shot(w,"expanded-blur-35");
        QVERIFY(s->setPreference("playerCoverBackground",false));QTRY_VERIFY(backdrop->property("source").toUrl().path()=="/background");QVERIFY(backdrop->property("source").toUrl().query().contains("blur=35"));
        QVERIFY(s->saveAppearance(true));SettingsBridge reopened(&f.library);reopened.beginAppearance();QCOMPARE(reopened.appearanceDraft()["blurFooter"].toDouble(),7.3);QCOMPARE(reopened.appearanceDraft()["blurRight"].toDouble(),41.);QCOMPARE(reopened.appearanceDraft()["blurExpanded"].toDouble(),35.);
        QVERIFY(!s->editAppearance("blurFooter",50.1));QVERIFY(!s->editAppearance("blurFooter",-0.1));
    }
    void actualGaussianStrength() {
        Fixture f(2);QVERIFY(f.ok);const auto file=pattern(f);QVERIFY(!file.isEmpty());UiImages provider;QSize size;
        const auto base="blur?file="+QString::fromUtf8(QUrl::toPercentEncoding(QUrl::fromLocalFile(file).toString()));
        const auto sharp=provider.requestImage(base+"&blur=0",&size,QSize(640,640)),soft=provider.requestImage(base+"&blur=35",&size,QSize(640,640));
        QVERIFY(!sharp.isNull() && !soft.isNull());QVERIFY(sharp!=soft);QVERIFY(detail(soft)<detail(sharp)*.3);
        QCOMPARE(provider.requestImage(base,&size,QSize(640,640)),provider.requestImage(base+"&blur=50",&size,QSize(640,640)));
    }
    void toolbarAndWindowControls() {
        Fixture f(2);QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();auto *s=f.shell->settingsBridge();appearance(f,4);
        for(const QSize size:{QSize(1240,790),QSize(1000,660)}) {
            w->resize(size);QTest::qWait(100);auto *title=visual(root,"titleBar"),*reference=visual(root,"importMusicButton");QVERIFY(title && reference);
            for(const char *name:{"appearanceNewSave","appearanceOverwriteSave","appearanceReset"}){auto *button=visual(root,name);QVERIFY(button);QCOMPARE(button->height(),reference->height());QVERIFY(rect(button).top()>rect(title).bottom()+8);}
            for(int i=0;i<3;++i){auto *button=visual(root,"windowControl"+QString::number(i));QVERIFY(button);QCOMPARE(button->size(),QSizeF(42,32));}
            shot(w,QString("window-controls-%1").arg(size.width()));
        }
        QVERIFY(click(w,visual(root,"windowControl1")));QTRY_COMPARE(w->visibility(),QWindow::Maximized);QVERIFY(click(w,visual(root,"windowControl1")));QTRY_COMPARE(w->visibility(),QWindow::Windowed);
        QVERIFY(click(w,visual(root,"windowControl0")));QTRY_COMPARE(w->visibility(),QWindow::Minimized);w->showNormal();QVERIFY(QTest::qWaitForWindowExposed(w));
        QVERIFY(click(w,visual(root,"windowControl2")));auto *dialog=w->findChild<QObject *>("appearanceUnsavedDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("opened").toBool());QVERIFY(click(w,visual(root,"appearanceLeaveCancel")));QTRY_VERIFY(!dialog->property("opened").toBool());QVERIFY(w->isVisible());
        s->discardAppearance();w->setProperty("settingsSection","settings");QVERIFY(click(w,visual(root,"windowControl2")));QTRY_VERIFY(!w->isVisible());QVERIFY(f.shell->shutdown());
    }
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);AppearanceControlsTests tests;return QTest::qExec(&tests,argc,argv);}
#include "appearance-controls-tests.moc"
