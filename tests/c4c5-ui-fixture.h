#pragma once
#include "ui-bridge/qml-shell.h"
#include "translation-fixture.h"
#include "import-fixture.h"
#include <QQuickItem>
#include <QImage>
namespace c4c5_fixture {
inline bool wave(const QString &path){
    QFile file(path);if(!file.open(QIODevice::WriteOnly))return false;
    QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);out.writeRawData("RIFF",4);out<<quint32(960036);out.writeRawData("WAVEfmt ",8);
    out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);out.writeRawData("data",4);out<<quint32(960000);
    QByteArray pcm(960000,'\0');out.writeRawData(pcm.data(),pcm.size());return out.status()==QDataStream::Ok;
}
inline QQuickItem *visual(QQuickItem *item,const QString &name){
    if(item->objectName()==name)return item;for(auto *child:item->childItems())if(auto *found=visual(child,name))return found;return nullptr;
}
inline QList<QQuickItem *> delegates(QQuickItem *item){
    QList<QQuickItem *> items;if(item->objectName()=="lyricDelegate")items.append(item);
    for(auto *child:item->childItems())items.append(delegates(child));return items;
}
struct Fixture {
    QTemporaryDir dir;Library library{dir.filePath("data")};TranslationHttpMock http;
    std::unique_ptr<AiRuntime> runtime;std::unique_ptr<QmlShell> shell;Track a,b;bool ok=false;
    QByteArray source;
    explicit Fixture(int lines=80){
        if(!library.open()||!http.listen())return;
        a=seed(library,"中文 日本語 テスト English — Long title for elision","a");b=seed(library,"Second track","b");
        if(!a.id||!b.id||!wave(library.absolute(a.file))||!wave(library.absolute(b.file)))return;
        for(int i=0;i<lines;++i)source+=QString("[%1:%2.00]Hello moon %3 中文 日本語 English\n").arg(i/60,2,10,QChar('0')).arg(i%60,2,10,QChar('0')).arg(i).toUtf8();
        if(!library.saveLyrics(a.id,source)||!library.saveLyrics(b.id,"[00:00.00]Different\n[00:03.00]Second line\n"))return;
        QImage art(256,256,QImage::Format_RGB32);art.fill(QColor("#b52d50"));const auto cover=dir.filePath("cover.png");
        if(!art.save(cover)||!library.updateArtwork(a.id,cover))return;
        runtime=std::make_unique<AiRuntime>(&library,DeepSeekProvider::MockEndpoint{http.baseUrl()},std::make_unique<FakeCredentialStore>());
        if(!configureRuntime(*runtime))return;shell=std::make_unique<QmlShell>(&library,nullptr,true,runtime.get());ok=shell->load();
        if(ok)shell->facade()->play({a.id,b.id},a.id);
    }
};
}
