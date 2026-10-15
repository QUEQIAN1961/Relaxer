#include "window.h"
#include <QApplication>
#include <QDir>
#include <QHelpEvent>
#include <QTemporaryDir>
#include <QToolTip>
#include <QtTest>
using namespace relaxer;

class PopupTests : public QObject {
    Q_OBJECT
    static QWidget *visibleTooltip() {
        for (auto *widget : QApplication::topLevelWidgets())
            if (widget->objectName() == "qtooltip_label" && widget->isVisible()) return widget;
        return nullptr;
    }
    static void capture(QWidget *widget, const QString &name) {
        const auto directory = qEnvironmentVariable("RELAXER_QA_DIR");
        if (!directory.isEmpty()) {
            QVERIFY(QDir().mkpath(directory));
            QVERIFY(widget->grab().save(directory + "/" + name));
        }
    }
    static int matchingPixels(const QImage &image, const QColor &target) {
        int count = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const auto color = image.pixelColor(x, y);
                if (color.alpha() > 230 && qAbs(color.red() - target.red()) < 8 &&
                    qAbs(color.green() - target.green()) < 8 && qAbs(color.blue() - target.blue()) < 8)
                    ++count;
            }
        return count;
    }
    static void themeRows() {
        QTest::addColumn<int>("theme");
        QTest::newRow("light") << 0;
        QTest::newRow("dark") << 1;
    }
private slots:
    void initTestCase() {
        qApp->setStyle("Fusion");
        QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
        QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
    }
    void cleanup() {
        QToolTip::hideText();
        for (auto *widget : QApplication::topLevelWidgets())
            if (widget->windowType() == Qt::ToolTip) widget->hide();
    }
    void tooltipUsesReadableTheme_data() { themeRows(); }
    void tooltipUsesReadableTheme() {
        QFETCH(int, theme);
        QTemporaryDir directory;
        Library library(directory.filePath("data"));
        QVERIFY(library.open());
        library.setSetting("theme", theme);
        Track song;
        song.hash = "tooltip-song";
        song.file = "Music/tooltip-song.wav";
        song.title = "Rise of the Champ / 完整的歌曲名称";
        song.artist = "Alexander Hitchens / Mark Petrie";
        song.album = "Champion";
        song.duration = 148000;
        QVERIFY(library.insert(song));
        Window window(&library);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))window.playback()->media()->setAudioOutput(nullptr);
        window.show();
        QTest::qWait(60);
        auto *table = window.findChild<SongTable *>("songTable");
        QVERIFY(table);
        auto *item = table->item(0, SongTable::TitleColumn);
        QVERIFY(item);
        const auto position = table->visualItemRect(item).center();
        QTest::mouseMove(table->viewport(), position);
        QHelpEvent hover(QEvent::ToolTip, position, table->viewport()->mapToGlobal(position));
        QVERIFY(QApplication::sendEvent(table->viewport(), &hover));
        QTRY_VERIFY_WITH_TIMEOUT(visibleTooltip(), 1500);
        QCOMPARE(QToolTip::text(), song.title);
        auto *tooltip = visibleTooltip();
        QTest::qWait(100);
        QTRY_VERIFY_WITH_TIMEOUT(!tooltip->grab().isNull(), 1200);
        const auto image = tooltip->grab().toImage();
        const auto background = qApp->palette().color(QPalette::Button);
        const auto foreground = qApp->palette().color(QPalette::Text);
        QCOMPARE(QToolTip::palette().color(QPalette::ToolTipBase), background);
        QCOMPARE(QToolTip::palette().color(QPalette::ToolTipText), foreground);
        capture(tooltip, QString("popup-tooltip-%1.png").arg(theme == 0 ? "light" : "dark"));
        qInfo() << tooltip->metaObject()->className() << tooltip->size() << image.size() << image.pixelColor(image.width()/2,qRound(5*image.devicePixelRatio()));
        const int sampleY = qRound(5 * image.devicePixelRatio());
        QCOMPARE(image.pixelColor(image.width() / 2, sampleY).rgb(), background.rgb());
        QVERIFY(matchingPixels(image, background) > image.width() * 4);
        QVERIFY(matchingPixels(image, foreground) > 20);
        capture(tooltip, QString("popup-tooltip-%1.png").arg(theme == 0 ? "light" : "dark"));
    }
    void childMenuUsesRoundedOpaqueSurface_data() { themeRows(); }
    void childMenuUsesRoundedOpaqueSurface() {
        QFETCH(int, theme);
        QTemporaryDir directory;
        Library library(directory.filePath("data"));
        QVERIFY(library.open());
        library.setSetting("theme", theme);
        Window window(&library);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))window.playback()->media()->setAudioOutput(nullptr);
        window.show();
        QTest::qWait(60);
        RoundedMenu menu(&window);
        menu.addAction("播放");
        QMenu *child = menu.addMenu("加入歌单");
        child->addAction("主歌单");
        auto *create = child->addAction("创建新歌单并加入…");
        QMenu *iconChild = menu.addMenu(symbol("playlist"), "其他歌单");
        iconChild->addAction("收藏的歌单");
        QVERIFY(qobject_cast<RoundedMenu *>(child));
        QVERIFY(qobject_cast<RoundedMenu *>(iconChild));
        QVERIFY(child->property("roundedPopup").toBool());
        menu.popup(window.mapToGlobal(QPoint(360, 220)));
        QTRY_VERIFY(menu.isVisible());
        QTest::mouseMove(&menu, QPoint(3, 3));
        QTest::mouseMove(&menu, menu.actionGeometry(child->menuAction()).center());
        menu.setActiveAction(child->menuAction());
        QTest::keyClick(&menu, Qt::Key_Right);
        QTRY_VERIFY_WITH_TIMEOUT(child->isVisible(), 1800);
        const auto image = child->grab().toImage();
        const auto background = qApp->palette().color(QPalette::Button);
        const auto foreground = qApp->palette().color(QPalette::Text);
        QCOMPARE(image.pixelColor(image.width() / 2, qRound(3 * image.devicePixelRatio())).rgb(), background.rgb());
        QVERIFY(image.pixelColor(0, 0).alpha() < 40);
        QVERIFY(matchingPixels(image, background) > image.width() * 4);
        QVERIFY(matchingPixels(image, foreground) > 20);
        capture(child, QString("popup-submenu-%1.png").arg(theme == 0 ? "light" : "dark"));
        QSignalSpy triggered(create, &QAction::triggered);
        QTest::mouseClick(child, Qt::LeftButton, Qt::NoModifier, child->actionGeometry(create).center());
        QCOMPARE(triggered.size(), 1);
        menu.hide();
    }
    void tooltipThemePreservesOtherApplicationRules() {
        const QString savedStyle = qApp->styleSheet();
        const QVariant savedRule = qApp->property("relaxerTooltipStyle");
        qApp->setStyleSheet("QWidget#popupTestSentinel{margin:3px;}");
        QPalette palette;
        palette.setColor(QPalette::Button, QColor("#f0e2d5"));
        palette.setColor(QPalette::Text, QColor("#372820"));
        palette.setColor(QPalette::Mid, QColor("#baa48e"));
        applyPopupTheme(palette);
        applyPopupTheme(palette);
        QVERIFY(qApp->styleSheet().contains("QWidget#popupTestSentinel{margin:3px;}"));
        QCOMPARE(qApp->styleSheet().count("QToolTip{"), 1);
        QCOMPARE(QToolTip::palette().color(QPalette::ToolTipBase), QColor("#f0e2d5"));
        qApp->setStyleSheet(savedStyle);
        qApp->setProperty("relaxerTooltipStyle", savedRule);
    }
};
QTEST_MAIN(PopupTests)
#include "popup-tests.moc"