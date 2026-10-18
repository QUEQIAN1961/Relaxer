#pragma once
#include "ai/ai-runtime.h"
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
namespace relaxer {
class AiBridge final:public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY changed)
    Q_PROPERTY(QVariantList models READ models NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY changed)
public:
    AiBridge(ai::AiRuntime *runtime,QObject *parent=nullptr);
    QVariantMap draft()const{return m_draft;}QVariantList models()const;
    bool busy()const{return m_request!=0;}QString status()const{return m_status;}
    QString credentialStatus()const{return m_credentialStatus;}
    Q_INVOKABLE void resetDraft();
    Q_INVOKABLE bool edit(const QString &key,const QVariant &value);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool storeKey(QString secret);
    Q_INVOKABLE bool removeKey();
    Q_INVOKABLE bool checkCredential();
    Q_INVOKABLE bool refreshModels(bool confirmed=false);
signals:void changed();
private:
    ai::AiRuntime *m_runtime;QVariantMap m_draft;quint64 m_request=0;
    QString m_status,m_credentialStatus="凭据尚未检查 · 保存在Windows凭据管理器";
    void status(const QString &text);
};
}
