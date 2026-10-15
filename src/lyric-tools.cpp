#include "lyric-tools.h"
#include "appearance.h"
#include <QApplication>
#include <QCheckBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QVBoxLayout>
namespace relaxer {
FrostedLyricScroll::FrostedLyricScroll(QWidget *parent):QScrollBar(Qt::Vertical,parent){
    setObjectName("lyricScrollBar");setRange(0,10000);setSingleStep(250);setPageStep(1800);setFixedWidth(12);
    setStyleSheet("QScrollBar#lyricScrollBar{background:transparent;border:none;width:12px;margin:2px;}QScrollBar#lyricScrollBar::handle:vertical{background:rgba(100,100,110,100);border-radius:4px;min-height:28px;}QScrollBar#lyricScrollBar::add-line:vertical,QScrollBar#lyricScrollBar::sub-line:vertical{height:0;}QScrollBar#lyricScrollBar::add-page:vertical,QScrollBar#lyricScrollBar::sub-page:vertical{background:transparent;}");
}
void FrostedLyricScroll::cacheBackdrop(){
    if(!parentWidget()||width()<1||height()<1||!m_backdrop.isNull())return;
    const bool shown=isVisible();hide();const auto raw=parentWidget()->grab(geometry());if(shown)show();
    auto image=raw.toImage();const qreal dpr=image.devicePixelRatio();image.setDevicePixelRatio(1);
    m_backdrop=QPixmap::fromImage(gaussianBlur(image,50));m_backdrop.setDevicePixelRatio(dpr);
    setProperty("backgroundBuilds",property("backgroundBuilds").toInt()+1);
}
void FrostedLyricScroll::paintEvent(QPaintEvent *event){
    {QPainter p(this);p.setRenderHint(QPainter::Antialiasing);QPainterPath mask;mask.addRoundedRect(rect(),6,6);p.setClipPath(mask);if(!m_backdrop.isNull())p.drawPixmap(rect(),m_backdrop);auto tint=palette().color(QPalette::Window);tint.setAlpha(65);p.fillRect(rect(),tint);}
    QScrollBar::paintEvent(event);
}
LyricTools::LyricTools(LyricView *lyrics,QWidget *delayControls,QWidget *parent):QWidget(parent),m_lyrics(lyrics),m_delayControls(delayControls){
    setObjectName("lyricToolsHost");setMouseTracking(true);m_lyrics->setParent(this);m_lyrics->setMouseTracking(true);
    m_question=new QPushButton("?",this);m_question->setObjectName("lyricToolsQuestion");m_question->setFixedSize(20,20);m_question->setCursor(Qt::PointingHandCursor);m_question->setToolTip("歌词工具");
    m_question->setStyleSheet("QPushButton#lyricToolsQuestion{padding:0;min-height:0;border:1px solid rgba(128,128,128,55);border-radius:10px;background:rgba(128,128,128,15);font-size:12px;}");
    m_scroll=new FrostedLyricScroll(this);m_menu=new QFrame(this);m_menu->setObjectName("lyricToolsMenu");
    auto *layout=new QVBoxLayout(m_menu);layout->setContentsMargins(10,10,10,10);layout->setSpacing(6);
    m_delayEntry=new QPushButton("歌词延时",m_menu);m_delayEntry->setObjectName("lyricDelayEntry");layout->addWidget(m_delayEntry);
    m_translation=new QCheckBox("显示译文",m_menu);m_translation->setObjectName("lyricShowTranslation");m_translation->setChecked(true);layout->addWidget(m_translation);
    m_back=new QPushButton("‹ 返回",m_menu);m_back->setObjectName("lyricToolsBack");layout->addWidget(m_back);layout->addWidget(m_delayControls);
    m_back->hide();m_delayControls->hide();m_menu->hide();m_question->hide();m_scroll->hide();
    m_hideTimer.setSingleShot(true);m_hideTimer.setInterval(3000);m_cacheTimer.setSingleShot(true);m_cacheTimer.setInterval(100);
    connect(&m_hideTimer,&QTimer::timeout,this,[this]{if(!m_pinned&&!m_scroll->isSliderDown())hideControls();});
    connect(&m_cacheTimer,&QTimer::timeout,this,[this]{m_scroll->cacheBackdrop();});
    connect(m_question,&QPushButton::clicked,this,[this]{if(m_menu->isVisible()){closeTools();return;}reveal();m_menu->show();arrange();m_menu->raise();m_hideTimer.start();});
    connect(m_delayEntry,&QPushButton::clicked,this,[this]{if(!m_timeline){emit timedLyricsRequested();return;}m_pinned=true;m_hideTimer.stop();m_delayEntry->hide();m_back->show();m_delayControls->show();arrange();});
    connect(m_translation,&QCheckBox::toggled,this,[this](bool on){m_lyrics->setShowTranslation(on);emit translationDisplayChanged(on);});
    connect(m_back,&QPushButton::clicked,this,[this]{m_pinned=false;m_delayControls->hide();m_back->hide();m_delayEntry->show();arrange();m_hideTimer.start();});
    connect(m_scroll,&QScrollBar::valueChanged,this,[this](int value){m_lyrics->browseFraction(value/10000.0,m_scroll->isSliderDown());});
    connect(m_scroll,&QScrollBar::sliderPressed,this,[this]{reveal();m_hideTimer.stop();m_lyrics->setBrowsingDrag(true);});
    connect(m_scroll,&QScrollBar::sliderReleased,this,[this]{m_lyrics->setBrowsingDrag(false);if(!m_pinned)m_hideTimer.start();});
    connect(m_lyrics,&LyricView::scrollMetricsChanged,this,[this](int value,int page,bool scrollable){QSignalBlocker block(m_scroll);m_scroll->setValue(value);m_scroll->setPageStep(page);m_scroll->setEnabled(scrollable);if(!scrollable)m_scroll->hide();else if(m_controlsVisible&&m_scroll->isHidden())animateControl(m_scroll,true);});
    qApp->installEventFilter(this);
}
void LyricTools::setShowTranslation(bool on){QSignalBlocker block(m_translation);m_translation->setChecked(on);m_lyrics->setShowTranslation(on);}
void LyricTools::setAvailable(bool available,bool timeline){
    m_available=available;m_timeline=timeline;
    m_delayEntry->setEnabled(available);m_delayEntry->setText(timeline?"歌词延时":"歌词延时 · 查找滚动歌词");
    if(!available||(!timeline&&m_pinned))closeTools();
}
void LyricTools::invalidateBackdrop(){m_scroll->invalidateBackdrop();m_cacheTimer.start();}
void LyricTools::arrange(){
    m_lyrics->setGeometry(rect());m_scroll->setGeometry(width()-14,12,12,qMax(1,height()-48));m_question->move(width()-26,height()-26);
    if(m_delayControls->isVisible())m_delayControls->setMinimumWidth(300);
    m_menu->setMinimumWidth(m_delayControls->isVisible()?320:0);m_menu->adjustSize();m_menu->move(qMax(0,width()-m_menu->width()-28),qMax(0,height()-m_menu->height()-30));
    m_scroll->raise();m_question->raise();if(m_menu->isVisible())m_menu->raise();
}
void LyricTools::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);arrange();invalidateBackdrop();}
void LyricTools::animateControl(QWidget *widget,bool on){
    auto *effect=qobject_cast<QGraphicsOpacityEffect *>(widget->graphicsEffect());if(!effect){effect=new QGraphicsOpacityEffect(widget);effect->setOpacity(0);widget->setGraphicsEffect(effect);}
    auto *old=widget->findChild<QPropertyAnimation *>("controlFade");if(old){old->stop();delete old;}
    auto *animation=new QPropertyAnimation(effect,"opacity",widget);animation->setObjectName("controlFade");animation->setDuration(140);animation->setStartValue(effect->opacity());animation->setEndValue(on?1.:0.);
    if(on)widget->show();else connect(animation,&QPropertyAnimation::finished,widget,&QWidget::hide);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
void LyricTools::reveal(){
    if(!m_available||!isVisible())return;
    m_menu->setStyleSheet(QString("QFrame#lyricToolsMenu{background:%1;border:1px solid %2;border-radius:12px;}").arg(palette().color(QPalette::Base).name(),palette().color(QPalette::Mid).name()));
    if(!m_controlsVisible){m_controlsVisible=true;animateControl(m_question,true);if(m_scroll->isEnabled())animateControl(m_scroll,true);}
    if(!m_pinned&&!m_scroll->isSliderDown())m_hideTimer.start();
}
void LyricTools::hideControls(){m_controlsVisible=false;m_menu->hide();animateControl(m_question,false);animateControl(m_scroll,false);}
void LyricTools::closeTools(){m_hideTimer.stop();m_pinned=false;m_delayControls->hide();m_back->hide();m_delayEntry->show();hideControls();}
bool LyricTools::eventFilter(QObject *object,QEvent *event){
    if(!isVisible())return false;
    auto *widget=qobject_cast<QWidget *>(object);if(!widget||widget->window()!=window())return false;
    if(event->type()==QEvent::MouseMove){
        auto *mouse=static_cast<QMouseEvent *>(event);auto *surface=parentWidget();if(surface){const auto point=surface->mapFromGlobal(mouse->globalPosition().toPoint());if(surface->rect().contains(point)&&point.x()>=surface->width()/2)reveal();}
    }else if(event->type()==QEvent::MouseButtonPress&&m_menu->isVisible()){
        auto *mouse=static_cast<QMouseEvent *>(event);const auto point=mapFromGlobal(mouse->globalPosition().toPoint());
        if(!m_menu->geometry().contains(point)&&!m_question->geometry().contains(point)&&!m_scroll->geometry().contains(point))closeTools();
    }
    return false;
}
}
