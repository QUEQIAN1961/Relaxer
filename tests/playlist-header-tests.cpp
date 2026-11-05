#include "c4c5-ui-fixture.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QWheelEvent>
#include <cmath>
#include <functional>
using namespace c4c5_fixture;
namespace {
QRectF bounds(QQuickItem *item) { return item->mapRectToScene(QRectF(QPointF(), item->size())); }
bool click(QQuickWindow *window, QQuickItem *item) {
    if (!item || !item->isVisible() || !item->isEnabled()) return false;
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, bounds(item).center().toPoint());
    return true;
}
bool prepare(Fixture &f, QSize size = QSize(1240, 790)) {
    if (!f.ok) return false;
    auto *window = f.shell->window();
    window->showNormal(); window->setGeometry(QRect(QPoint(35, 40), size)); window->requestActivate();
    if (!QTest::qWaitForWindowExposed(window)) return false;
    QTest::qWait(100); f.shell->playbackBridge()->pause(); return true;
}
QQuickItem *button(QObject *dialog, int role) {
    auto *footer = qvariant_cast<QQuickItem *>(dialog->property("footer"));
    return footer ? visual(footer, "confirmationButton" + QString::number(role)) : nullptr;
}
QQuickItem *textButton(QQuickItem *root, const QString &text) {
    if (root->isVisible() && root->isEnabled() && root->property("text").toString() == text) return root;
    for (auto *child : root->childItems()) if (auto *found = textButton(child, text)) return found;
    return nullptr;
}
void wheel(QQuickWindow *window, QQuickItem *over, int delta) {
    // Send logical window coordinates directly, independent of Windows DPI.
    // Flickable also needs increasing timestamps to handle wheel velocity.
    static QElapsedTimer clock;
    if(!clock.isValid())clock.start();
    const QPointF point=bounds(over).center();
    QWheelEvent event(point,window->mapToGlobal(point.toPoint()),{},QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    event.setTimestamp(clock.elapsed()+1);
    QCoreApplication::sendEvent(window,&event);
}
void shot(QQuickWindow *window, const QString &name) {
    const auto directory = qEnvironmentVariable("RELAXER_PLAYLIST_SCREENSHOTS");
    if (directory.isEmpty()) return;
    QDir().mkpath(directory); window->grabWindow().save(QDir(directory).filePath(name + ".png"));
}
}

class PlaylistHeaderTests final : public QObject {
    Q_OBJECT
private slots:
    void onePlaylistPerConfirmation_data() {
        QTest::addColumn<bool>("sidebar"); QTest::addColumn<bool>("keyboard");
        QTest::newRow("sidebar-mouse") << true << false;
        QTest::newRow("grid-mouse") << false << false;
        QTest::newRow("sidebar-enter") << true << true;
        QTest::newRow("grid-enter") << false << true;
    }
    void onePlaylistPerConfirmation() {
        QFETCH(bool, sidebar); QFETCH(bool, keyboard);
        Fixture f; QVERIFY2(prepare(f), qPrintable(f.shell->warnings().join('\n')));
        auto *w = f.shell->window(); auto *root = w->contentItem();
        if (!sidebar) {
            QVERIFY(click(w, visual(root, "navigation1")));
            QTRY_COMPARE(f.shell->appBridge()->page(), 1); QTest::qWait(260);
        }
        const auto before = f.library.playlists().size();
        QVERIFY(click(w, visual(root, sidebar ? "sidebarCreatePlaylist" : "createPlaylistButton")));
        auto *editor = w->findChild<QObject *>("playlistEditor"); QVERIFY(editor);
        QSignalSpy accepted(editor, SIGNAL(accepted()));
        QTRY_VERIFY(editor->property("opened").toBool());
        auto *input = visual(root, "playlistNameInput"); QVERIFY(input);
        QVERIFY(input->setProperty("text", "一次创建一份"));
        if (keyboard) { input->forceActiveFocus(); QTest::keyClick(w, Qt::Key_Return); }
        else QVERIFY(click(w, button(editor, 0)));
        QTRY_COMPARE(f.library.playlists().size(), before + 1);
        QCOMPARE(accepted.count(), 1);
        QTest::qWait(100); QCOMPARE(f.library.playlists().size(), before + 1);
        QCOMPARE(f.library.playlists().last().name, QString("一次创建一份"));
        QVERIFY2(f.shell->warnings().isEmpty(), qPrintable(f.shell->warnings().join('\n')));
    }
    void cancelRenameAndIntentionalSameNames() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        QVERIFY(click(w,visual(root,"sidebarCreatePlaylist")));
        auto *editor=w->findChild<QObject *>("playlistEditor"); QVERIFY(editor);
        QSignalSpy rejected(editor,SIGNAL(rejected()));QTRY_VERIFY(editor->property("opened").toBool());
        QVERIFY(click(w,button(editor,1))); QTRY_VERIFY(!editor->property("visible").toBool());
        QCOMPARE(rejected.count(),1); QCOMPARE(f.library.playlists().size(),0);
        for(int i=0;i<2;++i) {
            QVERIFY(click(w,visual(root,"sidebarCreatePlaylist"))); QTRY_VERIFY(editor->property("opened").toBool());
            auto *input=visual(root,"playlistNameInput"); QVERIFY(input); input->setProperty("text","允许主动同名");
            QVERIFY(click(w,button(editor,0))); QTRY_COMPARE(f.library.playlists().size(),i+1);
            QTest::qWait(270);
        }
        const auto id=QString::number(f.library.playlists().first().id);
        QVERIFY(QMetaObject::invokeMethod(editor,"edit",Q_ARG(QVariant,id),Q_ARG(QVariant,QString("允许主动同名"))));
        QTRY_VERIFY(editor->property("opened").toBool());
        visual(root,"playlistNameInput")->setProperty("text","修改后的名称");
        QSignalSpy accepted(editor,SIGNAL(accepted())); QVERIFY(click(w,button(editor,0)));
        QTRY_VERIFY(!editor->property("visible").toBool()); QCOMPARE(accepted.count(),1);
        QCOMPARE(f.library.playlists().size(),2);
        bool renamed=false;for(const auto &p:f.library.playlists())if(QString::number(p.id)==id)renamed=p.name=="修改后的名称";
        QVERIFY(renamed); QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void createAndAddFromBothSelectionEntrypoints_data() {
        QTest::addColumn<bool>("bulk"); QTest::newRow("bulk-toolbar")<<true; QTest::newRow("track-context-menu")<<false;
    }
    void createAndAddFromBothSelectionEntrypoints() {
        QFETCH(bool,bulk); Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        if(bulk) {
            auto *page=visual(root,"localPage"); QVERIFY(page);
            const QVariant keys=QStringList{QString::number(f.a.id),QString::number(f.b.id)};
            QVERIFY(QMetaObject::invokeMethod(page,"setBulkKeys",Q_ARG(QVariant,keys)));
            QTRY_VERIFY(w->property("bulkActive").toBool()); QVERIFY(click(w,visual(root,"bulkNewPlaylist")));
        } else {
            auto *table=visual(root,"libraryListContainer"); QVERIFY(table);
            QTest::mouseClick(w,Qt::RightButton,Qt::NoModifier,table->mapToScene(QPointF(180,65)).toPoint());
            auto *add=textButton(root,"加入歌单");QVERIFY(add);
            // Menu items exist at their final positions before the height reveal
            // reaches them. Wait for the parent menu, not a fixed frame delay.
            auto *menu=qvariant_cast<QObject *>(add->property("menu"));QVERIFY(menu);
            QTRY_VERIFY(menu->property("opened").toBool());
            QSignalSpy activated(add,SIGNAL(triggered()));QVERIFY(activated.isValid());
            QVERIFY(click(w,add));
            QTRY_COMPARE(activated.count(),1);
            auto *submenu=qvariant_cast<QObject *>(add->property("subMenu"));QVERIFY(submenu);
            QTRY_VERIFY(submenu->property("opened").toBool());
            QTRY_VERIFY(textButton(root,"创建新歌单并加入…"));
            auto *create=textButton(root,"创建新歌单并加入…");QSignalSpy triggered(create,SIGNAL(triggered()));
            QVERIFY(triggered.isValid());
            // Like U06, synthetic input can return without emitting the action.
            // Retry only while the submenu is open and no trigger was delivered;
            // once delivered, never submit the create action twice.
            for(int attempt=0;attempt<20&&triggered.isEmpty()&&submenu->property("visible").toBool();++attempt){
                QVERIFY(click(w,create));
                if(triggered.isEmpty())QTest::qWait(50);
                qInfo()<<"playlist submenu input attempt"<<attempt+1<<"triggered"<<triggered.count()<<"open"<<submenu->property("visible").toBool();
            }
            QCOMPARE(triggered.count(),1);
        }
        auto *dialog=w->findChild<QObject *>(bulk?"bulkNewPlaylistDialog":"trackNewPlaylistDialog"); QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *input=visual(root,bulk?"bulkNewPlaylistName":"trackNewPlaylistName"); QVERIFY(input); input->setProperty("text","新建并加入");
        QSignalSpy accepted(dialog,SIGNAL(accepted())); QVERIFY(click(w,button(dialog,0)));
        QTRY_COMPARE(f.library.playlists().size(),1); QCOMPARE(accepted.count(),1);
        QCOMPARE(f.library.tracks(f.library.playlists().first().id).size(),bulk?2:1);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void headerMatchesExpandedAndCompactReference_data() {
        QTest::addColumn<QSize>("size"); QTest::addColumn<int>("tracks");
        QTest::newRow("wide-empty")<<QSize(1240,790)<<0;
        QTest::newRow("wide-six")<<QSize(1240,790)<<6;
        QTest::newRow("compact-six")<<QSize(1000,660)<<6;
        QTest::newRow("wide-scrollable")<<QSize(1240,790)<<50;
    }
    void headerMatchesExpandedAndCompactReference() {
        QFETCH(QSize,size); QFETCH(int,tracks); Fixture f; QVERIFY(prepare(f,size));
        QVERIFY(f.shell->settingsBridge()->setTheme(0));
        const auto id=f.library.createPlaylist("主歌单"); QVERIFY(id);
        QVector<qint64> ids;
        for(int i=0;i<tracks;++i)ids.append(seed(f.library,QString("歌曲 %1").arg(i),QString("header-%1").arg(i)).id);
        if(!ids.isEmpty())QVERIFY(f.library.addToPlaylist(id,ids));
        QVERIFY(f.shell->libraryBridge()->openPlaylist(QString::number(id))); QTest::qWait(100);
        auto *w=f.shell->window();auto *root=w->contentItem();
        auto *header=visual(root,"playlistHeader"),*cover=visual(root,"playlistCover"),*play=visual(root,"playlistPlayActions"),*edit=visual(root,"playlistEditActions");
        auto *stats=visual(root,"playlistStatisticsRow"),*description=visual(root,"playlistDescriptionRow"),*table=visual(root,"libraryListContainer");
        QVERIFY(header);QVERIFY(cover);QVERIFY(play);QVERIFY(edit);QVERIFY(stats);QVERIFY(description);QVERIFY(table);
        QTRY_COMPARE(cover->size(),QSizeF(160,160));QTRY_COMPARE(header->height(),160.);
        QCOMPARE(bounds(play).left(),bounds(cover).left()+180);QCOMPARE(bounds(play).left(),bounds(edit).left());
        QCOMPARE(bounds(stats).left(),bounds(play).left());QCOMPARE(bounds(description).left(),bounds(play).left());
        QVERIFY(bounds(edit).top()>bounds(play).bottom());QVERIFY(bounds(stats).top()>bounds(edit).bottom());
        QVERIFY(bounds(description).top()>bounds(stats).bottom());QVERIFY(!visual(root,"librarySummary")->isVisible());
        QCOMPARE(table->property("statisticsMode").toBool(),true);
        const auto tag=QString::fromLatin1(QTest::currentDataTag());shot(w,tag+"-expanded");
        wheel(w,cover,-120);
        QTest::qWait(65);const auto middle=cover->width();QVERIFY(middle>78&&middle<160);
        QVERIFY(bounds(stats).left()>bounds(play).left());shot(w,tag+"-moving");
        QTRY_COMPARE(cover->width(),78.);QCOMPARE(cover->height(),cover->width());
        QTRY_COMPARE(header->height(),78.);QCOMPARE(bounds(edit).bottom()-bounds(play).top(),cover->height());
        QVERIFY(bounds(stats).left()>bounds(play).right());QCOMPARE(bounds(stats).right(),bounds(header).right());
        QCOMPARE(bounds(description).right(),bounds(header).right());QCOMPARE(bounds(stats).top(),bounds(play).top());
        QCOMPARE(bounds(description).top(),bounds(edit).top());shot(w,tag+"-collapsed");
        QTest::qWait(120);QCOMPARE(cover->width(),78.);
        wheel(w,cover,120);QTRY_COMPARE(cover->width(),160.);shot(w,tag+"-restored");
        wheel(w,cover,-120);QTest::qWait(65);const auto before=cover->width();
        wheel(w,cover,120);QVERIFY(qAbs(cover->width()-before)<1);
        QTRY_COMPARE(cover->width(),160.);
        if(tracks>20) {
            wheel(w,table,-120);QTRY_COMPARE(cover->width(),78.);
            wheel(w,table,-120);
            QTRY_VERIFY(table->property("scrollY").toReal()>0);
            for(int i=0;i<8 && table->property("scrollY").toReal()>0;++i){wheel(w,table,120);QTest::qWait(70);}
            auto *list=visual(table,"trackListView");QVERIFY(list);
            QTRY_VERIFY2(table->property("scrollY").toReal()<=0,qPrintable(QString("contentY=%1 originY=%2 atBeginning=%3").arg(table->property("scrollY").toReal()).arg(list->property("originY").toReal()).arg(list->property("atYBeginning").toBool())));
            wheel(w,table,120);QTRY_COMPARE(cover->width(),160.);
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void metadataSlidesOutAboveThenInFromTheRight() {
        Fixture f;QVERIFY(prepare(f));const auto id=f.library.createPlaylist("文字对齐检查");QVERIFY(f.shell->libraryBridge()->openPlaylist(QString::number(id)));QTest::qWait(350);
        auto *w=f.shell->window();auto *root=w->contentItem();auto *cover=visual(root,"playlistCover"),*header=visual(root,"playlistHeader");QVERIFY(cover);QVERIFY(header);
        QTRY_COMPARE(header->property("layoutProgress").toReal(),0.);QTRY_COMPARE(cover->width(),160.);
        auto *stats=visual(root,"playlistStatisticsContent"),*description=visual(root,"playlistDescriptionContent"),*actions=visual(root,"playlistPlayActions");QVERIFY(stats);QVERIFY(description);QVERIFY(actions);
        QCOMPARE(bounds(stats).left(),bounds(actions).left());QCOMPARE(bounds(description).left(),bounds(actions).left());
        auto *viewport=visual(root,"playlistMetadataViewport");QVERIFY(viewport);QVERIFY(viewport->clip());
        const auto expandedTop=bounds(viewport).top(),expandedLeft=bounds(stats).left();
        for(bool collapse:{true,false}) {
            bool sawVertical=false,sawHorizontal=false,previousDocked=header->property("metadataDocked").toBool();
            auto previousStats=bounds(stats),previousDescription=bounds(description);wheel(w,cover,collapse?-120:120);
            for(int frame=0;frame<26;++frame) {
                QTest::qWait(16);w->grabWindow();const auto nowStats=bounds(stats),nowDescription=bounds(description);
                const bool docked=header->property("metadataDocked").toBool();const auto progress=header->property("collapseProgress").toReal();
                if(!docked) {
                    QCOMPARE(bounds(viewport).top(),expandedTop);QCOMPARE(nowStats.left(),expandedLeft);QCOMPARE(nowDescription.left(),expandedLeft);
                    if(progress>.01&&progress<.49)sawVertical=true;
                    if(!previousDocked){QVERIFY(collapse?nowStats.top()<=previousStats.top()+.5:nowStats.top()>=previousStats.top()-.5);QVERIFY(collapse?nowDescription.top()<=previousDescription.top()+.5:nowDescription.top()>=previousDescription.top()-.5);}
                }else {
                    if(progress>.51&&progress<.99)sawHorizontal=true;
                    if(previousDocked){QVERIFY(collapse?nowStats.left()<=previousStats.left()+.5:nowStats.left()>=previousStats.left()-.5);QVERIFY(collapse?nowDescription.left()<=previousDescription.left()+.5:nowDescription.left()>=previousDescription.left()-.5);}
                    QVERIFY(nowStats.left()>=bounds(viewport).left()-.5);QVERIFY(nowDescription.left()>=bounds(viewport).left()-.5);
                }
                previousDocked=docked;previousStats=nowStats;previousDescription=nowDescription;
                if(frame==5||frame==10||frame==15)shot(w,QString("metadata-%1-%2").arg(collapse?"collapse":"expand").arg(frame));
            }
            QVERIFY(sawVertical);QVERIFY(sawHorizontal);
            if(collapse){QVERIFY(qAbs(bounds(stats).right()-bounds(header).right())<1);QVERIFY(qAbs(bounds(description).right()-bounds(header).right())<1);}
            else{QCOMPARE(bounds(stats).left(),bounds(actions).left());QCOMPARE(bounds(description).left(),bounds(actions).left());}
        }
        for(int delay:{80,240}) {
            wheel(w,cover,-120);QTest::qWait(delay);const auto before=bounds(stats);wheel(w,cover,120);QVERIFY(QLineF(before.topLeft(),bounds(stats).topLeft()).length()<1);
            QTRY_COMPARE(header->property("collapseProgress").toReal(),0.);QCOMPARE(bounds(stats).left(),bounds(actions).left());
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void longMetadataStaysInsideTheCompactHeader() {
        Fixture f;QVERIFY(prepare(f,QSize(1000,660)));const auto id=f.library.createPlaylist("长信息歌单");
        QVERIFY(f.library.playlistDescription(id,QString(900,QChar(u'长'))));
        QVERIFY(f.library.playlistTags(id,playlistTagOptions()));
        QVERIFY(f.shell->libraryBridge()->openPlaylist(QString::number(id)));QTest::qWait(80);
        auto *w=f.shell->window();auto *root=w->contentItem();auto *header=visual(root,"playlistHeader"),*cover=visual(root,"playlistCover");
        QVERIFY(header);QVERIFY(cover);QTRY_COMPARE(cover->width(),160.);shot(w,"long-metadata-expanded");
        wheel(w,cover,-120);QTRY_COMPARE(cover->width(),78.);
        for(const char *name:{"playlistStatisticsRow","playlistDescriptionRow"}) {
            auto *row=visual(root,name);QVERIFY(row);QVERIFY(bounds(header).contains(bounds(row)));
            std::function<void(QQuickItem *)> inspect=[&](QQuickItem *item) {
                if(item->isVisible() && item->property("text").isValid()) {
                    QVERIFY(bounds(item).right()<=bounds(header).right()+.1);
                    QVERIFY(bounds(item).left()>=bounds(row).left()-.1);
                }
                for(auto *child:item->childItems())inspect(child);
            };inspect(row);
        }
        shot(w,"long-metadata-collapsed");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void scrollingReorderAndPlaylistSwitchRetainTheirBehavior() {
        Fixture f; QVERIFY(prepare(f));const auto first=f.library.createPlaylist("前一个"),second=f.library.createPlaylist("后一个");
        QVERIFY(f.library.addToPlaylist(first,{f.a.id,f.b.id}));QVERIFY(f.shell->libraryBridge()->openPlaylist(QString::number(first)));
        auto *w=f.shell->window();auto *root=w->contentItem();QTest::qWait(100);
        f.shell->libraryModel()->setSelectedId(QString::number(f.a.id));QTest::qWait(30);
        auto *down=visual(root,"playlistDown");QVERIFY(down);QVERIFY(down->isEnabled());QVERIFY(click(w,down));
        QCOMPARE(f.library.tracks(first).first().id,f.b.id);
        auto *header=visual(root,"playlistHeader"),*cover=visual(root,"playlistCover");QVERIFY(header);QVERIFY(cover);
        QVERIFY(f.shell->settingsBridge()->setPreference("reduceMotion",true));
        wheel(w,cover,-120);QTRY_COMPARE(cover->width(),78.);
        QVERIFY(f.shell->libraryBridge()->openPlaylist(QString::number(second)));QTRY_COMPARE(cover->width(),160.);
        QVERIFY(f.shell->libraryBridge()->openPlaylist("0"));QTRY_VERIFY(!header->isVisible());
        QVERIFY(visual(root,"librarySummary")->isVisible());QVERIFY(visual(root,"playAllButton")->isVisible());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void expandedPlayerHasCollapseWithoutTheExtraFullscreenButton() {
        Fixture f;QVERIFY(prepare(f));auto *w=f.shell->window();
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));QTRY_VERIFY(w->property("playerProgress").toReal()>.999);
        auto *page=visual(w->contentItem(),"nowPlayingPage");QVERIFY(page);
        QVERIFY(!visual(page,"fullscreenButton"));auto *collapse=visual(page,"playerCollapse");QVERIFY(collapse);QVERIFY(collapse->isVisible());
        auto *barrier=visual(w->contentItem(),"playerTransitionInputBarrier");QVERIFY(barrier);
        qInfo()<<"collapse readiness at .999"<<w->property("playerProgress")<<"input blocked"<<barrier->isVisible();
        // OutCubic passes .999 before the transition finishes. Wait for the
        // actual input guard, rather than clicking through a moving page.
        QTRY_VERIFY(!barrier->isVisible());QCOMPARE(w->property("playerProgress").toReal(),1.);
        QSignalSpy collapsed(page,SIGNAL(collapseRequested()));QVERIFY(collapsed.isValid());
        shot(w,"expanded-player-without-extra-fullscreen");QVERIFY(click(w,collapse));
        QCOMPARE(collapsed.count(),1);QTRY_COMPARE(w->property("playerProgress").toReal(),0.);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);
    PlaylistHeaderTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "playlist-header-tests.moc"
