#pragma once
#include "import-coordinator.h"
#include <QObject>
#include <QUrl>
#include <QVariantMap>
namespace relaxer {
class ImportBridge final:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(int current READ current NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
    Q_PROPERTY(bool closing READ closing NOTIFY changed)
public:
    ImportBridge(Library *library,ImportCoordinator *coordinator,QObject *parent=nullptr);
    bool busy()const{return m_coordinator&&m_coordinator->isBusy();}
    int current()const{return m_current;}int total()const{return m_total;}
    QString status()const{return m_status;}QVariantMap result()const{return m_result;}
    bool closing()const{return m_closing;}
    Q_INVOKABLE bool start(const QList<QUrl> &urls,const QString &playlistKey="0");
    Q_INVOKABLE bool cancel();
    Q_INVOKABLE void beginShutdown();
signals:void changed();void contentsChanged();void readyToClose();
private:
    Library *m_library;QPointer<ImportCoordinator> m_coordinator;quint64 m_task=0;
    int m_current=0,m_total=0;QString m_status;QVariantMap m_result;bool m_closing=false;
};
}
