// NCM container/key-box protocol adapted from taurusxin/ncmdump (MIT).
// See licenses/ncmdump-MIT.txt. Restores the original codec without transcoding.
#include "ncm-restore.h"
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QThread>
#include <array>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif
namespace relaxer {
namespace {
QByteArray decryptKey(QByteArray bytes){
#ifdef Q_OS_WIN
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_KEY_HANDLE key=nullptr;
    struct Cleanup {BCRYPT_ALG_HANDLE &a;BCRYPT_KEY_HANDLE &k;~Cleanup(){if(k)BCryptDestroyKey(k);if(a)BCryptCloseAlgorithmProvider(a,0);}} cleanup{algorithm,key};
    QByteArray secret("hzHRAmso5kInbaxW");
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_AES_ALGORITHM,nullptr,0)<0||BCryptSetProperty(algorithm,BCRYPT_CHAINING_MODE,reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_ECB)),sizeof(BCRYPT_CHAIN_MODE_ECB),0)<0||BCryptGenerateSymmetricKey(algorithm,&key,nullptr,0,reinterpret_cast<PUCHAR>(secret.data()),secret.size(),0)<0)return {};
    ULONG size=0;if(BCryptDecrypt(key,reinterpret_cast<PUCHAR>(bytes.data()),bytes.size(),nullptr,nullptr,0,nullptr,0,&size,BCRYPT_BLOCK_PADDING)<0)return {};
    QByteArray output(size,Qt::Uninitialized);if(BCryptDecrypt(key,reinterpret_cast<PUCHAR>(bytes.data()),bytes.size(),nullptr,nullptr,0,reinterpret_cast<PUCHAR>(output.data()),size,&size,BCRYPT_BLOCK_PADDING)<0)return {};output.resize(size);return output;
#else
    Q_UNUSED(bytes) return {};
#endif
}
}
NcmRestoreResult restoreNcm(const QString &input,const QString &outputBase){
    QFile file(input);if(!file.open(QIODevice::ReadOnly))return {{},"无法读取下载的 NCM 文件"};
    if(file.read(8)!="CTENFDAM"||!file.seek(10))return {{},"NCM 文件头无效"};
    QDataStream stream(&file);stream.setByteOrder(QDataStream::LittleEndian);quint32 length=0;stream>>length;
    if(length==0||length>1024*1024||length>file.size()-file.pos())return {{},"NCM 密钥数据损坏"};
    auto encoded=file.read(length);for(auto &byte:encoded)byte^=0x64;auto key=decryptKey(encoded);
    if(!key.startsWith("neteasecloudmusic")||key.size()<=17)return {{},"无法还原 NCM 音频密钥"};key=key.mid(17);
    std::array<quint8,256> box{},mask{};for(int i=0;i<256;++i)box[i]=i;
    int last=0,offset=0;for(int i=0;i<256;++i){const auto value=box[i];const int next=(value+last+quint8(key[offset]))&255;box[i]=box[next];box[next]=value;last=next;offset=(offset+1)%key.size();}
    for(int i=0;i<256;++i){const int j=(i+1)&255;mask[i]=box[(box[j]+box[(box[j]+j)&255])&255];}
    stream>>length;if(length>file.size()-file.pos()||!file.seek(file.pos()+length+5))return {{},"NCM 元数据损坏"};
    quint32 frame=0,image=0;stream>>frame>>image;
    if(stream.status()!=QDataStream::Ok||image>frame||frame>file.size()-file.pos()||!file.seek(file.pos()+frame))return {{},"NCM 封面数据损坏"};
    auto bytes=file.read(32768);qint64 position=0;
    auto decode=[&](QByteArray &data){for(int i=0;i<data.size();++i)data[i]=char(quint8(data[i])^mask[(position+i)&255]);position+=data.size();};decode(bytes);
    const auto format=bytes.startsWith("fLaC")?QString("flac"):(bytes.startsWith("ID3")||(bytes.size()>1&&quint8(bytes[0])==255&&(quint8(bytes[1])&224)==224))?QString("mp3"):QString();
    if(format.isEmpty())return {{},"NCM 内的音频不是 MP3 或 FLAC"};const auto path=QFileInfo(outputBase+"."+format).absoluteFilePath();
    if(QFileInfo::exists(path))return {{},"本地文件已存在，已阻止重复下载"};QSaveFile output(path);if(!output.open(QIODevice::WriteOnly))return {{},"无法创建本地音频文件"};
    while(!bytes.isEmpty()){
        if(QThread::currentThread()->isInterruptionRequested())return {{},"已取消下载"};
        if(output.write(bytes)!=bytes.size())return {{},"音频文件写入失败"};bytes=file.read(32768);decode(bytes);
    }
    if(file.error()!=QFile::NoError||!output.commit())return {{},"音频保存失败"};return {path,{}};
}
}
