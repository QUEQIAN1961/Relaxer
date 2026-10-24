#include "c4c5-ui-fixture.h"
#include <QApplication>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QJSValue>
#include <QAbstractItemModelTester>

using namespace c4c5_fixture;
namespace {
QRectF bounds(QQuickItem *item) { return item->mapRectToScene(QRectF(QPointF(), item->size())); }
bool click(QQuickWindow *window, QQuickItem *item) {
    // Let pending QML layouts settle before mapping a real pointer event.
    QTest::qWait(40);
    if (!item || !item->isVisible() || !item->isEnabled()) return false;
    if (!QRectF(QPointF(), window->size()).contains(bounds(item).center())) return false;
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, bounds(item).center().toPoint());
    return true;
}
bool prepare(Fixture &f) {
    if (!f.ok) return false;
    auto *w = f.shell->window(); w->showNormal(); w->resize(1240,790);
    f.shell->playbackBridge()->pause();
    return QTest::qWaitForWindowExposed(w);
}
void shot(QQuickWindow *w, const QString &name) {
    const auto folder = qEnvironmentVariable("RELAXER_UX13_EVIDENCE");
    if (!folder.isEmpty()) { QDir().mkpath(folder); w->grabWindow().save(QDir(folder).filePath(name + ".png")); }
}
QJsonObject candidate(const Track &t, int id = 1, bool timed = true) {
    return {{"id",id},{"trackName",t.title},{"artistName",t.artist},{"albumName",t.album},{"duration",60},
            {timed ? "syncedLyrics" : "plainLyrics",timed ? "[00:01.00]Candidate first\n[00:05.00]Candidate second\n" : "Plain candidate"},
            {"source","Controlled cache"}};
}
bool openWorkspace(Fixture &f, const QStringList &keys) {
    return QMetaObject::invokeMethod(f.shell->window(), "openLyricsTools", Q_ARG(QVariant,QVariant(keys)), Q_ARG(QVariant,false));
}
QVariantList selected(QQuickItem *workspace) {
    const auto v = workspace->property("selectedTracks");
    return v.canConvert<QJSValue>() ? v.value<QJSValue>().toVariant().toList() : v.toList();
}
void appearance(Fixture &f, int mode) {
    f.shell->appBridge()->navigate(3);
    f.shell->window()->setProperty("settingsSection","appearance");
    f.shell->settingsBridge()->beginAppearance();
    f.shell->settingsBridge()->setAppearanceMode(mode);
}
QColorDialog *colorDialog() {
    for (auto *w : QApplication::topLevelWidgets())
        if (w->objectName() == "appearanceFullColorDialog" && w->isVisible()) return qobject_cast<QColorDialog *>(w);
    return nullptr;
}
}
class ManualUx13Tests final : public QObject {
    Q_OBJECT
private slots:
    void recommendedSelectionKeepsExactSaveGuards() {
        QTemporaryDir dir; Library lib(dir.filePath("data")); QVERIFY(lib.open());
        const auto a = seed(lib,"Alpha","rec-a"), b = seed(lib,"Beta","rec-b");
        QVERIFY(lib.saveLookupCache(a.id,QJsonArray{candidate(a,1,false),candidate(a,2,true)},false));
        QVERIFY(lib.saveLookupCache(b.id,QJsonArray{candidate(b)},false));
        PlaybackFacade player(&lib,nullptr,true); LyricsSearchCoordinator service(&lib);
        LyricsSearchBridge model(&lib,&player,&service);
        QAbstractItemModelTester tester(&model,QAbstractItemModelTester::FailureReportingMode::QtTest);
        QVERIFY(model.setTargetTracks({QString::number(a.id),QString::number(b.id)},false,true));
        QVERIFY(!model.selectedKey().isEmpty()); QCOMPARE(model.rowCount(),2);
        QVERIFY(model.selectRecommended(true)); QVERIFY(model.previewText().startsWith("["));
        auto reranked = service.candidates(a.id);
        const auto better = candidate(a,3,true);
        reranked.records.prepend(better);
        emit service.candidatesChanged(reranked,0);
        QVERIFY(model.selectedKey().endsWith(LyricsSearchCoordinator::candidateIdentity(better)));
        QVERIFY(model.select(2)); const auto explicitChoice = model.selectedKey();
        const auto snapshot = service.candidates(a.id);
        emit service.candidatesChanged(snapshot,0);
        QCOMPARE(model.selectedKey(),explicitChoice);
        const auto stale = model.selectedKey();
        QVERIFY(model.setTargetTrack(QString::number(b.id))); QVERIFY(!model.selectedKey().isEmpty());
        QVERIFY(!model.saveSelected(stale,true)); QVERIFY(lib.track(b.id).lyrics.isEmpty());
        const auto key = model.selectedKey(); QVERIFY(!model.saveSelected(key,false));
        QVERIFY(lib.saveLyrics(b.id,"[00:00.00]Changed independently\n"));
        QVERIFY(!model.saveSelected(key,true)); QVERIFY(model.sourceText().contains("Changed independently"));
        service.shutdown(); QVERIFY(player.shutdown());
    }
    void workspaceSavesAndAdvancesThroughVisibleChoices() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        const auto c=seed(f.library,"Third pending","ux-c"),d=seed(f.library,"Fourth pending","ux-d");
        QVERIFY(f.library.saveLookupCache(c.id,QJsonArray{candidate(c)},false));
        QVERIFY(f.library.saveLookupCache(d.id,QJsonArray{candidate(d)},false));
        const QStringList keys{QString::number(f.a.id),QString::number(c.id),QString::number(f.b.id),QString::number(d.id)};
        QVERIFY(openWorkspace(f,keys)); auto *workspace=visual(root,"lyricsWorkspace"); QVERIFY(workspace);
        QVERIFY(QMetaObject::invokeMethod(workspace,"chooseTarget",Q_ARG(QVariant,QString::number(c.id))));
        auto *search=f.shell->lyricsSearchBridge(); QTRY_VERIFY(!search->selectedKey().isEmpty());
        QCOMPARE(search->previewText(),QString::fromUtf8(candidate(c)["syncedLyrics"].toString().toUtf8()));
        const auto playing=f.shell->facade()->current().id; const auto queue=f.shell->facade()->queue().items;
        for (const QSize size : {QSize(1240,790),QSize(1000,660)}) {
            w->resize(size); QTest::qWait(50);
            auto *multi=visual(root,"lyricsMultiSelect"); auto *all=visual(root,"lyricsSelectAll");
            QVERIFY(click(w,multi)); QTRY_VERIFY(all->isVisible()); QCOMPARE(multi->property("text").toString(),QString("取消"));
            auto *filter=visual(root,"lyricsFilterRow"); QVERIFY(filter);
            for (const char *name : {"lyricsMissingOnly","lyricsMissingTranslationOnly","lyricsMultiSelect","lyricsSelectAll"}) {
                auto *control=visual(root,name); QVERIFY(control);
                QVERIFY(bounds(filter).adjusted(-1,-1,1,1).contains(bounds(control)));
                QVERIFY(qAbs(bounds(control).center().y()-bounds(multi).center().y())<1);
            }
            QVERIFY(click(w,multi)); QTRY_VERIFY(!all->isVisible());
            auto *instrumental=visual(root,"lyricsInstrumentalButton"), *apply=visual(root,"lyricsApplyButton");
            QVERIFY(instrumental && apply); QCOMPARE(instrumental->height(),apply->height());
            QVERIFY(qAbs(instrumental->width()/(instrumental->width()+apply->width())-.35)<.01);
            QVERIFY(bounds(apply).bottom()>w->height()-65);
            shot(w, size.width()==1240 ? "lyrics-normal" : "lyrics-compact");
        }
        QVERIFY(click(w,visual(root,"lyricsMissingOnly")));
        QVERIFY(click(w,visual(root,"lyricsMultiSelect"))); QVERIFY(click(w,visual(root,"lyricsSelectAll")));
        QCOMPARE(selected(workspace).size(),2);
        shot(w,"lyrics-filtered-select-all");
        QVERIFY(click(w,visual(root,"lyricsMultiSelect"))); QCOMPARE(selected(workspace).size(),0);
        QVERIFY(click(w,visual(root,"lyricsApplyButton")));
        QTRY_COMPARE(search->targetTrackKey(),QString::number(d.id)); QTRY_VERIFY(!search->selectedKey().isEmpty());
        QVERIFY(!f.library.track(c.id).lyrics.isEmpty()); QVERIFY(f.library.track(d.id).lyrics.isEmpty());
        QVERIFY(click(w,visual(root,"lyricsApplyButton")));
        QTRY_VERIFY(!f.library.track(d.id).lyrics.isEmpty()); QCOMPARE(search->targetTrackKey(),QString::number(d.id));
        QVERIFY(search->selectedKey().isEmpty());
        QCOMPARE(f.shell->facade()->current().id,playing); QCOMPARE(f.shell->facade()->queue().items,queue);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void failedSaveDoesNotAdvanceOrReplaceNewerLyrics() {
        Fixture f; QVERIFY(prepare(f)); const auto c=seed(f.library,"Conflict","ux-conflict"),d=seed(f.library,"Next","ux-next");
        QVERIFY(f.library.saveLookupCache(c.id,QJsonArray{candidate(c)},false));
        QVERIFY(f.library.saveLookupCache(d.id,QJsonArray{candidate(d)},false));
        QVERIFY(openWorkspace(f,{QString::number(c.id),QString::number(d.id)}));
        auto *w=f.shell->window(); auto *search=f.shell->lyricsSearchBridge(); QTRY_VERIFY(!search->selectedKey().isEmpty());
        QVERIFY(f.library.saveLyrics(c.id,"[00:00.00]Newer local edit\n"));
        QVERIFY(click(w,visual(w->contentItem(),"lyricsApplyButton")));
        QCOMPARE(search->targetTrackKey(),QString::number(c.id)); QVERIFY(search->sourceText().contains("Newer local edit"));
        QVERIFY(f.library.track(d.id).lyrics.isEmpty());
    }
    void appearanceToolbarSwatchesAndLeaveActions() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem(); appearance(f,4);
        auto *s=f.shell->settingsBridge(); QVERIFY(s->editAppearance("customText",true)); QVERIFY(s->editAppearance("rightInk","#13579b"));
        for (const QSize size : {QSize(1240,790),QSize(1000,660)}) {
            w->resize(size); QTest::qWait(60);
            auto *back=visual(root,"appearanceBack"); auto *toolbar=visual(root,"appearanceToolbar"); QVERIFY(back && toolbar);
            qreal edge=bounds(back).right();
            for (const char *name : {"appearanceNewSave","appearanceOverwriteSave","appearanceReset"}) {
                auto *button=visual(root,name); QVERIFY(button && button->isVisible());
                QCOMPARE(bounds(button).center().y(),bounds(back).center().y());
                QVERIFY(bounds(button).left()>edge); edge=bounds(button).right();
                QVERIFY(bounds(toolbar).adjusted(-1,-1,1,1).contains(bounds(button)));
            }
            auto *swatch=visual(root,"appearanceColor-rightInk"); QVERIFY(swatch);
            QCOMPARE(swatch->property("swatchColor").value<QColor>(),QColor("#13579b"));
            auto *left=visual(root,"appearanceColor-left"); auto *right=visual(root,"appearanceColor-right");
            QVERIFY(left && right); QVERIFY(qAbs(left->width()-right->width())<=1);
            shot(w,size.width()==1240?"appearance-normal":"appearance-compact");
        }
        auto *scroll=visual(root,"appearanceScroll"); QVERIFY(scroll);
        auto *flickable=scroll->property("contentItem").value<QObject *>(); QVERIFY(flickable);
        QVERIFY(s->editAppearance("blurLeft",12.3));
        flickable->setProperty("contentY",flickable->property("contentHeight").toReal()-flickable->property("height").toReal());
        QTest::qWait(60);
        auto *blur=visual(root,"appearanceBlur-blurLeft"); QVERIFY(blur);
        QCOMPARE(blur->property("displayText").toString(),QLocale().toString(12.3,'f',1));
        auto *input=blur->property("contentItem").value<QObject *>(); QVERIFY(input);
        QVERIFY(input->property("width").toReal()>=input->property("implicitWidth").toReal());
        auto *rightBlur=visual(root,"appearanceBlur-blurRight"),*footerBlur=visual(root,"appearanceBlur-blurFooter"),*expandedBlur=visual(root,"appearanceBlur-blurExpanded");
        QVERIFY(rightBlur && footerBlur && expandedBlur);
        QCOMPARE(bounds(rightBlur).center().y(),bounds(blur).center().y());
        QCOMPARE(bounds(footerBlur).center().y(),bounds(expandedBlur).center().y());
        QVERIFY(bounds(footerBlur).top()>bounds(blur).bottom());
        QVERIFY(click(w,qobject_cast<QQuickItem *>(input)));
        QTest::keyClick(w,Qt::Key_A,Qt::ControlModifier);
        for (char character : QByteArray("23.4")) QTest::keyClick(w,Qt::Key(character));
        QTest::keyClick(w,Qt::Key_Return);
        QTRY_COMPARE(s->appearanceDraft()["blurLeft"].toDouble(),23.4);
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,blur->mapToScene(QPointF(blur->width()-8,9)).toPoint());
        QTRY_COMPARE(s->appearanceDraft()["blurLeft"].toDouble(),23.5);
        auto *slider=visual(root,"appearanceBlurSlider-blurRight"); QVERIFY(slider);
        QVERIFY(click(w,slider)); QTRY_VERIFY(s->appearanceDraft()["blurRight"].toDouble()>0);
        shot(w,"appearance-lower-options");
        QVERIFY(click(w,visual(root,"appearanceBack")));
        auto *dialog=w->findChild<QObject *>("appearanceUnsavedDialog"); QVERIFY(dialog); QTRY_VERIFY(dialog->property("opened").toBool());
        auto *cancel=visual(root,"appearanceLeaveCancel"),*discard=visual(root,"appearanceLeaveDiscard"),*save=visual(root,"appearanceLeaveSave");
        QVERIFY(cancel && discard && save); QCOMPARE(cancel->height(),save->height()); QCOMPARE(bounds(cancel).top(),bounds(discard).top());
        QVERIFY(bounds(cancel).right()+6<bounds(discard).left()); QVERIFY(bounds(discard).right()+6<bounds(save).left());
        shot(w,"appearance-unsaved"); QVERIFY(click(w,cancel)); QTRY_VERIFY(!dialog->property("opened").toBool()); QVERIFY(s->appearanceDirty());
        QVERIFY(click(w,visual(root,"appearanceBack"))); QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(click(w,discard)); QTRY_VERIFY(!s->appearanceDirty());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void translationConflictRequiresDecisionAndKeepsPreviewOnFailure() {
        Fixture f(2); QVERIFY(prepare(f)); auto *w=f.shell->window();
        const QByteArray source="[00:01.00]Hello moon\n[00:02.00]Walk home\n";
        QVERIFY(f.library.saveLyrics(f.a.id,source));
        const auto next=seed(f.library,"Next pending","ux-translation-next");
        QVERIFY(f.library.saveLookupCache(f.a.id,QJsonArray{candidate(f.a)},false));
        QVERIFY(f.library.saveLookupCache(next.id,QJsonArray{candidate(next)},false));
        QVERIFY(openWorkspace(f,{QString::number(f.a.id),QString::number(next.id)}));
        auto *translation=f.shell->translationBridge(); auto *search=f.shell->lyricsSearchBridge();
        QVERIFY(translation->classify(1));
        f.http.enqueue(completion(QString::fromUtf8(translationJson({{0,"月亮你好"},{1,"走回家"}}))));
        const auto task=translation->prepareAi(); QVERIFY(!task.isEmpty()); QVERIFY(translation->start(task));
        QTRY_VERIFY(translation->preview()); QCOMPARE(f.http.requestCount(),1);
        QVERIFY(click(w,visual(w->contentItem(),"lyricsApplyButton")));
        auto *dialog=w->findChild<QObject *>("lyricsTranslationConflict"); QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool()); QCOMPARE(search->sourceText().toUtf8(),source);
        QVERIFY(translation->preview()); QVERIFY(QMetaObject::invokeMethod(dialog,"reject"));
        QVERIFY(translation->preview());
        QVERIFY(click(w,visual(w->contentItem(),"lyricsApplyButton")));
        const QByteArray newer="[00:00.00]Independent change\n";
        QVERIFY(f.library.saveLyrics(f.a.id,newer)); QVERIFY(QMetaObject::invokeMethod(dialog,"accept"));
        QCOMPARE(search->sourceText().toUtf8(),newer); QCOMPARE(search->targetTrackKey(),QString::number(f.a.id));
        QVERIFY(translation->preview());
        QVERIFY(search->select(0)); // Explicitly refresh the source guard before replacing.
        QVERIFY(click(w,visual(w->contentItem(),"lyricsApplyButton")));
        QVERIFY(QMetaObject::invokeMethod(dialog,"accept"));
        QTRY_COMPARE(search->targetTrackKey(),QString::number(next.id));
        QVERIFY(translation->setTargetTrack(QString::number(f.a.id)));
        QCOMPARE(translation->state(),QString("Cancelled")); QVERIFY(!translation->preview());
    }
    void fullColorChooserAcceptCancelAndSessionLifetime() {
        Fixture f; QVERIFY(prepare(f)); appearance(f,3); auto *w=f.shell->window(); auto *s=f.shell->settingsBridge();
        const auto before=s->appearanceDraft()["right"].toString();
        QVERIFY(click(w,visual(w->contentItem(),"appearanceColor-right"))); QTRY_VERIFY(colorDialog());
        QPointer<QColorDialog> dialog=colorDialog(); QVERIFY(dialog->testOption(QColorDialog::DontUseNativeDialog));
        QVERIFY(dialog->findChildren<QSpinBox *>().size()>=6); QVERIFY(!dialog->findChildren<QLineEdit *>().isEmpty());
        QCOMPARE(dialog->windowHandle()->transientParent(),w);
        const auto folder=qEnvironmentVariable("RELAXER_UX13_EVIDENCE");
        if(!folder.isEmpty()) QVERIFY(dialog->grab().save(QDir(folder).filePath("full-color-chooser.png")));
        dialog->setCurrentColor(QColor("#234567")); dialog->reject(); QTRY_VERIFY(dialog.isNull()); QCOMPARE(s->appearanceDraft()["right"].toString(),before);
        QVERIFY(s->chooseAppearanceColor("right",w)); QTRY_VERIFY(colorDialog()); dialog=colorDialog();
        dialog->setCurrentColor(QColor("#234567")); dialog->accept(); QTRY_COMPARE(s->appearanceDraft()["right"].toString(),QString("#234567"));
        QVERIFY(s->saveAppearance(false)); s->beginAppearance(); QCOMPARE(s->appearanceDraft()["right"].toString(),QString("#234567"));
        QVERIFY(s->chooseAppearanceColor("right",w)); QTRY_VERIFY(colorDialog()); dialog=colorDialog();
        dialog->setCurrentColor(QColor("#abcdef")); s->discardAppearance(); QTRY_VERIFY(dialog.isNull());
        s->beginAppearance(); QCOMPARE(s->appearanceDraft()["right"].toString(),QString("#234567"));
    }
    void manualScrollHighlightsPreviewAndGuideThenReturns() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));
        auto *root=w->contentItem(); auto *panel=visual(root,"lyricsPanel"), *list=visual(root,"lyricsList"); QVERIFY(panel && list);
        QTRY_VERIFY(panel->isVisible()); QTRY_VERIFY(f.shell->lyricsModel()->rowCount()>10);
        QVERIFY(QMetaObject::invokeMethod(panel,"takeOver"));
        list->setProperty("contentY",600.); QTest::qWait(200);
        const int preview=panel->property("previewIndex").toInt(); QVERIFY(preview>=0); QVERIFY(preview!=f.shell->lyricsModel()->currentIndex());
        int emphasized=0;
        for(auto *row:delegates(list)) if(row->property("emphasized").toBool()) { ++emphasized; QCOMPARE(row->property("index").toInt(),preview); }
        QCOMPARE(emphasized,1);
        auto *guide=visual(root,"lyricsPreviewGuide"), *time=visual(root,"lyricsPreviewTime"); QVERIFY(guide && time);
        QVERIFY(guide->isVisible() && time->isVisible()); QVERIFY(guide->width()>100);
        QVERIFY(qAbs(bounds(guide).center().y()-bounds(list).center().y())<=.5);
        QVERIFY(qAbs(bounds(time).center().y()-bounds(list).center().y())<=.5);
        shot(w,"lyrics-scroll-guide"); QVERIFY(click(w,time)); QTRY_VERIFY(!panel->property("manualScroll").toBool()); QTRY_VERIFY(!guide->isVisible());
        QCOMPARE(f.shell->facade()->current().id,f.a.id);
    }
    void repeatedNavigationDoesNotRestartPageAnimation() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        int revision=w->property("navigationRevision").toInt(); QVERIFY(click(w,visual(root,"navigation0")));
        QCOMPARE(w->property("navigationRevision").toInt(),revision);
        QVERIFY(click(w,visual(root,"navigation1"))); QTRY_COMPARE(f.shell->appBridge()->page(),1); QTest::qWait(220);
        revision=w->property("navigationRevision").toInt(); QVERIFY(click(w,visual(root,"navigation1"))); QCOMPARE(w->property("navigationRevision").toInt(),revision);
        const auto key=f.shell->libraryBridge()->createPlaylist("UX playlist"); QVERIFY(key!="0");
        QVERIFY(QMetaObject::invokeMethod(w,"openPlaylist",Q_ARG(QVariant,key))); QTRY_COMPARE(f.shell->libraryBridge()->playlistId(),key); QTest::qWait(220);
        revision=w->property("navigationRevision").toInt(); QVERIFY(QMetaObject::invokeMethod(w,"openPlaylist",Q_ARG(QVariant,key))); QCOMPARE(w->property("navigationRevision").toInt(),revision);
        QVERIFY(click(w,visual(root,"navigation3"))); QTRY_COMPARE(f.shell->appBridge()->page(),3); QTest::qWait(220);
        revision=w->property("navigationRevision").toInt(); QVERIFY(click(w,visual(root,"navigation3"))); QCOMPARE(w->property("navigationRevision").toInt(),revision);
    }
    void sharedBackgroundUsesOneCanvasAcrossFooter() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem(); auto *s=f.shell->settingsBridge();
        QImage gradient(1240,790,QImage::Format_RGB32);
        for(int y=0;y<gradient.height();++y) for(int x=0;x<gradient.width();++x) gradient.setPixelColor(x,y,QColor(20+y*200/790,20+x*200/1240,90));
        const auto path=f.dir.filePath("continuous.png"); QVERIFY(gradient.save(path));
        s->beginAppearance(); QVERIFY(s->setAppearanceMode(4)); QVERIFY(s->editAppearance("sourceLeft",path)); QVERIFY(s->editAppearance("sourceRight",path));
        QVERIFY(s->editAppearance("shareFooter",true)); QVERIFY(s->editAppearance("blurLeft",0.)); QVERIFY(s->editAppearance("blurRight",0.));
        auto *page=visual(root,"pageBackground"),*footer=visual(root,"footerBackground"); QVERIFY(page && footer);
        visual(root,"pageSurface")->setVisible(false);
        for(bool lr:{true,false}) {
            QVERIFY(s->editAppearance("shareLR",lr));
            QTRY_COMPARE(page->property("status").toInt(),1); QTRY_COMPARE(footer->property("status").toInt(),1); QTest::qWait(100);
            QCOMPARE(page->height(),qreal(w->height())); QCOMPARE(footer->height(),qreal(w->height()));
            QCOMPARE(bounds(page).topLeft(),bounds(footer).topLeft());
            const auto image=w->grabWindow(); QVERIFY(!image.isNull()); const int seam=int(bounds(visual(root,"playerBar")).top());
            const auto above=image.pixelColor(900,seam-4),below=image.pixelColor(900,seam+4);
            QVERIFY2(qAbs(above.red()-below.red())<8,qPrintable(QString("Shared seam %1 vs %2").arg(above.name(),below.name())));
            shot(w,lr?"background-shared-all":"background-shared-right-footer");
        }
        QVERIFY(s->editAppearance("shareFooter",false)); QTRY_COMPARE(footer->height(),visual(root,"playerBar")->height());
        QVERIFY(page->height()<w->height());
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv); app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false);
    ManualUx13Tests tests; return QTest::qExec(&tests,argc,argv);
}
#include "manual-ux13-tests.moc"
