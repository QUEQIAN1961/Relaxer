#pragma once
#include "library.h"
#include "playback-facade.h"
#include <QAbstractListModel>
#include <QPointer>
#include <QTimer>
#include <QUrl>

namespace relaxer {
// A projection of the existing engine's manual-next forecast. This owns no
// Queue, shuffle bag, persistence, or database connection.
class UpcomingModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString firstTrackKey READ firstTrackKey NOTIFY firstChanged)
    Q_PROPERTY(QString firstTitle READ firstTitle NOTIFY firstChanged)
    Q_PROPERTY(QString firstArtist READ firstArtist NOTIFY firstChanged)
    Q_PROPERTY(QUrl firstCoverUrl READ firstCoverUrl NOTIFY firstChanged)
    Q_PROPERTY(QString firstCoverRevision READ firstCoverRevision NOTIFY firstChanged)
  public:
    enum Role { TrackId = Qt::UserRole + 1, TrackKey, Title, Artist, Album, Duration, Favorite, CoverUrl, CoverRevision, UpcomingIndex };
    explicit UpcomingModel(Library *library, PlaybackFacade *facade, QObject *parent = nullptr);
    int count() const { return int(m_tracks.size()); }
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVector<qint64> ids() const { return m_ids; }
    QString firstTrackKey() const { return m_tracks.isEmpty() ? QString("0") : QString::number(m_tracks.first().id); }
    QString firstTitle() const { return m_tracks.isEmpty() ? QString() : m_tracks.first().title; }
    QString firstArtist() const { return m_tracks.isEmpty() ? QString() : m_tracks.first().artist; }
    QUrl firstCoverUrl() const { return m_tracks.isEmpty() ? QUrl() : artwork(m_tracks.first()); }
    QString firstCoverRevision() const { return m_artworkRevisions.isEmpty() ? QString() : m_artworkRevisions.first(); }
    Q_INVOKABLE QString idAt(int row) const;
    // Revalidates the displayed row against the engine immediately before the
    // command. A stale row can never start an unrelated forecast entry.
    Q_INVOKABLE bool activate(int row);
    void refresh();
    // Counts this adapter's additional work, excluding the existing facade's
    // own snapshot reads. These are C++ diagnostics, not QML properties.
    quint64 metadataReadCount() const { return m_metadataReads; }
    quint64 forecastReadCount() const { return m_forecastReads; }
  signals:
    void countChanged();
    void firstChanged();
    void aboutToActivate(int row, const QString &trackKey, const QUrl &coverUrl);
  private:
    QPointer<Library> m_library;
    QPointer<PlaybackFacade> m_facade;
    QVector<Track> m_tracks;
    QVector<QString> m_artworkRevisions;
    QVector<qint64> m_ids, m_sourceIds;
    qint64 m_current = 0;
    int m_sourceIndex = -1;
    Mode m_mode = Mode::ListLoop;
    QTimer m_refresh;
    bool m_forecastDirty = true, m_metadataDirty = true;
    quint64 m_metadataReads = 0, m_forecastReads = 0;
    QUrl artwork(const Track &track) const;
    QString artworkRevision(const Track &track) const;
    void schedule(bool forecast, bool metadata = false);
    void observe();
};
}
