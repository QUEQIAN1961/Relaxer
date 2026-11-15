#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QString>

namespace relaxer {
// Account flags come from account responses, never from a song's pay status.
inline QString accountMembership(const QString &provider,const QJsonObject &profile,qint64 now=QDateTime::currentMSecsSinceEpoch()) {
    if(provider=="qq"){
        const auto identity=profile.value("identity").toObject();
        if(identity.value("svip").toInt()>0||profile.value("svip").toInt()>0)return QStringLiteral("超级会员");
        if(identity.value("vip").toInt()>0)return QStringLiteral("绿钻会员");
        return identity.contains("vip")||identity.contains("svip")?QStringLiteral("普通账号"):QString();
    }
    if(provider=="netease")return profile.contains("vipType")?(profile.value("vipType").toInt()>0?QStringLiteral("网易云音乐会员"):QStringLiteral("普通账号")):QString();
    const auto details=profile.value("vip_info").toObject();
    const auto value=[&](const QString &key){return details.contains(key)?details.value(key):profile.value(key);};
    bool known=false,active=false;
    for(const auto &key:{QString("is_vip"),QString("vip_type"),QString("vip_status")}){
        const auto flag=value(key);if(flag.isBool()){known=true;active|=flag.toBool();}else if(flag.isDouble()){known=true;active|=flag.toInt()>0;}
    }
    if(!known)return {};
    for(const auto &key:{QString("expire_time"),QString("vip_end_time")}){
        qint64 expiry=value(key).toVariant().toLongLong();if(expiry>0){if(expiry<100000000000LL)expiry*=1000;if(expiry<=now)active=false;}
    }
    return active?(provider=="kugou"?QStringLiteral("酷狗音乐会员"):QStringLiteral("汽水音乐会员")):QStringLiteral("普通账号");
}
}
