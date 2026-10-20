#include "c4c5-ui-fixture.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQmlContext>
#include <QFile>
#include <cmath>
using namespace c4c5_fixture;
namespace {
QRectF bounds(QQuickItem *item){return item->mapRectToScene(QRectF(QPointF(),item->size()));}
bool click(QQuickWindow *w,QQuickItem *item){if(!item||!item->isVisible()||!item->isEnabled())return false;QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,bounds(item).center().toPoint());return true;}
bool prepare(Fixture &f){
    if(!f.ok)return false;auto *w=f.shell->window();w->showNormal();w->setGeometry(45,50,1240,790);w->requestActivate();
    if(!QTest::qWaitForWindowExposed(w))return false;QTest::qWait(160);f.shell->playbackBridge()->pause();return true;
}
void shot(QQuickWindow *w,const QString &name){
    const auto folder=qEnvironmentVariable("RELAXER_UI2_SCREENSHOTS");if(folder.isEmpty())return;
    QDir().mkpath(folder);w->grabWindow().save(QDir(folder).filePath(name+".png"));
}
QList<QObject *> hints(QQuickWindow *w){return w->findChildren<QObject *>("meaningfulHint");}
bool anyHint(QQuickWindow *w){for(auto *h:hints(w))if(h->property("visible").toBool())return true;return false;}
}
class QmlUi2RefinementTests final:public QObject{
    Q_OBJECT
private slots:
    void topActionsKeepOneHeightInTallAndCompactLayouts(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        for(const QSize size:{QSize(1240,790),QSize(1000,660)}){
            w->resize(size);QTest::qWait(80);
            for(const char *name:{"importMusicButton","playAllButton","findMissingLyrics"}){
                auto *button=visual(root,name);QVERIFY(button);QCOMPARE(button->height(),36.);
                auto *background=qvariant_cast<QQuickItem *>(button->property("background"));QVERIFY(background);
                QVERIFY(background->property("border").isValid()||background->property("color").isValid());
                QVERIFY(bounds(button).right()<=w->width()+1);
            }
            auto *search=visual(root,"librarySearch");QVERIFY(search);QCOMPARE(search->height(),36.);
            auto *import=visual(root,"importMusicButton");QCOMPARE(bounds(import).center().y(),bounds(search).center().y());
            auto *stack=visual(root,"pageStack");QVERIFY(stack);QCOMPARE(bounds(stack).top(),22.);
            shot(w,size.width()==1240?"01-library":"02-compact-library");
            QVERIFY(click(w,import));
            auto *menu=w->findChild<QObject *>("importDialog");QVERIFY(menu);
            QTRY_VERIFY(menu->property("opened").toBool());
            auto *surface=qvariant_cast<QQuickItem *>(menu->property("contentItem"));QVERIFY(surface);
            const auto menuTop=bounds(surface).top()-menu->property("topPadding").toReal();
            QVERIFY(qAbs(menuTop-bounds(import).bottom()-6)<1);
            QVERIFY(bounds(surface).right()<=bounds(import).right()+1);
            shot(w,size.width()==1240?"07-import-menu-normal":"08-import-menu-compact");
            QVERIFY(QMetaObject::invokeMethod(menu,"close"));
            QTRY_VERIFY(!menu->property("visible").toBool());
        }
        QVERIFY(click(w,visual(root,"navigation1")));QTRY_COMPARE(f.shell->appBridge()->page(),1);QTest::qWait(260);
        auto *create=visual(root,"createPlaylistButton");QVERIFY(create);QCOMPARE(create->height(),36.);shot(w,"03-playlists-header");
        QVERIFY(click(w,visual(root,"navigation2")));QTRY_COMPARE(f.shell->appBridge()->page(),2);QTest::qWait(260);
        auto *edit=visual(root,"profileEdit");QVERIFY(edit);QCOMPARE(edit->height(),36.);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void bodyTypeIsReadableAndVisibleListActionsHaveBorders(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        auto *tokens=w->property("designTokens").value<QObject *>();QVERIFY(tokens);QCOMPARE(tokens->property("fontSize").toInt(),15);
        auto *play=visual(root,"playAllButton");auto *search=visual(root,"librarySearch");QVERIFY(play);QVERIFY(search);
        QVERIFY(play->property("font").value<QFont>().pixelSize()>=15);QVERIFY(play->property("font").value<QFont>().weight()>=QFont::Medium);
        QVERIFY(search->property("font").value<QFont>().pixelSize()>=15);
        auto *page=visual(root,"localPage");QVERIFY(page);
        int checked=0;
        std::function<void(QQuickItem *)> visit=[&](QQuickItem *item){
            if(item->isVisible()&&QByteArray(item->metaObject()->className()).startsWith("ShellButton")){
                auto *bg=qvariant_cast<QQuickItem *>(item->property("background"));QVERIFY(bg);
                auto *border=qvariant_cast<QObject *>(bg->property("border"));QVERIFY(border);QCOMPARE(border->property("width").toReal(),1.);
                QVERIFY(border->property("color").value<QColor>().alpha()>0);++checked;
            }
            for(auto *child:item->childItems())visit(child);
        };visit(page);QVERIFY(checked>=4);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void obviousActionsHaveNoHoverPopupAndModeRetainsItsMeaningfulHint(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        auto *import=visual(root,"importMusicButton");QVERIFY(import);QTest::mouseMove(w,bounds(import).center().toPoint());QTest::qWait(950);QVERIFY(!anyHint(w));
        auto *mode=visual(root,"playMode");QVERIFY(mode);QTest::mouseMove(w,bounds(mode).center().toPoint());QTRY_VERIFY_WITH_TIMEOUT(anyHint(w),1700);shot(w,"04-meaningful-mode-hint");
        QTest::mouseMove(w,bounds(import).center().toPoint());QTRY_VERIFY(!anyHint(w));
        QVERIFY(click(w,visual(root,"navigation3")));QTRY_COMPARE(f.shell->appBridge()->page(),3);QTest::qWait(260);
        QVERIFY(click(w,visual(root,"openAiServiceSettings")));QTRY_COMPARE(w->property("settingsSection").toString(),QString("ai"));QTest::qWait(260);
        auto *model=visual(root,"aiDefaultModel");QVERIFY(model);model->forceActiveFocus();QTest::mouseMove(w,bounds(model).center().toPoint());QTest::qWait(950);QVERIFY(!anyHint(w));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void choicesAndContextMenusUseRoundedInWindowSurfaces(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *root=w->contentItem();
        auto *page=visual(root,"localPage");QVERIFY(page);
        QQuickItem *choice=nullptr;std::function<void(QQuickItem *)> visit=[&](QQuickItem *item){if(!choice&&QByteArray(item->metaObject()->className()).startsWith("ChoiceBox"))choice=item;for(auto *child:item->childItems())visit(child);};visit(page);QVERIFY(choice);
        QVERIFY(click(w,choice));auto *popup=qvariant_cast<QObject *>(choice->property("popup"));QVERIFY(popup);QTRY_VERIFY(popup->property("visible").toBool());
        QCOMPARE(popup->property("popupType").toInt(),0);auto *bg=qvariant_cast<QQuickItem *>(popup->property("background"));QVERIFY(bg);QVERIFY(bg->property("radius").toReal()>=8);shot(w,"05-rounded-options");
        auto *list=qvariant_cast<QQuickItem *>(popup->property("contentItem"));QVERIFY(list);
        bool sawSelected=false;
        auto *tokens=w->property("designTokens").value<QObject *>();QVERIFY(tokens);
        std::function<void(QQuickItem *)> inspect=[&](QQuickItem *item){
            if(item->property("highlighted").toBool()){
                auto *text=qvariant_cast<QQuickItem *>(item->property("contentItem"));
                if(text){QCOMPARE(text->property("color").value<QColor>(),tokens->property("text").value<QColor>());sawSelected=true;}
            }
            for(auto *child:item->childItems())inspect(child);
        };inspect(list);QVERIFY(sawSelected);
        QVERIFY(QMetaObject::invokeMethod(popup,"close"));
        auto *table=visual(root,"libraryListContainer");QVERIFY(table);const auto point=table->mapToScene(QPointF(180,80)).toPoint();
        QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,point);QTest::qWait(120);
        bool found=false;for(auto *object:w->findChildren<QObject *>()){
            if(QByteArray(object->metaObject()->className()).startsWith("RoundMenu_")&&object->property("visible").toBool()){
                auto *background=qvariant_cast<QQuickItem *>(object->property("background"));QVERIFY(background);QVERIFY(background->property("radius").toReal()>=8);QCOMPARE(object->property("popupType").toInt(),0);found=true;
            }
        }QVERIFY(found);shot(w,"06-rounded-song-menu");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void backdropUsesTheSameTimingInBothDirectionsAndReversesWithoutAJump(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *clip=visual(w->contentItem(),"playerBackdropClip");auto *host=visual(w->contentItem(),"contentHost");QVERIFY(clip);QVERIFY(host);
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTest::qWait(110);const auto opening=clip->height()/host->height();
        QTRY_VERIFY(w->property("playerProgress").toReal()>.999);
        QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QTest::qWait(110);const auto closing=clip->height()/host->height();
        QVERIFY(opening>.2&&opening<.95);QVERIFY(closing>.05&&closing<.8);QVERIFY2(qAbs(opening+closing-1)<.18,"Background open/close use different easing");
        QTRY_VERIFY(w->property("playerProgress").toReal()<.001);
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTest::qWait(100);const auto before=clip->height();
        QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics"));QVERIFY(qAbs(clip->height()-before)<2);
        QTRY_VERIFY(w->property("playerProgress").toReal()<.001);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void upcomingLeavesBelowTheRightEdgeAndRisesVerticallyAtTheLeftEdge(){
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();auto *panel=visual(w->contentItem(),"nextQueuePanel");QVERIFY(panel);
        auto *clip=panel->parentItem();QVERIFY(clip);QVERIFY(clip->clip());
        QJsonArray frames;
        for(bool open:{true,false}){
            QVERIFY(QMetaObject::invokeMethod(w,open?"showLyrics":"closeLyrics"));
            bool departed=false,arrived=false,switched=false;
            QElapsedTimer timer;timer.start();
            while(timer.elapsed()<560){
                const auto rect=bounds(panel);const bool left=w->property("upcomingAtLeft").toBool();const qreal expected=left?8:w->width()-8-panel->width();
                QVERIFY(qAbs(rect.x()-expected)<.5);
                if(left!=open&&rect.top()>w->height()-panel->height()/2.)departed=true;
                if(left==open){switched=true;if(rect.top()<w->height()-12)arrived=true;}
                frames.append(QJsonObject{{"open",open},{"ms",double(timer.elapsed())},{"x",rect.x()},{"y",rect.y()},{"left",left}});
                if(timer.elapsed()<445&&timer.elapsed()%70<10)shot(w,QString("motion-%1-%2").arg(open?"open":"close").arg(timer.elapsed(),3,10,QChar('0')));
                QTest::qWait(7);
            }
            QVERIFY(departed);QVERIFY(switched);QVERIFY(arrived);
            QCOMPARE(w->property("upcomingAtLeft").toBool(),open);QVERIFY(qAbs(bounds(panel).bottom()-(w->height()-4))<1);
        }
        const auto folder=qEnvironmentVariable("RELAXER_UI2_SCREENSHOTS");
        if(!folder.isEmpty()){QFile out(QDir(folder).filePath("upcoming-frames.json"));QVERIFY(out.open(QIODevice::WriteOnly));out.write(QJsonDocument(frames).toJson());}
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);QmlUi2RefinementTests tests;return QTest::qExec(&tests,argc,argv);}
#include "qml-ui2-refinement-tests.moc"

