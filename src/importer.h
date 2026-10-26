#pragma once
#include "model.h"
#include <QObject>
#include <QSemaphore>
#include <QStringList>
#include <atomic>
#include <memory>
namespace relaxer {
QString toolPath(const QString &name);
class Importer : public QObject {
    Q_OBJECT
  public:
    Importer(QString root, QStringList sources) : m_root(std::move(root)), m_sources(std::move(sources)) {}
    void cancel() { m_cancel->store(true); }
    void setCancellationToken(std::shared_ptr<std::atomic_bool> token) { m_cancel=std::move(token); }
    void setMaximumWorkers(int maximum) {m_maximumWorkers=maximum;}
    static QStringList extensions();
    static ImportResult importOne(const QString &root, const QString &source,
                                  std::atomic_bool *cancel = nullptr);
    static ArtworkResult repairArtwork(const QString &root, const Track &track,
                                       std::atomic_bool *cancel = nullptr);
  public slots:
    virtual void run();
  signals:
    void scanned(int total,int ignored,bool complete);
    void imported(const relaxer::ImportResult &result);
    void progress(int current, int total, const QString &name);
    void finished(bool cancelled);

  private:
    QString m_root;
    QStringList m_sources;
    int m_maximumWorkers=0;
    std::shared_ptr<std::atomic_bool> m_cancel=std::make_shared<std::atomic_bool>(false);
};
class ArtworkRepair : public QObject {
    Q_OBJECT
  public:
    ArtworkRepair(QString root, QVector<Track> tracks)
        : m_root(std::move(root)), m_tracks(std::move(tracks)) {}
    void cancel() { m_cancel->store(true); }
    void setCancellationToken(std::shared_ptr<std::atomic_bool> token) { m_cancel=std::move(token); }
    void setResultGate(std::shared_ptr<QSemaphore> gate) {m_resultGate=std::move(gate);}
  public slots:
    virtual void run();
  signals:
    void repaired(const relaxer::ArtworkResult &result);
    void finished();

  private:
    QString m_root;
    QVector<Track> m_tracks;
    std::shared_ptr<QSemaphore> m_resultGate;
    std::shared_ptr<std::atomic_bool> m_cancel=std::make_shared<std::atomic_bool>(false);
};
} // namespace relaxer
