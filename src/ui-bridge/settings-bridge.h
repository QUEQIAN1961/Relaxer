#pragma once
#include "library.h"
#include "storage/settings-store.h"
#include "storage/appearance-store.h"
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
namespace relaxer {
class SettingsBridge final:public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap palette READ palette NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList appearances READ appearances NOTIFY settingsChanged)
    Q_PROPERTY(QString dataRoot READ dataRoot CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit SettingsBridge(Library *library,QObject *parent=nullptr);
    QVariantMap values()const;QVariantMap draft()const{return m_draft.toVariantMap();}
    QVariantMap palette()const;QVariantList appearances()const;
    QString dataRoot()const{return m_library->root();}QString status()const{return m_status;}
    Q_INVOKABLE void resetDraft();
    Q_INVOKABLE bool edit(const QString &key,const QVariant &value);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool setTheme(int theme);
    Q_INVOKABLE bool useAppearance(const QString &id);
    Q_INVOKABLE bool savePalette(const QString &name,const QString &background,const QString &panel,const QString &accent);
signals:void settingsChanged();void draftChanged();void statusChanged();
private:
    Library *m_library;SettingsStore m_store;AppearanceStore m_appearance;QJsonObject m_draft;QString m_status;
    void message(const QString &value);QString validate(const QJsonObject &value)const;
};
}
