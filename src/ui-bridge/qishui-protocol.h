// Native adaptation of GSQZ/qishui-music-api 0.2.2, src/protocol.cjs (MIT).
// Copyright (c) 2026 sayqz. Modified by QUEQIAN, 2026-11-14.
// See docs/third-party/music-platforms/qishui-car-MIT.txt.
#pragma once
#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRandomGenerator>
#include <QString>
#ifdef Q_OS_WIN
#include <windows.h>
#include <bcrypt.h>
#endif

namespace relaxer::qishui {
inline const QString prefix=QStringLiteral("/api/luna/v1/platform");
inline QMap<QString,QString> params(const QString &device){
    return {{"aid","765213"},{"app_id","765213"},{"app_name","byte_autoservice_lite"},{"version_code","100010"},{"version_name","1.0.10"},
        {"channel","bydSong"},{"sdk_aid","753999"},{"caller_name","luna_open_sdk"},{"sdk_version_code","100000100"},{"sdk_version_name","0.0.1"},
        {"sdk_channel","Test"},{"vehicle_type","BYD"},{"device_brand","BYD"},{"device_type","BYD"},{"device_platform","android"},
        {"resolution","1920x1080"},{"os_version","13"},{"ac","wifi"},{"device_id",device},{"did",device},{"iid",device},{"vehicle_id","qishui-"+device},
        {"_timestamp",QString::number(QDateTime::currentSecsSinceEpoch())},{"_nonce",QString::number(QRandomGenerator::global()->bounded(65536))}};
}
inline QMap<QString,QString> qrParams(){
    return {{"aid","1128"},{"client_key","aw4bbxfujo741d09"},
        {"scope","user_info,luna.openapi.platform.interaction,luna.openapi.platform.me_tab,luna.commerce.openapi_platform.vip_info"},
        {"state",""},{"next",""},{"jump_type","native"},{"optional_scope_check",""},{"optional_scope_uncheck",""},
        {"customize_params","{\"source\":\"pc_auth\",\"not_skip_confirm\":\"true\"}"},{"source_from","native"},{"device_platform","android"},
        {"app_identity",QString::fromLatin1(QCryptographicHash::hash("com.luna.music.car",QCryptographicHash::Md5).toHex())},
        {"signature","cf9024ea72d9c71813238b13ea107a3d"}};
}
inline QByteArray der(char tag,const QByteArray &bytes={}){
    QByteArray out(1,tag);const auto n=bytes.size();
    if(n<128)out+=char(n);else if(n<256){out+=char(0x81);out+=char(n);}else{out+=char(0x82);out+=char(n>>8);out+=char(n);}
    return out+bytes;
}
inline QByteArray integer(QByteArray bytes){
    while(bytes.size()>1&&bytes.front()==0)bytes.remove(0,1);
    if(quint8(bytes.front())&0x80)bytes.prepend(char(0));return der(2,bytes);
}
#ifdef Q_OS_WIN
struct EcKey {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_KEY_HANDLE key=nullptr;
    EcKey(){BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_ECDSA_P256_ALGORITHM,nullptr,0);}
    ~EcKey(){if(key)BCryptDestroyKey(key);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    bool generate(){return algorithm&&BCryptGenerateKeyPair(algorithm,&key,256,0)>=0&&BCryptFinalizeKeyPair(key,0)>=0;}
    bool restore(const QByteArray &blob){return algorithm&&BCryptImportKeyPair(algorithm,nullptr,BCRYPT_ECCPRIVATE_BLOB,&key,
        reinterpret_cast<PUCHAR>(const_cast<char *>(blob.constData())),ULONG(blob.size()),0)>=0;}
    QByteArray exportBlob(LPCWSTR type){
        ULONG length=0;if(!key||BCryptExportKey(key,nullptr,type,nullptr,0,&length,0)<0)return {};
        QByteArray blob(length,0);if(BCryptExportKey(key,nullptr,type,reinterpret_cast<PUCHAR>(blob.data()),length,&length,0)<0)return {};return blob;
    }
    QByteArray sign(const QByteArray &content){
        auto digest=QCryptographicHash::hash(content,QCryptographicHash::Sha256);QByteArray raw(64,0);ULONG length=0;
        if(!key||BCryptSignHash(key,nullptr,reinterpret_cast<PUCHAR>(digest.data()),ULONG(digest.size()),reinterpret_cast<PUCHAR>(raw.data()),64,&length,0)<0||length!=64)return {};
        return der(0x30,integer(raw.left(32))+integer(raw.mid(32)));
    }
};
#endif
inline QJsonObject createGuard(){
#ifdef Q_OS_WIN
    EcKey key;if(!key.generate())return {};
    const auto blob=key.exportBlob(BCRYPT_ECCPUBLIC_BLOB);const auto secret=key.exportBlob(BCRYPT_ECCPRIVATE_BLOB);
    if(blob.size()!=72||secret.isEmpty())return {};
    const auto spki=der(0x30,der(0x30,der(6,QByteArray::fromHex("2a8648ce3d0201"))+der(6,QByteArray::fromHex("2a8648ce3d030107")))+
        der(3,QByteArray::fromHex("0004")+blob.mid(8)));
    QByteArray subject;
    for(const auto &entry:QList<QPair<char,QByteArray>>{{3,"bd-ticket-guard"},{11,""},{10,""}})
        subject+=der(0x31,der(0x30,der(6,QByteArray::fromHex("5504")+entry.first)+der(12,entry.second)));
    const auto info=der(0x30,der(2,QByteArray(1,0))+der(0x30,subject)+spki+der(char(0xa0)));
    const auto signature=key.sign(info);if(signature.isEmpty())return {};
    const auto csr=der(0x30,info+der(0x30,der(6,QByteArray::fromHex("2a8648ce3d040302")))+der(3,QByteArray(1,0)+signature)).toBase64();
    QByteArray pem="-----BEGIN CERTIFICATE REQUEST-----\n";for(int i=0;i<csr.size();i+=64)pem+=csr.mid(i,64)+'\n';pem+="-----END CERTIFICATE REQUEST-----\n";
    return {{"guardKey",QString::fromLatin1(secret.toBase64())},{"csr",QString::fromLatin1(pem.toBase64())}};
#else
    return {};
#endif
}
inline QMap<QByteArray,QByteArray> guardHeaders(const QJsonObject &credential,const QString &path){
#ifdef Q_OS_WIN
    EcKey key;if(!key.restore(QByteArray::fromBase64(credential.value("guardKey").toString().toLatin1())))return {};
    const auto ticket=credential.value("ticket").toString();const auto cert=credential.value("guardCert").toString();const auto ts=credential.value("guardTsSign").toString();
    if(ticket.isEmpty()||cert.isEmpty()||ts.isEmpty())return {};
    const auto timestamp=QDateTime::currentSecsSinceEpoch();
    const auto signature=key.sign(("ticket="+ticket+"&path="+prefix+path+"&timestamp="+QString::number(timestamp)).toUtf8());if(signature.isEmpty())return {};
    const QJsonObject data{{"ts_sign",ts},{"req_content","ticket,path,timestamp"},{"req_sign",QString::fromLatin1(signature.toBase64())},{"timestamp",timestamp}};
    return {{"access-token",ticket.toUtf8()},{"bd-ticket-guard-version","1"},{"bd-ticket-guard-client-cert",cert.toUtf8()},
        {"bd-ticket-guard-client-data",QJsonDocument(data).toJson(QJsonDocument::Compact).toBase64()}};
#else
    return {};
#endif
}
}
