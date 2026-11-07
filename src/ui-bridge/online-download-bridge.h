#pragma once
#include "library.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSaveFile>
#include <QThread>
#include <QVariantMap>
#include <QJsonObject>
#include <QAbstractListModel>
#include <memory>
namespace relaxer {
class OnlineDownloadBridge final:public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY downloadsChanged)
    Q_PROPERTY(int taskRevision READ taskRevision NOTIFY tasksChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY tasksChanged)
    Q_PROPERTY(QString downloadDirectory READ downloadDirectory CONSTANT)
public:
    explicit OnlineDownloadBridge(Library *library,QObject *parent=nullptr);
    ~OnlineDownloadBridge() override;
    bool busy() const{return m_busy;}
    QString status() const{return m_status;}
    double progress() const{return m_progress;}
    int revision() const{return m_revision;}
    int taskRevision() const{return m_taskRevision;}
    QString downloadDirectory() const;
    Q_INVOKABLE bool openDownloadDirectory();
    int rowCount(const QModelIndex &parent={}) const override{return parent.isValid()?0:m_tasks.size();}
    QVariant data(const QModelIndex &index,int role) const override;
    QHash<int,QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap task(const QString &provider,const QVariantMap &song) const;
    Q_INVOKABLE void enqueue(const QString &provider,const QVariantMap &song,const QString &quality);
    bool enqueueTo(const QVariantMap &song,const QString &path,const QString &quality={},const QUrl &url={},const QString &extension={});
    Q_INVOKABLE void cancelTask(int taskId);
    Q_INVOKABLE bool isDownloaded(const QString &provider,const QVariantMap &song);
    Q_INVOKABLE void cancel();
    void chooseDestination(const QVariantMap &song,const QUrl &url,const QString &extension);
    bool downloadTo(const QVariantMap &song,const QUrl &url,const QString &extension,const QString &path);
    void resolutionFailed(const QVariantMap &song,const QString &error);
signals:
    void changed();
    void downloadsChanged();
    void tasksChanged();
    void taskAdded();
    void resolveRequested(const QVariantMap &song,const QString &quality);
    void downloaded(const QString &path);
private:
    Library *m_library;QNetworkAccessManager m_network;QPointer<QNetworkReply> m_reply;QPointer<QThread> m_worker;
    std::unique_ptr<QSaveFile> m_file;QString m_partial,m_requested,m_status,m_key;QByteArray m_prefix;
    bool m_busy=false;double m_progress=0;int m_revision=0;QJsonObject m_downloads;QVariantMap m_song;QHash<QString,QString> m_known;QVector<Track> m_local;bool m_localReady=false;
    struct Task {int id=0;QVariantMap song;QString key,path,quality,extension,state="queued",error;QUrl url;qint64 received=0,total=0;double progress=0;};
    QVector<Task> m_tasks;int m_nextId=0,m_activeId=0,m_taskRevision=0;bool m_cancelled=false;
    QString songKey(const QString &provider,const QVariantMap &song) const;
    QString destinationPath(const QVariantMap &song,const QString &extension);
    Task *activeTask();
    QVariantMap taskData(const Task &task) const;
    void updateTask();
    void startNext();
    bool startTransfer(const QVariantMap &song,const QUrl &url,const QString &extension,const QString &path);
    void complete(const QString &path,const QString &error={});
};
}
