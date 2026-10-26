#pragma once
#include "importer.h"
#include <QObject>
#include <QPointer>
#include <memory>

namespace relaxer {
class Library;
struct ImportRequest {
    QStringList sources;
    qint64 targetPlaylist=0;
    // Zero retains the existing limit: min(CPU count, 8).
    int maximumWorkers=0;
};
struct ArtworkSummary {
    quint64 taskId=0;
    int total=0,processed=0,recovered=0,retained=0,placeholder=0;
    int noSource=0,invalid=0,failed=0,stale=0,unstarted=0;
    bool cancelled=false;
    QStringList errors;
};
struct ImportSummary {
    quint64 taskId=0;
    qint64 targetPlaylist=0;
    int sourceCount=0,totalFiles=0,ignoredPaths=0,processed=0;
    int added=0,duplicates=0,failed=0,unstarted=0,skipped=0,playlistFailures=0;
    bool scanComplete=false,cancelled=false,partialSuccess=false;
    QString fatalError;
    QStringList errors;
};

// All public commands belong on this object's (and Library's) owning thread.
// Workers receive only paths, value objects and a shared cancellation token.
// A task's final signal follows both the result barrier and the thread's exit.
class ImportCoordinator : public QObject {
    Q_OBJECT
public:
    explicit ImportCoordinator(Library *library,QObject *parent=nullptr);
    ~ImportCoordinator() override;
    quint64 start(const ImportRequest &request);
    bool cancel(quint64 taskId=0);
    bool isBusy() const;
    bool isArtworkBusy() const;
    bool isIdle() const;
    bool isShuttingDown() const;
    quint64 activeTaskId() const;
    QString lastError() const;
    bool repairLegacyArtwork(bool retryFailed=false);
    void shutdown();
signals:
    void progress(quint64 taskId,int current,int total,const QString &name);
    void trackCommitted(quint64 taskId,qint64 trackId,bool added);
    void finished(const relaxer::ImportSummary &summary);
    void requestRejected(const QString &error);
    void artworkStarted(int total);
    void artworkCompleted(const relaxer::ArtworkSummary &summary);
    void artworkUpdated(qint64 trackId);
    void artworkFinished(int corrected);
    void shutdownReady();
protected:
    // A controlled backend can be substituted by the independent service tests.
    virtual Importer *createImporter(const ImportRequest &request,const std::shared_ptr<std::atomic_bool> &cancel);
    virtual ArtworkRepair *createArtworkRepair(const QVector<Track> &tracks,const std::shared_ptr<std::atomic_bool> &cancel);
private:
    struct Impl;
    std::unique_ptr<Impl> d;
    void finishImport();
    void finishArtwork();
    void flushProgress();
    void finishShutdown();
    bool ownerThread() const;
};
}
Q_DECLARE_METATYPE(relaxer::ImportRequest)
Q_DECLARE_METATYPE(relaxer::ImportSummary)
Q_DECLARE_METATYPE(relaxer::ArtworkSummary)
