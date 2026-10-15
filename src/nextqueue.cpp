#include "nextqueue.h"
#include "appearance.h"
#include "widgets.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QFontMetrics>
#include <QtMath>
namespace relaxer {
NextQueuePanel::NextQueuePanel(Library *library,bool large,QWidget *parent):QWidget(parent),m_library(library),m_large(large){
    setObjectName(large?"nextQueueLarge":"nextQueueCompact");setProperty("blurPercent",25);setProperty("queueLarge",large);
    setAttribute(Qt::WA_NoSystemBackground);setAttribute(Qt::WA_TranslucentBackground);setMouseTracking(true);setCursor(Qt::PointingHandCursor);hide();
    qApp->installEventFilter(this);
    m_geometry.setDuration(300);m_geometry.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_geometry,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){setGeometry(v.toRect());setProperty("queueAnimating",true);update();});
    connect(&m_geometry,&QVariantAnimation::finished,this,[this]{setProperty("queueAnimating",false);if(!m_presented)hide();});
    m_scrollSlide.setEasingCurve(QEasingCurve::OutCubic);connect(&m_scrollSlide,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){m_scrollPosition=v.toReal();update();});
}
QFont NextQueuePanel::rowFont() const {QFont f=font();f.setPixelSize(13);f.setWeight(QFont::Normal);return f;}
int NextQueuePanel::preferredWidth() const {
    const QFontMetrics metrics(rowFont());
    const int names=metrics.horizontalAdvance(QString(8,QChar(0x6b4c)))+metrics.horizontalAdvance("—")+metrics.horizontalAdvance(QString(5,QChar(0x6b4c)));
    const int start=32+(!m_large?36:coverSize())+8;
    return qMax(112,start+names+12);
}
void NextQueuePanel::changeEvent(QEvent *event){QWidget::changeEvent(event);if(event->type()==QEvent::FontChange){emit widthHintChanged();update();}}
QString NextQueuePanel::limitedText(const QString &text,const QFont &font,int han){const QFontMetrics metrics(font);return metrics.elidedText(text,Qt::ElideRight,metrics.horizontalAdvance(QString(han,QChar(0x6b4c))));}
int NextQueuePanel::shownSongs() const {if(!m_large&&!m_expanded)return 0;return qMin(int(m_tracks.size()),m_expanded?5:1);}
int NextQueuePanel::rowHeight() const {
    if(m_expanded)return m_large?46:38;
    if(m_large)return qMax(30,(m_footerHeight-4-headerHeight()-4)/qMax(1,shownSongs()));
    return 38;
}
int NextQueuePanel::targetHeight() const {
    if(m_large&&!m_expanded)return qMax(88,m_footerHeight-4);
    return headerHeight()+(m_tracks.isEmpty()&&m_expanded?28:shownSongs()*rowHeight())+4;
}
QRect NextQueuePanel::targetRect() const {return QRect(m_anchor.x(),m_anchor.y()-targetHeight(),m_width,targetHeight());}
QRect NextQueuePanel::rowRect(int index) const {return QRect(6,qRound(headerHeight()+(index-m_scrollPosition)*rowHeight()),m_width-12,rowHeight()-2);}
QRect NextQueuePanel::arrowRect() const {const int size=!m_large&&!m_expanded?28:34;return QRect(m_large?width()-size:0,0,size,headerHeight());}
void NextQueuePanel::setTracks(QVector<Track> tracks){
    bool same=tracks.size()==m_tracks.size();if(same)for(int i=0;i<tracks.size();++i)if(tracks[i].id!=m_tracks[i].id||tracks[i].title!=m_tracks[i].title||tracks[i].artist!=m_tracks[i].artist||tracks[i].cover!=m_tracks[i].cover){same=false;break;}if(same)return;
    m_tracks=std::move(tracks);m_scrollSlide.stop();m_scroll=qBound(0.,m_scroll,double(qMax(0,int(m_tracks.size())-5)));m_scrollPosition=m_scroll;m_hover=-1;setProperty("queueSongCount",int(m_tracks.size()));if(m_presented&&m_geometry.state()!=QAbstractAnimation::Running)moveToTarget(false);update();
}
void NextQueuePanel::setAnchor(QPoint anchor,int width,int footerHeight){const bool changed=m_anchor!=anchor||m_width!=width||m_footerHeight!=footerHeight;m_anchor=anchor;m_width=width;m_footerHeight=footerHeight;if(changed){m_background={};if(m_presented&&m_geometry.state()!=QAbstractAnimation::Running)setGeometry(targetRect());}update();}
void NextQueuePanel::cacheBackground(){
    if(!parentWidget()||!m_width)return;const int h=34+qMin(5,qMax(2,int(m_tracks.size())))*(m_large?46:38)+4;m_backgroundRect=QRect(m_anchor.x(),m_anchor.y()-h,m_width,h);
    const bool visible=isVisible();hide();const auto raw=parentWidget()->grab(m_backgroundRect);if(visible)show();auto image=raw.toImage();const qreal dpr=raw.devicePixelRatioF();image.setDevicePixelRatio(1);image=gaussianBlur(image,25);m_background=QPixmap::fromImage(image);m_background.setDevicePixelRatio(dpr);setProperty("backgroundBuilds",++m_backgroundBuilds);
}
void NextQueuePanel::invalidateBackground(){m_background={};if(m_presented)cacheBackground();update();}
void NextQueuePanel::moveToTarget(bool animate){const QRect target=targetRect();m_geometry.stop();if(!animate||!isVisible()){setGeometry(target);update();return;}m_geometry.setStartValue(geometry());m_geometry.setEndValue(target);m_geometry.start();}
void NextQueuePanel::present(bool on,bool animate){
    if(on==m_presented&&(isVisible()==on||m_geometry.state()==QAbstractAnimation::Running)){if(on&&m_geometry.state()!=QAbstractAnimation::Running)moveToTarget(false);return;}
    m_presented=on;m_expanded=false;m_hover=-1;m_scrollSlide.stop();m_scroll=0;m_scrollPosition=0;setProperty("queueExpanded",false);m_geometry.stop();emit widthHintChanged();
    const auto target=targetRect();if(on){if(m_background.isNull())cacheBackground();setGeometry(animate?target.translated(0,target.height()+20):target);show();raise();if(animate){m_geometry.setStartValue(geometry());m_geometry.setEndValue(target);m_geometry.start();}}
    else if(isVisible()&&animate){m_geometry.setStartValue(geometry());m_geometry.setEndValue(target.translated(0,target.height()+20));m_geometry.start();}else hide();
}
void NextQueuePanel::setExpanded(bool on,bool animate){if(m_expanded==on)return;const QRect before=geometry();m_geometry.stop();m_expanded=on;m_scrollSlide.stop();m_scroll=0;m_scrollPosition=0;m_hover=-1;setProperty("queueExpanded",on);emit widthHintChanged();if(m_background.isNull())cacheBackground();setGeometry(before);moveToTarget(animate);update();}
static QPixmap textImage(QString text,QFont font,QSize size,QColor ink){QPixmap pix(size);pix.fill(Qt::transparent);QPainter p(&pix);p.setFont(font);p.setPen(ink);p.drawText(pix.rect(),Qt::AlignLeft|Qt::AlignVCenter,text);return pix;}
MovingArtwork NextQueuePanel::artwork(int index) const {
    MovingArtwork art;const auto t=m_tracks.value(index);const auto r=!m_large&&!m_expanded?QRect(54,0,m_width-54,headerHeight()):rowRect(index);const auto f=rowFont();const QFontMetrics metrics(f);const int cover=coverSize();
    const int x=r.x()+26+cover+8,titleWidth=metrics.horizontalAdvance(QString(8,QChar(0x6b4c))),artistWidth=metrics.horizontalAdvance(QString(5,QChar(0x6b4c))),dash=metrics.horizontalAdvance("—");
    art.coverFrom=QRect(mapTo(parentWidget(),QPoint(r.x()+26,r.y()+(r.height()-cover)/2)),QSize(cover,cover));art.titleFrom=QRect(mapTo(parentWidget(),QPoint(x,r.y())),QSize(titleWidth,r.height()));art.artistFrom=QRect(mapTo(parentWidget(),QPoint(x+titleWidth+dash,r.y())),QSize(artistWidth,r.height()));
    art.cover=coverPixmap(m_library->absolute(t.cover),t.title,700);
    if(!m_large&&!m_expanded){
        art.titleFrom.setWidth(qMax(0,m_width-x-12));art.artistFrom={};
        art.titleSmall=textImage(metrics.elidedText(t.title,Qt::ElideRight,int(art.titleFrom.width())),f,art.titleFrom.size().toSize(),palette().color(QPalette::Text));
    }else{
        art.titleSmall=textImage(limitedText(t.title,f,8),f,art.titleFrom.size().toSize(),palette().color(QPalette::Text));
        art.artistSmall=textImage(limitedText(t.artist,f,5),f,art.artistFrom.size().toSize(),palette().color(QPalette::Text));
    }
    return art;
}
void NextQueuePanel::paintEvent(QPaintEvent *){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);QPainterPath mask;mask.addRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),12,12);p.setClipPath(mask);
    auto glass=[&]{if(!m_expanded)return;p.save();p.setClipping(false);QLinearGradient shine(0,0,width(),height());shine.setColorAt(0,QColor(255,255,255,190));shine.setColorAt(.55,QColor(255,255,255,70));shine.setColorAt(1,QColor(40,40,48,90));p.setBrush(Qt::NoBrush);p.setPen(QPen(QBrush(shine),1.4));p.drawRoundedRect(QRectF(rect()).adjusted(.8,.8,-.8,-.8),12,12);p.restore();};
    if(!m_background.isNull())p.drawPixmap(QPoint(m_backgroundRect.x()-x(),m_backgroundRect.y()-y()),m_background);
    auto ink=palette().color(QPalette::Text),accent=palette().color(QPalette::Highlight);auto f=rowFont();p.setFont(f);const int hh=headerHeight();const QRect arrow=arrowRect();
    auto highlight=[&](QRect r,bool outline){auto fill=ink;fill.setAlpha(20);p.setPen(outline?QPen(accent,1.1):QPen(Qt::NoPen));p.setBrush(fill);p.drawRoundedRect(QRectF(r).adjusted(1,1,-1,-1),7,7);};
    if(m_hover==-3)highlight(arrow,false);else if(m_hover==-2)highlight(m_large?QRect(0,0,width()-34,hh):QRect(arrow.width(),0,width()-arrow.width(),hh),false);
    p.setFont(f);
    p.setPen(ink);p.drawText(arrow,Qt::AlignCenter,m_expanded?"⌄":"⌃");p.drawText(QRect(m_large?12:!m_expanded?30:38,0,!m_large&&!m_expanded?44:m_large&&!m_expanded?76:58,hh),Qt::AlignLeft|Qt::AlignVCenter,"下一首");
    p.setFont(f);
    auto names=[&](const Track &t,int x,int y,int height){const QFontMetrics fm(f);const int tw=fm.horizontalAdvance(QString(8,QChar(0x6b4c))),aw=fm.horizontalAdvance(QString(5,QChar(0x6b4c))),dw=fm.horizontalAdvance("—");p.drawText(QRect(x,y,tw,height),Qt::AlignLeft|Qt::AlignVCenter,limitedText(t.title,f,8));p.drawText(QRect(x+tw,y,dw,height),Qt::AlignCenter,"—");p.drawText(QRect(x+tw+dw,y,aw,height),Qt::AlignLeft|Qt::AlignVCenter,limitedText(t.artist,f,5));};
    auto drawCover=[&](const Track &track,const QRect &rect){QPainterPath clip;clip.addRoundedRect(rect,5,5);p.save();p.setClipPath(clip,Qt::IntersectClip);p.drawPixmap(rect,coverPixmap(m_library->absolute(track.cover),track.title,80));p.restore();};
    if(m_tracks.isEmpty()){const bool rows=m_large||m_expanded;p.drawText(QRect(rows?12:80,rows?hh:0,width()-(rows?24:92),rows?height()-hh-4:hh),Qt::AlignLeft|Qt::AlignVCenter,"暂无待播放歌曲");glass();return;}
    if(!m_large&&!m_expanded){const int size=coverSize(),x=80+size+8;drawCover(m_tracks[0],QRect(80,(hh-size)/2,size,size));p.drawText(QRect(x,0,qMax(0,width()-x-12),hh),Qt::AlignLeft|Qt::AlignVCenter,QFontMetrics(f).elidedText(m_tracks[0].title,Qt::ElideRight,qMax(0,width()-x-12)));return;}
    p.save();p.setClipRect(QRect(0,hh,width(),qMax(0,height()-hh-4)),Qt::IntersectClip);const int first=qFloor(m_scrollPosition);
    for(int i=first;i<qMin(int(m_tracks.size()),first+shownSongs()+(m_expanded?1:0));++i){const auto r=rowRect(i);const auto &t=m_tracks[i];if(i==m_hover)highlight(r,true);p.setPen(ink);p.drawText(QRect(r.x()+2,r.y(),20,r.height()),Qt::AlignCenter,QString::number(i+1));int x=r.x()+26;{const int size=coverSize();drawCover(t,QRect(x,r.y()+(r.height()-size)/2,size,size));x+=size+8;}names(t,x,r.y(),r.height());}
    if(m_expanded&&m_tracks.size()>5){auto track=ink;track.setAlpha(35);p.setPen(Qt::NoPen);p.setBrush(track);p.drawRoundedRect(QRect(width()-6,hh+3,3,5*rowHeight()-8),1.5,1.5);track.setAlpha(110);p.setBrush(track);const qreal travel=(5*rowHeight()-28)*m_scrollPosition/qMax(1,int(m_tracks.size())-5);p.drawRoundedRect(QRectF(width()-6,hh+3+travel,3,20),1.5,1.5);}p.restore();glass();
}
bool NextQueuePanel::event(QEvent *event){
    if(event->type()==QEvent::ToolTip){event->accept();return true;}
    return QWidget::event(event);
}
bool NextQueuePanel::eventFilter(QObject *object,QEvent *event){
    if(m_expanded&&m_presented&&isVisible()&&event->type()==QEvent::MouseButtonPress){
        const auto *mouse=static_cast<QMouseEvent *>(event);
        const auto *widget=qobject_cast<QWidget *>(object);
        if(mouse->button()==Qt::LeftButton&&widget&&widget->window()==window()&&
           !rect().contains(mapFromGlobal(mouse->globalPosition().toPoint())))setExpanded(false);
    }
    return QWidget::eventFilter(object,event);
}
void NextQueuePanel::mouseMoveEvent(QMouseEvent *e){
    int hover=-1;if(e->pos().y()<headerHeight()){const QRect arrow=arrowRect();hover=arrow.contains(e->pos())?-3:-2;}else if(m_large||m_expanded)for(int i=qFloor(m_scrollPosition);i<qMin(int(m_tracks.size()),qFloor(m_scrollPosition)+shownSongs()+(m_expanded?1:0));++i)if(rowRect(i).contains(e->pos())){hover=i;break;}
    if(hover!=m_hover){m_hover=hover;setProperty("hoveredQueueRow",hover);update();}
}
void NextQueuePanel::leaveEvent(QEvent *){m_hover=-1;setProperty("hoveredQueueRow",-1);update();}
void NextQueuePanel::mouseReleaseEvent(QMouseEvent *e){if(e->button()!=Qt::LeftButton)return;const QRect arrow=arrowRect();if(arrow.contains(e->pos())){setExpanded(!m_expanded);return;}if(e->pos().y()<headerHeight()){if(!m_tracks.isEmpty())emit activated(0);return;}for(int i=qFloor(m_scrollPosition);i<qMin(int(m_tracks.size()),qFloor(m_scrollPosition)+shownSongs()+(m_expanded?1:0));++i)if(rowRect(i).contains(e->pos())){emit activated(i);return;}}
void NextQueuePanel::wheelEvent(QWheelEvent *e){if(!m_expanded){e->accept();return;}const bool pixels=!e->pixelDelta().isNull();const qreal distance=pixels?e->pixelDelta().y()/double(rowHeight()):e->angleDelta().y()/120.;m_scroll=qBound(0.,m_scroll-distance,double(qMax(0,int(m_tracks.size())-5)));m_scrollSlide.stop();m_scrollSlide.setDuration(pixels?85:170);m_scrollSlide.setStartValue(m_scrollPosition);m_scrollSlide.setEndValue(m_scroll);m_scrollSlide.start();m_hover=-1;e->accept();}
ArtworkFlight::ArtworkFlight(QWidget *parent):QWidget(parent){
    setObjectName("queueArtworkFlight");setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_NoSystemBackground);setAttribute(Qt::WA_TranslucentBackground);hide();
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_animation,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){m_progress=value.toReal();update();});
    connect(&m_animation,&QVariantAnimation::finished,this,[this]{hide();auto done=std::move(m_finished);m_art={};if(done)done();});
}
void ArtworkFlight::stop(){m_animation.stop();hide();m_art={};auto done=std::move(m_finished);if(done)done();}
void ArtworkFlight::start(MovingArtwork art,std::function<void()> done,int stepDirection){
    stop();m_art=std::move(art);m_finished=std::move(done);m_stepDirection=stepDirection;m_progress=0;
    setGeometry(parentWidget()->rect());show();raise();m_animation.setDuration(stepDirection?420:460);m_animation.setStartValue(0.);m_animation.setEndValue(1.);m_animation.start();
}
void ArtworkFlight::paintEvent(QPaintEvent *){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
    const auto drawCover=[&](const QRectF &rect,const QPixmap &image,qreal radius){
        if(image.isNull()||rect.isEmpty())return;QPainterPath clip;clip.addRoundedRect(rect,radius,radius);
        p.save();p.setClipPath(clip,Qt::IntersectClip);p.drawPixmap(rect,image,QRectF(image.rect()));p.restore();
    };
    // The old cover stays opaque until the replacement fully covers it.
    if(m_stepDirection==0){
        drawCover(m_art.coverOld,m_art.coverOutgoing,24);
        const auto lerp=[this](const QRectF &a,const QRectF &b){return QRectF(a.topLeft()+(b.topLeft()-a.topLeft())*m_progress,a.size()+(b.size()-a.size())*m_progress);};
        drawCover(lerp(m_art.coverFrom,m_art.coverTo),m_art.cover,5+19*m_progress);
    }else{
        QPainterPath pocket;pocket.addRoundedRect(m_art.coverTo,24,24);p.save();p.setClipPath(pocket,Qt::IntersectClip);
        if(m_stepDirection>0){
            drawCover(m_art.coverOld,m_art.coverOutgoing,24);
            drawCover(m_art.coverTo.translated(m_art.coverTo.width()*(1-m_progress),0),m_art.cover,24);
        }else{
            drawCover(m_art.coverTo,m_art.cover,24);
            drawCover(m_art.coverOld.translated(m_art.coverOld.width()*m_progress,0),m_art.coverOutgoing,24);
        }
        p.restore();
    }
}
}
