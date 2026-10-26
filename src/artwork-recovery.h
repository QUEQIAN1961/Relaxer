#pragma once
#include "model.h"
#include <QImage>
namespace relaxer {
constexpr qint64 maximumArtworkBytes=12*1024*1024;
ArtworkFileVersion artworkFileVersion(const QString &path);
QString artworkVersionKey(const ArtworkFileVersion &version);
bool isLegacyArtwork(const Track &track);
bool managedArtworkSource(const QString &root,const Track &track);
QString artworkAttemptKey(const QString &root,const Track &track);
// Both frontends and recovery use the same bounded decoder. A failure returns
// an empty image so each existing frontend can draw its usual placeholder.
QImage readArtworkImage(const QString &path,const QSize &size,Qt::AspectRatioMode aspect=Qt::KeepAspectRatio);
QImage decodeRecoveredArtwork(const QByteArray &png);
}
