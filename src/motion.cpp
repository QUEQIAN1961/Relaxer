#include "motion.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <algorithm>
#include <QtMath>
namespace relaxer {
static qreal bounded(qreal p) { return qBound(qreal(0),p,qreal(1)); }
static qreal ease(qreal p) { p=bounded(p); return 1-qPow(1-p,3); }
static QRectF between(const QRectF &a,const QRectF &b,qreal p) {
    return QRectF(a.topLeft()+(b.topLeft()-a.topLeft())*p,a.size()+(b.size()-a.size())*p);
}
QPixmap transparentSnapshot(QWidget *widget) {
    QPixmap image(widget->size()*widget->devicePixelRatioF());
    image.setDevicePixelRatio(widget->devicePixelRatioF());image.fill(Qt::transparent);
    widget->render(&image,QPoint(),QRegion(),QWidget::DrawChildren);return image;
}
QRect artworkRect(QWidget *widget,QWidget *ancestor) {
    const int side=qMin(widget->width(),widget->height());
    return QRect(widget->mapTo(ancestor,QPoint((widget->width()-side)/2,(widget->height()-side)/2)),QSize(side,side));
}
PageMotion::PageMotion(QWidget *parent):QWidget(parent) {
    setObjectName("pageMotion");setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);setAttribute(Qt::WA_TranslucentBackground);
    hide();m_animation.setStartValue(0.);m_animation.setEndValue(1.);
    connect(&m_animation,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){m_progress=v.toReal();update();});
    connect(&m_animation,&QVariantAnimation::finished,this,[this]{
        hide();auto callback=std::move(m_finished);m_old=QPixmap();m_next=QPixmap();m_background=QPixmap();m_art=MovingArtwork();
        if(callback)callback();
    });
}
void PageMotion::stop(){m_animation.stop();m_finished={};hide();m_art=MovingArtwork();m_old=QPixmap();m_next=QPixmap();m_background=QPixmap();}
void PageMotion::hold(const QRect &area,const QPixmap &image,const QPixmap &background) {
    m_animation.stop();m_finished={};m_area=area;m_old=image;m_next=QPixmap();m_background=background;
    m_player=false;m_progress=0;setGeometry(parentWidget()->rect());show();raise();update();
}
QPixmap PageMotion::currentPage() const {return const_cast<PageMotion *>(this)->grab(m_area);}
QPixmap PageMotion::currentContent(){m_contentOnly=true;auto whole=transparentSnapshot(this);m_contentOnly=false;const qreal dpr=whole.devicePixelRatioF();auto result=whole.copy(QRect(qRound(m_area.x()*dpr),qRound(m_area.y()*dpr),qRound(m_area.width()*dpr),qRound(m_area.height()*dpr)));result.setDevicePixelRatio(dpr);return result;}
MovingArtwork PageMotion::currentArtwork() const {
    MovingArtwork art=m_art;const qreal p=ease(m_progress);
    art.coverFrom=between(m_art.coverFrom,m_art.coverTo,p);
    art.titleFrom=between(m_art.titleFrom,m_art.titleTo,p);
    art.artistFrom=between(m_art.artistFrom,m_art.artistTo,p);return art;
}
qreal PageMotion::currentReveal() const {
    return m_initialReveal+((m_expanding?1.:0.)-m_initialReveal)*ease(m_progress);
}
qreal PageMotion::currentDetails() const {
    const qreal p=m_expanding?ease((m_progress-.52)/.48):ease(m_progress/.36);
    return m_initialDetails+((m_expanding?1.:0.)-m_initialDetails)*p;
}
void PageMotion::startPage(const QPixmap &next,const QPixmap &background,std::function<void()> finished) {
    m_next=next;m_background=background;m_player=false;m_finished=std::move(finished);
    m_animation.setDuration(220);m_animation.start();
}
void PageMotion::startPlayer(const QPixmap &oldClean,const QPixmap &nextClean,const QPixmap &backdrop,
                           const MovingArtwork &art,bool expanding,std::function<void()> finished,qreal initialReveal,qreal initialDetails) {
    m_old=oldClean;m_next=nextClean;m_background=backdrop;m_art=art;m_player=true;m_expanding=expanding;
    m_initialDetails=initialDetails<0?(expanding?0:1):initialDetails;
    m_initialReveal=initialReveal<0?(expanding?0:1):initialReveal;
    m_finished=std::move(finished);m_animation.setDuration(qMax(150,qRound(420*qAbs((expanding?1.:0.)-m_initialReveal))));m_animation.start();
}
// Only the narrow feather band needs a temporary image; the rest is a clipped blit.
static void rising(QPainter &p,const QRect &area,const QPixmap &image,qreal progress) {
    if(progress<=0)return;if(progress>=1){p.drawPixmap(area,image);return;}
    const qreal feather=qMin(qreal(42),area.height()*.08);
    const qreal top=area.bottom()+1-(area.height()+feather)*progress;
    const qreal solid=top+feather;
    p.save();p.setClipRect(QRectF(area.left(),solid,area.width(),area.bottom()+1-solid));p.drawPixmap(area,image);p.restore();
    const qreal dpr=image.devicePixelRatio();
    QPixmap band(qCeil(area.width()*dpr),qCeil(feather*dpr));band.setDevicePixelRatio(dpr);band.fill(Qt::transparent);
    QPainter b(&band);b.drawPixmap(QRectF(0,-(top-area.top()),area.width(),area.height()),image,QRectF(0,0,image.width(),image.height()));
    b.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    QLinearGradient fade(0,0,0,feather);fade.setColorAt(0,Qt::transparent);fade.setColorAt(1,Qt::black);
    b.fillRect(QRectF(0,0,area.width(),feather),fade);b.end();
    p.save();p.setClipRect(area);p.drawPixmap(QPointF(area.left(),top),band);p.restore();
}
void PageMotion::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
    if(m_next.isNull()){if(!m_contentOnly&&!m_background.isNull())p.drawPixmap(m_area,m_background);p.drawPixmap(m_area,m_old);return;}
    const qreal t=m_progress;
    if(!m_player){
        p.save();p.setClipRect(m_area);if(!m_contentOnly)p.drawPixmap(m_area,m_background);
        if(t<.38){const qreal out=ease(t/.38);p.setOpacity(1-out);QRectF r=m_area;
            const qreal shrink=.010*out;r.adjust(r.width()*shrink/2,r.height()*shrink/2,-r.width()*shrink/2,-r.height()*shrink/2);
            p.drawPixmap(r,m_old,QRectF(0,0,m_old.width(),m_old.height()));
        }else{const qreal in=ease((t-.38)/.62);p.setOpacity(in);QRectF r=m_area;r.translate(0,-7*(1-in));
            p.drawPixmap(r,m_next,QRectF(0,0,m_next.width(),m_next.height()));}
        p.restore();return;
    }
    const qreal travel=ease(t),reveal=currentReveal();
    // A return reveals the destination beneath the same backdrop as it descends.
    p.drawPixmap(m_area,m_expanding?m_old:m_next);
    rising(p,m_area,m_background,reveal);
    const qreal details=currentDetails();
    if(details>0){p.save();p.setClipRect(m_area);p.setOpacity(details);
        p.drawPixmap(m_area,m_expanding?m_next:m_old);p.restore();}
    const QRectF cover=between(m_art.coverFrom,m_art.coverTo,travel);
    p.save();QPainterPath clip;const qreal radius=m_expanding?10+14*travel:24-14*travel;
    clip.addRoundedRect(cover,radius,radius);p.setClipPath(clip);p.drawPixmap(cover,m_art.cover,QRectF(0,0,m_art.cover.width(),m_art.cover.height()));p.restore();
    auto text=[&](const QRectF &from,const QRectF &to,const QPixmap &small,const QPixmap &large){
        const QRectF r=between(from,to,travel);const qreal sourceOpacity=1-ease(t/.32),targetOpacity=ease((t-.38)/.62);
        p.save();p.setOpacity(sourceOpacity);const auto &source=m_expanding?small:large;
        p.drawPixmap(r,source,QRectF(0,0,source.width(),source.height()));
        p.setOpacity(targetOpacity);const auto &target=m_expanding?large:small;
        p.drawPixmap(r,target,QRectF(0,0,target.width(),target.height()));p.restore();
    };
    text(m_art.titleFrom,m_art.titleTo,m_art.titleSmall,m_art.titleLarge);
    text(m_art.artistFrom,m_art.artistTo,m_art.artistSmall,m_art.artistLarge);
}
}