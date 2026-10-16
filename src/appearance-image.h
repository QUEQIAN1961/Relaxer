#pragma once
#include <QColor>
#include <QImage>
#include <QString>
namespace relaxer {
QImage gaussianBlur(QImage image, double strength);
QImage fixedGaussianBlur(QImage image,int strength);
double blurRadius(double percent);
QColor autoInk(const QColor &color);
QImage backgroundImage(const QString &path, int maximum = 1600);
} // namespace relaxer
