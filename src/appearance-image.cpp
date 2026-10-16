#include "appearance-image.h"
#include <QImageReader>
#include <QtMath>
#include <cmath>
#include <utility>
namespace relaxer {
QColor autoInk(const QColor &color) {
    return color.redF() * .2126 + color.greenF() * .7152 + color.blueF() * .0722 > .53 ? QColor("#25252b")
                                                                                       : QColor("#f0edf1");
}
QImage backgroundImage(const QString &path, int maximum) {
    if (path.isEmpty())
        return {};
    QImageReader reader(path);
    reader.setAutoTransform(true);
    auto size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 100000000)
        return {};
    if (qMax(size.width(), size.height()) > maximum)
        reader.setScaledSize(size.scaled(maximum, maximum, Qt::KeepAspectRatio));
    return reader.read();
}
QImage fixedGaussianBlur(QImage image, int strength) {
    if (image.isNull() || !strength)
        return image;
    image = image.scaled(640, 640, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_ARGB32);
    // Three separable box passes approximate a Gaussian in linear time.
    int radius = qMax(1, qRound(strength / 3.0)), width = image.width(), height = image.height();
    for (int pass = 0; pass < 6; ++pass) {
        QImage output(image.size(), image.format());
        bool horizontal = pass % 2 == 0;
        int lines = horizontal ? height : width, length = horizontal ? width : height;
        for (int line = 0; line < lines; ++line) {
            auto pixel = [&](int pos) {
                int x = horizontal ? qBound(0, pos, width - 1) : line;
                int y = horizontal ? line : qBound(0, pos, height - 1);
                return reinterpret_cast<const QRgb *>(image.constScanLine(y))[x];
            };
            int r = 0, g = 0, b = 0, a = 0, count = 2 * radius + 1;
            auto add = [&](QRgb p, int sign) {
                r += sign * qRed(p);
                g += sign * qGreen(p);
                b += sign * qBlue(p);
                a += sign * qAlpha(p);
            };
            for (int i = -radius; i <= radius; ++i)
                add(pixel(i), 1);
            for (int pos = 0; pos < length; ++pos) {
                int x = horizontal ? pos : line, y = horizontal ? line : pos;
                reinterpret_cast<QRgb *>(output.scanLine(y))[x] =
                    qRgba(r / count, g / count, b / count, a / count);
                add(pixel(pos - radius), -1);
                add(pixel(pos + radius + 1), 1);
            }
        }
        image = std::move(output);
    }
    return image;
}
double blurRadius(double percent) {
    const double x=qBound(0.,percent,50.)/50.;
    return 17.*std::expm1(3.*x)/std::expm1(3.);
}
QImage gaussianBlur(QImage image,double strength) {
    if(image.isNull()||strength<=0)return image;
    // Keep fine detail at low values. Fractional box radii avoid a jump at 0.1%.
    image=image.convertToFormat(QImage::Format_ARGB32);
    const double radius=blurRadius(strength)*qMax(image.width(),image.height())/640.;
    const int lo=qFloor(radius),hi=lo+1;const double fraction=radius-lo;
    for(int pass=0;pass<6;++pass){
        QImage output(image.size(),image.format());const bool horizontal=pass%2==0;
        const int lines=horizontal?image.height():image.width(),length=horizontal?image.width():image.height();
        for(int line=0;line<lines;++line){
            auto pixel=[&](int p){const int x=horizontal?qBound(0,p,image.width()-1):line,y=horizontal?line:qBound(0,p,image.height()-1);return reinterpret_cast<const QRgb *>(image.constScanLine(y))[x];};
            int sums[2][4]{};
            auto add=[](int *sum,QRgb c,int sign){sum[0]+=sign*qRed(c);sum[1]+=sign*qGreen(c);sum[2]+=sign*qBlue(c);sum[3]+=sign*qAlpha(c);};
            for(int j=-hi;j<=hi;++j){const auto c=pixel(j);add(sums[1],c,1);if(j>=-lo&&j<=lo)add(sums[0],c,1);}
            for(int j=0;j<length;++j){
                int c[4];for(int k=0;k<4;++k)c[k]=qBound(0,qRound((1-fraction)*sums[0][k]/(2*lo+1)+fraction*sums[1][k]/(2*hi+1)),255);
                const int x=horizontal?j:line,y=horizontal?line:j;
                reinterpret_cast<QRgb *>(output.scanLine(y))[x]=qRgba(c[0],c[1],c[2],c[3]);
                add(sums[0],pixel(j-lo),-1);add(sums[0],pixel(j+lo+1),1);
                add(sums[1],pixel(j-hi),-1);add(sums[1],pixel(j+hi+1),1);
            }
        }
        image=std::move(output);
    }
    return image;
}

} // namespace relaxer
