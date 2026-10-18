#include "window.h"
#include "release-version.h"
#include "lyrics-document-reader.h"
#include "changelog.h"
#include "client-frame-windows.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDropEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmapCache>
#include <QSaveFile>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyleHints>
#include <QVBoxLayout>
#include <QWindow>
#include <QMouseEvent>
#include <QShowEvent>
#include <QStyleHints>
#include <QScrollBar>
#include <QPropertyAnimation>
#include <QPainter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QTextCursor>
#include "smooth-scroll.h"
#include <QWheelEvent>
#include <QPainterPath>
#include <algorithm>
static void initializeResources(){Q_INIT_RESOURCE(resources);}
namespace relaxer {
static QString displayVersion() {
    const auto development = qApp->property("relaxerDevelopmentTitle").toString();
    return development.isEmpty() ? QString::fromLatin1(release::WindowTitle) : development;
}
static QLabel *label(const QString &text, const QString &style, QWidget *parent = nullptr) {
    auto *l = new QLabel(text, parent);
    l->setObjectName(style);
    l->setTextFormat(Qt::PlainText);
    return l;
}
static QPushButton *button(const QString &text, const QString &type = "button") {
    auto *b = new QPushButton(text);
    b->setObjectName(type);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
static QPushButton *checkableSetting(const QString &caption,const QString &name,bool checked,QWidget *parent=nullptr){
    auto *button=new QPushButton(parent);button->setObjectName(name);button->setProperty("checkableSetting",true);button->setCheckable(true);button->setCursor(Qt::PointingHandCursor);
    const auto label=[button,caption](bool on){button->setText((on?QString("✓ "):QString())+caption);};QObject::connect(button,&QPushButton::toggled,button,label);button->setChecked(checked);label(checked);return button;
}
Window::Window(Library *l, QWidget *p) : QMainWindow(p), m_library(l), m_player(new PlaybackFacade(l, this)) {
    initializeResources();
    m_importCoordinator=new ImportCoordinator(m_library,this);
    m_importCoordinator->setObjectName("importCoordinator");
    m_aiRuntime = new ai::AiRuntime(m_library, this);
    installSmoothScrolling(qApp);
    setWindowTitle(displayVersion());
    setWindowFlag(Qt::FramelessWindowHint);
    qApp->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea,false);
#endif
    setWindowIcon(QIcon(":/icon.svg"));
    setMinimumSize(1000, 660);
    const auto available=QGuiApplication::primaryScreen()->availableGeometry().size();
    m_normalSize=QSize(qMax(1000,qMin(m_library->setting("windowWidth",1240).toInt(),available.width())),qMax(660,qMin(m_library->setting("windowHeight",790).toInt(),available.height())));
    resize(m_normalSize);
    m_sizeSaveTimer=new QTimer(this);m_sizeSaveTimer->setSingleShot(true);m_sizeSaveTimer->setInterval(350);connect(m_sizeSaveTimer,&QTimer::timeout,this,&Window::saveWindowSize);
    setAcceptDrops(true);
    auto *root = new QWidget;
    root->setObjectName("root");
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
    root->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea,false);
#endif
    auto *outer = new QVBoxLayout(root);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    m_contentHost = new QWidget;
    m_contentHost->setObjectName("contentHost");
    auto *content = new QHBoxLayout(m_contentHost);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);
    auto *side = m_sideSurface = new Surface;
    side->setObjectName("sidebar");
    side->setFixedWidth(205);
    buildSidebar(side);
    content->addWidget(side);
    m_shortcuts = new PlayerShortcuts(this);
    auto savedKeys = m_library->setting("shortcuts", PlayerShortcuts::defaults()).toObject();
    auto localKeys = savedKeys;
    localKeys["globalEnabled"] = false;
    m_shortcuts->apply(localKeys);
    m_shortcuts->apply(savedKeys);
    connect(m_shortcuts, &PlayerShortcuts::triggered, this, [this](PlayerAction action) {
        switch (action) {
        case PlayerAction::Toggle:
            m_player->toggle();
            break;
        case PlayerAction::Previous:
            switchTrack(-1);
            break;
        case PlayerAction::Next:
            switchTrack(1);
            break;
        case PlayerAction::VolumeUp:
            m_volume->setValue(qBound(0, m_player->volume() + 5, 100));
            break;
        case PlayerAction::VolumeDown:
            m_volume->setValue(qBound(0, m_player->volume() - 5, 100));
            break;
        }
    });
    m_pages = new QStackedWidget;
    m_pages->setObjectName("pages");
    m_pages->addWidget(buildLibrary());
    m_pages->addWidget(buildPlaylists());
    m_pages->addWidget(buildProfile());
    m_pages->addWidget(buildSettings());
    m_pages->addWidget(new QWidget);
    buildNowPlaying();m_nowPage->setParent(m_contentHost);m_nowPage->hide();
    m_pages->addWidget(buildVersionLog());
    m_paperSurface = new Surface;
    m_paperSurface->setObjectName("paperSurface");
    auto *paper = new QVBoxLayout(m_paperSurface);
    paper->setContentsMargins(0, 0, 0, 0);
    paper->addWidget(m_pages);
    content->addWidget(m_paperSurface, 1);
    outer->addWidget(m_contentHost, 1);
    auto *importPanel = new QWidget;
    auto *ip = new QHBoxLayout(importPanel);
    ip->setContentsMargins(22, 5, 22, 5);
    m_importLabel = label("", "muted");
    m_importProgress = new QProgressBar;
    m_importProgress->setFixedWidth(230);
    m_importProgress->setFixedHeight(10);
    m_importProgress->setTextVisible(false);
    m_cancel = button("取消导入", "plain");
    ip->addWidget(m_importLabel, 1);
    ip->addWidget(m_importProgress);
    ip->addWidget(m_cancel);
    importPanel->hide();
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        m_importCoordinator->cancel();
        m_cancel->setEnabled(false);
        m_importLabel->setText("正在结束当前文件…");
    });
    outer->addWidget(importPanel);
    importPanel->setObjectName("importPanel");
    connect(m_importCoordinator,&ImportCoordinator::progress,this,[this](quint64,int current,int total,const QString &name){
        m_importProgress->setRange(0,total);m_importProgress->setValue(current);
        if(!m_closePending)m_importLabel->setText(QString("导入 %1 / %2 · %3").arg(current).arg(total).arg(name));
    });
    connect(m_importCoordinator,&ImportCoordinator::finished,this,[this,importPanel](const ImportSummary &result){
        importPanel->hide();QPixmapCache::clear();refresh();
        const auto summary=QString(result.cancelled?"导入已取消：新增 %1 首，重复 %2 首，未导入 %3 首，跳过或未开始 %4 项。":"导入完成：新增 %1 首，重复 %2 首，未导入 %3 首，跳过或未开始 %4 项。")
            .arg(result.added).arg(result.duplicates).arg(result.failed).arg(result.skipped);
        if(m_closePending)return;
        toast(summary);
        if(result.failed||result.playlistFailures||!result.fatalError.isEmpty())
            QMessageBox::information(this,"导入结果",summary+"\n\n"+result.errors.join('\n')+(result.fatalError.isEmpty()?QString():"\n"+result.fatalError));
    });
    connect(m_importCoordinator,&ImportCoordinator::artworkUpdated,this,[this](qint64 id){if(m_player->current().id==id)refreshTrack();});
    connect(m_importCoordinator,&ImportCoordinator::artworkFinished,this,[this](int corrected){
        if(!corrected)return;
        QPixmapCache::clear();refresh();if(!m_closePending)toast(QString("已校正 %1 首歌曲的自动封面。").arg(corrected));
    });
    connect(m_importCoordinator,&ImportCoordinator::shutdownReady,this,[this]{if(m_closePending)QTimer::singleShot(0,this,[this]{close();});});
    m_bottomStack=new QStackedWidget; m_bottomStack->setObjectName("bottomStack");m_bottomStack->setFixedHeight(98);m_bottomStack->setAutoFillBackground(false);
    m_footerSurface=new Surface;m_footerSurface->setObjectName("footerSurface");auto *footerLayout=new QVBoxLayout(m_footerSurface);footerLayout->setContentsMargins(0,0,0,0);footerLayout->setSpacing(0);
    m_bottomStack->addWidget(buildPlayerBar());m_bottomStack->addWidget(buildBulkBar());footerLayout->addWidget(m_bottomStack);outer->addWidget(m_footerSurface);
    setCentralWidget(root);
    m_motion=new PageMotion(root);
    m_nextCompact=new NextQueuePanel(m_library,false,root);m_nextLarge=new NextQueuePanel(m_library,true,root);m_queueFlight=new ArtworkFlight(root);
    connect(m_nextCompact,&NextQueuePanel::activated,this,[this](int index){activateUpcoming(index,false);});
    connect(m_nextLarge,&NextQueuePanel::activated,this,[this](int index){activateUpcoming(index,true);});
    connect(m_nextCompact,&NextQueuePanel::widthHintChanged,this,&Window::updateQueueGeometry);
    connect(m_nextLarge,&NextQueuePanel::widthHintChanged,this,&Window::updateQueueGeometry);
    m_queueBackgroundTimer=new QTimer(this);m_queueBackgroundTimer->setSingleShot(true);m_queueBackgroundTimer->setInterval(100);
    connect(m_queueBackgroundTimer,&QTimer::timeout,this,[this]{if((m_motion&&m_motion->isVisible())||(m_queueFlight&&m_queueFlight->isVisible())){m_queueBackgroundTimer->start(450);return;}if(m_nextCompact->presented())m_nextCompact->invalidateBackground();if(m_nextLarge->presented())m_nextLarge->invalidateBackground();});
    m_topBar=new TitleBar(root);
    m_topBar->move(root->width()-m_topBar->width(),0);
    m_topBar->raise();
    statusBar()->setSizeGripEnabled(false);
    statusBar()->hide();
    m_toastTimer.setParent(this);
    connect(m_library, &Library::changed, this, &Window::refresh);
    connect(m_library, &Library::statsChanged, this, [this] {
        if (!m_statsPending) {
            m_statsPending = true;
            QTimer::singleShot(1200, this, [this] {
                m_statsPending = false;
                refreshStats();
            });
        }
    });
    connect(m_player, &PlaybackFacade::trackChanged, this, [this] {
        refreshTrack();
        const auto tracks=visibleTracks();
        m_songs->setSongs(tracks, m_library->root(), m_player->current().id);
        updateLibrarySummary(tracks);
    });
    connect(m_player, &PlaybackFacade::stateChanged, this, [this] {
        m_play->setSymbol(m_player->playing() ? "pause" : "play", autoInk(m_accent));
        QStringList modes = {"顺序播放", "列表循环", "单曲循环", "随机循环"};
        QStringList symbols = {"playlist", "repeat", "single", "shuffle"};
        m_mode->setSymbol(symbols[int(m_player->mode())], m_ink);
        m_mode->setAccessibleName(modes[int(m_player->mode())]);
        if(m_lastQueueMode!=m_player->mode()){m_lastQueueMode=m_player->mode();refreshQueue();}
    });
    connect(m_player, &PlaybackFacade::positionChanged, this, [this](qint64 pos, qint64 d) {
        m_time->setText(durationText(pos));
        m_duration->setText(durationText(d));
        if (!m_seek->isSliderDown()) {
            QSignalBlocker block(m_seek);
            m_seek->setRange(0, int(qMin<qint64>(d, 2147483647)));
            m_seek->setValue(int(qMin<qint64>(pos, 2147483647)));
        }
        m_seek->setEnabled(m_player->current().id && m_player->seekable());
        m_lyrics->setPosition(pos);
    });
    connect(m_player, &PlaybackFacade::error, this, [this](const QString &t) { toast(t); });
    connect(m_player, &PlaybackFacade::saveFailed, this, [this](const QString &t) { toast("播放状态未保存：" + t); });
    connect(m_player, &PlaybackFacade::seekableChanged, this,
            [this](bool) { m_seek->setEnabled(m_player->current().id && m_player->seekable()); });
    auto shortcut = [this](const QKeySequence &key, auto fn) {
        auto *a = new QAction(this);
        a->setShortcut(key);
        addAction(a);
        connect(a, &QAction::triggered, this, fn);
    };
    shortcut(QKeySequence("Ctrl+O"), [this] { chooseFiles(); });
    shortcut(QKeySequence("Ctrl+Shift+O"), [this] { chooseFolder(); });
    shortcut(QKeySequence("Ctrl+F"), [this] {
        showLibrary();
        m_search->setFocus();
    });
    shortcut(QKeySequence(Qt::Key_MediaPlay), [this] {
        if (!m_player->playing())
            m_player->toggle();
    });
    shortcut(QKeySequence(Qt::Key_MediaPause), [this] {
        if (m_player->playing())
            m_player->toggle();
    });
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_tray = new QSystemTrayIcon(windowIcon(), this);
        m_tray->setToolTip("Relaxer");
        auto *menu = new RoundedMenu(this);
        connect(menu->addAction("打开 Relaxer"), &QAction::triggered, this, [this] {
            showNormal();
            raise();
            activateWindow();
        });
        connect(menu->addAction("播放 / 暂停"), &QAction::triggered, m_player, &PlaybackFacade::toggle);
        connect(menu->addAction("下一首"), &QAction::triggered, m_player, &PlaybackFacade::next);
        menu->addSeparator();
        connect(menu->addAction("退出"), &QAction::triggered, this, [this] {
            m_quitting = true;
            close();
        });
        m_tray->setContextMenu(menu);
        connect(m_tray, &QSystemTrayIcon::activated, this, [this](auto reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                showNormal();
                raise();
                activateWindow();
            }
        });
        m_tray->show();
    }
    check(AppearanceStore(m_library).initializeProgressColors());
    applyTheme();
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_library->setting("theme", 2).toInt() == 2 && m_appearancePreview.isEmpty())
            QTimer::singleShot(0, this, &Window::applyTheme);
    });
#endif
    refresh();
    m_player->restore();
    refreshTrack();
    m_player->setMode(m_player->mode());
    QTimer::singleShot(0, this, &Window::repairLegacyArtwork);
    QTimer::singleShot(0,this,[this]{m_restoringWindow=false;if(m_library->setting("windowMaximized",false).toBool())showMaximized();});
}
Window::~Window() {
    saveWindowSize();m_importCoordinator->shutdown();m_player->shutdown();
}
void Window::buildSidebar(QWidget *side) {
    auto *v = new QVBoxLayout(side);
    v->setContentsMargins(18, 27, 18, 20);
    v->setSpacing(14);
    auto *brand = new QWidget;
    brand->setObjectName("appBrand");
    brand->setProperty("windowDragHandle",true);
    auto *row = new QHBoxLayout(brand);
    row->setContentsMargins(8, 0, 0, 0);
    auto *icon = new QLabel;
    icon->setPixmap(QIcon(":/icon.svg").pixmap(30, 30));
    row->addWidget(icon);
    row->addWidget(label("Relaxer", "brand"));
    row->addStretch();
    v->addWidget(brand);
    v->addSpacing(16);
    v->addWidget(label("音乐资料库", "section"));
    m_nav = new QListWidget;
    m_nav->setObjectName("navigation");
    m_nav->setFocusPolicy(Qt::NoFocus);
    m_nav->setFixedHeight(220);
    m_nav->setIconSize(QSize(20,20));
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setSpacing(4);
    QStringList names = {"本地音乐", "歌单", "个人", "设置", "我喜欢"},
                icons = {"music", "playlist", "person", "settings", "heart"};
    for (int i = 0; i < names.size(); ++i) {
        auto *it = new QListWidgetItem(symbol(icons[i]), names[i], m_nav);
        it->setSizeHint(QSize(145, 36));
    }
    m_nav->setCurrentRow(0);
    v->addWidget(m_nav);
    const auto navigate=[this](QListWidgetItem *item){
        switch(m_nav->row(item)){
        case 0:showLibrary();break;
        case 1:showPlaylists();break;
        case 2:showProfile();break;
        case 3:showSettings();break;
        case 4:selectScope(-1);break;
        }
    };
    connect(m_nav,&QListWidget::itemClicked,this,navigate);
    connect(m_nav,&QListWidget::itemActivated,this,navigate);
    auto *heading = new QHBoxLayout;
    heading->addWidget(label("我的歌单", "section"));
    heading->addStretch();
    auto *add = new IconButton("plus", "创建歌单");
    add->setFixedSize(26, 26);
    connect(add, &QPushButton::clicked, this, &Window::createPlaylist);
    heading->addWidget(add);
    v->addLayout(heading);
    m_lists = new QListWidget;
    m_lists->setObjectName("playlistNavigation");
    m_lists->setSpacing(3);
    m_lists->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_lists->setContextMenuPolicy(Qt::CustomContextMenu);
    v->addWidget(m_lists, 1);
    connect(m_lists, &QListWidget::itemClicked, this,
            [this](auto *it) { selectScope(it->data(Qt::UserRole).toLongLong()); });
    connect(m_lists, &QListWidget::customContextMenuRequested, this, [this](QPoint p) {
        if (auto *it = m_lists->itemAt(p))
            playlistMenu(it->data(Qt::UserRole).toLongLong(), m_lists->viewport()->mapToGlobal(p));
    });
}
QWidget *Window::buildLibrary() {
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(34, 28, 34, 16);
    v->setSpacing(16);
    auto *top = new QHBoxLayout;
    auto *head = new QVBoxLayout;
    head->setSpacing(16);
    m_title = label("本地音乐", "heading");
    m_subtitle = label("你的音乐，随时在这里。", "muted");
    auto subtitlePolicy=m_subtitle->sizePolicy();subtitlePolicy.setRetainSizeWhenHidden(true);m_subtitle->setSizePolicy(subtitlePolicy);
    head->addWidget(m_title);
    head->addWidget(m_subtitle);
    top->addLayout(head, 1);
    m_search = new QLineEdit;
    m_search->setObjectName("search");
    m_search->setPlaceholderText("搜索歌曲、歌手或专辑");
    m_search->addAction(symbol("search"), QLineEdit::LeadingPosition);
    m_search->setFixedWidth(255);
    m_search->setClearButtonEnabled(true);
    top->addWidget(m_search);
    v->addLayout(top);
    connect(m_search, &QLineEdit::textChanged, this, [this] {
        const auto tracks=visibleTracks();
        m_songs->setSongs(tracks, m_library->root(), m_player->current().id);
        updateLibrarySummary(tracks);
        m_songStack->setCurrentIndex(m_songs->rowCount() ? 0 : 1);
    });
    m_libraryActions=new QWidget;
    m_libraryActions->setObjectName("libraryActions");
    auto *action=new QHBoxLayout(m_libraryActions);action->setContentsMargins(0,0,0,0);
    const auto playAll=[this]{m_player->play(m_songs->ids());};
    const auto shuffleAll=[this]{
        m_player->setMode(Mode::Shuffle);const auto ids=m_songs->ids();
        if(!ids.isEmpty())m_player->play(ids,ids[QRandomGenerator::global()->bounded(ids.size())]);
    };
    auto *play=button("播放全部","primary");play->setIcon(symbol("play",Qt::white));
    connect(play,&QPushButton::clicked,this,playAll);action->addWidget(play);
    auto *shuffle=button("随机播放");shuffle->setIcon(symbol("shuffle"));
    connect(shuffle,&QPushButton::clicked,this,shuffleAll);action->addWidget(shuffle);action->addStretch();
    m_filterHost=new QWidget;auto *filters=new QHBoxLayout(m_filterHost);filters->setContentsMargins(0,0,0,0);filters->setSpacing(8);
    m_artistFilter=new FilterButton("歌手");m_albumFilter=new FilterButton("专辑");filters->addWidget(m_artistFilter);filters->addWidget(m_albumFilter);
    auto *clear=button("清除","plain");clear->setObjectName("clearFilters");filters->addWidget(clear);
    auto *lyrics=button("查找缺失歌词");lyrics->setObjectName("lookupMissingLyrics");filters->addWidget(lyrics);connect(lyrics,&QPushButton::clicked,this,&Window::lookupMissingLyrics);
    action->insertWidget(2,m_filterHost);connect(m_artistFilter,&FilterButton::filterChanged,this,[this]{if(m_filterRefreshing)return;exitBulk();refreshFilters();refresh();});connect(m_albumFilter,&FilterButton::filterChanged,this,[this]{if(!m_filterRefreshing){exitBulk();refresh();}});
    connect(clear,&QPushButton::clicked,this,[this]{m_filterRefreshing=true;m_artistFilter->reset();m_albumFilter->reset();m_filterRefreshing=false;exitBulk();refresh();});
    v->addWidget(m_libraryActions);
    auto *import=button("导入音乐");import->setObjectName("importMusicButton");import->setIcon(symbol("plus"));
    auto *menu=new RoundedMenu(import);
    connect(menu->addAction("导入音乐文件…"),&QAction::triggered,this,&Window::chooseFiles);
    connect(menu->addAction("导入整个文件夹…"),&QAction::triggered,this,&Window::chooseFolder);
    import->setMenu(menu);top->addWidget(import);
    m_playlistHeader=new PlaylistHeader;v->addWidget(m_playlistHeader);m_playlistHeader->hide();
    m_listCover=m_playlistHeader->cover();m_rename=m_playlistHeader->options();
    m_reorderUp=m_playlistHeader->up();m_reorderDown=m_playlistHeader->down();
    connect(m_playlistHeader,&PlaylistHeader::playRequested,this,playAll);
    connect(m_playlistHeader,&PlaylistHeader::shuffleRequested,this,shuffleAll);
    connect(m_playlistHeader,&PlaylistHeader::tagsRequested,this,&Window::editPlaylistTags);
    connect(m_playlistHeader,&PlaylistHeader::descriptionRequested,this,&Window::editPlaylistDescription);
    connect(m_rename,&QPushButton::clicked,this,[this]{playlistMenu(m_scope,m_rename->mapToGlobal(QPoint(0,m_rename->height())));});
    connect(m_reorderUp,&QPushButton::clicked,this,[this]{reorder(-1);});
    connect(m_reorderDown,&QPushButton::clicked,this,[this]{reorder(1);});
    connect(m_listCover,&Cover::clicked,this,[this]{
        const auto id=m_scope;
        auto path=QFileDialog::getOpenFileName(this,"选择歌单封面",{},"图片 (*.png *.jpg *.jpeg *.webp)");
        if(!path.isEmpty())check(m_library->playlistCover(id,path));
    });
    m_songStack = new QStackedWidget;
    m_songs = new SongTable;
    m_songs->setObjectName("songTable");m_songs->setColumnLibrary(m_library);
    m_songs->viewport()->setProperty("playlistScroll",true);
    connect(m_songs->verticalScrollBar(),&QScrollBar::valueChanged,this,[this](int value){if(m_scope<=0)return;if(value>0){m_playlistScrolled=true;m_playlistHeader->collapseTo(1);}else if(m_playlistScrolled){m_playlistScrolled=false;m_playlistHeader->collapseTo(0);}});
    m_songStack->addWidget(m_songs);
    auto *empty = new QWidget;
    auto *ev = new QVBoxLayout(empty);
    ev->addStretch();
    auto *art = new QLabel;
    art->setAlignment(Qt::AlignCenter);
    art->setPixmap(symbol("music", QColor("#e5455f")).pixmap(76, 76));
    ev->addWidget(art);
    auto *title = label("把喜欢的音乐，放在这里。", "emptyTitle");
    title->setAlignment(Qt::AlignCenter);
    ev->addWidget(title);
    auto *hint =
        label("导入文件或文件夹，也可以直接拖入窗口。\n音乐会复制到统一目录，歌单和统计自动保存。", "muted");
    hint->setAlignment(Qt::AlignCenter);
    ev->addWidget(hint);
    auto *b = button("导入音乐", "primary");
    b->setMaximumWidth(145);
    ev->addWidget(b, 0, Qt::AlignHCenter);
    connect(b, &QPushButton::clicked, this, &Window::chooseFiles);
    ev->addStretch();
    m_songStack->addWidget(empty);
    v->addWidget(m_songStack, 1);
    connect(m_songs, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if(m_bulkTable)return;
        if(col==SongTable::FavoriteColumn)return;
        m_player->play(m_songs->ids(), m_songs->item(row, SongTable::TitleColumn)->data(Qt::UserRole).toLongLong());
    });
    connect(m_songs, &QTableWidget::cellClicked, this, [this](int row, int col) {
        if(m_bulkTable)return;
        if (col == SongTable::FavoriteColumn) {
            auto t = m_library->track(m_songs->item(row, SongTable::TitleColumn)->data(Qt::UserRole).toLongLong());
            check(m_library->favorite(t.id, !t.favorite));
        }
    });
    connect(m_songs, &QTableWidget::customContextMenuRequested, this,
            [this](auto p) { songMenu(m_songs, p); });
    connect(m_songs,&QTableWidget::itemSelectionChanged,this,&Window::updateBulk);
    return page;
}
QWidget *Window::buildPlaylists() {
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(34, 28, 34, 24);
    auto *top = new QHBoxLayout;
    auto *head=new QVBoxLayout;head->setSpacing(16);head->addWidget(label("歌单", "heading"));
    head->addWidget(label("给不同的心情，留一张歌单。", "muted"));top->addLayout(head,1);
    auto *add = button("创建歌单", "primary");
    add->setProperty("toolbarRole","createPlaylist");
    connect(add, &QPushButton::clicked, this, &Window::createPlaylist);
    top->addWidget(add);
    v->addLayout(top);

    m_cards = new QListWidget;
    m_cards->setObjectName("playlistCards");
    m_cards->setViewMode(QListView::IconMode);
    m_cards->setResizeMode(QListView::Adjust);
    m_cards->setMovement(QListView::Static);
    m_cards->setIconSize(QSize(170, 170));
    m_cards->setGridSize(QSize(204, 230));
    m_cards->setSpacing(10);
    m_cards->setWordWrap(true);
    m_cards->setContextMenuPolicy(Qt::CustomContextMenu);
    v->addWidget(m_cards, 1);
    connect(m_cards, &QListWidget::itemDoubleClicked, this,
            [this](auto *it) { selectScope(it->data(Qt::UserRole).toLongLong()); });
    connect(m_cards, &QListWidget::customContextMenuRequested, this, [this](QPoint p) {
        if (auto *it = m_cards->itemAt(p))
            playlistMenu(it->data(Qt::UserRole).toLongLong(), m_cards->viewport()->mapToGlobal(p));
    });
    return page;
}
QWidget *Window::buildProfile() {
    auto *page=new QWidget;auto *v=new QVBoxLayout(page);v->setContentsMargins(34,28,34,20);v->setSpacing(20);v->addWidget(label("个人","heading"));
    m_profileHeader=new ProfileHeader;m_avatar=m_profileHeader->avatar;m_profileName=m_profileHeader->name;m_statSongs=m_profileHeader->songs;m_statPlays=m_profileHeader->plays;m_statTime=m_profileHeader->time;v->addWidget(m_profileHeader);
    connect(m_profileHeader->edit,&QPushButton::clicked,this,[this]{
        QDialog dialog(this);dialog.setObjectName("profileEditDialog");dialog.setWindowTitle("编辑资料");dialog.resize(580,340);auto *layout=new QVBoxLayout(&dialog);
        layout->addWidget(label("昵称","section"));auto *name=new QLineEdit(m_library->setting("nickname","Relaxer Listener").toString(),&dialog);name->setObjectName("profileNicknameEdit");name->setMaxLength(60);layout->addWidget(name);
        layout->addWidget(label("个人简介","section"));auto *bio=new QPlainTextEdit(&dialog);bio->setObjectName("profileDescriptionEdit");bio->setPlainText(m_library->setting("profileDescription","收藏、回忆，还有反复听过的那些歌。").toString());layout->addWidget(bio,1);auto *count=label("","muted");layout->addWidget(count);
        connect(bio,&QPlainTextEdit::textChanged,&dialog,[bio,count]{auto text=bio->toPlainText();if(characterCount(text)>1000){QSignalBlocker block(bio);const int position=bio->textCursor().position();text.truncate(characterPrefixLength(text,1000));bio->setPlainText(text);auto cursor=bio->textCursor();cursor.setPosition(qMin(position,int(text.size())));bio->setTextCursor(cursor);}count->setText(QString("%1 / 1000 字").arg(characterCount(text)));});count->setText(QString("%1 / 1000 字").arg(characterCount(bio->toPlainText())));
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted)return;if(name->text().trimmed().isEmpty()){toast("昵称不能为空。");return;}
        check(m_library->setSetting("nickname",name->text().trimmed()));check(m_library->setSetting("profileDescription",bio->toPlainText()));refresh();
    });
    connect(m_avatar,&Cover::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,"选择头像",{},"图片 (*.png *.jpg *.jpeg *.webp)");if(path.isEmpty())return;QImageReader reader(path);const auto size=reader.size();if(!size.isValid()||qint64(size.width())*size.height()>120000000){toast("图片尺寸过大或无法读取");return;}reader.setAutoTransform(true);reader.setScaledSize(size.scaled(512,512,Qt::KeepAspectRatio));const auto image=reader.read();QSaveFile out(m_library->absolute("Profile/avatar.png"));if(!image.isNull()&&out.open(QIODevice::WriteOnly)&&image.save(&out,"PNG")&&out.commit()){m_library->setSetting("avatar","Profile/avatar.png");QPixmapCache::clear();refresh();}});
    auto *tabs=new QHBoxLayout;auto *recent=button("最近播放");auto *top=button("最常播放");recent->setObjectName("profileRecent");top->setObjectName("profileFrequent");recent->setCheckable(true);top->setCheckable(true);recent->setChecked(true);tabs->addWidget(recent);tabs->addWidget(top);tabs->addStretch();v->addLayout(tabs);
    m_recent=new SongTable;m_recent->setObjectName("recentTable");m_recent->setColumnLibrary(m_library);m_recent->viewport()->setProperty("profileScroll",true);v->addWidget(m_recent,1);
    connect(recent,&QPushButton::clicked,this,[this]{exitBulk();m_view="recent";updateProfileTabs();m_recent->setSongs(m_library->recent(),m_library->root(),m_player->current().id);m_recent->verticalScrollBar()->setValue(0);m_profileHeader->collapseTo(0);});
    connect(top,&QPushButton::clicked,this,[this]{exitBulk();m_view="top";updateProfileTabs();auto songs=m_library->tracks();std::stable_sort(songs.begin(),songs.end(),[](const auto &a,const auto &b){return a.plays>b.plays;});m_recent->setSongs(songs,m_library->root(),m_player->current().id);m_recent->verticalScrollBar()->setValue(0);m_profileHeader->collapseTo(0);});
    connect(m_recent->verticalScrollBar(),&QScrollBar::valueChanged,this,[this](int value){if(value>0){m_profileScrolled=true;m_profileHeader->collapseTo(1);}else if(m_profileScrolled){m_profileScrolled=false;m_profileHeader->collapseTo(0);}});
    connect(m_recent,&QTableWidget::cellDoubleClicked,this,[this](int row,int column){if(m_bulkTable||column==SongTable::FavoriteColumn)return;m_player->play(m_recent->ids(),m_recent->item(row,SongTable::TitleColumn)->data(Qt::UserRole).toLongLong());});
    connect(m_recent,&QTableWidget::cellClicked,this,[this](int row,int column){if(m_bulkTable)return;if(column==SongTable::FavoriteColumn){const auto t=m_library->track(m_recent->item(row,SongTable::TitleColumn)->data(Qt::UserRole).toLongLong());check(m_library->favorite(t.id,!t.favorite));}});
    connect(m_recent,&QTableWidget::customContextMenuRequested,this,[this](auto position){songMenu(m_recent,position);});connect(m_recent,&QTableWidget::itemSelectionChanged,this,&Window::updateBulk);return page;
}
QWidget *Window::buildSettings() {
    auto *host=new QWidget;host->setObjectName("settingsPage");auto *outer=new QVBoxLayout(host);outer->setContentsMargins(0,0,0,0);outer->setSpacing(0);
    auto *fixed=new QWidget;fixed->setObjectName("settingsHeader");auto *header=new QVBoxLayout(fixed);header->setContentsMargins(34,28,34,0);header->setSpacing(14);header->addWidget(label("设置","heading"));auto *divider=new QFrame;divider->setObjectName("settingsDivider");divider->setFrameShape(QFrame::HLine);divider->setFrameShadow(QFrame::Plain);divider->setFixedHeight(1);header->addWidget(divider);outer->addWidget(fixed);
    auto *scroll = new QScrollArea;
    scroll->setObjectName("settingsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(34, 18, 34, 28);
    v->setSpacing(18);
    auto *group = new QWidget;
    group->setObjectName("settingsCard");
    auto *basic = new QVBoxLayout(group);
    basic->setContentsMargins(22, 20, 22, 20);
    basic->setSpacing(16);
    // Share label/input rows: independent vertical layouts redistribute spare
    // height differently when combo boxes, buttons and spin boxes differ.
    auto *row = new QGridLayout;
    row->setHorizontalSpacing(18);
    row->setVerticalSpacing(8);
    const QStringList titles{"外观", "有效播放比例", "关闭窗口时"};
    for (int i = 0; i < titles.size(); ++i) {
        row->setColumnStretch(i, 1);
        row->addWidget(label(titles[i], "section"), 0, i, Qt::AlignTop);
    }
    auto *themeRow = new QHBoxLayout;
    themeRow->setContentsMargins(0, 0, 0, 0);
    auto *theme = new RoundedComboBox;
    theme->setObjectName("themeSetting");
    theme->addItems({"浅色", "深色", "跟随系统", "自定义"});
    theme->setCurrentIndex(qBound(0, m_library->setting("theme", 0).toInt(), 3));
    themeRow->addWidget(theme, 1, Qt::AlignTop);
    auto *edit = button("编辑");
    edit->setObjectName("editAppearance");
    themeRow->addWidget(edit, 0, Qt::AlignTop);
    row->addLayout(themeRow, 1, 0, Qt::AlignTop);
    connect(edit, &QPushButton::clicked, this, &Window::editAppearance);
    connect(theme, &QComboBox::currentIndexChanged, this, [this, theme](int value) {
        if (value == 3)
            editAppearance();
        else {
            m_library->setSetting("theme", value);
            applyTheme();
        }
        QSignalBlocker block(theme);
        theme->setCurrentIndex(qBound(0, m_library->setting("theme", 0).toInt(), 3));
    });
    auto *threshold = new PercentSpin;
    threshold->setObjectName("thresholdSetting");
    threshold->setRange(70, 100);
    threshold->setSuffix(" %");
    threshold->setValue(m_library->setting("threshold", 80).toInt());
    threshold->setToolTip("整轮没有手动跳转，实际播放达到指定比例才计一次。暂停不累计；比例从下一轮生效。达到"
                          "比例后再跳转会撤回本轮次数。");
    row->addWidget(threshold, 1, 1, Qt::AlignTop);
    connect(threshold, &QSpinBox::valueChanged, this,
            [this](int value) { m_library->setSetting("threshold", value); });
    auto *close = new RoundedComboBox;
    close->setObjectName("closeSetting");
    close->addItems({"退出播放器", "最小化到系统托盘"});
    close->setCurrentIndex(m_library->setting("closeToTray", false).toBool() ? 1 : 0);
    row->addWidget(close, 1, 2, Qt::AlignTop);
    connect(close, &QComboBox::currentIndexChanged, this,
            [this](int value) { m_library->setSetting("closeToTray", value == 1); });
    basic->addLayout(row);
    auto *lyricRow=new QHBoxLayout;
    lyricRow->addWidget(label("歌词颜色","section"));auto *lyricColor=button("选择颜色");lyricColor->setObjectName("lyricColorSetting");lyricRow->addWidget(lyricColor);
    auto *autoColor=button("跟随文字颜色","plain");autoColor->setObjectName("lyricColorReset");lyricRow->addWidget(autoColor);
    connect(lyricColor,&QPushButton::clicked,this,[this]{auto current=QColor(m_library->setting("lyricColor").toString());if(!current.isValid())current=m_ink;const auto color=QColorDialog::getColor(current,this,"歌词颜色",QColorDialog::DontUseNativeDialog);if(color.isValid()){m_library->setSetting("lyricColor",color.name());applyTheme();}});
    connect(autoColor,&QPushButton::clicked,this,[this]{m_library->setSetting("lyricColor",QString());applyTheme();});
    auto *fontButton=button("字体");fontButton->setObjectName("uiFontSetting");lyricRow->addWidget(fontButton);
    connect(fontButton,&QPushButton::clicked,this,[this]{
        QDialog dialog(this);dialog.setWindowTitle("界面字体");dialog.setMinimumWidth(420);auto *layout=new QVBoxLayout(&dialog);
        const auto saved=m_library->setting("uiFont").toObject();auto *path=new QLabel(saved["file"].toString().isEmpty()?QString("未安装字体，使用系统字体"):saved["file"].toString(),&dialog);path->setWordWrap(true);layout->addWidget(path);
        auto *pick=button("选择字体文件");layout->addWidget(pick,0,Qt::AlignLeft);QString chosen;
        connect(pick,&QPushButton::clicked,&dialog,[&]{const auto file=QFileDialog::getOpenFileName(&dialog,"选择字体",{},"字体 (*.ttf *.otf *.ttc)");if(file.isEmpty())return;chosen=file;path->setText(QFileInfo(file).fileName());});
        const QList<QPair<QString,QString>> regions{{"songs","歌曲列表"},{"lyrics","歌词"},{"footer","底栏"},{"next","下一首"},{"rest","其余界面文字"}};
        QHash<QString,QCheckBox *> boxes;for(const auto &region:regions){auto *box=new QCheckBox(region.second,&dialog);box->setChecked(saved[region.first].toBool());boxes.insert(region.first,box);layout->addWidget(box);}
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel|QDialogButtonBox::Reset,&dialog);buttons->button(QDialogButtonBox::Save)->setText("保存");buttons->button(QDialogButtonBox::Cancel)->setText("取消");buttons->button(QDialogButtonBox::Reset)->setText("恢复默认");layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(buttons->button(QDialogButtonBox::Reset),&QPushButton::clicked,&dialog,[this,&dialog]{m_library->setSetting("uiFont",QJsonObject());applyTheme();dialog.accept();});
        connect(buttons,&QDialogButtonBox::accepted,&dialog,[this,&dialog,&chosen,boxes]{QJsonObject font;if(!chosen.isEmpty()){QDir().mkpath(m_library->absolute("Fonts"));const auto name=QFileInfo(chosen).fileName();const auto relative="Fonts/"+name;QFile::remove(m_library->absolute(relative));if(!QFile::copy(chosen,m_library->absolute(relative))){QMessageBox::warning(&dialog,"字体","字体文件复制失败。");return;}font["file"]=relative;}else font["file"]=m_library->setting("uiFont").toObject()["file"];for(auto it=boxes.cbegin();it!=boxes.cend();++it)font[it.key()]=it.value()->isChecked();m_library->setSetting("uiFont",font);applyTheme();dialog.accept();});
        dialog.exec();
    });
    auto *coverBackground=checkableSetting("展开播放时使用封面背景","playerCoverBackground",m_library->setting("playerCoverBackground",true).toBool());lyricRow->addWidget(coverBackground);lyricRow->addStretch();basic->addLayout(lyricRow);
    connect(coverBackground,&QPushButton::toggled,this,[this](bool on){m_library->setSetting("playerCoverBackground",on);applyTheme();});
    auto *folderRow = new QHBoxLayout;
    auto *path = label(m_library->root(), "muted");
    path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    path->setWordWrap(true);
    folderRow->addWidget(path, 1);
    auto *folder = button("打开音乐资料目录");
    connect(folder, &QPushButton::clicked, this,
            [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_library->root())); });
    folderRow->addWidget(folder);
    basic->addLayout(folderRow);
    v->addWidget(group);

    auto *aiCard = new QWidget;
    aiCard->setObjectName("settingsCard");
    auto *aiLayout = new QVBoxLayout(aiCard);
    aiLayout->setContentsMargins(22, 18, 22, 18);
    aiLayout->setSpacing(8);
    auto *aiHeader = new QHBoxLayout;
    aiHeader->addWidget(label("AI 服务", "profileName"));
    aiHeader->addStretch(1);
    auto *aiOpen = button("管理 AI 服务", "openAiServiceSettings");
    aiHeader->addWidget(aiOpen);
    aiLayout->addLayout(aiHeader);
    auto *aiDescription = label("配置 DeepSeek 凭据、模型和歌词翻译。", "muted");
    aiDescription->setWordWrap(true);
    aiLayout->addWidget(aiDescription);
    connect(aiOpen, &QPushButton::clicked, this, &Window::openAiSettings);
    v->addWidget(aiCard);

    auto *keys = new QWidget;
    keys->setObjectName("settingsCard");
    auto *kv = new QVBoxLayout(keys);
    kv->setContentsMargins(22, 20, 22, 20);
    kv->addWidget(label("快捷键", "profileName"));
    auto *hint =
        label("应用内：播放器窗口激活时生效。全局：切换应用或最小化后也能使用。音量每次调整 5%。", "muted");
    hint->setWordWrap(true);
    kv->addWidget(hint);
    auto *enabled = checkableSetting("启用全局快捷键","globalShortcutsEnabled",false);
    enabled->setObjectName("globalShortcutsEnabled");
    auto config = m_library->setting("shortcuts", PlayerShortcuts::defaults()).toObject();
    enabled->setChecked(config["globalEnabled"].toBool());
    if (!m_shortcuts->globalSupported()) {
        enabled->setEnabled(false);

    }
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(9);
    grid->addWidget(label("功能", "section"), 0, 0);
    grid->addWidget(label("应用内", "section"), 0, 1);
    grid->addWidget(label("全局", "section"), 0, 2);
    auto editors = std::make_shared<QMap<QString, ShortcutEdit *>>();
    for (int i = 0; i < PlayerShortcuts::actionKeys().size(); ++i) {
        auto action = PlayerShortcuts::actionKeys()[i];
        grid->addWidget(label(PlayerShortcuts::actionNames()[i], "shortcutAction"), i + 1, 0);
        int column = 1;
        for (const auto &scope : {QString("app"), QString("global")}) {
            auto value = config[scope].toObject()[action].toString();
            auto *editor = new ShortcutEdit(QKeySequence::fromString(value, QKeySequence::PortableText));
            editor->setObjectName("shortcut-" + scope + "-" + action);
    
            (*editors)[scope + "/" + action] = editor;
            grid->addWidget(editor, i + 1, column++);
        }
    }
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    kv->addLayout(grid);
    auto *status = label(m_shortcuts->status(), "muted");
    status->setObjectName("shortcutStatus");
    status->setWordWrap(true);
    kv->addWidget(status);
    connect(m_shortcuts, &PlayerShortcuts::statusChanged, status, &QLabel::setText);
    auto *keyActions = new QHBoxLayout;
    auto *keyHint=label("录入后点击保存；输入文字时不触发应用内播放快捷键。","muted");keyHint->setWordWrap(true);keyHint->setMinimumWidth(0);keyHint->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);keyActions->addWidget(keyHint,1);
    auto *reset = button("恢复默认");
    reset->setObjectName("shortcutsReset");
    auto *save = button("保存快捷键");
    save->setObjectName("shortcutsSave");
    keyActions->addWidget(enabled);
    keyActions->addWidget(reset);
    keyActions->addWidget(save);
    kv->addLayout(keyActions);
    connect(reset, &QPushButton::clicked, this, [editors, enabled] {
        auto defaults = PlayerShortcuts::defaults();
        enabled->setChecked(false);
        for (auto it = editors->begin(); it != editors->end(); ++it) {
            auto pieces = it.key().split('/');
            it.value()->setKeySequence(QKeySequence::fromString(
                defaults[pieces[0]].toObject()[pieces[1]].toString(), QKeySequence::PortableText));
        }
    });
    connect(save, &QPushButton::clicked, this, [this, editors, enabled, status] {
        QJsonObject candidate{{"globalEnabled", enabled->isChecked()}};
        for (const auto &scope : {QString("app"), QString("global")}) {
            QJsonObject values;
            for (const auto &action : PlayerShortcuts::actionKeys())
                values[action] =
                    editors->value(scope + "/" + action)->keySequence().toString(QKeySequence::PortableText);
            candidate[scope] = values;
        }
        auto previous = m_shortcuts->config();
        QString error;
        if (!m_shortcuts->apply(candidate, &error)) {
            status->setText(error);
            return;
        }
        if (!m_library->setSetting("shortcuts", candidate)) {
            m_shortcuts->apply(previous);
            status->setText("快捷键未保存：" + m_library->lastError());
            return;
        }
        status->setText("快捷键已保存。" + m_shortcuts->status());
    });
    v->addWidget(keys);
    auto *about = new QWidget;
    about->setObjectName("settingsCard");
    auto *av = new QVBoxLayout(about);
    av->setContentsMargins(22, 20, 22, 20);
    av->addWidget(label(displayVersion(), "profileName"));
    av->addWidget(label("为本地音乐留一个安静的位置。", "muted"));
    av->addWidget(label("本地音乐播放器 · 支持联网查找歌词", "muted"));

    auto *bottom = new QHBoxLayout;
    bottom->addStretch();
    auto *toggle = button("版本日志", "plain");
    toggle->setObjectName("versionLogToggle");
    bottom->addWidget(toggle);
    av->addLayout(bottom);
    connect(toggle,&QPushButton::clicked,this,&Window::showVersionLog);
    v->addWidget(about);
    v->addStretch();
    scroll->setWidget(page);outer->addWidget(scroll,1);
    return host;
}
QWidget *Window::buildVersionLog(){auto *page=new QWidget;page->setObjectName("versionLogPage");auto *outer=new QVBoxLayout(page);outer->setContentsMargins(34,28,34,28);auto *header=new QHBoxLayout;auto *head=new QVBoxLayout;head->setSpacing(16);head->addWidget(label("版本日志","heading"));auto *spacer=label(" ","muted");auto policy=spacer->sizePolicy();policy.setRetainSizeWhenHidden(true);spacer->setSizePolicy(policy);head->addWidget(spacer);spacer->hide();header->addLayout(head);header->addStretch();auto *back=button("返回设置","plain");back->setObjectName("versionLogBack");connect(back,&QPushButton::clicked,this,&Window::showSettings);header->addWidget(back);outer->addLayout(header);auto *scroll=new QScrollArea;scroll->setObjectName("versionLogScroll");scroll->setFrameShape(QFrame::NoFrame);scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);auto *body=new QWidget;auto *layout=new QVBoxLayout(body);layout->setContentsMargins(0,0,0,0);layout->addWidget(new ChangeLog);auto *copyright=label(displayVersion()+" | © QUEQIAN. All rights reserved.","copyright");copyright->setWordWrap(true);layout->addWidget(copyright);layout->addStretch();scroll->setWidget(body);outer->addWidget(scroll,1);return page;}
void Window::editAppearance() {
    if(m_appearancePage&&m_pages->currentWidget()==m_appearancePage)return;
    if(m_pages->currentIndex()==3)m_settingsScrollPosition=findChild<QScrollArea *>("settingsScroll")->verticalScrollBar()->value();
    if(m_appearancePage){m_pages->removeWidget(m_appearancePage);m_appearancePage->deleteLater();}
    m_appearancePage=new AppearancePage(m_library,
        [this](const QJsonObject &draft){m_appearancePreview=draft;applyTheme();},
        [this]{m_appearancePreview={};applyTheme();},m_pages,m_currentAppearance);
    connect(m_appearancePage,&AppearancePage::finished,this,&Window::showSettings);
    setPage(m_pages->addWidget(m_appearancePage));applyTheme();
}
void Window::openAiSettings() {
    if (m_pages->currentIndex() == 3)
        m_settingsScrollPosition = findChild<QScrollArea *>("settingsScroll")->verticalScrollBar()->value();
    if (!m_aiPage) {
        m_aiPage = new ai::AiSettingsPage(m_aiRuntime, m_pages);
        connect(m_aiPage, &ai::AiSettingsPage::backRequested, this, &Window::showSettings);
        m_pages->addWidget(m_aiPage);
    }
    setPage(m_pages->indexOf(m_aiPage));
}
QWidget *Window::buildNowPlaying() {
    m_nowPage = new Backdrop;
    auto *v = new QVBoxLayout(m_nowPage);
    v->setContentsMargins(34, 28, 34, 20);
    auto *toolbar=new QWidget;toolbar->setObjectName("nowToolbar");
    auto *top = new QHBoxLayout(toolbar);top->setContentsMargins(0,0,0,0);
    auto *back = new IconButton("collapse", "收起");
    connect(back, &QPushButton::clicked, this, &Window::closeNowPlaying);
    back->setObjectName("playerCollapse");back->setFixedWidth(92);back->setText("收起");
    top->addWidget(back);
    top->addStretch();
    v->addWidget(toolbar);
    auto *main = new QHBoxLayout;
    main->setSpacing(32);
    auto *left = new QVBoxLayout;
    left->setContentsMargins(25, 10, 15, 0);
    left->addStretch();
    m_nowCover = new Cover;
    m_nowCover->setObjectName("nowCover");
    m_nowCover->setMinimumSize(160, 160);
    m_nowCover->setMaximumSize(430, 430);
    m_nowCover->setRadius(24);
    auto coverPolicy=m_nowCover->sizePolicy();coverPolicy.setRetainSizeWhenHidden(true);m_nowCover->setSizePolicy(coverPolicy);
    left->addWidget(m_nowCover, 1, Qt::AlignHCenter);
    left->addSpacing(22);
    m_nowTitle = label("还没有音乐正在播放", "nowTitle");
    m_nowTitle->setWordWrap(true);
    m_nowTitle->setAlignment(Qt::AlignCenter);
    left->addWidget(m_nowTitle);
    m_nowArtist = label("选一首喜欢的歌，慢慢听。", "nowArtist");
    m_nowArtist->setWordWrap(true);
    m_nowArtist->setAlignment(Qt::AlignCenter);
    left->addWidget(m_nowArtist);
    left->addSpacing(12);
    left->addStretch();
    main->addLayout(left, 1);
    m_lyrics = new LyricView;
    m_delayControls=new QWidget;auto *delay=new QHBoxLayout(m_delayControls);delay->setContentsMargins(0,0,12,0);delay->addStretch();delay->addWidget(label("歌词延时","muted"));
    m_lyricDelay=new BlurPercentSpin;m_lyricDelay->setObjectName("lyricDelay");m_lyricDelay->setDecimals(2);m_lyricDelay->setRange(-5.,5.);m_lyricDelay->setSingleStep(.01);m_lyricDelay->setSuffix(" s");m_lyricDelay->setFixedWidth(136);m_lyricDelay->setKeyboardTracking(false);m_lyricDelay->setAccelerated(true);m_lyricDelay->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);m_lyricDelay->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);m_lyricDelay->setToolTip("正数让歌词晚出现，负数提前；可直接输入，回车或离开输入框后保存。超过 ±5 秒会自动收回到边界。");delay->addWidget(m_lyricDelay);
    auto *zero=button("归零","plain");zero->setObjectName("lyricDelayReset");delay->addWidget(zero);
    connect(m_lyricDelay,&QDoubleSpinBox::valueChanged,this,[this](double value){const auto t=m_player->current();if(t.id&&!m_refreshing)check(m_library->setLyricOffset(t.id,qRound(value*1000)));});
    connect(zero,&QPushButton::clicked,m_lyricDelay,[this]{m_lyricDelay->setValue(0);});
    m_lyricTools=new LyricTools(m_lyrics,m_delayControls,m_nowPage);main->addWidget(m_lyricTools,1);
    const bool showTranslation=m_library->setting("showLyricTranslation",true).toBool();m_lyricTools->setShowTranslation(showTranslation);
    connect(m_lyricTools,&LyricTools::translationDisplayChanged,this,[this](bool on){m_library->setSetting("showLyricTranslation",on);});
    connect(m_lyrics, &LyricView::seekRequested, m_player, &PlaybackFacade::seek);
    connect(m_lyricTools,&LyricTools::timedLyricsRequested,this,[this]{
        const auto track=m_player->current();if(!track.id||m_library->isInstrumental(track.id))return;
        m_lyricTools->closeTools();
        LyricsLookup dialog(m_library,{track.id},this,QUrl("https://lrclib.net/api"),{},true,m_aiRuntime);
        dialog.setProperty("requireTimedLyrics",true);
        dialog.findChild<QListWidget *>("lyricsTracks")->setCurrentRow(0);
        dialog.findChild<QCheckBox *>("lyricsReplace")->setChecked(true);
        dialog.findChild<QCheckBox *>("lyricsSyncedOnly")->setChecked(true);
        dialog.exec();refreshTrack();
    });
    v->addLayout(main, 1);
    m_nowPage->setMouseTracking(true);for(auto *widget:m_nowPage->findChildren<QWidget *>())widget->setMouseTracking(true);
    return m_nowPage;
}
QWidget *Window::buildPlayerBar() {
    auto *bar = new QWidget;
    bar->setObjectName("playerBar");
    bar->setMinimumHeight(98);
    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(22, 12, 22, 12);
    h->setSpacing(14);
    // Equal side columns keep playback controls at the window's center.
    auto *left = new QWidget;
    left->setObjectName("playerMetadata");
    left->setFixedWidth(320);
    auto *metadata = new QHBoxLayout(left);
    metadata->setContentsMargins(0, 0, 0, 0);
    metadata->setSpacing(14);
    m_bottomCover = new Cover;
    m_bottomCover->setObjectName("bottomCover");
    m_bottomCover->setFixedSize(62, 62);
    m_bottomCover->setRadius(10);
    metadata->addWidget(m_bottomCover);
    connect(m_bottomCover, &Cover::clicked, this, &Window::showNowPlaying);
    auto *text = new QWidget;
    auto *tv = new QVBoxLayout(text);
    tv->setContentsMargins(0, 0, 0, 0);
    tv->setSpacing(4);
    m_bottomTitle = label("Relaxer", "bottomSongTitle");
    m_bottomArtist = label("选一首歌，开始放松。", "muted");
    m_bottomTitle->setFixedWidth(194);
    m_bottomArtist->setFixedWidth(194);
    m_bottomTitle->setProperty("openPlaying",true);m_bottomArtist->setProperty("openPlaying",true);
    m_bottomTitle->setCursor(Qt::PointingHandCursor);m_bottomArtist->setCursor(Qt::PointingHandCursor);
    tv->addWidget(m_bottomTitle);
    tv->addWidget(m_bottomArtist);
    text->setFixedWidth(194);
    metadata->addWidget(text);
    m_like = new IconButton("heart", "收藏歌曲");
    connect(m_like, &QPushButton::clicked, this, [this] {
        auto t = m_player->current();
        if (t.id)
            check(m_library->favorite(t.id, !t.favorite));
    });
    for(QWidget *widget:QList<QWidget *>{static_cast<QWidget *>(m_bottomCover),m_bottomTitle,m_bottomArtist,static_cast<QWidget *>(m_like),static_cast<QWidget *>(m_nowCover),m_nowTitle,m_nowArtist}){auto policy=widget->sizePolicy();policy.setRetainSizeWhenHidden(true);widget->setSizePolicy(policy);}
    h->addWidget(left);
    auto *middle = new QWidget;
    middle->setObjectName("playerCenter");
    auto *center = new QVBoxLayout(middle);
    center->setContentsMargins(0, 0, 0, 0);
    center->setSpacing(2);
    auto *control = new QHBoxLayout;
    control->setSpacing(12);
    control->addStretch();
    m_mode = new IconButton("repeat", "列表循环");
    connect(m_mode, &QPushButton::clicked, this,
            [this] { m_player->setMode(static_cast<Mode>((int(m_player->mode()) + 1) % 4)); });
    control->addWidget(m_like);
    auto *prev = new IconButton("previous", "上一首");
    connect(prev, &QPushButton::clicked, this, [this]{switchTrack(-1);});
    control->addWidget(prev);
    m_play = new CircleButton("play", "播放 / 暂停");
    m_play->setObjectName("playButton");
    m_play->setFixedSize(42, 42);
    m_play->setSymbol("play", Qt::white);
    connect(m_play, &QAbstractButton::clicked, m_player, &PlaybackFacade::toggle);
    control->addWidget(m_play);
    auto *next = new IconButton("next", "下一首");
    connect(next, &QPushButton::clicked, this, [this]{switchTrack(1);});
    control->addWidget(next);
    control->addWidget(m_mode);
    control->addStretch();
    center->addLayout(control);
    auto *progress = new QHBoxLayout;
    progress->setSpacing(8);
    m_time = label("0:00", "time");
    m_duration = label("0:00", "time");
    m_time->setFixedWidth(40);
    m_duration->setFixedWidth(40);
    m_seek = new Slider;
    m_seek->setObjectName("seekSlider");
    m_seek->setEnabled(false);
    progress->addWidget(m_time);
    progress->addWidget(m_seek, 1);
    progress->addWidget(m_duration);
    center->addLayout(progress);
    connect(m_seek, &QSlider::sliderReleased, this, [this] { m_player->seek(m_seek->value()); });
    for(auto *button:middle->findChildren<QAbstractButton *>()){
        button->setToolTip({});button->setProperty("noHoverTip",true);
    }
    h->addWidget(middle, 1);
    auto *right = new QWidget;
    right->setFixedWidth(320);
    right->setObjectName("playerRight");auto *volumeHost=m_volumeHost=new QWidget(right);volumeHost->setObjectName("volumeControls");volumeHost->setFixedHeight(36);volumeHost->setGeometry(0,0,320,36);
    m_volumeMotion=new QPropertyAnimation(volumeHost,"pos",this);m_volumeMotion->setDuration(300);m_volumeMotion->setEasingCurve(QEasingCurve::OutCubic);
    auto *volume = new QHBoxLayout(volumeHost);
    volume->setContentsMargins(0, 0, 0, 0);
    volume->setSpacing(14);
    volume->addStretch();
    auto *vol = new IconButton("volume", "音量");
    vol->setObjectName("volumeButton");
    connect(vol, &QPushButton::clicked, this, [this] { m_volume->setValue(m_volume->value() ? 0 : 65); });
    volume->addWidget(vol);
    m_volume = new Slider;
    m_volume->setObjectName("volumeSlider");
    m_volume->setRange(0, 100);
    m_volume->setValue(m_player->volume());
    m_volume->setFixedWidth(80);
    connect(m_volume, &QSlider::valueChanged, m_player, &PlaybackFacade::setVolume);
    vol->setSymbol(volumeSymbol(m_volume->value()),m_ink);
    connect(m_volume,&QSlider::valueChanged,vol,[this,vol](int value){vol->setSymbol(volumeSymbol(value),m_ink);});
    volume->addWidget(m_volume);
    auto *expand = m_playbackToggle = new IconButton("expand", "展开播放");
    expand->setProperty("playbackToggle",true);
    connect(expand,&QPushButton::clicked,this,[this]{if(m_playerExpanded)closeNowPlaying();else showNowPlaying();});
    volume->addWidget(expand);
    h->addWidget(right);
    return bar;
}
QWidget *Window::buildBulkBar(){
    auto *bar=new QWidget;bar->setObjectName("bulkBar");bar->setMinimumHeight(98);auto *layout=new QVBoxLayout(bar);layout->setContentsMargins(22,10,22,10);layout->setSpacing(4);
    auto *top=new QHBoxLayout;m_bulkCount=label("已选 0 首歌曲","songTitle");m_bulkCount->setObjectName("bulkCount");top->addWidget(m_bulkCount);auto *all=button("全选","plain");all->setObjectName("bulkSelectAll");top->addWidget(all);auto *clear=button("取消选择","plain");clear->setObjectName("bulkClear");top->addWidget(clear);top->addStretch();auto *lyrics=button("查找歌词","plain");lyrics->setObjectName("bulkLyrics");lyrics->setProperty("plainAction",true);top->addWidget(lyrics);m_bulkActions<<lyrics;auto *exit=button("退出多选","plain");exit->setObjectName("bulkExit");exit->setProperty("plainAction",true);top->addWidget(exit);layout->addLayout(top);
    connect(all,&QPushButton::clicked,this,[this]{if(m_bulkTable)m_bulkTable->selectAll();});connect(clear,&QPushButton::clicked,this,[this]{if(m_bulkTable)m_bulkTable->clearSelection();});connect(exit,&QPushButton::clicked,this,&Window::exitBulk);connect(lyrics,&QPushButton::clicked,this,[this]{if(m_bulkTable)lookupLyrics(m_bulkTable->selectedIds(),true);});
    auto *actions=new QHBoxLayout;m_bulkDelete=button("删除");m_bulkDelete->setObjectName("bulkDelete");actions->addWidget(m_bulkDelete);auto *unlike=button("取消喜欢");unlike->setObjectName("bulkUnlike");actions->addWidget(unlike);auto *like=button("全部喜欢");like->setObjectName("bulkLike");actions->addWidget(like);auto *add=button("添加至歌单");add->setObjectName("bulkAddPlaylist");actions->addWidget(add);auto *create=button("新建歌单并加入");create->setObjectName("bulkNewPlaylist");actions->addWidget(create);m_bulkRemove=button("从歌单移除");m_bulkRemove->setObjectName("bulkRemovePlaylist");actions->addWidget(m_bulkRemove);actions->addStretch();layout->addLayout(actions);m_bulkActions<<unlike<<like<<add<<create;
    connect(m_bulkDelete,&QPushButton::clicked,this,&Window::batchDelete);connect(unlike,&QPushButton::clicked,this,[this]{if(m_bulkTable)check(m_library->favorites(m_bulkTable->selectedIds(),false));});connect(like,&QPushButton::clicked,this,[this]{if(m_bulkTable)check(m_library->favorites(m_bulkTable->selectedIds(),true));});
    connect(add,&QPushButton::clicked,this,[this,add]{if(!m_bulkTable)return;const auto ids=m_bulkTable->selectedIds();RoundedMenu menu(this);for(const auto &playlist:m_library->playlists())connect(menu.addAction(playlist.name),&QAction::triggered,this,[this,playlist,ids]{check(m_library->addToPlaylist(playlist.id,ids));});if(menu.isEmpty()){auto *empty=menu.addAction("还没有歌单，请先新建");empty->setEnabled(false);}menu.exec(add->mapToGlobal(QPoint(0,0))-QPoint(0,menu.sizeHint().height()));});
    connect(create,&QPushButton::clicked,this,[this]{if(m_bulkTable)newPlaylistWith(m_bulkTable->selectedIds());});connect(m_bulkRemove,&QPushButton::clicked,this,[this]{if(m_bulkTable)removeFromPlaylist(m_bulkTable->selectedIds(),m_bulkScope);});return bar;
}
void Window::enterBulk(SongTable *table){if(!table||!table->rowCount())return;exitBulk();m_bulkTable=table;m_bulkScope=table==m_songs?m_scope:0;table->setMultiSelect(true);m_bottomStack->setCurrentIndex(1);if(m_nextCompact)m_nextCompact->present(false,false);updateBulk();}
void Window::exitBulk(){if(!m_bulkTable)return;m_bulkTable->setMultiSelect(false);m_bulkTable=nullptr;if(m_bottomStack)m_bottomStack->setCurrentIndex(0);if(m_nextCompact&&!m_playerExpanded)m_nextCompact->present(true,false);}
void Window::updateBulk(){if(!m_bulkTable||!m_bulkCount)return;const int count=m_bulkTable->selectedIds().size();m_bulkCount->setText(QString("已选 %1 首歌曲").arg(count));for(auto *action:m_bulkActions)action->setEnabled(count>0);m_bulkDelete->setEnabled(count>0&&m_bulkTable==m_songs&&m_bulkScope==0);m_bulkRemove->setEnabled(count>0&&m_bulkTable==m_songs&&m_bulkScope>0);}
void Window::batchDelete(){
    if(!m_bulkTable||m_bulkTable!=m_songs||m_bulkScope!=0)return;const auto ids=m_bulkTable->selectedIds();if(ids.isEmpty())return;
    QMessageBox dialog(QMessageBox::Warning,"删除所选歌曲",QString("删除 %1 首歌曲、它们的歌单关联和播放统计，并删除软件保存的音乐副本？\n导入前的原始音乐文件会保留。").arg(ids.size()),QMessageBox::NoButton,this);auto *remove=dialog.addButton("删除软件内副本",QMessageBox::DestructiveRole);remove->setObjectName("confirmManagedDelete");auto *cancel=dialog.addButton("取消",QMessageBox::RejectRole);dialog.setDefaultButton(cancel);dialog.exec();if(dialog.clickedButton()!=remove)return;
    m_player->forget(ids);if(check(m_library->removeManaged(ids))){exitBulk();if(!m_library->lastError().isEmpty())toast(m_library->lastError());}
}
void Window::removeFromPlaylist(const QVector<qint64> &ids,qint64 playlist){
    if(ids.isEmpty()||playlist<=0)return;
    QMessageBox dialog(QMessageBox::Question,"从歌单移除",QString("从当前歌单移除 %1 首歌曲。是否同时从资料库删除，并删除软件保存的音乐副本？\n同时删除会清理其他歌单中的关联；导入前的原始文件会保留。").arg(ids.size()),QMessageBox::NoButton,this);
    auto *only=dialog.addButton("仅从当前歌单移除",QMessageBox::AcceptRole);only->setObjectName("removePlaylistOnly");auto *remove=dialog.addButton("同时删除软件内音乐",QMessageBox::DestructiveRole);remove->setObjectName("removePlaylistDelete");dialog.addButton("取消",QMessageBox::RejectRole);dialog.setDefaultButton(only);dialog.exec();
    if(dialog.clickedButton()==only){if(check(m_library->removeFromPlaylist(playlist,ids)))exitBulk();}else if(dialog.clickedButton()==remove){m_player->forget(ids);if(check(m_library->removeManaged(ids)))exitBulk();}
}
void Window::newPlaylistWith(const QVector<qint64> &ids){if(ids.isEmpty())return;bool ok;const auto name=QInputDialog::getText(this,"新建歌单并加入","歌单名称",QLineEdit::Normal,"新歌单",&ok);if(ok&&!name.trimmed().isEmpty()){const auto id=m_library->createPlaylist(name.trimmed().left(120));if(id)check(m_library->addToPlaylist(id,ids));else check(false);}}
void Window::refreshFilters(){
    if(!m_artistFilter||!m_albumFilter||m_filterRefreshing)return;m_filterRefreshing=true;const auto tracks=m_library->tracks();QStringList artists,albums;
    for(const auto &t:tracks){artists<<t.artist;if(!t.artist.contains("AC/DC",Qt::CaseInsensitive))for(const auto &name:t.artist.split('/',Qt::SkipEmptyParts))artists<<name.trimmed();}m_artistFilter->setOptions(artists);const auto artist=m_artistFilter->value();
    for(const auto &t:tracks){bool matches=artist.isEmpty()||t.artist.compare(artist,Qt::CaseInsensitive)==0;if(!t.artist.contains("AC/DC",Qt::CaseInsensitive))for(const auto &name:t.artist.split('/',Qt::SkipEmptyParts))if(name.trimmed().compare(artist,Qt::CaseInsensitive)==0)matches=true;if(matches)albums<<t.album;}m_albumFilter->setOptions(albums);m_filterRefreshing=false;
}
void Window::lookupLyrics(const QVector<qint64> &ids,bool pinExplicit){if(ids.isEmpty())return;LyricsLookup dialog(m_library,ids,this,QUrl("https://lrclib.net/api"),{},pinExplicit,m_aiRuntime);dialog.exec();refreshTrack();}
void Window::lookupMissingLyrics(){QVector<qint64> ids;for(const auto &t:m_library->tracks())ids<<t.id;if(ids.isEmpty()){toast("资料库中还没有歌曲。");return;}lookupLyrics(ids,false);}
void Window::saveWindowSize(){if(m_restoringWindow||!m_library||!m_normalSize.isValid())return;m_library->setSettings({{"windowWidth",m_normalSize.width()},{"windowHeight",m_normalSize.height()},{"windowMaximized",isMaximized()}});}
void Window::changeEvent(QEvent *event){QMainWindow::changeEvent(event);if(event->type()==QEvent::WindowStateChange&&!m_restoringWindow&&m_sizeSaveTimer)m_sizeSaveTimer->start();}
QVector<Track> Window::visibleTracks() const {
    auto tracks=m_library->tracks(m_scope,m_search?m_search->text():QString());
    if(m_scope==0&&m_artistFilter&&m_albumFilter){const auto artist=m_artistFilter->value(),album=m_albumFilter->value();tracks.erase(std::remove_if(tracks.begin(),tracks.end(),[&](const Track &t){const auto artists=t.artist.contains("AC/DC",Qt::CaseInsensitive)?QStringList{t.artist}:t.artist.split('/',Qt::SkipEmptyParts);bool matches=artist.isEmpty()||t.artist.compare(artist,Qt::CaseInsensitive)==0;for(const auto &name:artists)if(name.trimmed().compare(artist,Qt::CaseInsensitive)==0)matches=true;return !matches||(!album.isEmpty()&&t.album.compare(album,Qt::CaseInsensitive)!=0);}),tracks.end());}
    return tracks;
}
void Window::refreshPlaylists() {
    QSignalBlocker block(m_lists);
    m_lists->clear();
    m_cards->clear();
    for (const auto &p : m_library->playlists()) {
        auto *it = new QListWidgetItem(symbol("playlist", m_sideInk), p.name, m_lists);
        it->setData(Qt::UserRole, p.id);
        it->setSizeHint(QSize(165, 33));
        QString seed = p.name;
        QString path = m_library->absolute(p.cover);
        if (path.isEmpty()) {
            auto ts = m_library->tracks(p.id);
            if (!ts.isEmpty()) {
                path = m_library->absolute(ts[0].cover);
                seed = ts[0].title;
            }
        }
        auto *card = new QListWidgetItem(QIcon(coverPixmap(path, seed, 350)),
                                         QString("%1\n%2 首歌曲").arg(p.name).arg(p.count), m_cards);
        card->setData(Qt::UserRole, p.id);

    }
    updateNavigation();
}
void Window::updateLibrarySummary(const QVector<Track> &tracks){
    qint64 duration=0;
    if(m_scope!=0)for(const auto &track:tracks)duration+=track.duration;
    const auto total=m_scope==0?SongTable::fileSizeText(m_songs->totalFileSize()):durationText(duration);
    m_subtitle->setText(QString("%1 首歌曲  ·  %2  ·  本地音乐资料库").arg(tracks.size()).arg(total));
}
void Window::refresh() {
    if (m_refreshing)
        return;
    m_refreshing = true;
    refreshFilters();
    refreshPlaylists();
    QString name = "本地音乐";
    if (m_scope == -1)
        name = "我喜欢";
    if (m_scope > 0) {
        bool found = false;
        for (const auto &p : m_library->playlists())
            if (p.id == m_scope) {
                found = true;
                name = p.name;
                break;
            }
        if (!found)
            m_scope = 0;
    }
    auto ts = visibleTracks();
    m_title->setText(name);
    m_songs->setColumnsMode(m_scope==0?SongTable::LocalMetadata:SongTable::Statistics);
    m_songs->setSongs(ts, m_library->root(), m_player->current().id);
    updateLibrarySummary(ts);
    m_songStack->setCurrentIndex(ts.isEmpty() ? 1 : 0);
    m_libraryActions->setVisible(m_scope<=0);
    m_filterHost->setVisible(m_scope==0);
    m_playlistHeader->setVisible(m_scope>0);
    m_subtitle->setVisible(m_scope<=0);
    if(m_scope>0)for(const auto &p:m_library->playlists())if(p.id==m_scope)m_playlistHeader->setPlaylist(p,m_library->root());
    updateNavigation();
    updateProfileTabs();
    m_profileName->setText(m_library->setting("nickname", "Relaxer Listener").toString());
    m_avatar->setCover(m_library->absolute(m_library->setting("avatar").toString()), "Relaxer Listener");
    auto recent = m_library->recent();
    if (m_view == "top") {
        recent = m_library->tracks();
        std::stable_sort(recent.begin(), recent.end(),
                         [](const auto &a, const auto &b) { return a.plays > b.plays; });
    }
    m_recent->setSongs(recent, m_library->root(), m_player->current().id);
    refreshStats();
    refreshTrack();
    m_refreshing = false;
}
void Window::updateProfileTabs(){if(auto *b=findChild<QPushButton *>("profileRecent"))b->setChecked(m_view!="top");if(auto *b=findChild<QPushButton *>("profileFrequent"))b->setChecked(m_view=="top");}
void Window::refreshStats() {
    if(m_profileHeader)m_profileHeader->setDescription(m_library->setting("profileDescription","收藏、回忆，还有反复听过的那些歌。").toString());
    m_statSongs->setText(QString::number(m_library->tracks().size()));
    m_statPlays->setText(QString::number(m_library->totalPlays()));
    auto ms = m_library->totalHeard();
    m_statTime->setText(ms >= 3600000 ? QString("%1 小时 %2 分").arg(ms / 3600000).arg((ms / 60000) % 60)
                                      : QString("%1 分钟").arg(ms / 60000));
    if(m_profileHeader)m_profileHeader->update();
    m_songs->updateStats(visibleTracks());
    m_recent->updateStats(m_library->tracks());
    if(m_scope>0)for(const auto &p:m_library->playlists())if(p.id==m_scope)m_playlistHeader->setPlaylist(p,m_library->root());
}
void Window::refreshTrack() {
    auto t = m_player->current();
    if(m_loadedLyricId!=t.id&&m_motion&&m_motion->isVisible()){++m_motionRevision;settleMotion();}
    if(m_queueFlight&&m_queueFlight->isVisible()&&m_loadedLyricId!=t.id)m_queueFlight->stop();
    auto title = t.id ? t.title : "Relaxer";
    auto artist = t.id ? t.artist : "选一首歌，开始放松。";
    m_bottomTitle->setText(
        m_bottomTitle->fontMetrics().elidedText(title, Qt::ElideRight, m_bottomTitle->width()));
    m_bottomTitle->setToolTip(m_bottomTitle->text()!=title?title:QString());
    m_bottomArtist->setText(
        m_bottomArtist->fontMetrics().elidedText(artist, Qt::ElideRight, m_bottomArtist->width()));
    m_bottomArtist->setToolTip(m_bottomArtist->text()!=artist?artist:QString());
    m_nowTitle->setText(t.id ? t.title : "还没有音乐正在播放");
    m_nowArtist->setText(t.id ? t.artist + "  ·  " + t.album : "选一首喜欢的歌，慢慢听。");
    m_bottomCover->setCover(m_library->absolute(t.cover), title);
    m_nowCover->setCover(m_library->absolute(t.cover), title);
    updatePlayerBackground();
    m_like->setSymbol(t.favorite ? "liked" : "heart", t.favorite ? m_accent : m_ink);
    m_like->setEnabled(t.id);
    Lyrics lyric;QByteArray lyricData;
    if (!t.lyrics.isEmpty()) {
        auto document = LyricsDocumentReader::readDocument(
            m_library->absolute(t.lyrics), LyricsDocumentReader::MaximumBytes,
            LyricsDocumentReader::OversizePolicy::ReadPrefix);
        lyricData = std::move(document.file.bytes);
        lyric = std::move(document.lyrics);
    }
    if(m_library->isInstrumental(t.id)){lyricData=QString("纯音乐，请欣赏").toUtf8();lyric=Lyrics::parse(lyricData);}
    const bool timeline=lyric.timed&&!lyric.instrumental;
    m_lyricTools->setAvailable(t.id&&!lyric.instrumental,timeline);m_lyricDelay->setEnabled(timeline);if(m_loadedLyricId!=t.id)m_lyricTools->invalidateBackdrop();
    {QSignalBlocker block(m_lyricDelay);m_lyricDelay->setValue(m_library->lyricOffset(t.id)/1000.);}
    if(m_loadedLyricId!=t.id||m_loadedLyricData!=lyricData){m_loadedLyricId=t.id;m_loadedLyricData=lyricData;m_lyrics->setLyrics(std::move(lyric));}
    m_lyrics->setOffset(m_library->lyricOffset(t.id));
    m_lyrics->setPosition(m_player->position());
    refreshQueue();
    if (m_tray)
        m_tray->setToolTip(("Relaxer · " + title).left(120));
}
void Window::refreshQueue(){
    if(!m_nextCompact||!m_nextLarge)return;
    const auto ids=m_player->upcoming();QHash<qint64,Track> byId;
    for(const auto &t:m_library->tracks())byId[t.id]=t;
    QVector<Track> tracks;for(auto id:ids){auto t=byId.value(id);if(!t.id){t.id=id;t.title="歌曲已移除";}tracks<<t;}
    m_nextCompact->setTracks(tracks);m_nextLarge->setTracks(tracks);m_lastQueueMode=m_player->mode();updateQueueGeometry();
    if(isVisible()){const bool big=m_playerExpanded;m_nextCompact->present(!big&&m_bottomStack->currentIndex()==0,false);m_nextLarge->present(big,false);}
    if(m_queueBackgroundTimer)m_queueBackgroundTimer->start(100);
}
void Window::updateQueueGeometry(){
    if(!m_nextCompact||!centralWidget()||!m_bottomStack)return;auto *root=centralWidget();const auto bottom=m_bottomStack->mapTo(root,QPoint());
    const int compactWidth=m_nextCompact->preferredWidth(),largeWidth=m_nextLarge->preferredWidth();
    m_nextCompact->setAnchor(QPoint(root->width()-8-compactWidth,bottom.y()+m_bottomStack->height()-4),compactWidth,m_bottomStack->height());
    m_nextLarge->setAnchor(QPoint(8,bottom.y()+m_bottomStack->height()-4),largeWidth,m_bottomStack->height());
    updateVolumePosition();
}
MovingArtwork Window::capturePlayingArtwork() const {
    MovingArtwork art;auto *root=centralWidget();
    art.coverOld=artworkRect(m_nowCover,root);
    const auto image=transparentSnapshot(m_nowCover);const auto local=artworkRect(m_nowCover,m_nowCover);const qreal dpr=image.devicePixelRatioF();
    art.coverOutgoing=image.copy(QRect(qRound(local.x()*dpr),qRound(local.y()*dpr),qRound(local.width()*dpr),qRound(local.height()*dpr)));art.coverOutgoing.setDevicePixelRatio(dpr);
    return art;
}
void Window::startTrackAnimation(MovingArtwork art,int direction){
    if(m_motion)m_motion->stop();
    m_nowPage->layout()->activate();auto *root=centralWidget();const auto track=m_player->current();
    art.cover=coverPixmap(m_library->absolute(track.cover),track.title,700);art.coverTo=artworkRect(m_nowCover,root);
    m_nowTitle->show();m_nowArtist->show();
    m_queueFlight->start(std::move(art),[this]{m_nowCover->show();m_nowTitle->show();m_nowArtist->show();if(m_queueBackgroundTimer)m_queueBackgroundTimer->start(0);},direction);
    if(m_topBar)m_topBar->raise();
}
void Window::switchTrack(int direction){
    if(!m_playerExpanded){if(direction>0)m_player->next();else m_player->previous();return;}
    ++m_motionRevision;settleMotion();const auto previous=m_player->current().id;auto art=capturePlayingArtwork();
    m_nowCover->hide();
    if(direction>0)m_player->next();else m_player->previous();
    if(!m_player->current().id||m_player->current().id==previous){m_nowCover->show();m_nowTitle->show();m_nowArtist->show();return;}
    startTrackAnimation(std::move(art),direction);
}
void Window::activateUpcoming(int index,bool large){
    auto *panel=large?m_nextLarge:m_nextCompact;const auto wanted=panel->trackAt(index);if(!wanted.id)return;
    const bool fly=large&&m_playerExpanded;
    MovingArtwork art;if(fly){++m_motionRevision;settleMotion();art=capturePlayingArtwork();const auto incoming=panel->artwork(index);art.coverFrom=incoming.coverFrom;m_nowCover->hide();}
    m_player->playUpcoming(index);
    if(!fly)return;if(m_player->current().id!=wanted.id){m_nowCover->show();m_nowTitle->show();m_nowArtist->show();return;}
    m_nextLarge->setExpanded(false);
    startTrackAnimation(std::move(art));
}
void Window::updateNavigation(){
    if(!m_pages||!m_nav||!m_lists)return;
    const int page=m_pages->currentIndex();int row=-1;
    if(page==0)row=m_scope==0?0:m_scope==-1?4:-1;
    else if(page>=1&&page<=3)row=page;
    else if(page==5||(m_appearancePage&&m_pages->currentWidget()==m_appearancePage)||(m_aiPage&&m_pages->currentWidget()==m_aiPage))row=3;
    QSignalBlocker navBlock(m_nav),listBlock(m_lists);
    m_nav->clearSelection();m_nav->setCurrentRow(row);
    m_lists->clearSelection();m_lists->setCurrentRow(-1);
    if(page==0&&m_scope>0)for(int i=0;i<m_lists->count();++i)
        if(m_lists->item(i)->data(Qt::UserRole).toLongLong()==m_scope){m_lists->setCurrentRow(i);break;}
}
void Window::settleMotion(){
    if(m_motion)m_motion->stop();
    if(m_queueFlight)m_queueFlight->stop();
    const bool expanded=m_playerExpanded;
    if(m_nowPage){m_nowPage->setVisible(expanded);if(expanded)m_nowPage->raise();}
    for(QWidget *widget:QList<QWidget *>{static_cast<QWidget *>(m_bottomCover),m_bottomTitle,m_bottomArtist})widget->setVisible(!expanded);
    for(QWidget *widget:QList<QWidget *>{static_cast<QWidget *>(m_nowCover),m_nowTitle,m_nowArtist})widget->show();
    if(m_like)m_like->show();
    if(m_queueBackgroundTimer)m_queueBackgroundTimer->start(0);
}
void Window::updatePlaybackToggle(){
    if(!m_playbackToggle)return;
    m_playbackToggle->setSymbol(m_playerExpanded?"contract":"expand",m_ink);
    const QString name=m_playerExpanded?"收起播放":"展开播放";
    m_playbackToggle->setToolTip(name);m_playbackToggle->setAccessibleName(name);
}
void Window::updateVolumePosition(bool animate){
    if(!m_volumeHost||!m_volumeMotion||m_volumeTransition)return;
    const QPoint target(0,m_playerExpanded?qMax(0,(m_volumeHost->parentWidget()->height()-m_volumeHost->height())/2):0);
    const bool running=m_volumeMotion->state()==QAbstractAnimation::Running;
    if(running&&m_volumeMotion->endValue().toPoint()==target)return;
    const auto start=m_volumeHost->pos();m_volumeMotion->stop();
    if((animate||running)&&isVisible()&&start!=target){
        m_volumeMotion->setStartValue(start);m_volumeMotion->setEndValue(target);m_volumeMotion->start();
    }else m_volumeHost->move(target);
}
void Window::closeNowPlaying(){if(m_playerExpanded)setPage(m_returnPage);}
void Window::setPlayerPage(bool expanded,int page){
    m_volumeTransition=true;
    if(m_volumeMotion)m_volumeMotion->stop();
    if(m_queueFlight&&m_queueFlight->isVisible())settleMotion();
    const bool continuing=m_motion->isVisible()&&m_motion->isPlayer();const auto travelling=continuing?m_motion->currentArtwork():MovingArtwork();
    const qreal initialReveal=continuing?m_motion->currentReveal():-1,initialDetails=continuing?m_motion->currentDetails():-1;
    const quint64 revision=++m_motionRevision;auto *root=centralWidget();const QRect area(m_contentHost->mapTo(root,QPoint()),m_contentHost->size());
    const auto previous=m_motion->isVisible()&&m_motion->isPlayer()?m_motion->currentPage():m_contentHost->grab();settleMotion();m_lyricTools->closeTools();
    if(expanded)m_returnPage=m_pages->currentIndex();else if(page!=m_returnPage)m_pages->setCurrentIndex(page);
    m_playerExpanded=expanded;updatePlaybackToggle();m_nowPage->setGeometry(m_contentHost->rect());m_nowPage->show();m_nowPage->raise();m_nowPage->layout()->activate();updateNavigation();
    m_contentHost->layout()->activate();updateQueueGeometry();
    m_nextCompact->present(!expanded&&m_bottomStack->currentIndex()==0,isVisible());m_nextLarge->present(expanded,isVisible());
    m_volumeTransition=false;updateVolumePosition(true);
    if(!isVisible()){settleMotion();return;}
    m_motion->hold(area,previous);
    QTimer::singleShot(0,this,[this,revision,expanded,area,travelling,continuing,initialReveal,initialDetails]{
        if(revision!=m_motionRevision)return;auto *root=centralWidget();m_nowPage->layout()->activate();if(m_lyricTools->layout())m_lyricTools->layout()->activate();
        MovingArtwork art;const auto position=[root](QWidget *widget){return QRect(widget->mapTo(root,QPoint()),widget->size());};
        art.coverFrom=expanded?artworkRect(m_bottomCover,root):artworkRect(m_nowCover,root);art.coverTo=expanded?artworkRect(m_nowCover,root):artworkRect(m_bottomCover,root);
        art.titleFrom=position(expanded?m_bottomTitle:m_nowTitle);art.titleTo=position(expanded?m_nowTitle:m_bottomTitle);
        art.artistFrom=position(expanded?m_bottomArtist:m_nowArtist);art.artistTo=position(expanded?m_nowArtist:m_bottomArtist);
        const auto track=m_player->current();art.cover=coverPixmap(m_library->absolute(track.cover),track.id?track.title:QString("Relaxer"),700);
        art.titleSmall=transparentSnapshot(m_bottomTitle);art.titleLarge=transparentSnapshot(m_nowTitle);art.artistSmall=transparentSnapshot(m_bottomArtist);art.artistLarge=transparentSnapshot(m_nowArtist);
        m_bottomCover->hide();m_bottomTitle->hide();m_bottomArtist->hide();m_nowCover->hide();m_nowTitle->hide();m_nowArtist->hide();
        if(expanded)m_nextLarge->invalidateBackground();else m_nextCompact->invalidateBackground();
        const auto player=m_nowPage->grab();m_nowPage->hide();const auto ordinary=m_contentHost->grab();m_nowPage->show();
        QPixmap background(m_nowPage->size()*m_nowPage->devicePixelRatioF());background.setDevicePixelRatio(m_nowPage->devicePixelRatioF());background.fill(Qt::transparent);m_nowPage->render(&background,QPoint(),QRegion(),QWidget::DrawWindowBackground);
        if(continuing){art.coverFrom=travelling.coverFrom;art.titleFrom=travelling.titleFrom;art.artistFrom=travelling.artistFrom;}
        const auto done=[this,revision]{if(revision==m_motionRevision)settleMotion();};
        m_motion->startPlayer(expanded?ordinary:player,expanded?player:ordinary,background,art,expanded,done,initialReveal,initialDetails);
        if(m_topBar)m_topBar->raise();
        QTimer::singleShot(800,this,[this,revision]{if(revision==m_motionRevision&&m_motion->isVisible()&&m_motion->isPlayer())settleMotion();});
    });
}
void Window::setPage(int page){
    if(m_appearancePage&&m_pages->currentWidget()==m_appearancePage&&page!=4&&page!=m_pages->indexOf(m_appearancePage)){
        if(!m_appearancePage->confirmLeave())return;
    }
    exitBulk();if(page==4||m_playerExpanded){if(page==4&&m_playerExpanded)return;setPlayerPage(page==4,page);return;}
    if(m_pages->currentIndex()==page&&(page!=0||m_presentedScope==m_scope)){updateNavigation();return;}
    const quint64 revision=++m_motionRevision;const auto previous=m_motion->isVisible()?m_motion->currentContent():transparentSnapshot(m_pages);settleMotion();
    m_pages->setCurrentIndex(page);m_presentedScope=m_scope;updateNavigation();m_pages->layout()->activate();updateQueueGeometry();
    if(!isVisible())return;auto *root=centralWidget();const QRect area(m_paperSurface->mapTo(root,QPoint()),m_paperSurface->size());
    m_pages->hide();const auto background=m_paperSurface->grab();m_pages->show();m_motion->hold(area,previous,background);
    QTimer::singleShot(0,this,[this,revision]{if(revision!=m_motionRevision)return;m_pages->layout()->activate();if(auto *layout=m_pages->currentWidget()->layout())layout->activate();const auto next=transparentSnapshot(m_pages);m_pages->hide();const auto background=m_paperSurface->grab();m_pages->show();m_motion->startPage(next,background,[this,revision]{if(revision==m_motionRevision)settleMotion();});if(m_topBar)m_topBar->raise();});
}
void Window::showLibrary(){m_scope=0;m_view="local";setPage(0);m_filterRefreshing=true;m_artistFilter->reset();m_albumFilter->reset();m_filterRefreshing=false;m_search->clear();refresh();}
void Window::editPlaylistTags(){
    const auto id=m_scope;if(id<=0)return;
    for(const auto &p:m_library->playlists())if(p.id==id){PlaylistTagsDialog dialog(p.tags,this);
        if(dialog.exec()==QDialog::Accepted)check(m_library->playlistTags(id,dialog.selectedTags()));
        return;
    }
}
void Window::editPlaylistDescription(){
    const auto id=m_scope;if(id<=0)return;
    for(const auto &p:m_library->playlists())if(p.id==id){PlaylistDescriptionDialog dialog(p.description,this);
        if(dialog.exec()==QDialog::Accepted)check(m_library->playlistDescription(id,dialog.description()));
        return;
    }
}
void Window::selectScope(qint64 id) {
    m_scope = id;
    m_playlistScrolled=false;m_songs->verticalScrollBar()->setValue(0);m_playlistHeader->collapseTo(0);
    setPage(0);
    m_search->clear();
    refresh();
}
void Window::showPlaylists() {
    setPage(1);
    refreshPlaylists();
}
void Window::showProfile() {
    setPage(2);
    refresh();
}
void Window::showSettings(){bool returning=m_pages->currentIndex()==5||(m_appearancePage&&m_pages->currentWidget()==m_appearancePage)||(m_aiPage&&m_pages->currentWidget()==m_aiPage);setPage(3);if(returning&&m_pages->currentIndex()==3){setFocus(Qt::OtherFocusReason);auto *scroll=findChild<QScrollArea *>("settingsScroll");scroll->verticalScrollBar()->setValue(m_settingsScrollPosition);QTimer::singleShot(0,this,[this,scroll]{if(m_pages->currentIndex()==3)scroll->verticalScrollBar()->setValue(m_settingsScrollPosition);});}}
void Window::showVersionLog(){if(m_pages->currentIndex()==3)m_settingsScrollPosition=findChild<QScrollArea *>("settingsScroll")->verticalScrollBar()->value();setPage(5);}
void Window::showNowPlaying() {
    setPage(4);
}
void Window::createPlaylist() {
    bool ok;
    auto n = QInputDialog::getText(this, "创建歌单", "歌单名称", QLineEdit::Normal, "新歌单", &ok);
    if (ok && !n.trimmed().isEmpty()) {
        auto id = m_library->createPlaylist(n.left(120));
        if (id)
            selectScope(id);
        else
            check(false);
    }
}
void Window::playlistMenu(qint64 id, const QPoint &pos) {
    if (id <= 0)
        return;
    QString name;
    for (auto p : m_library->playlists())
        if (p.id == id)
            name = p.name;
    RoundedMenu menu(this);
    auto *open = menu.addAction("打开歌单");
    auto *rename = menu.addAction("重命名…");
    auto *cover = menu.addAction("设置封面…");
    menu.addSeparator();
    auto *remove = menu.addAction("删除歌单");
    auto *picked = menu.exec(pos);
    if (picked == open)
        selectScope(id);
    else if (picked == rename) {
        bool ok;
        auto n = QInputDialog::getText(this, "重命名歌单", "名称", QLineEdit::Normal, name, &ok);
        if (ok && !n.trimmed().isEmpty())
            check(m_library->renamePlaylist(id, n.left(120)));
    } else if (picked == cover) {
        auto p = QFileDialog::getOpenFileName(this, "歌单封面", {}, "图片 (*.png *.jpg *.jpeg *.webp)");
        if (!p.isEmpty())
            check(m_library->playlistCover(id, p));
    } else if (picked == remove &&
               QMessageBox::question(this, "删除歌单", "删除「" + name + "」？音乐文件和播放统计会保留。",
                                     QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        check(m_library->deletePlaylist(id));
}
void Window::songMenu(SongTable *table, const QPoint &pos) {
    auto ids = table->selectedIds();
    if (ids.isEmpty())
        return;
    if(m_bulkTable==table){RoundedMenu selection(this);connect(selection.addAction("全选"),&QAction::triggered,table,&QTableWidget::selectAll);connect(selection.addAction("取消选择"),&QAction::triggered,table,&QTableWidget::clearSelection);connect(selection.addAction("退出多选"),&QAction::triggered,this,&Window::exitBulk);selection.exec(table->viewport()->mapToGlobal(pos));return;}
    RoundedMenu menu(this);
    auto *multi=menu.addAction("多选");multi->setObjectName("enterMultiSelect");
    auto *findLyrics=menu.addAction("查找歌词…");findLyrics->setObjectName("findLyrics");
    menu.addSeparator();
    auto *play = menu.addAction("播放");
    auto *like = menu.addAction("收藏所选歌曲");
    auto *unlike = menu.addAction("取消收藏");
    auto *add = menu.addMenu("加入歌单");
    for (auto p : m_library->playlists()) {
        auto *a = add->addAction(p.name);
        connect(a, &QAction::triggered, this, [this, ids, p] { check(m_library->addToPlaylist(p.id, ids)); });
    }
    auto *create = add->addAction("创建新歌单并加入…");
    connect(create, &QAction::triggered, this, [this, ids] {
        bool ok;
        auto n = QInputDialog::getText(this, "创建歌单", "名称", QLineEdit::Normal, "新歌单", &ok);
        if (ok && !n.trimmed().isEmpty()) {
            auto id = m_library->createPlaylist(n.left(120));
            if (id)
                check(m_library->addToPlaylist(id, ids));
        }
    });
    QAction *art = nullptr, *lyrics = nullptr, *folder = nullptr;
    if (ids.size() == 1) {
        menu.addSeparator();
        art = menu.addAction("设置歌曲封面…");
        lyrics = menu.addAction("导入本地歌词…");
        folder = menu.addAction("打开音乐所在目录");
    }
    menu.addSeparator();
    QAction *from = nullptr;
    if (table == m_songs && m_scope > 0)
        from = menu.addAction("从此歌单移除");
    auto *remove = menu.addAction("从资料库移除…");
    auto *picked = menu.exec(table->viewport()->mapToGlobal(pos));
    if (!picked)
        return;
    if(picked==multi){enterBulk(table);return;}
    if(picked==findLyrics){lookupLyrics(ids,true);return;}
    if (picked == play)
        m_player->play(table->ids(), ids[0]);
    else if (picked == like || picked == unlike) {
        for (auto id : ids)
            check(m_library->favorite(id, picked == like));
    } else if (picked == art) {
        auto p = QFileDialog::getOpenFileName(this, "歌曲封面", {}, "图片 (*.png *.jpg *.jpeg *.webp)");
        if (!p.isEmpty()) {
            QPixmapCache::clear();
            check(m_library->updateArtwork(ids[0], p));
        }
    } else if (picked == lyrics) {
        auto p = QFileDialog::getOpenFileName(this, "导入歌词", {}, "歌词 (*.lrc *.txt)");
        if (!p.isEmpty())
            check(m_library->updateLyrics(ids[0], p));
    } else if (picked == folder)
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QFileInfo(m_library->absolute(m_library->track(ids[0]).file)).absolutePath()));
    else if (picked == from)
        removeFromPlaylist(ids,m_scope);
    else if (picked == remove &&
             QMessageBox::question(
                 this, "移除歌曲",
                 QString("从资料库移除 %1 首歌曲及其歌单关联和统计？\n管理目录中的音乐副本仍会保留。")
                     .arg(ids.size()),
                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_player->forget(ids);
        check(m_library->remove(ids));
    }
}
void Window::reorder(int direction) {
    if (m_scope <= 0)
        return;
    if (!m_search->text().isEmpty()) {
        toast("先清空搜索，再调整完整歌单顺序。");
        return;
    }
    auto ids = m_songs->ids();
    auto selected = m_songs->selectedIds();
    if (selected.isEmpty())
        return;
    if (direction < 0) {
        for (int i = 1; i < ids.size(); ++i)
            if (selected.contains(ids[i]) && !selected.contains(ids[i - 1]))
                ids.swapItemsAt(i, i - 1);
    } else {
        for (int i = ids.size() - 2; i >= 0; --i)
            if (selected.contains(ids[i]) && !selected.contains(ids[i + 1]))
                ids.swapItemsAt(i, i + 1);
    }
    check(m_library->reorder(m_scope, ids));
}
void Window::chooseFiles() {
    auto p = QFileDialog::getOpenFileNames(
        this, "导入本地音乐", QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        "音乐 (" + []{QStringList wildcards;for(const auto &ext:Importer::extensions())wildcards<<"*."+ext;return wildcards.join(' ');}() + ")");
    if (!p.isEmpty())
        importSources(p);
}
void Window::chooseFolder() {
    auto p = QFileDialog::getExistingDirectory(
        this, "导入音乐文件夹", QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if (!p.isEmpty())
        importSources({p});
}
void Window::importSources(const QStringList &paths) {
    const ImportRequest request{paths,m_scope>0?m_scope:0};
    if(!m_importCoordinator->start(request)){toast(m_importCoordinator->lastError());return;}
    findChild<QWidget *>("importPanel")->show();m_cancel->setEnabled(true);m_importProgress->setRange(0,0);
}
void Window::repairLegacyArtwork() {m_importCoordinator->repairLegacyArtwork();}
bool Window::check(bool ok) {
    if (!ok)
        QMessageBox::warning(this, "Relaxer",
                             m_library->lastError().isEmpty() ? "操作未完成，请检查文件或名称。"
                                                              : m_library->lastError());
    return ok;
}
void Window::toast(const QString &msg) {
    if(!centralWidget()||msg.isEmpty())return;
    if(!m_toastNotice){m_toastNotice=new QLabel(centralWidget());m_toastNotice->setObjectName("toastNotice");m_toastNotice->setTextFormat(Qt::PlainText);m_toastNotice->setWordWrap(true);m_toastNotice->setContentsMargins(16,10,16,10);m_toastNotice->setAttribute(Qt::WA_TransparentForMouseEvents);m_toastTimer.setObjectName("toastDismissTimer");m_toastTimer.setSingleShot(true);m_toastTimer.setInterval(7000);connect(&m_toastTimer,&QTimer::timeout,m_toastNotice,&QWidget::hide);}
    m_toastNotice->setText(msg);positionToast();m_toastNotice->show();m_toastNotice->raise();m_toastTimer.start();
}
void Window::positionToast(){
    if(!m_toastNotice||!centralWidget())return;auto *root=centralWidget();const auto metrics=m_toastNotice->fontMetrics();
    const int width=qBound(180,metrics.horizontalAdvance(m_toastNotice->text())+34,qMax(180,qMin(640,root->width()-32)));
    const int height=metrics.boundingRect(QRect(0,0,width-34,10000),Qt::TextWordWrap,m_toastNotice->text()).height()+24;
    const int bottom=m_bottomStack?m_bottomStack->mapTo(root,QPoint()).y():root->height();
    m_toastNotice->setGeometry((root->width()-width)/2,qMax(12,bottom-height-12),width,height);
    m_toastNotice->setStyleSheet(QString("QLabel#toastNotice{background:%1;color:%2;border:1px solid %3;border-radius:10px;}").arg(palette().color(QPalette::Base).name(),m_ink.name(),palette().color(QPalette::Mid).name()));
}
void Window::closeEvent(QCloseEvent *e) {
    saveWindowSize();
    if(!m_closePending&&!m_quitting&&m_tray&&m_library->setting("closeToTray",false).toBool()){
        hide();e->ignore();return;
    }
    if(!m_closePending&&m_appearancePage&&m_pages->currentWidget()==m_appearancePage&&!m_appearancePage->confirmLeave()){e->ignore();return;}
    if(!m_importCoordinator->isIdle()){
        e->ignore();m_closePending=true;m_cancel->setEnabled(false);m_importLabel->setText("正在结束当前文件并安全关闭…");
        m_importCoordinator->shutdown();return;
    }
    m_importCoordinator->shutdown();m_player->shutdown();e->accept();
}
void Window::dragEnterEvent(QDragEnterEvent *e) {
    if (e->mimeData()->hasUrls())
        for (auto u : e->mimeData()->urls())
            if (u.isLocalFile()) {
                e->acceptProposedAction();
                return;
            }
}
void Window::dropEvent(QDropEvent *e) {
    QStringList files;
    for (auto u : e->mimeData()->urls())
        if (u.isLocalFile())
            files << u.toLocalFile();
    if (!files.isEmpty()) {
        importSources(files);
        e->acceptProposedAction();
    }
}
void Window::keyPressEvent(QKeyEvent *e) {
    if(e->key()==Qt::Key_Escape&&!m_playerExpanded&&((m_appearancePage&&m_pages->currentWidget()==m_appearancePage)||(m_aiPage&&m_pages->currentWidget()==m_aiPage))){showSettings();return;}
    if(e->key()==Qt::Key_Escape&&m_bulkTable){exitBulk();e->accept();return;}
    if(e->key()==Qt::Key_Escape&&m_pages->currentIndex()==5){showSettings();e->accept();return;}
    if (e->key() == Qt::Key_Escape && m_playerExpanded) {
        closeNowPlaying();
        e->accept();
        return;
    }
    QMainWindow::keyPressEvent(e);
}
void Window::updateNativeFrameTheme(){
#ifdef Q_OS_WIN
    if(m_frameInitialized&&QGuiApplication::platformName()=="windows")
        client_frame::theme(reinterpret_cast<HWND>(winId()),m_dark);
#endif
}
void Window::showEvent(QShowEvent *event){
    QMainWindow::showEvent(event);
    if(m_topBar){m_topBar->move(centralWidget()->width()-m_topBar->width(),0);m_topBar->raise();}
    QTimer::singleShot(0,this,[this]{updateQueueGeometry();if(m_nextCompact)m_nextCompact->present(!m_playerExpanded&&m_bottomStack->currentIndex()==0,false);if(m_nextLarge)m_nextLarge->present(m_playerExpanded,false);});
#ifdef Q_OS_WIN
    if(!m_frameInitialized&&QGuiApplication::platformName()=="windows"){
        m_frameInitialized=true;client_frame::initialize(reinterpret_cast<HWND>(winId()),m_dark);
    }
#endif
}
void Window::resizeEvent(QResizeEvent *event){
    QMainWindow::resizeEvent(event);
    if(m_sizeSaveTimer&&isVisible()&&!m_restoringWindow&&!isMaximized()&&!isMinimized()){m_normalSize=size();m_sizeSaveTimer->start();}
    if((m_motion&&m_motion->isVisible())||(m_queueFlight&&m_queueFlight->isVisible())){++m_motionRevision;settleMotion();}
    if(m_topBar){m_topBar->move(centralWidget()->width()-m_topBar->width(),0);m_topBar->raise();}
    if(m_nowPage)m_nowPage->setGeometry(m_contentHost->rect());
    if(m_nextCompact){updateQueueGeometry();m_queueBackgroundTimer->start(100);}positionToast();
}
bool Window::isDragPosition(const QPoint &pos) const {
    QWidget *target=childAt(pos);
    for(auto *w=target;w&&w!=this;w=w->parentWidget()){
        if(qobject_cast<QAbstractButton *>(w)||qobject_cast<QLineEdit *>(w))return false;
        if(w->property("windowDragHandle").toBool())return true;
    }
    return pos.y()>=0&&pos.y()<24&&pos.x()>=6&&pos.x()<width()-6;
}
bool Window::eventFilter(QObject *object,QEvent *event){
    if(event->type()==QEvent::ToolTip&&object->property("noHoverTip").toBool()){event->accept();return true;}
    if(event->type()==QEvent::Resize){auto *widget=qobject_cast<QWidget *>(object);if(widget&&widget->objectName()=="settingsCard")roundedMask(widget,14);if(widget==m_contentHost&&m_nowPage){m_nowPage->setGeometry(m_contentHost->rect());if(m_nextCompact)updateQueueGeometry();}}
    if(event->type()==QEvent::Wheel&&object->property("playlistScroll").toBool()&&m_scope>0&&m_playlistHeader){
        auto *wheel=static_cast<QWheelEvent *>(event);const qreal delta=wheel->pixelDelta().isNull()?wheel->angleDelta().y()/120.*40:wheel->pixelDelta().y();
        if((delta<0&&m_playlistHeader->target()<1)||(delta>0&&m_songs->verticalScrollBar()->value()==0&&m_playlistHeader->target()>0)){m_playlistScrolled=delta<0;m_playlistHeader->collapseTo(delta<0?1:0);wheel->accept();return true;}
    }
    if(event->type()==QEvent::Wheel&&object->property("profileScroll").toBool()&&m_profileHeader){
        auto *wheel=static_cast<QWheelEvent *>(event);const qreal delta=wheel->pixelDelta().isNull()?wheel->angleDelta().y()/120.*40:wheel->pixelDelta().y();
        if((delta<0&&m_profileHeader->target()<1)||(delta>0&&m_recent->verticalScrollBar()->value()==0&&m_profileHeader->target()>0)){m_profileScrolled=delta<0;m_profileHeader->collapseTo(delta<0?1:0);wheel->accept();return true;}
    }
    if(event->type()==QEvent::MouseButtonRelease&&object->property("openPlaying").toBool()){
        if(static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton){showNowPlaying();return true;}
    }
    if(event->type()==QEvent::Show){
        auto *dialog=qobject_cast<QDialog *>(object);
        if(dialog&&isAncestorOf(dialog)&&dialog->isWindow()){
            dialog->setWindowOpacity(0);auto *animation=new QPropertyAnimation(dialog,"windowOpacity",dialog);
            animation->setDuration(140);animation->setStartValue(0.);animation->setEndValue(1.);
            animation->setEasingCurve(QEasingCurve::OutCubic);animation->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    auto *widget=qobject_cast<QWidget *>(object);
    if(widget&&widget->window()==this&&(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonDblClick)){
        auto *mouse=static_cast<QMouseEvent *>(event);const auto pos=mapFromGlobal(mouse->globalPosition().toPoint());
        if(mouse->button()==Qt::LeftButton){
            if(!isMaximized()&&!isFullScreen()&&!qobject_cast<QAbstractButton *>(widget)){
                Qt::Edges edges;
                if(pos.x()<6)edges|=Qt::LeftEdge;
                if(pos.x()>=width()-6)edges|=Qt::RightEdge;
                if(pos.y()<6)edges|=Qt::TopEdge;
                if(pos.y()>=height()-6)edges|=Qt::BottomEdge;
                if(edges&&windowHandle()&&windowHandle()->startSystemResize(edges))return true;
            }
            if(isDragPosition(pos)){
                if(event->type()==QEvent::MouseButtonDblClick){if(isMaximized())showNormal();else showMaximized();return true;}
                if(windowHandle()&&windowHandle()->startSystemMove())return true;
            }
        }
    }
    return QMainWindow::eventFilter(object,event);
}
bool Window::nativeEvent(const QByteArray &type,void *message,qintptr *result){
#ifdef Q_OS_WIN
    if(QGuiApplication::platformName()=="windows"){
        auto *msg=static_cast<MSG *>(message);auto *max=m_topBar?m_topBar->findChild<CaptionButton *>("windowMaximize"):nullptr;
        auto pointerPosition=[this]{POINT p{};GetCursorPos(&p);ScreenToClient(reinterpret_cast<HWND>(winId()),&p);return QPoint(qRound(p.x/devicePixelRatioF()),qRound(p.y/devicePixelRatioF()));};
        auto onMaximum=[this,max](const QPoint &p){return max&&QRect(max->mapTo(this,QPoint()),max->size()).contains(p);};
        auto hover=[max](bool value){if(max&&max->property("nativeHover").toBool()!=value){max->setProperty("nativeHover",value);max->update();}};
        switch(msg->message){
        case WM_NCCALCSIZE:{
            RECT *rect=msg->wParam?&reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam)->rgrc[0]:reinterpret_cast<RECT *>(msg->lParam);
            if(IsZoomed(msg->hwnd)&&!isFullScreen())client_frame::constrainMaximized(msg->hwnd,*rect);
            *result=0;return true;
        }
        case WM_NCHITTEST:{
            POINT p{GET_X_LPARAM(msg->lParam),GET_Y_LPARAM(msg->lParam)};ScreenToClient(msg->hwnd,&p);
            RECT rect{};GetClientRect(msg->hwnd,&rect);
            const auto edge=client_frame::resizeHit(rect,p,qMax(1,qRound(6*devicePixelRatioF())),isMaximized()||isFullScreen());
            if(edge!=HTCLIENT){*result=edge;return true;}
            const QPoint logical(qRound(p.x/devicePixelRatioF()),qRound(p.y/devicePixelRatioF()));
            *result=onMaximum(logical)?HTMAXBUTTON:isDragPosition(logical)?HTCAPTION:HTCLIENT;return true;
        }
        case WM_NCLBUTTONDOWN:
            if(msg->wParam==HTMAXBUTTON&&max){m_maximumPressed=true;max->setDown(true);SetCapture(msg->hwnd);*result=0;return true;}break;
        case WM_NCLBUTTONDBLCLK:
            if(msg->wParam==HTMAXBUTTON){*result=0;return true;}break;
        case WM_NCLBUTTONUP:
        case WM_LBUTTONUP:
            if(m_maximumPressed&&max){const bool click=onMaximum(pointerPosition());m_maximumPressed=false;max->setDown(false);ReleaseCapture();if(click)max->click();*result=0;return true;}break;
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
            m_maximumPressed=false;if(max)max->setDown(false);break;
        case WM_NCMOUSEMOVE:{
            hover(msg->wParam==HTMAXBUTTON);TRACKMOUSEEVENT tracking{};tracking.cbSize=sizeof(tracking);tracking.dwFlags=TME_LEAVE|TME_NONCLIENT;tracking.hwndTrack=msg->hwnd;TrackMouseEvent(&tracking);break;
        }
        case WM_NCMOUSELEAVE:case WM_MOUSEMOVE:hover(false);break;
        }
    }
#endif
    return QMainWindow::nativeEvent(type,message,result);
}
void Window::updatePlayerBackground(){
    const int theme=m_appearancePreview.isEmpty()?m_library->setting("theme",0).toInt():m_appearancePreview["preset"].toInt(3);
    AppearanceStore store(m_library);
    const auto appearance=m_appearancePreview.isEmpty()?store.active():m_appearancePreview;
    const bool custom=theme==3&&(appearance["mode"]=="palette"||appearance["mode"]=="image");
    const bool cover=custom?m_library->setting("playerCoverBackground",true).toBool():true;
    if(auto *button=findChild<QPushButton *>("playerCoverBackground")){
        QSignalBlocker block(button);button->setChecked(cover);button->setEnabled(custom);
        button->setText((cover?QString("✓ "):QString())+"展开播放时使用封面背景");
    }
    QString image;
    if(custom&&appearance["mode"]=="image"){
        const auto region=appearance["shareLR"].toBool()?QString("Left"):QString("Right");
        image=appearance["source"+region].toString();
        if(image.isEmpty())image=store.imagePath(appearance,region.toLower());
    }
    auto color=QColor(appearance["right"].toString());
    if(!color.isValid())color=palette().color(QPalette::Window);
    const double blur=appearance["blurExpanded"].toDouble(15.);
    m_nowPage->setCustomBackground(color,image,blur);
    m_nowPage->setProperty("expandedBackgroundBlur",qBound(0.,blur,50.));
    const auto track=m_player->current();
    m_nowPage->setBlur(custom?appearance["blurRight"].toDouble():25.);
    m_nowPage->setArtwork(m_library->absolute(track.cover),track.id?track.title:QString("Relaxer"),m_dark);
    const bool customBackground=custom&&(!cover||!QFileInfo::exists(m_library->absolute(track.cover)));
    m_nowPage->useCustomBackground(customBackground);
    m_nowPage->setProperty("usingCustomBackground",customBackground);
}
void Window::applyTheme() {
    int theme = m_appearancePreview.isEmpty()?m_library->setting("theme",0).toInt():m_appearancePreview["preset"].toInt(3);
    m_dark = theme == 1;
#ifdef Q_OS_WIN
    if (theme == 2) {
        QSettings system(
            "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            QSettings::NativeFormat);
        m_dark = system.value("AppsUseLightTheme", 1).toInt() == 0;
    }
#else
    if (theme == 2)
        m_dark = qApp->palette().color(QPalette::Window).lightness() < 110;
#endif
    QString bg = m_dark ? "#19191e" : "#ffffff", side = m_dark ? "#222228" : "#f5f5f7",
            fg = m_dark ? "#f0edf1" : "#25252b", muted = m_dark ? "#a09ca6" : "#86818a",
            line = m_dark ? "#34343c" : "#e9e7eb", card = m_dark ? "#25252d" : "#f7f6f8";
    bool custom = theme == 3;
    AppearanceStore store(m_library);
    auto appearance = m_appearancePreview.isEmpty() ? store.active() : m_appearancePreview;
    QString leftImage, rightImage;
    QColor accent("#e5455f"), sideInk(fg);
    auto color = [&](const QString &key, const QColor &fallback) {
        QColor value(appearance[key].toString());
        return value.isValid() ? value : fallback;
    };
    if (custom) {
        bg = color("right", QColor(bg)).name();
        side = color("left", QColor(side)).name();
        accent = color("accent", accent);
        if (appearance["mode"] == "image") {
            leftImage = appearance["sourceLeft"].toString();
            rightImage = appearance["sourceRight"].toString();
            if (leftImage.isEmpty())
                leftImage = store.imagePath(appearance, "left");
            if (rightImage.isEmpty())
                rightImage = store.imagePath(appearance, "right");
            if (appearance["shareLR"].toBool())
                rightImage = leftImage;
            auto average = [](const QString &path, QColor fallback) {
                auto image = backgroundImage(path, 32).convertToFormat(QImage::Format_RGB32);
                if (image.isNull())
                    return fallback;
                qint64 r = 0, g = 0, b = 0;
                for (int y = 0; y < image.height(); ++y)
                    for (int x = 0; x < image.width(); ++x) {
                        auto pixel = image.pixel(x, y);
                        r += qRed(pixel);
                        g += qGreen(pixel);
                        b += qBlue(pixel);
                    }
                int count = image.width() * image.height();
                return QColor(int(r / count), int(g / count), int(b / count));
            };
            bg = average(rightImage, QColor(bg)).name();
            side = average(leftImage, QColor(side)).name();
        }
        fg =
            (appearance["customText"].toBool() ? color("rightInk", autoInk(QColor(bg))) : autoInk(QColor(bg)))
                .name();
        sideInk = appearance["customText"].toBool() ? color("leftInk", autoInk(QColor(side)))
                                                    : autoInk(QColor(side));
        m_dark = autoInk(QColor(bg)).lightness() > 128;
        QColor mid = QColor(fg), background(bg);
        auto mix = [](QColor one, QColor two, double amount) {
            return QColor(qRound(one.red() * amount + two.red() * (1 - amount)),
                          qRound(one.green() * amount + two.green() * (1 - amount)),
                          qRound(one.blue() * amount + two.blue() * (1 - amount)));
        };
        muted = (appearance["customText"].toBool() ? color("muted", mid) : mix(mid, background, .68)).name();
        line = mix(mid, background, .16).name();
        card = (m_dark ? background.lighter(115) : background.darker(103)).name();
    } else
        sideInk = QColor(fg);
    if(custom)m_currentAppearance=store.draft(appearance);
    else {m_currentAppearance=AppearanceStore::defaults();m_currentAppearance["left"]=side;m_currentAppearance["right"]=bg;m_currentAppearance["accent"]=accent.name();m_currentAppearance["leftInk"]=sideInk.name();m_currentAppearance["rightInk"]=fg;m_currentAppearance["muted"]=muted;m_currentAppearance["customText"]=true;}
    m_accent = accent;
    m_ink = QColor(fg);
    m_sideInk = sideInk;
    const bool imageMode=custom && appearance["mode"]=="image";
    const bool shareImages=imageMode && appearance["shareLR"].toBool();
    const bool shareFooter=imageMode && (!appearance.contains("shareFooter") || appearance["shareFooter"].toBool());
    bool footerCustom=false;
    QString footerImage;
    QColor footerColor(card);
    QWidget *footerShared=nullptr;
    auto *shared=shareImages ? (shareFooter ? centralWidget() : m_contentHost) : nullptr;
    if(shareFooter){footerImage=shareImages?leftImage:rightImage;footerShared=centralWidget();footerCustom=true;}
    else if(imageMode){footerImage=appearance["sourceFooter"].toString();if(footerImage.isEmpty())footerImage=store.imagePath(appearance,"footer");footerCustom=!footerImage.isEmpty();}
    else if(custom && appearance["customFooter"].toBool()){footerColor=color("footer",QColor(card));footerCustom=true;}
    m_sideSurface->setLook(QColor(side), leftImage, appearance["blurLeft"].toDouble(), shared);
    m_paperSurface->setLook(QColor(bg), shareFooter&&!shareImages?footerImage:rightImage, appearance["blurRight"].toDouble(), shareFooter&&!shareImages?footerShared:shared);
    if(m_footerSurface)m_footerSurface->setLook(footerCustom?footerColor:QColor(card), footerCustom?footerImage:QString(), 0, footerCustom?footerShared:nullptr);
    m_lyrics->setLyricColor(QColor(m_library->setting("lyricColor").toString()));
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(bg));
    pal.setColor(QPalette::Base, QColor(bg));
    pal.setColor(QPalette::Text, QColor(fg));
    pal.setColor(QPalette::WindowText, QColor(fg));
    pal.setColor(QPalette::Button, QColor(card));
    pal.setColor(QPalette::ButtonText, QColor(fg));
    pal.setColor(QPalette::PlaceholderText, QColor(muted));
    pal.setColor(QPalette::Highlight, accent);
    pal.setColor(QPalette::HighlightedText, autoInk(accent));
    pal.setColor(QPalette::Mid,QColor(line));
#if defined(Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(6,9,0)
    if(theme==2&&m_appearancePreview.isEmpty())qApp->styleHints()->unsetColorScheme();
    else qApp->styleHints()->setColorScheme(m_dark?Qt::ColorScheme::Dark:Qt::ColorScheme::Light);
#endif
    pal.setColor(QPalette::ToolTipBase,QColor(card));pal.setColor(QPalette::ToolTipText,QColor(fg));
    qApp->setPalette(pal);applyPopupTheme(pal);
    QString css =
        QString(
            R"(QWidget {color:%1;font-family:'Segoe UI','Noto Sans CJK SC','Microsoft YaHei';font-size:13px;} QMainWindow,QWidget#root,QStackedWidget#pages,QScrollArea,QTableWidget,QListWidget {background:%2;} QWidget#sidebar {background:%3;border-right:1px solid %4;} QLabel {background:transparent;} QLabel#brand {font-size:23px;font-weight:700;letter-spacing:-0.5px;} QLabel#heading {font-size:30px;font-weight:700;} QLabel#section {font-size:11px;color:%5;font-weight:600;} QLabel#sidebarFoot {font-size:9px;color:%5;line-height:1.6;} QLabel#muted {color:%5;font-size:12px;} QLabel#songTitle {font-weight:600;font-size:14px;} QLabel#emptyTitle {font-size:22px;font-weight:600;} QLabel#profileName {font-size:22px;font-weight:600;} QLabel#statValue {font-size:28px;font-weight:700;} QLabel#nowTitle {font-size:27px;font-weight:700;} QLabel#nowArtist {font-size:16px;color:%5;} QLabel#time {font-size:10px;color:%5;} QWidget#statCard,QWidget#settingsCard {background:%6;border-radius:14px;} QListWidget#navigation,QListWidget#playlistNavigation {background:transparent;border:none;outline:none;} QListWidget#navigation::item,QListWidget#playlistNavigation::item {border-radius:8px;padding-left:10px;color:%1;} QListWidget#navigation::item:selected,QListWidget#playlistNavigation::item:selected {background:rgba(229,69,95,24);color:#e5455f;} QListWidget#navigation::item:hover,QListWidget#playlistNavigation::item:hover {background:rgba(128,128,128,12);} QListWidget#playlistCards {border:none;outline:none;} QListWidget#playlistCards::item {border-radius:12px;padding:7px;} QListWidget#playlistCards::item:selected {background:rgba(229,69,95,24);} QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox {background:%6;border:1px solid %4;border-radius:8px;padding:9px 12px;selection-background-color:#e5455f;min-height:17px;} QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus {border-color:#e5455f;} QPushButton {background:%6;border:1px solid %4;border-radius:8px;padding:9px 15px;min-height:16px;font-weight:500;} QPushButton:hover {background:rgba(229,69,95,20);border-color:#e5455f;} QPushButton:disabled {color:%5;} QPushButton#primary,QPushButton#playButton {background:#e5455f;color:white;border:none;} QPushButton#primary:hover,QPushButton#playButton:hover {background:#d33651;} QPushButton#playButton {border-radius:21px;padding:0;} QPushButton#iconButton,QPushButton#plain,QPushButton#sideButton,QPushButton#playerCollapse {background:transparent;border:none;padding:5px;} QPushButton#sideButton {text-align:left;padding-left:10px;} QPushButton#iconButton:hover,QPushButton#plain:hover,QPushButton#sideButton:hover,QPushButton#playerCollapse:hover {background:rgba(128,128,128,15);} QWidget#playerBar {background:%3;border-top:1px solid %4;} QHeaderView::section {background:%2;color:%5;border:none;border-bottom:1px solid %4;padding-left:12px;font-size:11px;} QTableWidget {border:none;outline:none;} QSlider::groove:horizontal {height:3px;background:%4;border-radius:1px;} QSlider::sub-page:horizontal {background:#e5455f;border-radius:1px;} QSlider::handle:horizontal {background:#e5455f;width:10px;height:10px;margin:-4px 0;border-radius:5px;} QScrollBar:vertical {background:transparent;width:8px;margin:0;} QScrollBar::handle:vertical {background:%4;border-radius:4px;min-height:30px;} QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;} QScrollBar:horizontal {height:8px;background:transparent;} QScrollBar::handle:horizontal {background:%4;border-radius:4px;min-width:30px;} QMenu {background:%2;border:1px solid %4;padding:6px;} QMenu::item {padding:8px 25px;} QMenu::item:selected {background:rgba(229,69,95,24);} QProgressBar {background:%4;border:none;border-radius:4px;} QProgressBar::chunk {background:#e5455f;border-radius:4px;} QStatusBar {background:%3;border-top:1px solid %4;})")
            .arg(fg, bg, side, line, muted, card);
    css += "QPushButton[appearanceModeButton=true]:checked{border-color:#e5455f;background:rgba(229,69,95,24);} QPushButton[appearanceHistoryAction=true]{padding:7px 10px;}";
    css.replace("#e5455f", accent.name());
    css.replace("#d33651", accent.darker(110).name());
    css.replace("rgba(229,69,95,24)",
                QString("rgba(%1,%2,%3,24)").arg(accent.red()).arg(accent.green()).arg(accent.blue()));
    css.replace("rgba(229,69,95,20)",
                QString("rgba(%1,%2,%3,20)").arg(accent.red()).arg(accent.green()).arg(accent.blue()));
    css.replace(
        "QMainWindow,QWidget#root,QStackedWidget#pages,QScrollArea,QTableWidget,QListWidget {background:" +
            bg + ";}",
        "QMainWindow,QWidget#root {background:" + bg +
            ";} QStackedWidget#pages,QScrollArea,QTableWidget,QListWidget {background:transparent;}");
    css.replace("QWidget#sidebar {background:" + side + ";", "QWidget#sidebar {background:transparent;");
    if (custom)
        css.replace("QWidget#playerBar {background:" + side + ";",
                    "QWidget#playerBar {background:" + card + ";");
    css.replace("color:white;border:none;", "color:" + autoInk(accent).name() + ";border:none;");
    css += "QKeySequenceEdit{background:" + card + ";border:1px solid " + line +
           ";border-radius:8px;padding:5px;}";
    css += "QLabel#shortcutAction{font-weight:600;font-size:14px;} QCheckBox{spacing:8px;}"
           "QCheckBox::indicator{width:14px;height:14px;border:1px solid " +
           line + ";border-radius:3px;background:" + card +
           ";} QCheckBox::indicator:checked{background:" + accent.name() + ";border-color:" + accent.name() +
           ";image:url(:/" +
           (autoInk(accent) == QColor("#25252b") ? QString("check-dark.svg") : QString("check-light.svg")) +
           ");} QCheckBox::indicator:disabled{border-color:" + muted + ";}";
    css += "QWidget#sidebar QLabel,QWidget#sidebar "
           "QPushButton,QListWidget#navigation::item,QListWidget#playlistNavigation::item{color:" +
           sideInk.name() + ";}";
    css += "QListWidget#navigation::item:selected,QListWidget#playlistNavigation::item:selected{color:" +
           accent.name() + ";}";
    if (custom && appearance["mutedBold"].toBool())
        css += "QWidget#paperSurface QLabel#muted,QWidget#paperSurface QLabel#nowArtist,"
               "QWidget#paperSurface QLabel#time{font-weight:600;}";
    css+="QWidget#windowControls{background:transparent;border:none;} QMainWindow,QWidget#root{border:none;} QLabel#copyright{font-size:11px;color:"+muted+";} QMenu{background:"+card+";border:1px solid "+line+";border-radius:10px;padding:6px;} QMenu[roundedPopup=true]{background:transparent;border:none;} QMenu::item{padding:8px 14px;border-radius:6px;} QMenu::item:selected{background:"+accent.name()+";color:"+autoInk(accent).name()+";}";
    css+="QPushButton[metadataLink=true]{padding:0;min-height:0;background:transparent;border:none;font-size:12px;font-weight:400;color:"+muted+";} QPushButton[metadataLink=true]:hover{color:"+accent.name()+";text-decoration:underline;} QPlainTextEdit{background:"+card+";border:1px solid "+line+";border-radius:8px;padding:10px;} QLabel#descriptionRemaining{color:"+muted+";font-size:12px;}";
    css+="QToolTip{background:"+card+";color:"+fg+";border:1px solid "+line+";border-radius:6px;padding:5px 8px;}";
    css+="QComboBox::drop-down{border:none;width:24px;background:transparent;} QComboBox::down-arrow{image:url(:/arrow-down-"+(m_dark?QString("light"):QString("dark"))+".svg);width:12px;height:12px;} QSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{border:none;background:transparent;width:22px;} QSpinBox::up-arrow,QDoubleSpinBox::up-arrow{image:url(:/arrow-up-"+(m_dark?QString("light"):QString("dark"))+".svg);width:10px;height:10px;} QSpinBox::down-arrow,QDoubleSpinBox::down-arrow{image:url(:/arrow-down-"+(m_dark?QString("light"):QString("dark"))+".svg);width:10px;height:10px;} QWidget#bulkBar{background:"+card+";border-top:1px solid "+line+";} QWidget#filterPopup{background:transparent;} QListWidget#filterOptions{background:transparent;border:none;} QListWidget#filterOptions::item{padding:7px;border-radius:6px;} QListWidget#filterOptions::item:hover{background:"+line+";} QPushButton[plainAction=true]{background:transparent;border:none;padding:5px;}";
    css+="QPushButton#profileRecent:checked,QPushButton#profileFrequent:checked{background:"+accent.name()+";color:"+autoInk(accent).name()+";border:none;}";
    css+="QHeaderView::section,QHeaderView,QScrollArea#versionLogScroll > QWidget,QScrollArea#settingsScroll > QWidget,QWidget#versionLogPage{background:transparent;}";
    css+="QLabel#bottomSongTitle{font-weight:600;font-size:17.5px;}";
    if (custom) {
        const auto colors=store.withProgressColors(appearance);
        const bool manual=colors["customProgressColors"].toBool();
        const auto fill=colors[manual?"progressFill":"recommendedProgressFill"].toString();
        const auto track=colors[manual?"progressTrack":"recommendedProgressTrack"].toString();
        for (const auto &name : {QString("seekSlider"),QString("volumeSlider")}) {
            css+="QSlider#"+name+"::groove:horizontal,QSlider#"+name+"::add-page:horizontal{background:"+track+";}";
            css+="QSlider#"+name+"::sub-page:horizontal,QSlider#"+name+"::handle:horizontal{background:"+fill+";}";
        }
    }
    css+="QPushButton#volumeButton{background:transparent;border:none;padding:5px;}"
         "QPushButton#volumeButton:hover{background:rgba(128,128,128,15);}";
    {const auto uiFont=m_library->setting("uiFont").toObject();static int installed=-1;static QString installedFile;const auto relative=uiFont["file"].toString();const auto absolute=relative.isEmpty()?QString():m_library->absolute(relative);if(installedFile!=absolute){if(installed>=0)QFontDatabase::removeApplicationFont(installed);installed=-1;installedFile=absolute;if(QFileInfo::exists(absolute))installed=QFontDatabase::addApplicationFont(absolute);}const auto family=installed<0?QString():QFontDatabase::applicationFontFamilies(installed).value(0);const QString stack="'Segoe UI','Noto Sans CJK SC','Microsoft YaHei'";auto familyFor=[&](const char *key){return !family.isEmpty()&&uiFont[key].toBool()?QString("'%1',%2").arg(QString(family).replace('\'',QString()),stack):stack;};if(uiFont["rest"].toBool()&&!family.isEmpty())css.replace("font-family:"+stack,"font-family:"+familyFor("rest"));css+="QTableWidget{font-family:"+familyFor("songs")+";} QWidget#lyricView{font-family:"+familyFor("lyrics")+";} QWidget#playerBar,QWidget#playerBar QLabel,QWidget#bulkBar,QWidget#bulkBar QLabel{font-family:"+familyFor("footer")+";} QWidget#nextQueueLarge,QWidget#nextQueueCompact{font-family:"+familyFor("next")+";}";}
    css+="QDoubleSpinBox#lyricDelay{padding:4px 8px;min-width:96px;}";
    if(footerCustom)css+="QWidget#playerBar,QWidget#bulkBar,QStackedWidget#bottomStack{background:transparent;}";
    setStyleSheet(css);
    m_title->ensurePolished();m_subtitle->ensurePolished();m_search->ensurePolished();
    const int toolbarHeight=m_search->sizeHint().height();
    for(const QString &name:{QString("importMusicButton"),QString("createPlaylistButton"),QString("versionLogBack"),QString("nowQueueButton")})if(auto *control=findChild<QPushButton *>(name))control->setFixedHeight(toolbarHeight);
    for(auto *control:findChildren<QPushButton *>())if(control->property("toolbarRole").isValid())control->setFixedHeight(toolbarHeight);
    if(auto *toolbar=findChild<QWidget *>("nowToolbar"))toolbar->setFixedHeight(m_title->sizeHint().height()+m_subtitle->sizeHint().height()+16);
    updateNativeFrameTheme();
    for (auto *area : findChildren<QAbstractScrollArea *>()) {
        area->viewport()->setAutoFillBackground(false);
        area->viewport()->setProperty("transparentViewport",true);
        area->viewport()->setStyleSheet("QWidget[transparentViewport=true]{background:transparent;}");
    }
    for(auto *area:findChildren<QScrollArea *>())if(auto *body=area->widget()){body->setAutoFillBackground(false);body->setProperty("clearScrollBody",true);body->setStyleSheet("QWidget[clearScrollBody=true]{background:transparent;}");}
    m_play->setAccent(accent);
    m_play->setSymbol(m_player->playing()?"pause":"play",autoInk(accent));
    const QColor iconColor = m_ink;
    for (auto *b : findChildren<IconButton *>()) {
        QColor tint =
            m_sideSurface->isAncestorOf(b) ? m_sideInk : iconColor;
        if (b == m_like && m_player->current().favorite)
            tint = accent;
        b->setSymbol(b->property("symbolName").toString(), tint);
    }
    const QStringList navSymbols = {"music", "playlist", "person", "settings", "heart"};
    for (int i = 0; i < m_nav->count(); ++i)
        m_nav->item(i)->setIcon(symbol(navSymbols[i], sideInk));
    m_lyrics->dark = m_dark;
    if(m_lyricTools)m_lyricTools->invalidateBackdrop();
    if(m_queueBackgroundTimer)m_queueBackgroundTimer->start(100);
    m_lyrics->update();
    if(auto *themeSetting=findChild<QComboBox *>("themeSetting")){QSignalBlocker block(themeSetting);themeSetting->setCurrentIndex(qBound(0,m_library->setting("theme",0).toInt(),3));}
    if(m_nowPage)updatePlayerBackground();
    if (m_lists)
        refreshPlaylists();
}
} // namespace relaxer
