#pragma once
#include "model.h"
#include <QObject>
#include <QStringList>
#include <atomic>
namespace relaxer {
QString toolPath(const QString &name);
class Importer : public QObject {
    Q_OBJECT
  public:
    Importer(QString root, QStringList sources) : m_root(std::move(root)), m_sources(std::move(sources)) {}
    void cancel() { m_cancel.store(true); }
    static QStringList extensions();
    static ImportResult importOne(const QString &root, const QString &source,
                                  std::atomic_bool *cancel = nullptr);
    static ArtworkResult repairArtwork(const QString &root, const Track &track,
                                       std::atomic_bool *cancel = nullptr);
  public slots:
    void run();
  signals:
    void imported(const relaxer::ImportResult &result);
    void progress(int current, int total, const QString &name);
    void finished(bool cancelled);

  private:
    QString m_root;
    QStringList m_sources;
    std::atomic_bool m_cancel{false};
};
class ArtworkRepair : public QObject {
    Q_OBJECT
  public:
    ArtworkRepair(QString root, QVector<Track> tracks)
        : m_root(std::move(root)), m_tracks(std::move(tracks)) {}
    void cancel() { m_cancel.store(true); }
  public slots:
    void run();
  signals:
    void repaired(const relaxer::ArtworkResult &result);
    void finished();

  private:
    QString m_root;
    QVector<Track> m_tracks;
    std::atomic_bool m_cancel{false};
};
} // namespace relaxer
