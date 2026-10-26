#include "artwork-recovery.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
namespace relaxer {
ArtworkFileVersion artworkFileVersion(const QString &path) {
    ArtworkFileVersion v;const QFileInfo f(path);v.path=f.absoluteFilePath();
    if(f.isFile()) {v.canonical=f.canonicalFilePath();v.size=f.size();v.modified=f.lastModified();
        v.metadataChanged=f.metadataChangeTime();v.created=f.birthTime();}
    return v;
}
QString artworkVersionKey(const ArtworkFileVersion &v) {
    const auto bytes=QJsonDocument(QJsonArray{v.path,v.canonical,QString::number(v.size),
        v.modified.toString(Qt::ISODateWithMs),v.metadataChanged.toString(Qt::ISODateWithMs),v.created.toString(Qt::ISODateWithMs)}).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
bool isLegacyArtwork(const Track &t) {
    static const QRegularExpression safe("^[A-Za-z0-9_-]{1,128}$");
    return t.id>0 && t.coverOrigin.isEmpty() && safe.match(t.hash).hasMatch() && t.cover=="Artwork/"+t.hash+".png";
}
bool managedArtworkSource(const QString &root,const Track &track) {
    if(QDir::isAbsolutePath(track.file)||QDir::cleanPath(track.file)!=track.file||!track.file.startsWith("Music/"))return false;
    const auto dir=QFileInfo(QDir(root).filePath("Music")).canonicalFilePath();
    const auto source=artworkFileVersion(QDir(root).filePath(track.file));
    return !dir.isEmpty() && source.size>=0 && source.canonical.startsWith(dir+"/",Qt::CaseInsensitive);
}
QString artworkAttemptKey(const QString &root,const Track &track) {
    return track.hash+":"+track.file+":"+track.cover+":"+
        artworkVersionKey(artworkFileVersion(QDir(root).filePath(track.file)))+":"+
        artworkVersionKey(artworkFileVersion(QDir(root).filePath(track.cover)));
}
static QImage decode(const QByteArray &bytes,const QSize &target,Qt::AspectRatioMode aspect,bool recovered) {
    if(bytes.isEmpty()||bytes.size()>maximumArtworkBytes)return {};
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);
    reader.setAutoTransform(true);const auto size=reader.size();
    if(!size.isValid()||qint64(size.width())*size.height()>120000000)return {};
    if(recovered && (reader.format()!="png"||size.width()>800||size.height()>800))return {};
    reader.setScaledSize(size.scaled(target,aspect));return reader.read();
}
QImage readArtworkImage(const QString &path,const QSize &size,Qt::AspectRatioMode aspect) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>maximumArtworkBytes)return {};
    const auto bytes=file.read(maximumArtworkBytes+1);if(!file.atEnd())return {};
    return decode(bytes,size,aspect,false);
}
QImage decodeRecoveredArtwork(const QByteArray &png) {return decode(png,QSize(800,800),Qt::KeepAspectRatio,true);}
}
