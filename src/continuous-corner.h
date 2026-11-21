#pragma once
#include <QPainterPath>
#include <algorithm>
namespace relaxer {
inline constexpr qreal ArtworkCornerRatio=.08;
inline qreal artworkCornerRadius(const QRectF &rect){return std::max(qreal(0),std::min(rect.width(),rect.height())*ArtworkCornerRatio);}
inline QPainterPath continuousCornerPath(const QRectF &rect,qreal radius){
    const qreal c=std::max(qreal(0),std::min({radius*1.6,rect.width()/2,rect.height()/2}));
    const auto l=rect.left(),t=rect.top(),r=rect.right(),b=rect.bottom();QPainterPath path;
    const qreal u=c*.1830582617584078;
    path.moveTo(l+c,t);path.lineTo(r-c,t);
    path.cubicTo(r-4*u,t,r-2*u,t,r-u,t+u);path.cubicTo(r,t+2*u,r,t+4*u,r,t+c);
    path.lineTo(r,b-c);path.cubicTo(r,b-4*u,r,b-2*u,r-u,b-u);path.cubicTo(r-2*u,b,r-4*u,b,r-c,b);
    path.lineTo(l+c,b);path.cubicTo(l+4*u,b,l+2*u,b,l+u,b-u);path.cubicTo(l,b-2*u,l,b-4*u,l,b-c);
    path.lineTo(l,t+c);path.cubicTo(l,t+4*u,l,t+2*u,l+u,t+u);path.cubicTo(l+2*u,t,l+4*u,t,l+c,t);
    path.closeSubpath();return path;
}
}
