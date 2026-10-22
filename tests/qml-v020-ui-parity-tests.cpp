#include "c4c5-ui-fixture.h"
#include "ui-bridge/upcoming-model.h"
#include "ui-bridge/ui-images.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QLineF>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QQuickWindow>
#include <QSet>
#include <cmath>
#include <functional>

using namespace c4c5_fixture;
namespace {
QRectF sceneBounds(QQuickItem *item) {
    return item ? item->mapRectToScene(QRectF(0, 0, item->width(), item->height())) : QRectF();
}
bool click(QQuickWindow *window, QQuickItem *item, QPointF local = QPointF(-1, -1)) {
    if (!window || !item || !item->isVisible() || !item->isEnabled() || item->width() <= 0 || item->height() <= 0) return false;
    if (local.x() < 0) local = QPointF(item->width() / 2, item->height() / 2);
    const auto point = item->mapToScene(local).toPoint();
    if (!QRect(QPoint(), window->size()).contains(point)) return false;
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
    return true;
}
bool prepare(Fixture &fixture) {
    if (!fixture.ok || !fixture.shell->window()) return false;
    auto *window = fixture.shell->window();
    window->showNormal(); window->setGeometry(55, 60, 1240, 790);
    window->raise(); window->requestActivate();
    if (!QTest::qWaitForWindowExposed(window) || !QTest::qWaitForWindowActive(window)) return false;
    QTest::qWait(150);
    return true;
}
QObject *contextObject(QQuickWindow *window, const char *name) {
    auto *context = QQmlEngine::contextForObject(window);
    return context ? qvariant_cast<QObject *>(context->contextProperty(QString::fromLatin1(name))) : nullptr;
}
QQuickItem *grabSnapshot(QQuickItem *paper) {
    if (!paper) return nullptr;
    for (auto *item : paper->childItems())
        if (item->property("source").toUrl().scheme() == "itemgrabber") return item;
    return nullptr;
}
QList<QQuickItem *> namedVisuals(QQuickItem *root, const QString &name) {
    QList<QQuickItem *> result;
    if (!root) return result;
    if (root->objectName() == name) result.append(root);
    for (auto *child : root->childItems()) result.append(namedVisuals(child, name));
    return result;
}
bool hasVisualType(QQuickItem *root, const QByteArray &typeName) {
    if (QByteArray(root->metaObject()->className()).contains(typeName)) return true;
    for (auto *child : root->childItems()) if (hasVisualType(child, typeName)) return true;
    return false;
}
}

class QmlV020UiParityTests final : public QObject {
    Q_OBJECT
private slots:
    void encodedLocalArtworkUsesTheActualFileAndRestoredProgressIsVisible() {
        Fixture f;QVERIFY(prepare(f));f.shell->playbackBridge()->pause();
        UiImages images;QSize size;
        const auto url=f.shell->playbackBridge()->coverUrl();QVERIFY(url.isLocalFile());
        const auto result=images.requestImage("artwork?file="+QString::fromLatin1(QUrl::toPercentEncoding(url.toString()))+"&resolution=64",&size,{});
        QCOMPARE(size,QSize(64,64));QCOMPARE(result.pixelColor(32,32),QColor("#b52d50"));
        QTRY_VERIFY(f.shell->facade()->seekable());f.shell->facade()->seek(1600);
        auto *slider=visual(f.shell->window()->contentItem(),"playbackProgress");QVERIFY(slider);
        QTRY_VERIFY(slider->property("value").toReal()>1000);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void originalSidebarAndFooterKeepTheFiveNavigationDestinations() {
        Fixture f; QVERIFY2(prepare(f), qPrintable(f.shell->warnings().join('\n')));
        auto *window = f.shell->window(); auto *content = window->contentItem();
        auto *sidebar = visual(content, "sidebar"); QVERIFY(sidebar);
        const QStringList names{"本地音乐", "歌单", "个人", "设置", "我喜欢"};
        int navigationCount = 0;
        QList<QQuickItem *> all;std::function<void(QQuickItem *)> collect=[&](QQuickItem *item){all.append(item);for(auto *child:item->childItems())collect(child);};collect(sidebar);
        for (auto *object : all) if (object->objectName().startsWith("navigation")) ++navigationCount;
        QCOMPARE(navigationCount, 5);
        for (int index = 0; index < names.size(); ++index) {
            auto *button = visual(sidebar, "navigation" + QString::number(index)); QVERIFY(button);
            QCOMPARE(button->property("text").toString(), names[index]);
            QVERIFY(button->isVisible()); QVERIFY(button->isEnabled());
            QVERIFY(sceneBounds(sidebar).contains(sceneBounds(button).center()));
        }
        QVERIFY(!visual(sidebar, "lyricsNavigation"));
        QVERIFY(!visual(sidebar, "importNavigation"));
        auto *bar = visual(content, "playerBar"); auto *cover = visual(content, "bottomCover");
        auto *title = visual(content, "currentTitle"); auto *center = visual(content, "playerCenter");
        QVERIFY(bar); QVERIFY(cover); QVERIFY(title); QVERIFY(center);
        QCOMPARE(bar->height(), 98.); QCOMPARE(cover->size(), QSizeF(62, 62));
        QCOMPARE(sceneBounds(bar).bottom(), qreal(window->height()));
        QVERIFY(sceneBounds(cover).center().x() < window->width() / 3.);
        QVERIFY(sceneBounds(bar).contains(sceneBounds(cover)));
        QCOMPARE(title->property("text").toString(), f.a.title);
        QVERIFY(!window->property("lyricsOpen").toBool());
        QVERIFY(window->visibility() != QWindow::FullScreen);
        QCOMPARE(f.http.requestCount(), 0);
        QVERIFY2(f.shell->warnings().isEmpty(), qPrintable(f.shell->warnings().join('\n')));
    }

    void coverAndTitleClicksAnimateIntoPlayerAndCollapseBackToTheSamePage() {
        Fixture f; QVERIFY2(prepare(f), qPrintable(f.shell->warnings().join('\n')));
        auto *window = f.shell->window(); auto *content = window->contentItem();
        QVERIFY(click(window, visual(content, "navigation2")));
        QTRY_COMPARE(f.shell->appBridge()->page(), 2); QTest::qWait(300);
        auto *center = visual(content, "playerCenter"); auto *flight = visual(content, "sharedArtworkFlight");
        QVERIFY(center); QVERIFY(flight);
        const auto bounds = window->geometry(); const auto centerBounds = sceneBounds(center);
        const auto queue = f.shell->facade()->queue().items;
        QTRY_VERIFY(f.shell->facade()->position() > 300);
        for (const auto &trigger : {QString("bottomCover"), QString("currentTitle")}) {
            const auto position = f.shell->facade()->position();
            auto *metadata = visual(content, trigger); QVERIFY(metadata);
            QElapsedTimer motion; motion.start();
            QVERIFY(click(window, metadata));
            QVERIFY(window->property("lyricsOpen").toBool());
            QVERIFY(window->visibility() != QWindow::FullScreen);
            QTRY_VERIFY_WITH_TIMEOUT(flight->isVisible(), 180);
            const auto first = sceneBounds(flight);
            QTest::qWait(45);
            const auto second = sceneBounds(flight);
            QVERIFY2(motion.elapsed() < 420, "No moving cover frame was observed within the requested expansion animation");
            QVERIFY(second.width() > first.width() + 2);
            QVERIFY(QLineF(first.center(), second.center()).length() > 2);
            QVERIFY2(second.width() > 62 && second.width() < 500, "Cover must visibly interpolate between the footer and expanded positions");
            QCOMPARE(sceneBounds(center), centerBounds);
            QTRY_VERIFY_WITH_TIMEOUT(!window->property("coverTravelling").toBool(), 1200);
            QTRY_VERIFY_WITH_TIMEOUT(qAbs(window->property("playerProgress").toReal() - 1.) < .001, 1200);
            auto *large = visual(content, "currentArtwork"); QVERIFY(large); QVERIFY(large->width() > 150);
            QVERIFY(click(window, visual(content, "playerCollapse")));
            QTRY_VERIFY_WITH_TIMEOUT(!window->property("lyricsOpen").toBool(), 400);
            QTRY_VERIFY_WITH_TIMEOUT(window->property("playerProgress").toReal() < .001, 1200);
            QTRY_VERIFY(!flight->isVisible());
            QCOMPARE(f.shell->appBridge()->page(), 2);
            QCOMPARE(window->geometry(), bounds);
            QCOMPARE(sceneBounds(center), centerBounds);
            QCOMPARE(f.shell->facade()->queue().items, queue);
            QCOMPARE(f.shell->facade()->current().id, f.a.id);
            QVERIFY(f.shell->facade()->position() >= position);
        }
        QVERIFY2(f.shell->warnings().isEmpty(), qPrintable(f.shell->warnings().join('\n')));
    }

    void sidebarNavigationProducesRealOutgoingAndIncomingAnimationFrames() {
        Fixture f; QVERIFY2(prepare(f), qPrintable(f.shell->warnings().join('\n')));
        auto *window = f.shell->window(); auto *content = window->contentItem();
        auto *surface = visual(content, "pageSurface"); auto *paper = visual(content, "paperSurface");
        QVERIFY(surface); QVERIFY(paper);
        const auto queue = f.shell->facade()->queue().items;
        for (const int destination : {1, 3, 0}) {
            bool sawOutgoingSnapshot = false, sawOutgoingFade = false, sawIncomingFade = false, sawIncomingMovement = false;
            QVERIFY(click(window, visual(content, "navigation" + QString::number(destination))));
            QElapsedTimer frames; frames.start();
            while (frames.elapsed() < 650) {
                if (auto *snapshot = grabSnapshot(paper); snapshot && snapshot->isVisible()) {
                    sawOutgoingSnapshot = true;
                    sawOutgoingFade = sawOutgoingFade || (snapshot->opacity() > .001 && snapshot->opacity() < .999);
                }
                sawIncomingFade = sawIncomingFade || (surface->opacity() > .001 && surface->opacity() < .999);
                sawIncomingMovement = sawIncomingMovement || (surface->y() < -.05 && surface->y() > -8);
                QTest::qWait(7);
            }
            QCOMPARE(f.shell->appBridge()->page(), destination);
            QVERIFY2(sawOutgoingSnapshot, "Sidebar navigation never displayed an outgoing page snapshot");
            QVERIFY2(sawOutgoingFade, "Outgoing page did not visibly fade across native frames");
            QVERIFY2(sawIncomingFade, "New page appeared without an intermediate fade frame");
            QVERIFY2(sawIncomingMovement, "New page appeared without the original vertical movement");
            QCOMPARE(surface->opacity(), 1.); QCOMPARE(surface->y(), 0.);
            QCOMPARE(f.shell->facade()->queue().items, queue);
            QCOMPARE(f.shell->facade()->current().id, f.a.id);
        }
        QVERIFY2(f.shell->warnings().isEmpty(), qPrintable(f.shell->warnings().join('\n')));
    }

    void upcomingTracksStayBottomRightThenBottomLeftAndActivateTheForecast() {
        Fixture f; QVERIFY2(prepare(f), qPrintable(f.shell->warnings().join('\n')));
        auto *window = f.shell->window(); auto *content = window->contentItem();
        auto *forecast = qobject_cast<UpcomingModel *>(contextObject(window, "upcomingModel")); QVERIFY(forecast);
        f.shell->facade()->setMode(Mode::ListLoop);
        QTRY_VERIFY(forecast->count() > 0);
        auto *panel = visual(content, "nextQueuePanel"); QVERIFY(panel);
        QTRY_VERIFY(panel->isVisible());
        const auto queue = f.shell->facade()->queue().items;
        const auto compact = sceneBounds(panel);
        QVERIFY(qAbs(compact.right() - (window->width() - 8)) < 2);
        QVERIFY(qAbs(compact.bottom() - (window->height() - 4)) < 2);
        const auto expected = f.shell->facade()->upcoming(); QVERIFY(!expected.isEmpty());
        QTRY_COMPARE(forecast->ids(), expected);
        QSignalSpy activated(forecast, &UpcomingModel::aboutToActivate);
        QVERIFY(click(window, panel, QPointF(panel->width() / 2, 17)));
        QTRY_COMPARE(f.shell->facade()->current().id, expected.first());
        QCOMPARE(activated.size(), 1); QCOMPARE(f.shell->facade()->queue().items, queue);
        QVERIFY(click(window, visual(content, "bottomCover")));
        QTRY_VERIFY_WITH_TIMEOUT(window->property("playerProgress").toReal() > .999, 1200);
        // The panel has its own outgoing/incoming animations. Player progress
        // can reach its endpoint before the incoming panel animation finishes.
        QTRY_VERIFY_WITH_TIMEOUT(!panel->property("relocating").toBool(), 1200);
        QTRY_VERIFY(qAbs(sceneBounds(panel).left() - 8.) < 2);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(sceneBounds(panel).bottom() - (window->height() - 4)) < 2, 1200);
        QVERIFY(panel->property("expandedPlayer").toBool());
        const auto expandedExpected = f.shell->facade()->upcoming(); QVERIFY(!expandedExpected.isEmpty());
        QTRY_COMPARE(forecast->ids(), expandedExpected);
        auto *list = visual(panel, "upcomingRows"); QVERIFY(list);
        QQuickItem *first = nullptr;
        QTRY_VERIFY(([&] { for (auto *item : namedVisuals(list, "upcomingDelegate")) if (item->property("index").toInt() == 0) { first = item; return true; } return false; })());
        QTRY_VERIFY_WITH_TIMEOUT(panel->isEnabled(),800);
        QVERIFY(click(window, first));
        QTRY_COMPARE(f.shell->facade()->current().id, expandedExpected.first());
        QCOMPARE(f.shell->facade()->queue().items, queue);
        QCOMPARE(activated.size(), 2);
        QVERIFY(window->property("lyricsOpen").toBool());
        QVERIFY2(f.shell->warnings().isEmpty(), qPrintable(f.shell->warnings().join('\n')));
    }

    void lyricsToolsTargetTheSelectedNonPlayingSongWithoutTouchingPlayback() {
        Fixture f; QVERIFY2(prepare(f), qPrintable(f.shell->warnings().join('\n')));
        auto *window = f.shell->window(); auto *facade = f.shell->facade();
        QTRY_VERIFY(facade->position() > 300);
        f.shell->playbackBridge()->pause(); facade->seek(1600); QTest::qWait(100);
        const auto current = facade->current().id; const auto queue = facade->queue().items; const auto position = facade->position();
        const QVariant keys = QStringList{QString::number(f.b.id)};
        QVERIFY(QMetaObject::invokeMethod(window, "openLyricsTools", Q_ARG(QVariant, keys), Q_ARG(QVariant, QVariant(false))));
        auto *workspace = window->findChild<QObject *>("lyricsWorkspace"); QVERIFY(workspace);
        QTRY_VERIFY(workspace->property("visible").toBool());
        QCOMPARE(f.shell->lyricsSearchBridge()->targetTrackKey(), QString::number(f.b.id));
        QCOMPARE(f.shell->translationBridge()->targetTrackKey(), QString::number(f.b.id));
        QCOMPARE(f.shell->lyricsSearchBridge()->targetTitle(), f.b.title);
        QVERIFY(f.shell->lyricsSearchBridge()->sourceText().contains("Different"));
        QCOMPARE(facade->current().id, current); QCOMPARE(facade->queue().items, queue); QCOMPARE(facade->position(), position);
        QVERIFY(!facade->playing()); QCOMPARE(f.http.requestCount(), 0);
        auto *close = visual(window->contentItem(), "lyricsWorkspaceClose"); QVERIFY(close); QVERIFY(click(window, close));
        QTRY_VERIFY(!workspace->property("visible").toBool());
        QCOMPARE(facade->current().id, current); QCOMPARE(facade->queue().items, queue); QCOMPARE(facade->position(), position);
        QCOMPARE(f.shell->lyricsSearchBridge()->targetTrackKey(), QString::number(f.a.id));
        QVERIFY2(f.shell->warnings().isEmpty(), qPrintable(f.shell->warnings().join('\n')));
    }

    void settingsKeepToolsOutOfTabsAndAiIsAnIndependentOriginalEntry() {
        Fixture f; QVERIFY2(prepare(f), qPrintable(f.shell->warnings().join('\n')));
        auto *window = f.shell->window(); auto *content = window->contentItem();
        QVERIFY(click(window, visual(content, "navigation3")));
        QTRY_COMPARE(f.shell->appBridge()->page(), 3); QTest::qWait(300);
        auto *settings = visual(content, "settingsPage"); QVERIFY(settings); QVERIFY(settings->isVisible());
        QVERIFY(!hasVisualType(settings, "TabBar"));
        QVERIFY(!visual(settings, "importPage")); QVERIFY(!visual(settings, "lyricsWorkspace")); QVERIFY(!visual(settings, "aiPage"));
        auto *manage = visual(settings, "openAiServiceSettings"); QVERIFY(manage);
        QVERIFY(click(window, manage));
        QTRY_COMPARE(window->property("settingsSection").toString(), QString("ai")); QTest::qWait(300);
        auto *ai = visual(content, "aiPage"); QVERIFY(ai); QVERIFY(ai->isVisible());
        QVERIFY(!settings->isVisible());
        QCOMPARE(f.http.requestCount(), 0);
        const QByteArray catalog = R"({"object":"list","data":[{"id":"deepseek-flash","name":"Controlled display model","context_window":1048576,"max_output_tokens":393216,"input_modalities":["text"],"output_modalities":["text"],"effort":{"supported_levels":["low","high","max"],"default_level":"high"}}]})";
        f.http.enqueue(catalog);
        auto *refresh = visual(ai, "modelRefreshButton"); QVERIFY(refresh);
        QVERIFY(click(window, refresh));
        QTRY_VERIFY(!f.shell->aiBridge()->busy());
        QTRY_COMPARE(f.http.requestCount(), 1);
        QVERIFY(f.http.request(0).startsWith("GET /models"));
        QVERIFY(f.http.requestBody(0).isEmpty());
        QCOMPARE(f.runtime->configuration().defaults.providerId, QString("deepseek"));
        QCOMPARE(f.runtime->configuration().defaults.modelId, QString("deepseek-flash"));
        QVERIFY(f.runtime->validateConfiguration(f.runtime->translationConfiguration()).isEmpty());
        auto *back = visual(ai, "aiSettingsBack"); QVERIFY(back); QVERIFY(click(window, back));
        QTRY_COMPARE(window->property("settingsSection").toString(), QString("settings"));
        QTRY_VERIFY(settings->isVisible());
        QVERIFY2(f.shell->warnings().isEmpty(), qPrintable(f.shell->warnings().join('\n')));
    }


    void loadedModelsCanBeSelectedWithTheActualDropdownAndPersisted() {
        Fixture f; QVERIFY2(prepare(f), qPrintable(f.shell->warnings().join('\n')));
        auto second=confirmedFlash();second.id="deepseek-v4-pro";second.displayName="Pro model";
        QVERIFY(f.runtime->acceptModelCatalog("deepseek",{confirmedFlash(),second}));
        auto *window=f.shell->window();window->setHeight(1040);
        auto *content=window->contentItem();
        QVERIFY(click(window,visual(content,"navigation3")));
        QTRY_COMPARE(f.shell->appBridge()->page(),3);QTest::qWait(300);
        QVERIFY(click(window,visual(content,"openAiServiceSettings")));
        QTRY_COMPARE(window->property("settingsSection").toString(),QString("ai"));QTest::qWait(300);
        auto *model=visual(content,"aiDefaultModel");QVERIFY(model);QVERIFY(model->isEnabled());
        QCOMPARE(model->property("count").toInt(),3);
        QVERIFY(click(window,model));auto *popup=qvariant_cast<QObject *>(model->property("popup"));QVERIFY(popup);
        QTRY_VERIFY(popup->property("visible").toBool());
        QTest::keyClick(window,Qt::Key_End);QTest::keyClick(window,Qt::Key_Return);
        QTRY_COMPARE(f.runtime->configuration().defaults.modelId,QString("deepseek-v4-pro"));
        QTRY_COMPARE(model->property("currentValue").toString(),QString("deepseek-v4-pro"));
        QVERIFY(f.runtime->validateConfiguration(f.runtime->translationConfiguration()).isEmpty());
        QVERIFY(click(window,model));QTest::keyClick(window,Qt::Key_Home);QTest::keyClick(window,Qt::Key_Down);QTest::keyClick(window,Qt::Key_Return);
        QTRY_COMPARE(f.runtime->configuration().defaults.modelId,QString("deepseek-flash"));
        QTRY_COMPARE(model->property("currentValue").toString(),QString("deepseek-flash"));
        QCOMPARE(f.http.requestCount(),0);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void spaceTypedIntoSearchDoesNotTriggerTheLocalPlaybackShortcut() {
        Fixture f; QVERIFY2(prepare(f), qPrintable(f.shell->warnings().join('\n')));
        auto *window = f.shell->window(); auto *content = window->contentItem();
        QVERIFY(!f.shell->settingsBridge()->values().value("shortcuts").toMap().value("globalEnabled").toBool());
        f.shell->playbackBridge()->pause(); QTRY_VERIFY(!f.shell->facade()->playing());
        auto *search = visual(content, "librarySearch"); QVERIFY(search); QVERIFY(click(window, search));
        QTRY_VERIFY(search->hasActiveFocus());
        const auto queue = f.shell->facade()->queue().items;
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_COMPARE(search->property("text").toString(), QString(" "));
        QVERIFY(!f.shell->facade()->playing());
        QCOMPARE(f.shell->facade()->current().id, f.a.id); QCOMPARE(f.shell->facade()->queue().items, queue);
        auto *table=visual(content,"libraryListContainer");QVERIFY(table);QVERIFY(click(window,table,QPointF(table->width()/2,85)));QTRY_VERIFY(!search->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(f.shell->facade()->playing());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!f.shell->facade()->playing());
        QCOMPARE(f.shell->facade()->queue().items, queue);
        QVERIFY2(f.shell->warnings().isEmpty(), qPrintable(f.shell->warnings().join('\n')));
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false);
    QmlV020UiParityTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "qml-v020-ui-parity-tests.moc"
