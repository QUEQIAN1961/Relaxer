// Native adaptation of MakcRe/KuGouMusicApi v1.6.0 util/crypto.js (MIT).
// Copyright (c) 2023 MakcRe. Modified by QUEQIAN, 2026-11-14.
// See docs/third-party/music-platforms/kugou-MIT.txt.
#pragma once
#include <QByteArray>
#include <QCryptographicHash>
#ifdef Q_OS_WIN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#endif

namespace relaxer::kugou {
inline QByteArray aes(const QByteArray &input,const QByteArray &secret,bool encrypt){
#ifdef Q_OS_WIN
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_KEY_HANDLE key=nullptr;QByteArray result;
    const auto digest=QCryptographicHash::hash(secret,QCryptographicHash::Md5).toHex();auto material=digest.left(16);auto iv=digest.mid(16,16);
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_AES_ALGORITHM,nullptr,0)<0)return {};
    if(BCryptSetProperty(algorithm,BCRYPT_CHAINING_MODE,reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_CBC)),sizeof(BCRYPT_CHAIN_MODE_CBC),0)>=0&&
        BCryptGenerateSymmetricKey(algorithm,&key,nullptr,0,reinterpret_cast<PUCHAR>(material.data()),ULONG(material.size()),0)>=0){
        ULONG length=0;auto operation=encrypt?BCryptEncrypt:BCryptDecrypt;auto bytes=reinterpret_cast<PUCHAR>(const_cast<char *>(input.constData()));
        if(operation(key,bytes,ULONG(input.size()),nullptr,reinterpret_cast<PUCHAR>(iv.data()),16,nullptr,0,&length,BCRYPT_BLOCK_PADDING)>=0){
            iv=digest.mid(16,16);result.resize(length);
            if(operation(key,bytes,ULONG(input.size()),nullptr,reinterpret_cast<PUCHAR>(iv.data()),16,reinterpret_cast<PUCHAR>(result.data()),length,&length,BCRYPT_BLOCK_PADDING)<0)result.clear();
            else result.resize(length);
        }
    }
    if(key)BCryptDestroyKey(key);BCryptCloseAlgorithmProvider(algorithm,0);return result;
#else
    return {};
#endif
}
inline QByteArray portrait(const QByteArray &plain){
#ifdef Q_OS_WIN
    const auto der=QByteArray::fromBase64("MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDIAG7QOELSYoIJvTFJhMpe1s/gbjDJX51HBNnEl5HXqTW6lQ7LC8jr9fWZTwusknp+sVGzwd40MwP6U5yDE27M/X1+UR4tvOGOqp94TJtQ1EPnWGWXngpeIW5GxoQGao1rmYWAu6oi1z9XkChrsUdC6DJE5E221wf/4WLFxwAtRQIDAQAB");
    CERT_PUBLIC_KEY_INFO *info=nullptr;DWORD size=0;BCRYPT_KEY_HANDLE key=nullptr;QByteArray result;
    if(!CryptDecodeObjectEx(X509_ASN_ENCODING,X509_PUBLIC_KEY_INFO,reinterpret_cast<const BYTE *>(der.constData()),DWORD(der.size()),CRYPT_DECODE_ALLOC_FLAG,nullptr,&info,&size))return {};
    if(CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING,info,0,nullptr,&key)){
        ULONG length=0;auto bytes=reinterpret_cast<PUCHAR>(const_cast<char *>(plain.constData()));
        if(BCryptEncrypt(key,bytes,ULONG(plain.size()),nullptr,nullptr,0,nullptr,0,&length,BCRYPT_PAD_PKCS1)>=0){
            result.resize(length);
            if(BCryptEncrypt(key,bytes,ULONG(plain.size()),nullptr,nullptr,0,reinterpret_cast<PUCHAR>(result.data()),length,&length,BCRYPT_PAD_PKCS1)<0)result.clear();
            else result.resize(length);
        }
        BCryptDestroyKey(key);
    }
    LocalFree(info);return result.toHex().toUpper();
#else
    return {};
#endif
}
}
