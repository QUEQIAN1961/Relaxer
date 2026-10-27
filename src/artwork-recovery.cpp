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
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#elif defined(Q_OS_UNIX)
#include <sys/file.h>
#include <sys/stat.h>
#endif
namespace relaxer {
ArtworkSourceGuard::ArtworkSourceGuard(const QString &path) {
#ifdef Q_OS_WIN
    const auto handle=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,
        FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return;
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(handle,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) {CloseHandle(handle);return;}
    const auto fd=_open_osfhandle(reinterpret_cast<intptr_t>(handle),_O_RDONLY|_O_BINARY);
    if(fd==-1){CloseHandle(handle);return;}
    if(!file.open(fd,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle)){_close(fd);return;}
    identity=QByteArray::number(info.dwVolumeSerialNumber)+":"+QByteArray::number(info.nFileIndexHigh)+":"+QByteArray::number(info.nFileIndexLow);
#elif defined(Q_OS_UNIX)
    file.setFileName(path);
    if(!file.open(QIODevice::ReadOnly))return;
    struct stat info{};
    // Advisory on Unix: non-cooperating writers are additionally detected by
    // digest/metadata checks, but cannot be excluded as Windows sharing does.
    if(flock(file.handle(),LOCK_SH|LOCK_NB)!=0||fstat(file.handle(),&info)!=0||!S_ISREG(info.st_mode)){file.close();return;}
    identity=QByteArray::number(qulonglong(info.st_dev))+":"+QByteArray::number(qulonglong(info.st_ino));
#else
    Q_UNUSED(path); // No integrity guard available: fail closed.
#endif
}
QByteArray ArtworkSourceGuard::sha256(std::atomic_bool *cancel) {
    if(!isOpen()||!file.seek(0))return {};
    const auto size=file.size();if(size<0)return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);QByteArray chunk(256*1024,Qt::Uninitialized);qint64 total=0;
    while(total<size){
        if(cancel&&cancel->load())return {};
        const auto n=file.read(chunk.data(),qMin(qint64(chunk.size()),size-total));
        if(n<=0)return {};
        hash.addData(QByteArrayView(chunk.constData(),n));total+=n;
    }
    if((cancel&&cancel->load())||file.error()!=QFileDevice::NoError||file.size()!=size||!file.atEnd())return {};
    return hash.result();
}
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
