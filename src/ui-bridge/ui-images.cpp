#include "ui-images.h"
#include "artwork-recovery.h"
#include "appearance-image.h"
#include <QPainter>
#include <QPainterPath>
#include <QCryptographicHash>
#include <QImageReader>
#include <QUrlQuery>
#include <QMutexLocker>
#include <QFileInfo>
namespace relaxer {
static QImage iconImage(const QString &name, const QColor &c) {
    QImage pm(64, 64, QImage::Format_ARGB32_Premultiplied);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(2, 2);
    p.setPen(QPen(c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (name == "play") {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(QPolygonF{{11, 7}, {11, 25}, {25, 16}});
    } else if (name == "pause") {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(QRectF(9, 7, 5, 18), 1, 1);
        p.drawRoundedRect(QRectF(19, 7, 5, 18), 1, 1);
    } else if (name == "next" || name == "previous") {
        if (name == "previous") {
            p.translate(32, 0);
            p.scale(-1, 1);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(QPolygonF{{8, 8}, {8, 24}, {21, 16}});
        p.drawRoundedRect(QRectF(22, 8, 3, 16), 1, 1);
    } else if (name == "music") {
        p.drawLine(13, 23, 13, 8);
        p.drawLine(13, 8, 25, 5);
        p.drawLine(25, 5, 25, 20);
        p.setBrush(c);
        p.drawEllipse(QRectF(6, 21, 7, 5));
        p.drawEllipse(QRectF(18, 18, 7, 5));
    } else if (name == "playlist") {
        for (int y : {9, 16, 23})
            p.drawLine(6, y, 19, y);
        p.drawLine(25, 18, 25, 26);
        p.drawLine(21, 22, 29, 22);
    } else if (name == "heart" || name == "liked") {
        QPainterPath path;
        path.moveTo(16, 27);
        path.cubicTo(4, 19, 2, 12, 7, 7);
        path.cubicTo(11, 3, 15, 6, 16, 9);
        path.cubicTo(18, 4, 23, 4, 26, 8);
        path.cubicTo(31, 14, 24, 21, 16, 27);
        if (name == "liked")
            p.setBrush(c);
        p.drawPath(path);
    } else if (name == "person") {
        p.drawEllipse(QRectF(11, 5, 10, 10));
        p.drawArc(QRectF(5, 17, 22, 18), 0, 180 * 16);
    } else if (name == "settings") {
        p.drawEllipse(QRectF(9, 9, 14, 14));
        p.drawEllipse(QRectF(13, 13, 6, 6));
        for (int i = 0; i < 8; ++i) {
            p.save();
            p.translate(16, 16);
            p.rotate(i * 45);
            p.drawLine(0, -10, 0, -13);
            p.restore();
        }
    } else if (name == "search") {
        p.drawEllipse(QRectF(6, 5, 15, 15));
        p.drawLine(20, 20, 27, 27);
    } else if (name == "plus") {
        p.drawLine(16, 7, 16, 25);
        p.drawLine(7, 16, 25, 16);
    } else if (name == "repeat" || name == "single") {
        p.drawLine(7, 10, 25, 10);
        p.drawLine(25, 10, 21, 6);
        p.drawLine(25, 10, 21, 14);
        p.drawLine(25, 22, 7, 22);
        p.drawLine(7, 22, 11, 18);
        p.drawLine(7, 22, 11, 26);
        p.drawLine(6, 10, 6, 17);
        p.drawLine(26, 15, 26, 22);
        if (name == "single") {
            p.setFont(QFont("Segoe UI", 9, QFont::Bold));
            p.drawText(QRectF(10, 10, 12, 12), Qt::AlignCenter, "1");
        }
    } else if (name == "shuffle") {
        p.drawLine(6, 8, 10, 8);
        p.drawLine(10, 8, 22, 24);
        p.drawLine(22, 24, 28, 24);
        p.drawLine(24, 20, 28, 24);
        p.drawLine(24, 28, 28, 24);
        p.drawLine(6, 24, 10, 24);
        p.drawLine(10, 24, 22, 8);
        p.drawLine(22, 8, 28, 8);
        p.drawLine(24, 4, 28, 8);
        p.drawLine(24, 12, 28, 8);
    } else if (name == "volume" || name == "volumeMuted" || name == "volumeLow" ||
               name == "volumeMedium" || name == "volumeHigh") {
        p.drawPolygon(QPolygonF{{4, 12}, {9, 12}, {14, 7}, {14, 25}, {9, 20}, {4, 20}});
        if(name=="volumeMuted"){
            p.drawLine(19,12,26,20);p.drawLine(19,20,26,12);
        }else{
            const int waves=name=="volumeLow"?1:name=="volumeMedium"?2:3;
            for(int i=0;i<waves;++i){
                const qreal x=18+4*i,reach=3+2*i,half=3+3*i;
                QPainterPath wave;wave.moveTo(x,16-half);wave.quadTo(x+reach,16,x,16+half);p.drawPath(wave);
            }
        }
    } else if (name == "expand") {
        p.drawLine(7, 13, 7, 7);
        p.drawLine(7, 7, 13, 7);
        p.drawLine(19, 7, 25, 7);
        p.drawLine(25, 7, 25, 13);
        p.drawLine(25, 19, 25, 25);
        p.drawLine(25, 25, 19, 25);
        p.drawLine(13, 25, 7, 25);
        p.drawLine(7, 25, 7, 19);
    } else if (name == "contract") {
        p.drawLine(7,13,13,13);p.drawLine(13,13,13,7);
        p.drawLine(19,7,19,13);p.drawLine(19,13,25,13);
        p.drawLine(25,19,19,19);p.drawLine(19,19,19,25);
        p.drawLine(13,25,13,19);p.drawLine(13,19,7,19);
    } else if (name == "collapse") {
        p.drawLine(7, 11, 16, 20);
        p.drawLine(16, 20, 25, 11);
    } else if (name == "back") {
        p.drawLine(20, 7, 11, 16);
        p.drawLine(11, 16, 20, 25);
    } else if (name == "folder") {
        p.drawPath([] {
            QPainterPath f;
            f.moveTo(5, 9);
            f.lineTo(13, 9);
            f.lineTo(16, 12);
            f.lineTo(27, 12);
            f.lineTo(27, 26);
            f.lineTo(5, 26);
            f.closeSubpath();
            return f;
        }());
    } else if (name == "up" || name == "down") {
        if (name == "down") {
            p.translate(0, 32);
            p.scale(1, -1);
        }
        p.drawLine(16, 25, 16, 7);
        p.drawLine(9, 14, 16, 7);
        p.drawLine(16, 7, 23, 14);
    } else if (name == "clock") {
        p.drawEllipse(QRectF(5, 5, 22, 22));
        p.drawLine(16, 10, 16, 16);
        p.drawLine(16, 16, 21, 19);
    } else {
        p.setFont(QFont("Segoe UI", 18, QFont::Bold));
        p.drawText(QRectF(0, 0, 32, 32), Qt::AlignCenter, "…");
    }
    return pm;
}

QImage UiImages::requestImage(const QString &id,QSize *resultSize,const QSize &requested) {
    const QUrl url("image://relaxer/"+id);const QUrlQuery query(url);
    if(url.path().startsWith("/icon/")) {
        auto image=iconImage(url.path().mid(6),QColor(query.queryItemValue("color",QUrl::FullyDecoded)));
        if(resultSize)*resultSize=image.size();return image;
    }
    const bool blur=url.path()=="/blur";
    const int preferred=query.queryItemValue("resolution").toInt();
    const int size=qBound(64,requested.isValid()?qMax(requested.width(),requested.height()):(preferred>0?preferred:640),960);
    const QFileInfo info(QUrl(query.queryItemValue("file",QUrl::FullyDecoded)).toLocalFile());
    const QString cacheKey=id+QString::number(size)+QString::number(info.size())+artworkVersionKey(artworkFileVersion(info.absoluteFilePath()));
    {QMutexLocker lock(&m_mutex);if(auto *cached=m_cache.object(cacheKey)){if(resultSize)*resultSize=cached->size();return *cached;}}
    const auto seed=query.queryItemValue("seed",QUrl::FullyDecoded);const auto file=QUrl(query.queryItemValue("file",QUrl::FullyDecoded));QImage out;
    if(url.path()=="/background"){
        auto background=file.isLocalFile()?backgroundImage(file.toLocalFile(),1600):QImage();
        if(!background.isNull())background=gaussianBlur(background,qBound(0.,query.queryItemValue("blur").toDouble(),50.));
        if(resultSize)*resultSize=background.size();
        if(!background.isNull()){QMutexLocker lock(&m_mutex);m_cache.insert(cacheKey,new QImage(background),qMax(1,int(background.sizeInBytes()/1024)));}
        return background;
    }

    if(file.isLocalFile()) {
        out=readArtworkImage(file.toLocalFile(),QSize(size,size),Qt::KeepAspectRatioByExpanding);
        if(!out.isNull()){out=out.scaled(size,size,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);out=out.copy((out.width()-size)/2,(out.height()-size)/2,size,size);}
    }
    if (out.isNull()) {
        auto h = QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha256);
        int hue = (uchar(h[0]) * 360) / 256;
        out = QImage(size, size, QImage::Format_ARGB32_Premultiplied);
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient g(0, 0, size, size);
        g.setColorAt(0, QColor::fromHsl(hue, 90, 170));
        g.setColorAt(1, QColor::fromHsl((hue + 38) % 360, 105, 85));
        p.fillRect(out.rect(), g);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 22));
        p.drawEllipse(QRectF(size * .15, -size * .25, size * 1.3, size * 1.3));
        p.setBrush(QColor(255, 255, 255, 25));
        p.drawEllipse(QRectF(-size * .3, size * .3, size, size));
        p.setPen(QPen(QColor(255, 255, 255, 180), size * .025, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(size * .46, size * .62), QPointF(size * .46, size * .30));
        p.drawLine(QPointF(size * .46, size * .30), QPointF(size * .68, size * .25));
        p.drawLine(QPointF(size * .68, size * .25), QPointF(size * .68, size * .55));
        p.setBrush(QColor(255, 255, 255, 180));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(size * .32, size * .59, size * .14, size * .10));
        p.drawEllipse(QRectF(size * .54, size * .52, size * .14, size * .10));
        p.setPen(QColor(255, 255, 255, 165));
        QFont f("Segoe UI");
        f.setPixelSize(qMax(10, size / 28));
        f.setLetterSpacing(QFont::AbsoluteSpacing, size / 100.);
        p.setFont(f);
        p.drawText(QRectF(size * .1, size * .86, size * .8, size * .06), Qt::AlignCenter, "RELAXER");
    }

    if(blur)out=gaussianBlur(out,query.hasQueryItem("blur") ? qBound(0.,query.queryItemValue("blur").toDouble(),50.) : 50.);
    else {QImage rounded(out.size(),QImage::Format_ARGB32_Premultiplied);rounded.fill(Qt::transparent);QPainter p(&rounded);p.setRenderHint(QPainter::Antialiasing);QPainterPath mask;const auto ratio=qBound(.0,query.queryItemValue("radiusRatio").isEmpty()?.075:query.queryItemValue("radiusRatio").toDouble(),.5);mask.addRoundedRect(rounded.rect(),size*ratio,size*ratio);p.setClipPath(mask);p.drawImage(0,0,out);p.end();out=rounded;}
    if(resultSize)*resultSize=out.size();
    {QMutexLocker lock(&m_mutex);m_cache.insert(cacheKey,new QImage(out),qMax(1,int(out.sizeInBytes()/1024)));}
    return out;
}
}
