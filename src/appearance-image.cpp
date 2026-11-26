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
QImage gaussianBlur(QImage image, double strength) {
    if (image.isNull() || strength <= 0)
        return image;

    image = image.convertToFormat(QImage::Format_ARGB32);
    if (image.isNull())
        return image;

    const int width = image.width();
    const int height = image.height();
    const double radius = blurRadius(strength) * qMax(width, height) / 640.;
    const int lo = qFloor(radius), hi = lo + 1;
    const double fraction = radius - lo;
    QImage output(image.size(), image.format());
    for (int pass = 0; pass < 6; ++pass) {
        if (output.isNull())
            return image;

        const bool horizontal = pass % 2 == 0;
        const int lines = horizontal ? height : width;
        const int length = horizontal ? width : height;
        const auto inputStride = image.bytesPerLine() / qsizetype(sizeof(QRgb));
        const auto outputStride = output.bytesPerLine() / qsizetype(sizeof(QRgb));
        const auto inputStep = horizontal ? 1 : inputStride;
        const auto outputStep = horizontal ? 1 : outputStride;
        const auto *inputBits = reinterpret_cast<const QRgb *>(image.constBits());
        auto *outputBits = reinterpret_cast<QRgb *>(output.bits());
        for (int line = 0; line < lines; ++line) {
            const auto *input = inputBits + line * (horizontal ? inputStride : 1);
            auto *dest = outputBits + line * (horizontal ? outputStride : 1);
            const auto pixel = [&](int position) {
                return input[qBound(0, position, length - 1) * inputStep];
            };
            int sums[2][4]{};
            const auto add = [](int *sum, QRgb color, int sign) {
                sum[0] += sign * qRed(color);
                sum[1] += sign * qGreen(color);
                sum[2] += sign * qBlue(color);
                sum[3] += sign * qAlpha(color);
            };
            for (int index = -hi; index <= hi; ++index) {
                const auto color = pixel(index);
                add(sums[1], color, 1);
                if (index >= -lo && index <= lo)
                    add(sums[0], color, 1);
            }
            for (int index = 0; index < length; ++index) {
                int color[4];
                for (int channel = 0; channel < 4; ++channel)
                    color[channel] = qBound(0, qRound((1 - fraction) * sums[0][channel] / (2 * lo + 1)
                                                 + fraction * sums[1][channel] / (2 * hi + 1)), 255);
                dest[index * outputStep] = qRgba(color[0], color[1], color[2], color[3]);
                add(sums[0], pixel(index - lo), -1);
                add(sums[0], pixel(index + lo + 1), 1);
                add(sums[1], pixel(index - hi), -1);
                add(sums[1], pixel(index + hi + 1), 1);
            }
        }

        image.swap(output);
        // Release the supplied buffer, which may share storage with the caller.
        // The remaining passes alternate between two private buffers.
        if (pass == 0)
            output = QImage(image.size(), image.format());
    }
    return image;
}

} // namespace relaxer
