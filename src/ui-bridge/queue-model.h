#pragma once
#include "library-model.h"
#include "playback-facade.h"

namespace relaxer {
// Read-only projection of PlaybackFacade::queue(); never a second Queue engine.
class QueueListModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
  public:
    QueueListModel(Library *library, PlaybackFacade *facade, QObject *parent = nullptr);
    int count() const { return m_tracks.size(); }
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE int indexForKey(const QString &id) const;
    QVector<qint64> ids() const { return m_ids; }
  signals:
    void countChanged();
    void missingTracks(const QVector<qint64> &ids);
  private:
    QPointer<Library> m_library;
    QPointer<PlaybackFacade> m_facade;
    QVector<Track> m_tracks;
    QVector<qint64> m_ids;
    QTimer m_refresh;
    void schedule();
    void refresh();
};
}
