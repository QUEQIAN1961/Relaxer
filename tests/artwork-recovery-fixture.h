#pragma once
#include "artwork-recovery.h"
#include "import-coordinator.h"
#include "import-fixture.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
#include <QJsonObject>
#include <QSemaphore>
#include <QTemporaryDir>
#include <functional>
namespace artwork_fixture {
using namespace relaxer;
inline bool write(const QString &path,const QByteArray &bytes) {
    QFile f(path);return f.open(QIODevice::WriteOnly|QIODevice::Truncate)&&f.write(bytes)==bytes.size();
}
inline QByteArray bytes(const QString &path) {QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
inline QByteArray png(const QColor &color) {
    QImage image(96,80,QImage::Format_RGB32);image.fill(color);QByteArray b;QBuffer out(&b);out.open(QIODevice::WriteOnly);image.save(&out,"PNG");return b;
}
inline bool ffmpeg(const QStringList &args) {
    QProcess p;p.start(toolPath("ffmpeg"),QStringList{"-v","error","-y"}+args);
    return p.waitForFinished(20000)&&p.exitStatus()==QProcess::NormalExit&&p.exitCode()==0;
}
inline QString embedded(const QString &dir,const QString &name,const QColor &color,int tone=440) {
    const auto image=QDir(dir).filePath(name+".png"),audio=QDir(dir).filePath(name+".mp3");
    if(!write(image,png(color))||!ffmpeg({"-f","lavfi","-i",QString("sine=frequency=%1:duration=0.25").arg(tone),"-i",image,
        "-map","0:a","-map","1:v","-c:a","libmp3lame","-c:v","png","-disposition:v:0","attached_pic",
        "-metadata","title=Same title","-metadata","album=Same album",audio}))return {};
    return audio;
}
inline Track legacy(Library &lib,const QString &source,const QByteArray &cache=png(Qt::blue)) {
    auto imported=Importer::importOne(lib.root(),source);if(!imported.error.isEmpty())return {};
    auto t=imported.track;t.id=0;t.cover="Artwork/"+t.hash+".png";t.coverOrigin.clear();
    if(!cache.isNull()&&!write(lib.absolute(t.cover),cache))return {};
    t.id=lib.insert(t);return t.id?lib.track(t.id):Track{};
}
inline bool closeColor(const QImage &image,const QColor &color) {
    if(image.isNull())return false;const auto c=image.pixelColor(image.width()/2,image.height()/2);
    return qAbs(c.red()-color.red())<8&&qAbs(c.green()-color.green())<8&&qAbs(c.blue()-color.blue())<8;
}
class ScriptedRepair final:public ArtworkRepair {
public:
    ScriptedRepair(const QString &root,const QVector<Track> &tracks,std::shared_ptr<std::atomic_bool> token,
                   std::function<void(ScriptedRepair &)> fn):ArtworkRepair(root,tracks),root(root),tracks(tracks),token(std::move(token)),fn(std::move(fn)){}
    void run()override {fn(*this);}
    QString root;QVector<Track> tracks;std::shared_ptr<std::atomic_bool> token;std::function<void(ScriptedRepair &)> fn;
};
class ControlledCoordinator final:public ImportCoordinator {
public:
    using ImportCoordinator::ImportCoordinator;
    std::function<void(ScriptedRepair &)> script;
    std::atomic_int *produced=nullptr;
protected:
    ArtworkRepair *createArtworkRepair(const QVector<Track> &tracks,const std::shared_ptr<std::atomic_bool> &cancel)override {
        ArtworkRepair *worker=script?new ScriptedRepair(libraryRoot,tracks,cancel,script):ImportCoordinator::createArtworkRepair(tracks,cancel);
        if(produced)connect(worker,&ArtworkRepair::repaired,worker,[n=produced](const ArtworkResult &){++*n;},Qt::DirectConnection);
        return worker;
    }
public:QString libraryRoot;
};
}
