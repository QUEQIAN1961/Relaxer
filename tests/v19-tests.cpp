#include "window.h"
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QStyleOptionSlider>
#include <QDataStream>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QToolTip>
#include <QHelpEvent>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>
#include "lookup-drive.h"
using namespace relaxer;
class V19Server : public QTcpServer {
public:
    bool hold = false;
    QByteArray answer;
    int requests = 0;
    V19Server() {
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                const auto bytes = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", bytes);
                if (!bytes.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
                socket->setProperty("answered", true);
                ++requests;
                if (hold) return;
                if(answer.isEmpty())socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}");
                else socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(answer.size())+"\r\nConnection: close\r\n\r\n"+answer);
                socket->disconnectFromHost();
            });
        });
    }
    QUrl endpoint() const { return QUrl(QString("http://127.0.0.1:%1/api").arg(serverPort())); }
};
class V19Tests : public QObject {
    Q_OBJECT
    static Track seed(Library &library, const QString &name, const QString &hash) {
        Track t;
        t.title=name; t.artist="歌手"; t.album="专辑"; t.hash=hash;
        t.file="Music/"+hash+".wav";t.duration=10000;t.coverOrigin="none";
        QFile file(library.absolute(t.file));
        if (!file.open(QIODevice::WriteOnly)) return {};
        QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData("RIFF",4);out<<quint32(160036);out.writeRawData("WAVEfmt ",8);
        out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);
        out.writeRawData("data",4);out<<quint32(160000);out.writeRawData(QByteArray(160000,'\0').constData(),160000);
        file.close();t.id=library.insert(t);return library.track(t.id);
    }
    static void capture(QWidget &widget, const QString &name) {
        const auto dir=qEnvironmentVariable("RELAXER_QA_DIR");
        if(!dir.isEmpty()){QDir().mkpath(dir);widget.grab().save(dir+"/"+name);}
    }
    static void choosePrompt(const QString &choice) {
        QTimer::singleShot(0,[choice]{
            for(auto *widget:QApplication::topLevelWidgets())
                if(auto *box=qobject_cast<QMessageBox *>(widget))
                    if(box->objectName()=="appearanceUnsavedPrompt")
                        for(auto *button:box->buttons())if(button->text()==choice){button->click();return;}
        });
    }
private slots:
    void initTestCase(){qApp->setStyle("Fusion");}
    void unsavedAppearanceChoices_data(){QTest::addColumn<QString>("choice");for(const auto &choice:{QString("不保存"),QString("保存"),QString("取消")})QTest::newRow(qPrintable(choice))<<choice;}
    void unsavedAppearanceChoices(){
        QFETCH(QString,choice);QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());
        AppearanceStore store(&library);QVERIFY(store.save(AppearanceStore::defaults(),{}));const auto before=store.state();
        Window w(&library);w.playback()->media()->setAudioOutput(nullptr);w.show();w.showSettings();QTest::qWait(300);
        w.findChild<QPushButton *>("editAppearance")->click();auto *page=w.findChild<AppearancePage *>();QVERIFY(page);QTest::qWait(300);
        auto *pages=w.findChild<QStackedWidget *>("pages");QCOMPARE(pages->currentWidget(),page);QVERIFY(!page->isWindow());
        for(auto *widget:QApplication::topLevelWidgets())QVERIFY(widget!=page);
        page->findChild<BlurPercentSpin *>("appearanceBlurValueExpanded")->setValue(32.5);
        choosePrompt(choice);page->findChild<QPushButton *>("appearanceBack")->click();
        if(choice=="取消"){
            QCOMPARE(pages->currentWidget(),page);QCOMPARE(store.state(),before);
            choosePrompt("不保存");page->findChild<QPushButton *>("appearanceBack")->click();
        }
        QCOMPARE(pages->currentWidget()->objectName(),QString("settingsPage"));QTest::qWait(150);
        if(choice=="保存"){
            QCOMPARE(store.state()["items"].toArray().size(),2);
            QVERIFY(store.active()["id"]!=before["active"]);QCOMPARE(store.active()["blurExpanded"].toDouble(),32.5);
        }else QCOMPARE(store.state(),before);
        QCOMPARE(w.findChild<Backdrop *>()->property("expandedBackgroundBlur").toDouble(),store.active()["blurExpanded"].toDouble(15));
    }
    void presetsAndCoverBackgroundRule(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());l.setSetting("playerCoverBackground",false);
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.show();w.showSettings();QTest::qWait(300);
        auto *cover=w.findChild<QPushButton *>("playerCoverBackground");QVERIFY(cover->isChecked());QVERIFY(!cover->isEnabled());
        QVERIFY(cover->text().contains("展开播放时使用封面背景"));QVERIFY(!w.findChild<Backdrop *>()->property("usingCustomBackground").toBool());
        w.findChild<QPushButton *>("editAppearance")->click();auto *page=w.findChild<AppearancePage *>();QTest::qWait(300);
        QVERIFY(page->findChild<QPushButton *>("appearanceSavePreset")->isVisible());QVERIFY(!page->findChild<QPushButton *>("appearanceNewSave")->isVisible());
        page->findChild<QPushButton *>("appearanceMode3")->click();QVERIFY(cover->isEnabled());QVERIFY(!cover->isChecked());
        page->findChild<QPushButton *>("appearanceMode1")->click();QVERIFY(!cover->isEnabled());QVERIFY(cover->isChecked());
        page->findChild<QPushButton *>("appearanceMode3")->click();QVERIFY(cover->isEnabled());QVERIFY(!cover->isChecked());
        page->findChild<QPushButton *>("appearanceMode2")->click();page->findChild<QPushButton *>("appearanceSavePreset")->click();
        QCOMPARE(l.setting("theme").toInt(),2);QCOMPARE(AppearanceStore(&l).state()["items"].toArray().size(),0);QVERIFY(!l.setting("playerCoverBackground").toBool());
    }
    void imageHistoryAndThreeIndependentBlurControls(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());AppearanceStore store(&l);
        QImage image(240,180,QImage::Format_RGB32);
        for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)image.setPixelColor(x,y,((x/4+y/4)%2)?QColor("#93c5c8"):QColor("#dc7c86"));
        const auto left=dir.filePath("left.png"),right=dir.filePath("right.png");QVERIFY(image.save(left));image.invertPixels();QVERIFY(image.save(right));
        auto theme=AppearanceStore::defaults();theme["mode"]="image";theme["sourceLeft"]=left;theme["shareLR"]=true;theme["blurLeft"]=50;theme["blurRight"]=2;
        theme.remove("blurExpanded");QVERIFY(store.save(theme,{}));QCOMPARE(store.active()["blurExpanded"].toDouble(),15.);
        theme["shareLR"]=false;theme["sourceRight"]=right;QVERIFY(store.save(theme,{}));l.setSetting("playerCoverBackground",false);
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.show();w.showSettings();QTest::qWait(300);
        w.findChild<QPushButton *>("editAppearance")->click();auto *page=w.findChild<AppearancePage *>();QTest::qWait(300);
        const auto rows=page->findChildren<QWidget *>("appearanceHistoryItem");QCOMPARE(rows.size(),2);
        QCOMPARE(rows[0]->findChildren<QLabel *>("appearanceHistoryImage").size(),1);QCOMPARE(rows[1]->findChildren<QLabel *>("appearanceHistoryImage").size(),2);
        for(auto *row:rows)for(const auto &key:{QString("left"),QString("right"),QString("accent")})QVERIFY(row->findChild<QLabel *>("appearanceHistoryColor-"+key));
        auto *leftGroup=page->findChild<QWidget *>("appearanceBlurGroupLeft");auto *rightGroup=page->findChild<QWidget *>("appearanceBlurGroupRight");auto *expandedGroup=page->findChild<QWidget *>("appearanceBlurGroupExpanded");
        QCOMPARE(leftGroup->y(),rightGroup->y());QCOMPARE(rightGroup->y(),expandedGroup->y());QVERIFY(qAbs(leftGroup->width()-expandedGroup->width())<=1);
        for(int width:{1000,1240}){w.resize(width,790);QTest::qWait(80);QVERIFY(expandedGroup->geometry().right()<expandedGroup->parentWidget()->width());}
        capture(w,"v19-r3-appearance.png");
        auto *surface=w.findChild<Surface *>("sidebar");const auto oldSidebar=surface->grab();
        auto *backdrop=w.findChild<Backdrop *>();QPixmap oldBackground(backdrop->size());oldBackground.fill(Qt::transparent);backdrop->render(&oldBackground,QPoint(),QRegion(),QWidget::DrawWindowBackground);
        page->findChild<BlurPercentSpin *>("appearanceBlurValueExpanded")->setValue(40);QTest::qWait(160);
        QCOMPARE(backdrop->property("expandedBackgroundBlur").toDouble(),40.);QCOMPARE(surface->grab().toImage(),oldSidebar.toImage());
        QCOMPARE(page->findChild<BlurPercentSpin *>("appearanceBlurValueLeft")->value(),50.);QCOMPARE(page->findChild<BlurPercentSpin *>("appearanceBlurValueRight")->value(),2.);
        QPixmap newBackground(backdrop->size());newBackground.fill(Qt::transparent);backdrop->render(&newBackground,QPoint(),QRegion(),QWidget::DrawWindowBackground);QVERIFY(oldBackground.toImage()!=newBackground.toImage());
        page->findChild<QPushButton *>("appearanceNewSave")->click();QCOMPARE(store.active()["blurExpanded"].toDouble(),40.);
        w.showNowPlaying();QTest::qWait(500);capture(w,"v19-r3-custom-background.png");
    }
    void compactQueueSameWidthAndNoIdleTint(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto track=seed(l,"歌曲名称超过此前八字限制仍按剩余宽度显示","queue");
        QWidget root;root.resize(460,360);root.setAutoFillBackground(true);auto palette=root.palette();palette.setColor(QPalette::Window,QColor("#7892a3"));root.setPalette(palette);root.show();
        NextQueuePanel panel(&l,false,&root);panel.setTracks({track});panel.setAnchor(QPoint(100,330),panel.preferredWidth());panel.present(true,false);QTest::qWait(30);
        const int width=panel.width();const auto first=panel.grab().toImage();QCOMPARE(first.pixelColor(40,4),QColor("#7892a3"));
        QVERIFY(panel.artwork(0).artistSmall.isNull());QVERIFY(panel.artwork(0).titleFrom.width()>QFontMetrics(panel.font()).horizontalAdvance("歌曲名称"));
        QTest::mouseMove(&panel,QPoint(45,4));QVERIFY(panel.grab().toImage().pixelColor(40,4)!=first.pixelColor(40,4));
        QEvent leave(QEvent::Leave);QApplication::sendEvent(&panel,&leave);QCOMPARE(panel.grab().toImage().pixelColor(40,4),first.pixelColor(40,4));
        capture(root,"v19-r3-queue-collapsed.png");panel.setExpanded(true,false);QCOMPARE(panel.preferredWidth(),width);QCOMPARE(panel.width(),width);capture(root,"v19-r3-queue-expanded.png");
    }
    void progressColorMigrationAndPersistence(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());AppearanceStore store(&l);
        auto legacy=AppearanceStore::defaults();legacy["id"]="legacy";legacy["name"]="已有方案";
        l.setSetting("appearanceHistory",QJsonObject{{"active","legacy"},{"items",QJsonArray{legacy}}});
        QVERIFY(store.initializeProgressColors());const auto migrated=store.active();
        QCOMPARE(store.state()["items"].toArray().size(),1);QCOMPARE(migrated["id"].toString(),QString("legacy"));
        QVERIFY(QColor(migrated["recommendedProgressFill"].toString()).isValid());
        QVERIFY(QColor(migrated["recommendedProgressTrack"].toString()).isValid());
        QVERIFY(!migrated["customProgressColors"].toBool());
        QVERIFY(store.initializeProgressColors());QCOMPARE(store.active(),migrated);
        auto dark=legacy;dark["right"]="#080808";const auto darkColors=store.withProgressColors(dark,true);
        QVERIFY(QColor(darkColors["recommendedProgressFill"].toString()).lightness()>150);
        QVERIFY(QColor(migrated["recommendedProgressFill"].toString()).lightness()<150);
        auto manual=store.draft(migrated);manual["customProgressColors"]=true;manual["progressColorsCustomized"]=true;
        manual["progressFill"]="#123abc";manual["progressTrack"]="#e8bb31";
        QVERIFY(store.save(manual,"legacy"));QCOMPARE(store.active()["progressFill"].toString(),QString("#123abc"));
        manual=store.draft(store.active());manual["customProgressColors"]=false;manual["right"]="#080808";
        QVERIFY(store.save(manual,"legacy"));QCOMPARE(store.active()["progressFill"].toString(),QString("#123abc"));
        QCOMPARE(store.active()["recommendedProgressFill"],darkColors["recommendedProgressFill"]);
        Library reopened(dir.filePath("data"));QVERIFY(reopened.open());QCOMPARE(AppearanceStore(&reopened).active(),store.active());
    }
    void progressColorControlsPreviewRollbackAndSave(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());AppearanceStore store(&l);
        QVERIFY(store.save(AppearanceStore::defaults(),{}));const auto original=store.active();
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();w.showSettings();QTest::qWait(300);
        w.findChild<QPushButton *>("editAppearance")->click();auto *page=w.findChild<AppearancePage *>();QVERIFY(page);QTest::qWait(300);
        auto *enabled=page->findChild<QCheckBox *>("appearanceCustomProgressColors");
        auto *fill=page->findChild<QPushButton *>("appearanceColor-progressFill");
        auto *track=page->findChild<QPushButton *>("appearanceColor-progressTrack");
        QVERIFY(enabled);QVERIFY(!enabled->isChecked());QVERIFY(!fill->isEnabled());QVERIFY(!track->isEnabled());
        enabled->setChecked(true);QVERIFY(fill->isEnabled());QVERIFY(track->isEnabled());
        const auto choose=[](const QColor &color){QTimer::singleShot(0,[color]{auto *dialog=qobject_cast<QColorDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->setCurrentColor(color);dialog->accept();});};
        choose(QColor("#1578b8"));fill->click();choose(QColor("#f4c969"));track->click();
        const auto verifyPixels=[&](const QColor &filled,const QColor &empty){
            for(const auto &name:{QString("seekSlider"),QString("volumeSlider")}){
                auto *slider=w.findChild<Slider *>(name);QVERIFY(slider);QSignalBlocker block(slider);slider->setRange(0,100);slider->setValue(50);
                const auto image=slider->grab().toImage();const auto ratio=slider->devicePixelRatioF();
                QCOMPARE(image.pixelColor(qRound(slider->width()*.25*ratio),qRound(slider->height()*.5*ratio)),filled);
                QCOMPARE(image.pixelColor(qRound(slider->width()*.75*ratio),qRound(slider->height()*.5*ratio)),empty);
            }
        };
        verifyPixels(QColor("#1578b8"),QColor("#f4c969"));
        enabled->setChecked(false);verifyPixels(QColor(original["recommendedProgressFill"].toString()),QColor(original["recommendedProgressTrack"].toString()));
        enabled->setChecked(true);verifyPixels(QColor("#1578b8"),QColor("#f4c969"));
        for(int width:{1000,1240}){w.resize(width,790);QTest::qWait(50);QVERIFY(track->geometry().right()<track->parentWidget()->width());}
        capture(w,"v19-r4-progress-colors.png");
        choosePrompt("不保存");page->findChild<QPushButton *>("appearanceBack")->click();QCOMPARE(store.active(),original);
        verifyPixels(QColor(original["recommendedProgressFill"].toString()),QColor(original["recommendedProgressTrack"].toString()));
        w.findChild<QPushButton *>("editAppearance")->click();
        page=qobject_cast<AppearancePage *>(w.findChild<QStackedWidget *>("pages")->currentWidget());QVERIFY(page);
        page->findChild<QCheckBox *>("appearanceCustomProgressColors")->setChecked(true);
        choose(QColor("#1578b8"));page->findChild<QPushButton *>("appearanceColor-progressFill")->click();
        choose(QColor("#f4c969"));page->findChild<QPushButton *>("appearanceColor-progressTrack")->click();
        page->findChild<QPushButton *>("appearanceNewSave")->click();
        QVERIFY(store.active()["customProgressColors"].toBool());QCOMPARE(store.active()["progressFill"].toString(),QString("#1578b8"));
        w.showNowPlaying();QTest::qWait(500);verifyPixels(QColor("#1578b8"),QColor("#f4c969"));
        w.showSettings();QTest::qWait(500);w.findChild<QPushButton *>("editAppearance")->click();
        page=qobject_cast<AppearancePage *>(w.findChild<QStackedWidget *>("pages")->currentWidget());QVERIFY(page);
        page->findChild<QPushButton *>("appearanceMode1")->click();
        QVERIFY(!page->findChild<QWidget *>("appearanceCustomOptions")->isVisible());
        QVERIFY(!w.styleSheet().contains("QSlider#seekSlider"));
        page->findChild<QPushButton *>("appearanceMode3")->click();verifyPixels(QColor("#1578b8"),QColor("#f4c969"));
        page->findChild<QPushButton *>("appearanceMode4")->click();QVERIFY(page->findChild<QCheckBox *>("appearanceCustomProgressColors")->isEnabled());
        choosePrompt("不保存");page->findChild<QPushButton *>("appearanceBack")->click();
    }
    void expandedQueueShowsOnlyOneLargerSong(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());
        const auto a=seed(l,"第一首完整显示歌曲","one-a"),b=seed(l,"第二首应在展开后显示","one-b");
        QWidget root;root.resize(600,360);root.show();NextQueuePanel panel(&l,true,&root);
        panel.setTracks({a,b});panel.setAnchor(QPoint(8,350),panel.preferredWidth());panel.present(true,false);
        QCOMPARE(panel.height(),94);const auto first=panel.artwork(0);QCOMPARE(first.coverFrom.width(),40.);
        QVERIFY(first.coverFrom.bottom()<panel.geometry().bottom());
        QSignalSpy activated(&panel,&NextQueuePanel::activated);
        QTest::mouseClick(&panel,Qt::LeftButton,Qt::NoModifier,QPoint(90,70));QCOMPARE(activated.size(),1);QCOMPARE(activated[0][0].toInt(),0);
        QTest::mouseClick(&panel,Qt::LeftButton,Qt::NoModifier,QPoint(90,93));QCOMPARE(activated.size(),1);
        capture(root,"v19-r4-one-song.png");panel.setExpanded(true,false);
        const auto second=panel.artwork(1);QCOMPARE(second.coverFrom.width(),36.);
        QTest::mouseClick(&panel,Qt::LeftButton,Qt::NoModifier,second.coverFrom.center().toPoint()-panel.pos());
        QCOMPARE(activated.size(),2);QCOMPARE(activated[1][0].toInt(),1);
    }
    void queueOutsideClickAndNoTooltip_data(){
        QTest::addColumn<bool>("large");QTest::newRow("left")<<true;QTest::newRow("right")<<false;
    }
    void queueOutsideClickAndNoTooltip(){
        QFETCH(bool,large);QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());
        QVector<Track> tracks;for(int i=0;i<6;++i)tracks<<seed(l,QString("歌曲标题%1足够长会省略").arg(i),QString("queue-r5-%1").arg(i));
        QWidget root;root.resize(640,500);QPushButton other("其他按钮",&root);other.setGeometry(500,20,120,36);
        root.show();NextQueuePanel panel(&l,large,&root);panel.setTracks(tracks);panel.setAnchor(QPoint(8,480),panel.preferredWidth());panel.present(true,false);
        QFont font=panel.font();font.setPixelSize(13);QFontMetrics metrics(font);
        if(large)QCOMPARE(panel.artwork(0).titleFrom.width(),qreal(metrics.horizontalAdvance(QString(8,QChar(0x6b4c)))));
        for(bool expanded:{false,true}){
            panel.setExpanded(expanded,false);QToolTip::hideText();
            const auto artwork=panel.artwork(0);const auto cover=artwork.coverFrom.toRect().translated(-panel.pos());
            QVERIFY(panel.rect().adjusted(4,4,-4,-4).contains(cover));
            if(!large&&expanded){QCOMPARE(cover.width(),26);QVERIFY(cover.top()>=34+4);}
            QTest::mouseMove(&panel,cover.center());QHelpEvent help(QEvent::ToolTip,cover.center(),panel.mapToGlobal(cover.center()));
            QApplication::sendEvent(&panel,&help);QTest::qWait(20);QVERIFY(!QToolTip::isVisible());
            QHelpEvent headerHelp(QEvent::ToolTip,QPoint(40,12),panel.mapToGlobal(QPoint(40,12)));QApplication::sendEvent(&panel,&headerHelp);QVERIFY(!QToolTip::isVisible());
        }
        QSignalSpy activated(&panel,&NextQueuePanel::activated);
        QTest::mouseClick(&panel,Qt::LeftButton,Qt::NoModifier,panel.artwork(0).coverFrom.center().toPoint()-panel.pos());
        QCOMPARE(activated.size(),1);QVERIFY(panel.expanded());
        QTest::mouseClick(&root,Qt::LeftButton,Qt::NoModifier,QPoint(400,100));QVERIFY(!panel.expanded());QTRY_VERIFY(!panel.property("queueAnimating").toBool());
        panel.setExpanded(true,false);QSignalSpy clicked(&other,&QPushButton::clicked);QTest::mouseClick(&other,Qt::LeftButton);
        QCOMPARE(clicked.size(),1);QVERIFY(!panel.expanded());QTRY_VERIFY(!panel.property("queueAnimating").toBool());
        capture(root,large?"v19-r5-left-queue.png":"v19-r5-right-queue.png");
    }
    void volumeIconChangesAndSurvivesThemeRefresh(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());l.setSetting("volume",0);
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(100);
        auto *slider=w.findChild<Slider *>("volumeSlider");auto *volume=w.findChild<IconButton *>("volumeButton");QVERIFY(slider);QVERIFY(volume);
        QMap<int,QString> levels{{0,"volumeMuted"},{1,"volumeLow"},{33,"volumeLow"},{34,"volumeMedium"},{66,"volumeMedium"},{67,"volumeHigh"},{100,"volumeHigh"}};
        QVector<QImage> icons;
        for(auto it=levels.begin();it!=levels.end();++it){
            slider->setValue(it.key());QCOMPARE(volume->property("symbolName").toString(),it.value());QCOMPARE(l.setting("volume").toInt(),it.key());
            if(it.key()==0||it.key()==33||it.key()==66||it.key()==100)icons<<volume->icon().pixmap(22,22).toImage();
        }
        QCOMPARE(icons.size(),4);for(int i=0;i<icons.size();++i)for(int j=i+1;j<icons.size();++j)QVERIFY(icons[i]!=icons[j]);
        volume->click();QCOMPARE(slider->value(),0);QCOMPARE(volume->property("symbolName").toString(),QString("volumeMuted"));
        volume->click();QCOMPARE(slider->value(),65);QCOMPARE(volume->property("symbolName").toString(),QString("volumeMedium"));
        w.showSettings();QTest::qWait(300);w.findChild<QPushButton *>("editAppearance")->click();QTest::qWait(300);
        auto *page=qobject_cast<AppearancePage *>(w.findChild<QStackedWidget *>("pages")->currentWidget());QVERIFY(page);
        page->findChild<QPushButton *>("appearanceMode1")->click();QCOMPARE(volume->property("symbolName").toString(),QString("volumeMedium"));
        slider->setValue(0);page->findChild<QPushButton *>("appearanceMode0")->click();QCOMPARE(volume->property("symbolName").toString(),QString("volumeMuted"));
        choosePrompt("不保存");page->findChild<QPushButton *>("appearanceBack")->click();
    }
    void localMusicSummaryTotalsBytesAndTracksSearch(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"第一首","size-a"),b=seed(l,"第二首","size-b"),missing=seed(l,"缺失文件","size-missing");
        QVERIFY(QFile::remove(l.absolute(missing.file)));const qint64 bytes=QFileInfo(l.absolute(a.file)).size()+QFileInfo(l.absolute(b.file)).size();
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();QTest::qWait(100);
        const auto summary=[&]()->QString{for(auto *label:w.findChildren<QLabel *>())if(label->text().contains("本地音乐资料库"))return label->text();return {};};
        QCOMPARE(summary(),QString("3 首歌曲  ·  %1  ·  本地音乐资料库").arg(SongTable::fileSizeText(bytes)));
        auto *search=w.findChild<QLineEdit *>("search");QVERIFY(search);search->setText("第一首");
        QCOMPARE(summary(),QString("1 首歌曲  ·  %1  ·  本地音乐资料库").arg(SongTable::fileSizeText(QFileInfo(l.absolute(a.file)).size())));
        search->setText("不存在的歌");QCOMPARE(summary(),QString("0 首歌曲  ·  0.00MB  ·  本地音乐资料库"));
        search->clear();QCOMPARE(summary(),QString("3 首歌曲  ·  %1  ·  本地音乐资料库").arg(SongTable::fileSizeText(bytes)));
        capture(w,"v19-r5-local-size.png");
    }
    void playbackToggleAndContinuousVolumeMotion(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();w.showProfile();QTest::qWait(400);
        auto *volume=w.findChild<QWidget *>("volumeControls");IconButton *toggle=nullptr;
        for(auto *button:volume->findChildren<IconButton *>())if(button->property("playbackToggle").toBool())toggle=button;
        QVERIFY(toggle);auto *pages=w.findChild<QStackedWidget *>("pages");const int originalPage=pages->currentIndex();
        QCOMPARE(toggle->property("symbolName").toString(),QString("expand"));const auto openIcon=toggle->icon().pixmap(22,22).toImage();
        const QPoint initial=volume->pos();QTest::mouseClick(toggle,Qt::LeftButton);QCOMPARE(volume->pos(),initial);
        QCOMPARE(toggle->property("symbolName").toString(),QString("contract"));QVERIFY(toggle->icon().pixmap(22,22).toImage()!=openIcon);
        const int target=(volume->parentWidget()->height()-volume->height())/2;QVERIFY(target>initial.y());
        QTest::qWait(70);QVERIFY(volume->y()>initial.y());QVERIFY(volume->y()<target);
        const QPoint middle=volume->pos();QTest::mouseClick(toggle,Qt::LeftButton);QCOMPARE(volume->pos(),middle);
        QCOMPARE(toggle->property("symbolName").toString(),QString("expand"));QTest::qWait(500);QCOMPARE(volume->y(),0);
        QVERIFY(!w.findChild<Backdrop *>()->isVisible());QCOMPARE(pages->currentIndex(),originalPage);
        QTest::mouseClick(toggle,Qt::LeftButton);QTest::qWait(500);QCOMPARE(volume->y(),target);QVERIFY(w.findChild<Backdrop *>()->isVisible());
        capture(w,"v19-r6-expanded-player.png");
        QTest::keyClick(&w,Qt::Key_Escape);QTest::qWait(500);QCOMPARE(volume->y(),0);QCOMPARE(toggle->property("symbolName").toString(),QString("expand"));
    }
    void centralPlayerButtonsHaveNoHoverTips(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(100);
        auto *center=w.findChild<QWidget *>("playerCenter");const auto buttons=center->findChildren<QAbstractButton *>();QCOMPARE(buttons.size(),5);
        for(auto *button:buttons){
            QVERIFY(button->toolTip().isEmpty());QVERIFY(!button->accessibleName().isEmpty());QToolTip::hideText();
            QTest::mouseMove(button,button->rect().center());QHelpEvent help(QEvent::ToolTip,button->rect().center(),button->mapToGlobal(button->rect().center()));
            QApplication::sendEvent(button,&help);QVERIFY(!QToolTip::isVisible());
        }
        for(auto mode:{Mode::Sequential,Mode::ListLoop,Mode::SingleLoop,Mode::Shuffle})w.playback()->setMode(mode);
        for(auto *button:buttons)QVERIFY(button->toolTip().isEmpty());
    }
    void onlyActiveLyricGrowsAndPreviousReturnsToNormal(){
        LyricView view;view.resize(540,360);auto palette=view.palette();palette.setColor(QPalette::Text,Qt::black);palette.setColor(QPalette::Window,Qt::white);view.setPalette(palette);view.setAutoFillBackground(true);
        view.setLyrics(Lyrics::parse(QByteArray("[00:01.00]MMMM\n[00:05.00]NNNN")));view.show();view.setPosition(0);view.browseFraction(0,true);QTest::qWait(30);
        const auto snapshot=[&]{return view.grab().toImage().copy(82,view.height()/2-36,view.width()-110,72);};
        const auto bounds=[](const QImage &image){QRect result;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto c=image.pixelColor(x,y);if(c.red()<240&&c.green()<240&&c.blue()<240)result=result.united(QRect(x,y,1,1));}return result;};
        const auto normal=snapshot();const auto ordinary=bounds(normal);QVERIFY(!ordinary.isEmpty());
        view.setPosition(1000);QCOMPARE(view.property("activeLyric").toInt(),0);const auto active=snapshot();const auto bigger=bounds(active);
        QVERIFY(bigger.width()>ordinary.width()*1.12);QVERIFY(bigger.width()<ordinary.width()*1.28);QVERIFY(bigger.height()>ordinary.height());
        capture(view,"v19-r6-current-lyric.png");view.setPosition(6000);QCOMPARE(view.property("activeLyric").toInt(),1);
        QCOMPARE(view.property("manualLyric").toInt(),0);QCOMPARE(snapshot(),normal);
        view.setLyrics(Lyrics::parse(QString("[00:01.00]%1\n[00:05.00]下一句").arg(QString(60,QChar(0x6b4c))).toUtf8()));view.setPosition(2000);QTest::qWait(400);
        QSignalSpy seek(&view,&LyricView::seekRequested);QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,view.rect().center());QCOMPARE(seek.size(),1);QCOMPARE(seek[0][0].toLongLong(),qint64(1000));
    }
    void manualRetryAfterNetworkTimeoutContactsSource(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto track=seed(l,"重试歌曲","retry-timeout");
        V19Server server;QVERIFY(server.listen(QHostAddress::LocalHost));server.hold=true;
        LyricsLookup dialog(&l,{track.id},nullptr,server.endpoint());dialog.setProperty("lyricsRequestTimeoutMs",80);dialog.setProperty("lyricsSearchBudgetMs",700);
        dialog.show();relaxerClickSearch(&dialog);auto *tracks=dialog.findChild<QListWidget *>("lyricsTracks");tracks->setCurrentRow(0);
        QTRY_VERIFY_WITH_TIMEOUT(server.requests>0,3000);QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled(),3000);
        const int before=server.requests;server.hold=false;
        server.answer=QJsonDocument(QJsonObject{{"id",42},{"trackName",track.title},{"artistName",track.artist},{"albumName",track.album},{"duration",10},{"syncedLyrics","[00:00.00]first\n[00:02.00]second"}}).toJson(QJsonDocument::Compact);
        dialog.findChild<QPushButton *>("lyricsManualSearch")->click();
        QTRY_VERIFY_WITH_TIMEOUT(server.requests>before,1500);
        QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QListWidget *>("lyricsVersions")->count()>0,3000);
        QVERIFY(dialog.findChild<QPushButton *>("lyricsApply")->isEnabled());
    }
    void manualRetryAfterConnectionFailureContactsSource(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto track=seed(l,"连接重试","retry-connection");
        V19Server server;QVERIFY(server.listen(QHostAddress::LocalHost));const auto endpoint=server.endpoint();const auto port=server.serverPort();server.close();
        LyricsLookup dialog(&l,{track.id},nullptr,endpoint);dialog.setProperty("lyricsSearchBudgetMs",1000);dialog.show();relaxerClickSearch(&dialog);
        auto *tracks=dialog.findChild<QListWidget *>("lyricsTracks");tracks->setCurrentRow(0);QTest::qWait(200);
        QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled(),3000);
        QVERIFY(server.listen(QHostAddress::LocalHost,port));
        server.answer=QJsonDocument(QJsonObject{{"id",44},{"trackName",track.title},{"artistName",track.artist},{"duration",10},{"syncedLyrics","[00:00.00]first\n[00:02.00]second"}}).toJson(QJsonDocument::Compact);
        dialog.findChild<QPushButton *>("lyricsManualSearch")->click();
        QTRY_VERIFY_WITH_TIMEOUT(server.requests>0,2000);QTRY_VERIFY(dialog.findChild<QListWidget *>("lyricsVersions")->count()>0);
    }
    void sourceSlotsBoundAndQueuedSongsKeepTheirBudget(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());V19Server server;QVERIFY(server.listen(QHostAddress::LocalHost));server.hold=true;
        QVector<qint64> ids;for(int i=0;i<10;++i)ids<<seed(l,QString("排队歌曲 %1").arg(i),QString("queued%1").arg(i)).id;
        LyricsLookup dialog(&l,ids,nullptr,server.endpoint());dialog.findChild<QCheckBox *>("lyricsReplace")->setChecked(true);
        dialog.setProperty("lyricsRequestTimeoutMs",1000);dialog.setProperty("lyricsSearchBudgetMs",200);dialog.show();relaxerClickSearch(&dialog);
        QTRY_VERIFY_WITH_TIMEOUT(server.requests>0,3000);QVERIFY(dialog.findChildren<QNetworkReply *>().size()<=6);
        QTest::qWait(240);auto *tracks=dialog.findChild<QListWidget *>("lyricsTracks");int searching=0;
        for(int i=0;i<tracks->count();++i)searching+=tracks->item(i)->data(Qt::UserRole+1).toInt()==1;
        QVERIFY(searching>0);QVERIFY(dialog.findChild<QPushButton *>("lyricsStop")->isEnabled());
        dialog.findChild<QPushButton *>("lyricsStop")->click();
    }
    void ordinaryLyricsOfferTimedSearchAndFiveSecondAdjustmentWorks(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto track=seed(l,"延时修复","delay-fix");
        QVERIFY(l.saveLyrics(track.id,"first ordinary line\nsecond ordinary line"));l.setSetting("queue",QJsonArray{double(track.id)});l.setSetting("current",double(track.id));
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.show();w.showNowPlaying();QTest::qWait(500);
        auto *tools=w.findChild<LyricTools *>();auto *view=w.findChild<LyricView *>();auto *entry=w.findChild<QPushButton *>("lyricDelayEntry");auto *spin=w.findChild<BlurPercentSpin *>("lyricDelay");QVERIFY(!view->hasTimeline());
        QVERIFY(entry->isEnabled());QVERIFY(entry->text().contains("查找滚动歌词"));QVERIFY(!spin->isEnabled());
        QObject::disconnect(tools,nullptr,&w,nullptr);QSignalSpy requested(tools,&LyricTools::timedLyricsRequested);
        QTest::mouseMove(view,view->rect().center());QTRY_VERIFY(w.findChild<QPushButton *>("lyricToolsQuestion")->isVisible());
        w.findChild<QPushButton *>("lyricToolsQuestion")->click();entry->click();QCOMPARE(requested.size(),1);
        QVERIFY(l.saveLyrics(track.id,"[00:01.00]first timed line\n[00:04.00]second timed line"));QVERIFY(view->hasTimeline());QVERIFY(entry->isEnabled());QVERIFY(spin->isEnabled());
        QCOMPARE(spin->minimum(),-5.);QCOMPARE(spin->maximum(),5.);spin->setValue(5.);
        QCOMPARE(l.lyricOffset(track.id),5000);QCOMPARE(view->property("lyricOffsetMs").toInt(),5000);
        spin->setValue(-5.);QCOMPARE(l.lyricOffset(track.id),-5000);QCOMPARE(view->property("lyricOffsetMs").toInt(),-5000);
        QSignalSpy seek(view,&LyricView::seekRequested);QTest::qWait(400);QTest::mouseClick(view,Qt::LeftButton,Qt::NoModifier,view->rect().center());QCOMPARE(seek.size(),1);
        capture(w,"v19-lyrics-fix-delay.png");
    }
    void timedSearchDoesNotReplaceExistingLyricsWithPlainText(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto track=seed(l,"保留原歌词","timed-only");
        const QByteArray old("saved ordinary line\nsecond line");QVERIFY(l.saveLyrics(track.id,old));V19Server server;QVERIFY(server.listen(QHostAddress::LocalHost));
        server.answer=QJsonDocument(QJsonObject{{"id",12},{"trackName",track.title},{"artistName",track.artist},{"albumName",track.album},{"duration",10},{"plainLyrics","new ordinary line\nsecond line"}}).toJson(QJsonDocument::Compact);
        LyricsLookup dialog(&l,{track.id},nullptr,server.endpoint());dialog.setProperty("requireTimedLyrics",true);dialog.findChild<QCheckBox *>("lyricsReplace")->setChecked(true);dialog.show();dialog.findChild<QCheckBox *>("lyricsMissingOnly")->setChecked(false);relaxerClickSearch(&dialog);
        QTRY_VERIFY(server.requests>0);QTRY_VERIFY(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled());
        QFile file(l.absolute(l.track(track.id).lyrics));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),old);
        QVERIFY(dialog.findChild<QListWidget *>("lyricsTracks")->item(0)->data(Qt::UserRole+2).toString().contains("原有歌词已保留"));
    }
    void lyricsSearchTenSongsWithRemainingQueued(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());V19Server server;QVERIFY(server.listen(QHostAddress::LocalHost));server.hold=true;
        QVector<qint64> ids;for(int i=0;i<12;++i)ids<<seed(l,QString("并发歌曲 %1").arg(i),QString("parallel%1").arg(i)).id;
        LyricsLookup dialog(&l,ids,nullptr,server.endpoint());dialog.show();relaxerClickSearch(&dialog);QTRY_VERIFY_WITH_TIMEOUT(server.requests>0,5000);
        QCOMPARE(dialog.property("parallelTracks").toInt(),10);auto *tracks=dialog.findChild<QListWidget *>("lyricsTracks");
        int searching=0,waiting=0;for(int i=0;i<tracks->count();++i){const int state=tracks->item(i)->data(Qt::UserRole+1).toInt();searching+=state==1;waiting+=state==0;}
        QCOMPARE(searching,10);QCOMPARE(waiting,2);dialog.findChild<QPushButton *>("lyricsStop")->click();
    }
    void queueBackgroundRestoresWhenPlayerCollapses(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"当前歌曲","cache-a"),b=seed(l,"待播放歌曲","cache-b");
        l.setSetting("queue",QJsonArray{double(a.id),double(b.id)});l.setSetting("current",double(a.id));
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.resize(1240,790);w.show();w.showNowPlaying();QTest::qWait(500);
        w.findChild<QPushButton *>("playerCollapse")->click();QTRY_VERIFY(!w.findChild<QWidget *>("pageMotion")->isVisible());QTest::qWait(30);
        auto *compact=w.findChild<NextQueuePanel *>("nextQueueCompact");compact->setExpanded(true,false);
        QEvent leave(QEvent::Leave);QApplication::sendEvent(compact,&leave);
        const auto image=compact->grab().toImage();QCOMPARE(image.pixelColor(110,6),QColor(Qt::white));
    }
    void manualLyricsAdvanceIncludesFailedAndWraps(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());V19Server server;QVERIFY(server.listen(QHostAddress::LocalHost));
        QVector<qint64> ids;for(int i=0;i<4;++i)ids<<seed(l,QString("跳转歌曲 %1").arg(i),QString("jump%1").arg(i)).id;
        QVERIFY(l.saveLyrics(ids[0],QString("[00:00.00]已有歌词\n[00:02.00]第二行").toUtf8()));
        LyricsLookup dialog(&l,ids,nullptr,server.endpoint());dialog.show();relaxerClickSearch(&dialog);QTest::qWait(200);QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("lyricsStart")->isEnabled(),5000);
        auto *tracks=dialog.findChild<QListWidget *>("lyricsTracks");const auto rowFor=[&](int i){for(int r=0;r<tracks->count();++r)if(tracks->item(r)->data(Qt::UserRole).toLongLong()==ids[i])return r;return -1;};QCOMPARE(tracks->item(rowFor(0))->data(Qt::UserRole+1).toInt(),2);
        for(int i=1;i<4;++i)QCOMPARE(tracks->item(rowFor(i))->data(Qt::UserRole+1).toInt(),4);
        tracks->setCurrentRow(rowFor(3));
        QTimer::singleShot(0,[]{for(auto *widget:QApplication::topLevelWidgets())if(widget->objectName()=="lyricsPasteDialog"){
            widget->findChild<QPlainTextEdit *>("lyricsPasteText")->setPlainText("[00:00.00]人工歌词\n[00:02.00]人工歌词第二行");
            widget->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();}});
        dialog.findChild<QPushButton *>("lyricsPaste")->click();QCOMPARE(tracks->currentRow(),rowFor(1));
        dialog.findChild<QPushButton *>("lyricsMarkInstrumental")->click();QCOMPARE(tracks->currentRow(),rowFor(2));
        dialog.findChild<QPushButton *>("lyricsMarkInstrumental")->click();QCOMPARE(tracks->currentRow(),rowFor(2));
        for(int i=0;i<4;++i)QCOMPARE(tracks->item(rowFor(i))->data(Qt::UserRole+1).toInt(),2);
    }
    void expandedTextSwitchesImmediatelyAndCollapseTextIsClickable(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"原歌曲","a"),b=seed(l,"新歌曲","b"),c=seed(l,"再下一首","c");
        l.setSetting("queue",QJsonArray{double(a.id),double(b.id),double(c.id)});l.setSetting("current",double(a.id));
        Window w(&l);w.playback()->media()->setAudioOutput(nullptr);w.show();w.showNowPlaying();QTest::qWait(500);
        auto *title=w.findChild<QLabel *>("nowTitle");auto *artist=w.findChild<QLabel *>("nowArtist");auto *flight=w.findChild<ArtworkFlight *>();
        const auto titleRect=title->geometry();const auto coverRect=artworkRect(w.findChild<Cover *>("nowCover"),w.centralWidget());
        auto *large=w.findChild<NextQueuePanel *>("nextQueueLarge");QTest::mouseClick(large,Qt::LeftButton,Qt::NoModifier,QPoint(50,12));
        QVERIFY(flight->isVisible());QCOMPARE(title->text(),b.title);QVERIFY(!title->isHidden());QVERIFY(!artist->isHidden());QCOMPARE(title->geometry(),titleRect);
        QTest::qWait(120);QCOMPARE(title->geometry(),titleRect);QCOMPARE(artworkRect(w.findChild<Cover *>("nowCover"),w.centralWidget()),coverRect);
        QTRY_VERIFY(!flight->isVisible());capture(w,"v19-r3-expanded-player.png");
        auto *collapse=w.findChild<QPushButton *>("playerCollapse");QVERIFY(collapse);QTest::mouseClick(collapse,Qt::LeftButton,Qt::NoModifier,QPoint(collapse->width()-12,collapse->height()/2));
        QTest::qWait(500);QVERIFY(!w.findChild<Backdrop *>()->isVisible());
    }
};
QTEST_MAIN(V19Tests)
#include "v19-tests.moc"
