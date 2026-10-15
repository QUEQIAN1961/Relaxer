#include <QFontDatabase>
#include <QMediaDevices>
#include <QAudioDevice>
#include "window.h"
#include <QComboBox>
#include <QImage>
#include <QJsonArray>
#include <QMouseEvent>
#include <QProcess>
#include <QStyleOptionSlider>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
class GuiTests : public QObject {
    Q_OBJECT
    static QPoint handleCenter(Slider *slider) {
        QStyleOptionSlider option;
        option.initFrom(slider);
        option.orientation = slider->orientation();
        option.minimum = slider->minimum();
        option.maximum = slider->maximum();
        option.sliderPosition = slider->sliderPosition();
        option.sliderValue = slider->value();
        option.upsideDown = slider->invertedAppearance() ^ (slider->layoutDirection() == Qt::RightToLeft);
        return slider->style()
            ->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, slider)
            .center();
    }
    static void movePointer(QWidget *widget, const QPoint &position) {
        QMouseEvent event(QEvent::MouseMove, position, widget->mapToGlobal(position), Qt::NoButton,
                          Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(widget, &event);
    }
    static bool makeAudio(const QString &file) {
        QProcess ff;
        ff.start(toolPath("ffmpeg"),
                 {"-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:duration=12", "-y", file});
        return ff.waitForFinished(20000) && ff.exitCode() == 0;
    }
  private slots:
    void initTestCase(){const auto font=qEnvironmentVariable("RELAXER_QA_FONT");if(!font.isEmpty())QVERIFY(QFontDatabase::addApplicationFont(font)>=0);}
    void oldCoversRepairedAtStartup() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto source = dir.filePath("song.wav");
        QVERIFY(makeAudio(source));
        auto imported = Importer::importOne(library.root(), source);
        QVERIFY(imported.error.isEmpty());
        imported.track.cover = "Artwork/" + imported.track.hash + ".png";
        imported.track.coverOrigin.clear();
        QImage wrong(64, 64, QImage::Format_RGB32);
        wrong.fill(Qt::blue);
        QVERIFY(wrong.save(library.absolute(imported.track.cover)));
        const auto id = library.insert(imported.track);
        const auto list = library.createPlaylist("Preserve playlist");
        QVERIFY(library.addToPlaylist(list, {id}));
        auto session = library.startSession(id, 1000);
        QVERIFY(library.saveSession(session, 800, 80));
        Window window(&library);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))window.playback()->media()->setAudioOutput(nullptr);
        window.show();
        QTRY_COMPARE_WITH_TIMEOUT(library.track(id).coverOrigin, QString("none"), 6000);
        QVERIFY(library.track(id).cover.isEmpty());
        QCOMPARE(library.track(id).plays, 1);
        QCOMPARE(library.track(id).heard, 800);
        QCOMPARE(library.tracks(list).first().id, id);
        // Closing during a pending background repair must safely join its worker.
        imported.track.hash = "pending-close";
        imported.track.cover = "Artwork/pending-close.png";
        library.insert(imported.track);
        {
            Window closing(&library);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))closing.playback()->media()->setAudioOutput(nullptr);
            QCoreApplication::processEvents();
        }
    }
    void playGlyphCentered() {
        CircleButton button("play", "test");
        button.setObjectName("playButton");
        button.setFixedSize(42, 42);
        button.show();
        for (const auto &name : QStringList{"play", "pause"}) {
            button.setSymbol(name, Qt::white);
            QTest::qWait(20);
            const auto image = button.grab().toImage();
            const qreal scale = image.devicePixelRatio();
            qreal sumX = 0, sumY = 0;
            int count = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const qreal lx = (x + .5) / scale, ly = (y + .5) / scale;
                    if (qHypot(lx - 21, ly - 21) > 19)
                        continue;
                    const auto color = image.pixelColor(x, y);
                    if (color.red() > 245 && color.green() > 245 && color.blue() > 245) {
                        sumX += lx;
                        sumY += ly;
                        ++count;
                    }
                }
            QVERIFY(count > 20);
            QVERIFY2(qAbs(sumX / count - 21) < 1.3 && qAbs(sumY / count - 21) < 1.3,
                     qPrintable(QString("%1 glyph center=(%2,%3), scale=%4")
                                    .arg(name)
                                    .arg(sumX / count)
                                    .arg(sumY / count)
                                    .arg(scale)));
        }
    }
    void sliderDragging_data() {
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("maximum");
        QTest::addColumn<bool>("rtl");
        QTest::newRow("volume") << 80 << 100 << false;
        QTest::newRow("seek") << 500 << 240000 << false;
        QTest::newRow("rtl") << 300 << 100 << true;
    }
    void sliderDragging() {
        QFETCH(int, width);
        QFETCH(int, maximum);
        QFETCH(bool, rtl);
        Slider slider;
        slider.resize(width, 28);
        slider.setStyleSheet(
            "QSlider::groove:horizontal{height:4px;background:#ddd;}"
            "QSlider::handle:horizontal{width:12px;margin:-5px 0;background:#e5455f;border-radius:6px;}");
        slider.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        slider.setRange(0, maximum);
        slider.setValue(maximum / 2);
        slider.show();
        QTest::qWait(20);
        QSignalSpy pressed(&slider, &QSlider::sliderPressed);
        QSignalSpy released(&slider, &QSlider::sliderReleased);
        QSignalSpy moved(&slider, &QSlider::sliderMoved);
        const auto start = handleCenter(&slider) + QPoint(2, 0);
        QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, start);
        QVERIFY(slider.isSliderDown());
        QCOMPARE(slider.value(), maximum / 2);
        movePointer(&slider, QPoint(rtl ? width / 5 : width * 4 / 5, start.y()));
        QVERIFY(slider.value() > maximum * .7);
        QVERIFY(moved.size() > 0);
        QVERIFY(released.isEmpty());
        movePointer(&slider, QPoint(rtl ? -30 : width + 30, start.y()));
        QCOMPARE(slider.value(), maximum);
        QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier,
                            QPoint(rtl ? -30 : width + 30, start.y()));
        QVERIFY(!slider.isSliderDown());
        QCOMPARE(pressed.size(), 1);
        QCOMPARE(released.size(), 1);
        slider.setValue(maximum / 2);
        const auto stationary = handleCenter(&slider) + QPoint(2, 0);
        QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, stationary);
        QCOMPARE(slider.value(), maximum / 2);
        QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(rtl ? width - 1 : 0, 14));
        QCOMPARE(slider.value(), 0);
        slider.setTracking(false);
        QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(width / 2, 14));
        QVERIFY(slider.sliderPosition() > maximum * .4);
        QCOMPARE(slider.value(), 0);
        QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(width / 2, 14));
        QVERIFY(slider.value() > maximum * .4);
    }
    void playerBarStableAndRealSeek() {
        if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))QSKIP("当前测试会话的声卡输出初始化失败；需在正常音频设备环境验证");
        QTemporaryDir d;
        Library library(d.filePath("data"));
        QVERIFY(library.open());
        QVERIFY(library.setSetting("volume", 0));
        const auto source = d.filePath("layout.wav");
        QVERIFY(makeAudio(source));
        auto imported = Importer::importOne(library.root(), source);
        QVERIFY(imported.error.isEmpty());
        imported.track.title = "A";
        imported.track.artist = "B";
        const auto shortId = library.insert(imported.track);
        auto longTrack = imported.track;
        longTrack.hash = "layout-long";
        longTrack.title = QString("Hotel California 用于验证长标题 ").repeated(20);
        longTrack.artist = QString("Very Long Artist ").repeated(20);
        const auto longId = library.insert(longTrack);
        Window window(&library);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))window.playback()->media()->setAudioOutput(nullptr);
        window.show();
        auto *play = window.findChild<CircleButton *>("playButton");
        auto *seek = window.findChild<Slider *>("seekSlider");
        auto *volume = window.findChild<Slider *>("volumeSlider");
        auto *bar = window.findChild<QWidget *>("playerBar");
        auto *title = window.findChild<QLabel *>("songTitle");
        QVERIFY(play && seek && volume && bar && title);
        const bool audioAvailable=!qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS") && !QMediaDevices::audioOutputs().isEmpty();
        const auto selectTrack=[&](qint64 id){if(audioAvailable)window.playback()->play({shortId,longId},id);else{library.setSetting("queue",QJsonArray{double(shortId),double(longId)});library.setSetting("current",double(id));window.playback()->restore();}};
        for (const int width : {1000, 1240, 1600}) {
            window.resize(width, 790);
            selectTrack(shortId);
            if(audioAvailable)QTRY_VERIFY_WITH_TIMEOUT(window.playback()->media()->isSeekable(),6000);
            window.playback()->media()->pause();
            QTest::qWait(20);
            const auto shortPosition = play->mapTo(bar, QPoint(0, 0));
            const auto seekRect = QRect(seek->mapTo(bar, QPoint(0, 0)), seek->size());
            selectTrack(longId);
            if(audioAvailable)QTRY_VERIFY_WITH_TIMEOUT(window.playback()->media()->isSeekable(),6000);
            window.playback()->media()->pause();
            QTest::qWait(20);
            QCOMPARE(play->mapTo(bar, QPoint(0, 0)), shortPosition);
            QCOMPARE(QRect(seek->mapTo(bar, QPoint(0, 0)), seek->size()), seekRect);
            QVERIFY(qAbs(shortPosition.x() + play->width() / 2. - bar->width() / 2.) <= 1);
            QCOMPARE(title->toolTip(), longTrack.title);
            QVERIFY(title->fontMetrics().horizontalAdvance(title->text()) <= title->width());
        }
        volume->setValue(30);
        auto volumeStart = handleCenter(volume);
        QTest::mousePress(volume, Qt::LeftButton, Qt::NoModifier, volumeStart);
        movePointer(volume, QPoint(volume->width() - 2, volumeStart.y()));
        QVERIFY(window.playback()->volume() > 90);
        QTest::mouseRelease(volume, Qt::LeftButton, Qt::NoModifier,
                            QPoint(volume->width() - 2, volumeStart.y()));
        window.playback()->setVolume(0);
        if(!audioAvailable)QSKIP("布局和音量拖动已验证；实际音频跳转需要音频输出设备");
        QSignalSpy release(seek, &QSlider::sliderReleased);
        const auto before = window.playback()->position();
        const auto start = handleCenter(seek);
        QTest::mousePress(seek, Qt::LeftButton, Qt::NoModifier, start);
        movePointer(seek, QPoint(seek->width() * 3 / 4, start.y()));
        QCOMPARE(window.playback()->position(), before);
        QVERIFY(seek->isSliderDown());
        QTest::mouseRelease(seek, Qt::LeftButton, Qt::NoModifier, QPoint(seek->width() * 3 / 4, start.y()));
        QCOMPARE(release.size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(window.playback()->position() > 7000, 4000);
        window.playback()->save();
        const auto session = library.session(qint64(library.setting("activeSession").toDouble()));
        QVERIFY(session.sought);
        QCOMPARE(library.track(longId).plays, 0);
    }
    void navigationAndThemes() {
        QTemporaryDir d;
        Library l(d.path());
        QVERIFY(l.open());
        Track t;
        t.hash = "gui";
        t.file = "Music/gui.wav";
        t.title = "a very long title 用于验证布局";
        t.artist = "Test Artist";
        t.album = "Tests";
        t.duration = 180000;
        auto id = l.insert(t);
        auto p = l.createPlaylist("测试歌单");
        QVERIFY(l.addToPlaylist(p, {id}));
        Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);
        w.show();
        QTest::qWait(100);
        auto *table = w.findChild<SongTable *>("songTable");
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 1);
        auto *search = w.findChild<QLineEdit *>("search");
        QVERIFY(search);
        search->setText("nonexistent");
        QCOMPARE(table->rowCount(), 0);
        search->clear();
        QCOMPARE(table->rowCount(), 1);
        w.showPlaylists();
        w.showProfile();
        w.showSettings();
        auto *theme = w.findChild<QComboBox *>("themeSetting");
        QVERIFY(theme);
        theme->setCurrentIndex(1);
        QCOMPARE(l.setting("theme").toInt(), 1);
        theme->setCurrentIndex(0);
        w.showNowPlaying();
        QTest::qWait(80);
        QVERIFY(w.grab().width() >= 1000);
        w.showLibrary();
        QVERIFY(table->isVisible());
    }
    void percentageRange() {
        QTemporaryDir d;
        Library l(d.path());
        QVERIFY(l.open());
        Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);
        auto *spin = w.findChild<QSpinBox *>("thresholdSetting");
        QVERIFY(spin);
        QCOMPARE(spin->minimum(), 70);
        QCOMPARE(spin->maximum(), 100);
        spin->setValue(90);
        QCOMPARE(l.setting("threshold").toInt(), 90);
    }
};
QTEST_MAIN(GuiTests)
#include "gui-tests.moc"
