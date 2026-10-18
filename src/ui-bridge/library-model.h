#pragma once
#include "library.h"
#include <QAbstractListModel>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QTimer>

namespace relaxer {
// One read-only snapshot, refreshed once per coalesced Library::changed burst.
// No database connection, persistence or playback authority is owned here.
class TrackListModel final : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Role { TrackId = Qt::UserRole + 1, Title, Artist, Album, Duration, Favorite, File, TrackKey };
    explicit TrackListModel(Library *library, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    qint64 idAt(int row) const;
    int rowForId(qint64 id) const;
    QVector<qint64> ids() const;
    bool matches(int row, const QString &text) const;
    qint64 playlist() const { return m_playlist; }
    void setPlaylist(qint64 playlist);
    void refresh();
  private:
    QPointer<Library> m_library;
    QVector<Track> m_tracks;
    QHash<qint64, int> m_rows;
    QTimer m_refresh;
    qint64 m_playlist = 0;
};

class TrackFilterModel final : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int sortKey READ sortKey WRITE setSortKey NOTIFY sortChanged)
    Q_PROPERTY(bool descending READ descending WRITE setDescending NOTIFY sortChanged)
    Q_PROPERTY(QString selectedId READ selectedId WRITE setSelectedId NOTIFY selectionChanged)
    Q_PROPERTY(int selectedRow READ selectedRow NOTIFY selectionChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
  public:
    explicit TrackFilterModel(TrackListModel *source, QObject *parent = nullptr);
    QString filterText() const { return m_filter; }
    void setFilterText(const QString &text);
    int sortKey() const { return m_sortKey; }
    void setSortKey(int key);
    bool descending() const { return m_descending; }
    void setDescending(bool value);
    QString selectedId() const { return QString::number(m_selected); }
    void setSelectedId(const QString &id);
    int selectedRow() const;
    int count() const { return rowCount(); }
    QVector<qint64> ids() const;
    Q_INVOKABLE QString idAt(int row) const;
  signals:
    void filterTextChanged();
    void sortChanged();
    void selectionChanged();
    void countChanged();
  protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
  private:
    QPointer<TrackListModel> m_source;
    QString m_filter;
    qint64 m_selected = 0;
    int m_sortKey = 0;
    bool m_descending = false;
    QTimer m_notifications;
    void changed();
};

class PlaylistListModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
  public:
    explicit PlaylistListModel(Library *library, QObject *parent = nullptr);
    int count() const { return m_playlists.size(); }
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool contains(qint64 id) const;
  signals:
    void countChanged();
  private:
    QPointer<Library> m_library;
    QVector<Playlist> m_playlists;
    QTimer m_refresh;
    void refresh();
};
}
