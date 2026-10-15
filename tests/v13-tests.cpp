#include "window.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QHeaderView>
#include <QSqlError>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
using namespace relaxer;
class V13Tests:public QObject {
    Q_OBJECT
    static Track seed(Library &library,const QString &name,const QString &hash){
        Track t;t.hash=hash;t.file="Music/"+hash+".wav";t.title=name;t.artist="Relaxer";t.album="本地音乐";t.duration=10000;t.coverOrigin="none";
        QFile f(library.absolute(t.file));if(f.open(QIODevice::WriteOnly))f.resize(8120000);
        t.id=library.insert(t);return library.track(t.id);
    }
    static void capture(QWidget &widget,const QString &name){const auto dir=qEnvironmentVariable("RELAXER_QA_DIR");if(!dir.isEmpty()){QVERIFY(QDir().mkpath(dir));QVERIFY(widget.grab().save(dir+"/"+name));}}
    static void openPlaylist(Window &w,qint64 id){auto *nav=w.findChild<QListWidget *>("playlistNavigation");for(int i=0;i<nav->count();++i)if(nav->item(i)->data(Qt::UserRole).toLongLong()==id){QTest::mouseClick(nav->viewport(),Qt::LeftButton,Qt::NoModifier,nav->visualItemRect(nav->item(i)).center());return;}QFAIL("Playlist navigation missing");}
private slots:
    void initTestCase(){qApp->setStyle("Fusion");const auto font=qEnvironmentVariable("RELAXER_QA_FONT");if(!font.isEmpty())QVERIFY(QFontDatabase::addApplicationFont(font)>=0);}
    void legacyDatabaseMigrationAndPersistence(){
        QTemporaryDir dir;const auto root=dir.filePath("data");QVERIFY(QDir().mkpath(root));
        const auto connection=QString("legacy-v13");{
            auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(root+"/relaxer.sqlite");QVERIFY(db.open());QSqlQuery q(db);
            QVERIFY(q.exec("CREATE TABLE tracks(id INTEGER PRIMARY KEY,hash TEXT UNIQUE,file TEXT,title TEXT,artist TEXT,album TEXT,cover TEXT,lyrics TEXT,duration INTEGER,plays INTEGER,heard INTEGER,favorite INTEGER,added TEXT)"));
            QVERIFY(q.exec("INSERT INTO tracks VALUES(17,'old','Music/old.wav','旧歌曲','歌手','专辑','','',10000,7,8123,1,'2026-10-12 08:00:00')"));
            QVERIFY(q.exec("CREATE TABLE playlists(id INTEGER PRIMARY KEY,name TEXT,cover TEXT)"));
            QVERIFY(q.exec("INSERT INTO playlists VALUES(9,'原来的歌单','')"));
            QVERIFY(q.exec("CREATE TABLE playlist_tracks(playlist INTEGER REFERENCES playlists(id) ON DELETE CASCADE,track INTEGER REFERENCES tracks(id) ON DELETE CASCADE,position INTEGER,PRIMARY KEY(playlist,track))"));
            QVERIFY(q.exec("INSERT INTO playlist_tracks VALUES(9,17,0)"));
            QVERIFY(q.exec("CREATE TABLE settings(key TEXT PRIMARY KEY,value TEXT)"));QVERIFY(q.exec("INSERT INTO settings VALUES('theme','[1]')"));
        }QSqlDatabase::removeDatabase(connection);
        {Library l(root);QVERIFY(l.open());const auto t=l.track(17);QCOMPARE(t.plays,qint64(7));QCOMPARE(t.heard,qint64(8123));QVERIFY(t.favorite);QCOMPARE(t.importedAt,QString("2026-10-12 08:00:00"));QCOMPARE(l.setting("theme").toInt(),1);
            const auto p=l.playlists().first();QCOMPARE(p.id,qint64(9));QCOMPARE(p.count,1);QCOMPARE(p.plays,qint64(7));QVERIFY(p.tags.isEmpty());QVERIFY(p.description.isEmpty());QCOMPARE(l.tracks(9).first().id,qint64(17));
            QVERIFY(l.playlistTags(9,{"ACG","民谣","民谣"}));QVERIFY(l.playlistDescription(9,"旧资料库升级后\n保存歌单简介"));QCOMPARE(l.insert(t),qint64(17));QCOMPARE(l.track(17).importedAt,t.importedAt);
        }
        {Library l(root);QVERIFY(l.open());QCOMPARE(l.playlists().first().tags,QStringList({"民谣","ACG"}));QCOMPARE(l.playlists().first().description,QString("旧资料库升级后\n保存歌单简介"));QCOMPARE(l.tracks(9).first().plays,qint64(7));}
    }
    void metadataValidationAndPlayCount(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"晨光","a"),b=seed(l,"夜晚","b");const auto id=l.createPlaylist("测试");
        QVERIFY(l.addToPlaylist(id,{a.id,b.id}));QVERIFY(!l.playlistTags(id,{}));QVERIFY(!l.playlistTags(id,{"不存在"}));QVERIFY(l.playlistTags(id,playlistTagOptions()));QVERIFY(!l.playlistTags(999,{"流行"}));
        QVERIFY(l.playlistDescription(id,QString(1000,QChar(u'中'))));QVERIFY(!l.playlistDescription(id,QString(1001,QChar(u'中'))));QCOMPARE(characterCount(l.playlists().first().description),1000);
        auto one=l.startSession(a.id,10000),two=l.startSession(b.id,10000);QVERIFY(l.saveSession(one,8000,80));QVERIFY(l.saveSession(two,8000,80));QCOMPARE(l.playlists().first().plays,qint64(2));
        one.sought=true;QVERIFY(l.saveSession(one,0,80));QCOMPARE(l.playlists().first().plays,qint64(1));QVERIFY(l.removeFromPlaylist(id,{b.id}));QCOMPARE(l.playlists().first().plays,qint64(0));
        QVERIFY(l.playlistDescription(id,{}));QVERIFY(l.deletePlaylist(id));QVERIFY(!l.playlistDescription(id,"不能写入已删除的歌单"));QVERIFY(l.playlists().isEmpty());
    }
    void descriptionCharacterLimitAndUndo(){
        const QString emoji=QString::fromUtf8("👨‍👩‍👧‍👦");const QString combining=QString::fromUtf8("é");QCOMPARE(characterCount(emoji+combining+"中"),3);
        PlaylistDescriptionDialog dialog("已有文字");dialog.show();auto *editor=dialog.findChild<QPlainTextEdit *>();auto *remaining=dialog.findChild<QLabel *>("descriptionRemaining");
        QCOMPARE(remaining->text(),QString("还可输入 996 字 / 1000 字"));editor->selectAll();editor->insertPlainText(QString(999,QChar(u'中'))+emoji+"超限");
        QCOMPARE(characterCount(dialog.description()),1000);QVERIFY(dialog.description().endsWith(emoji));QCOMPARE(remaining->text(),QString("还可输入 0 字 / 1000 字"));
        editor->undo();QCOMPARE(dialog.description(),QString("已有文字"));QCOMPARE(remaining->text(),QString("还可输入 996 字 / 1000 字"));
        editor->selectAll();editor->insertPlainText("一行\n二行");QCOMPARE(characterCount(dialog.description()),5);capture(dialog,"05-description-editor.png");
    }
    void tagsDialogMinimumAndAll(){PlaylistTagsDialog dialog({});dialog.show();auto *buttons=dialog.findChild<QDialogButtonBox *>();QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());QCOMPARE(dialog.findChildren<QCheckBox *>().size(),10);
        for(auto *check:dialog.findChildren<QCheckBox *>())check->setChecked(true);QCOMPARE(dialog.selectedTags(),playlistTagOptions());QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());capture(dialog,"04-tags-editor.png");
        for(auto *check:dialog.findChildren<QCheckBox *>())check->setChecked(false);QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());}
    void chromeAndTheme_data(){QTest::addColumn<int>("theme");QTest::newRow("light")<<0;QTest::newRow("dark")<<1;}
    void chromeAndTheme(){QFETCH(int,theme);QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());l.setSetting("theme",theme);seed(l,"晨光","a");Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(40);
        QVERIFY(w.windowFlags().testFlag(Qt::FramelessWindowHint));QVERIFY(!w.findChild<QLabel *>("windowCaption"));QVERIFY(!w.findChild<QWidget *>("windowTopBar"));
        auto *controls=w.findChild<TitleBar *>("windowControls");QVERIFY(controls);QCOMPARE(controls->size(),QSize(138,34));QCOMPARE(w.centralWidget()->layout()->indexOf(controls),-1);
        QCOMPARE(w.findChild<QWidget *>("contentHost")->mapTo(&w,QPoint()).y(),0);QCOMPARE(controls->mapTo(&w,QPoint()).x()+controls->width(),w.width());
        auto *themeBox=w.findChild<RoundedComboBox *>("themeSetting");QVERIFY(themeBox);themeBox->setCurrentIndex(1-theme);QTest::qWait(30);
        const auto image=w.grab().toImage();const QColor color=qApp->palette().color(QPalette::Window);const auto dpr=image.devicePixelRatio();
        QCOMPARE(image.pixelColor(image.width()-2,qRound(100*dpr)).rgb(),color.rgb());
        capture(w,theme==1?"01-local-light.png":"02-local-dark.png");
    }
    void navigationAndFavoriteConsistency(){
        QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto a=seed(l,"晨光","a"),b=seed(l,"夜晚","b");const auto id=l.createPlaylist("轻松一刻");QVERIFY(l.addToPlaylist(id,{a.id}));Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(40);
        auto *nav=w.findChild<QListWidget *>("navigation");auto *lists=w.findChild<QListWidget *>("playlistNavigation");auto *pages=w.findChild<QStackedWidget *>("pages");auto *table=w.findChild<SongTable *>("songTable");QCOMPARE(nav->count(),5);
        for(int i=1;i<nav->count();++i)QCOMPARE(nav->visualItemRect(nav->item(i)).height(),nav->visualItemRect(nav->item(0)).height());
        openPlaylist(w,id);QCOMPARE(nav->currentRow(),-1);QCOMPARE(lists->currentRow(),0);QCOMPARE(table->ids(),QVector<qint64>{a.id});
        QTest::mouseClick(nav->viewport(),Qt::LeftButton,Qt::NoModifier,nav->visualItemRect(nav->item(0)).center());QCOMPARE(nav->currentRow(),0);QCOMPARE(lists->currentRow(),-1);QCOMPARE(table->rowCount(),2);
        w.findChild<QLineEdit *>("search")->setText("不存在");QCOMPARE(table->rowCount(),0);QTest::mouseClick(nav->viewport(),Qt::LeftButton,Qt::NoModifier,nav->visualItemRect(nav->item(0)).center());QCOMPARE(table->rowCount(),2);QVERIFY(w.findChild<QLineEdit *>("search")->text().isEmpty());
        const auto clickHeart=[&](SongTable *t){QTest::mouseClick(t->viewport(),Qt::LeftButton,Qt::NoModifier,t->visualItemRect(t->item(0,SongTable::FavoriteColumn)).center());};
        const auto favoriteId=table->ids().first();clickHeart(table);QVERIFY(l.track(favoriteId).favorite);
        QTest::mouseClick(nav->viewport(),Qt::LeftButton,Qt::NoModifier,nav->visualItemRect(nav->item(4)).center());QCOMPARE(nav->currentRow(),4);QCOMPARE(table->ids(),QVector<qint64>{favoriteId});clickHeart(table);QVERIFY(!l.track(favoriteId).favorite);QCOMPARE(table->rowCount(),0);
        auto session=l.startSession(b.id,10000);QVERIFY(session.id);w.showProfile();QCOMPARE(pages->currentIndex(),2);QCOMPARE(nav->currentRow(),2);auto *recent=w.findChild<SongTable *>("recentTable");QCOMPARE(recent->ids(),QVector<qint64>{b.id});clickHeart(recent);QVERIFY(l.track(b.id).favorite);
        w.showNowPlaying();QCOMPARE(nav->currentRow(),2);w.showVersionLog();QCOMPARE(nav->currentRow(),3);w.showLibrary();QCOMPARE(table->columnsMode(),SongTable::LocalMetadata);
    }
    void localMetadataSurvivesStatisticsRefresh(){QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"晨光","a");Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(30);auto *table=w.findChild<SongTable *>("songTable");
        QCOMPARE(table->columnCount(),8);QCOMPARE(table->horizontalHeaderItem(SongTable::FavoriteColumn)->text(),QString());QCOMPARE(table->horizontalHeaderItem(SongTable::DetailsColumn)->text(),QString("文件大小"));
        QCOMPARE(table->item(0,SongTable::DetailsColumn)->text(),QString("8.12MB"));QVERIFY(QDate::fromString(table->item(0,SongTable::ImportDateColumn)->text(),"yyyy-MM-dd").isValid());
        auto session=l.startSession(t.id,10000);QVERIFY(l.saveSession(session,8000,80));QTest::qWait(1300);QCOMPARE(table->item(0,SongTable::DetailsColumn)->text(),QString("8.12MB"));
        const auto id=l.createPlaylist("晚风");QVERIFY(l.addToPlaylist(id,{t.id}));openPlaylist(w,id);QCOMPARE(table->columnCount(),7);QCOMPARE(table->horizontalHeaderItem(SongTable::DetailsColumn)->text(),QString("播放次数"));QCOMPARE(table->item(0,SongTable::DetailsColumn)->text(),QString("1"));
        QCOMPARE(SongTable::fileSizeText(-1),QString("—"));QCOMPARE(SongTable::fileSizeText(0),QString("0.00MB"));QCOMPARE(SongTable::importDateText("2026-10-12"),QString("2026-10-12"));
    }
    void selectedPlayingTitleContrast(){
        SongTable table;table.resize(1000,230);Track t;t.id=1;t.title="正在播放的歌曲";t.coverOrigin="none";table.setSongs({t},{},1);table.show();QTest::qWait(20);
        QPalette pal=table.palette();pal.setColor(QPalette::Highlight,QColor("#e5455f"));pal.setColor(QPalette::HighlightedText,Qt::white);table.setPalette(pal);
        const auto titleRect=table.visualItemRect(table.item(0,SongTable::TitleColumn)).adjusted(72,0,-2,0);
        const auto count=[&](const QColor &target){const auto img=table.viewport()->grab(titleRect).toImage();int result=0;for(int y=0;y<img.height();++y)for(int x=0;x<img.width();++x){const auto c=img.pixelColor(x,y);if(qAbs(c.red()-target.red())<15&&qAbs(c.green()-target.green())<15&&qAbs(c.blue()-target.blue())<15)++result;}return result;};
        QVERIFY(count(QColor("#e5455f"))>20);table.selectRow(0);QTest::qWait(20);QVERIFY(count(Qt::white)>20);capture(table,"06-selected-playing.png");
    }
    void playlistFourRowGeometry_data(){QTest::addColumn<int>("width");QTest::addColumn<QString>("description");for(int width:{1000,1240,1600}){QTest::newRow(qPrintable(QString("one-line-%1").arg(width)))<<width<<QString("慢慢听，让生活放松一点。");QTest::newRow(qPrintable(QString("two-line-%1").arg(width)))<<width<<QString("一起听音乐，收藏那些温柔的时刻。").repeated(30);}}
    void playlistFourRowGeometry(){QFETCH(int,width);QFETCH(QString,description);QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"夜晚","a");const auto id=l.createPlaylist("晚风与回忆");QVERIFY(l.addToPlaylist(id,{t.id}));QVERIFY(l.playlistTags(id,{"民谣","轻音乐"}));QVERIFY(l.playlistDescription(id,description));Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.resize(width,790);w.show();QTest::qWait(40);openPlaylist(w,id);QTest::qWait(40);
        auto *header=w.findChild<PlaylistHeader *>();QVERIFY(header->isVisible());QCOMPARE(header->cover()->size(),QSize(160,160));QVERIFY(!w.findChild<QWidget *>("libraryActions")->isVisible());
        QList<QWidget *> rows;for(int i=0;i<4;++i)rows<<header->findChild<QWidget *>(QString("playlistRow%1").arg(i));const int step=rows[1]->y()-rows[0]->y();QCOMPARE(rows[0]->y(),0);QCOMPARE(rows[2]->y()-rows[1]->y(),step);QCOMPARE(rows[3]->y()-rows[2]->y(),step);
        auto *preview=header->findChild<DescriptionPreview *>();QCOMPARE(preview->mapTo(header,QPoint()).y()+preview->height(),160);QVERIFY(preview->displayLines().size()<=2);
        if(description.size()>100){QCOMPARE(preview->displayLines().size(),2);QVERIFY(preview->isElided());QVERIFY(preview->displayLines().last().endsWith(QChar(0x2026)));}else{QCOMPARE(preview->displayLines().size(),1);QVERIFY(!preview->isElided());}
        if(width==1240)capture(w,description.size()>100?"03-playlist-two-lines.png":"03-playlist-one-line.png");
    }
    void playlistMetadataSaveCancelAndLiveStats(){QTemporaryDir dir;Library l(dir.filePath("data"));QVERIFY(l.open());const auto t=seed(l,"晨光","a");const auto id=l.createPlaylist("测试歌单");QVERIFY(l.addToPlaylist(id,{t.id}));Window w(&l);if(qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))w.playback()->media()->setAudioOutput(nullptr);w.show();QTest::qWait(40);openPlaylist(w,id);
        auto *tagLink=w.findChild<QPushButton *>("playlistTagsLink");auto *descriptionLink=w.findChild<QPushButton *>("playlistDescriptionLink");
        QTimer::singleShot(0,&w,[]{auto *dialog=qobject_cast<PlaylistTagsDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);for(auto *check:dialog->findChildren<QCheckBox *>())check->setChecked(true);dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();});tagLink->click();QCOMPARE(l.playlists().first().tags,playlistTagOptions());
        QTimer::singleShot(0,&w,[]{auto *dialog=qobject_cast<PlaylistDescriptionDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->findChild<QPlainTextEdit *>()->setPlainText("保存简介");dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();});descriptionLink->click();QCOMPARE(l.playlists().first().description,QString("保存简介"));
        QTimer::singleShot(0,&w,[]{auto *dialog=qobject_cast<PlaylistDescriptionDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->findChild<QPlainTextEdit *>()->setPlainText("取消这次编辑");dialog->reject();});descriptionLink->click();QCOMPARE(l.playlists().first().description,QString("保存简介"));
        QTimer::singleShot(0,&w,[]{auto *dialog=qobject_cast<PlaylistTagsDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);for(auto *check:dialog->findChildren<QCheckBox *>())check->setChecked(false);dialog->reject();});tagLink->click();QCOMPARE(l.playlists().first().tags,playlistTagOptions());
        auto session=l.startSession(t.id,10000);QVERIFY(l.saveSession(session,8000,80));auto *header=w.findChild<PlaylistHeader *>();QTRY_VERIFY_WITH_TIMEOUT(header->findChild<QLabel *>("muted")->text().contains("1 次播放"),2500);
        w.findChild<QLineEdit *>("search")->setText("不匹配");QVERIFY(header->findChild<QLabel *>("muted")->text().contains("1 首歌曲"));
    }
};
QTEST_MAIN(V13Tests)
#include "v13-tests.moc"
