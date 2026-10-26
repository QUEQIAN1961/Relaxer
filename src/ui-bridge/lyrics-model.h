#pragma once
#include "lyrics-document-reader.h"
#include "playback-facade.h"
#include "translation-coordinator.h"
#include <QAbstractListModel>
#include <QPointer>
#include <QTimer>
#include <QUrl>
namespace relaxer {
class LyricsModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY documentChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString trackKey READ trackKey NOTIFY documentChanged)
    Q_PROPERTY(bool timed READ timed NOTIFY documentChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY documentChanged)
    Q_PROPERTY(bool preview READ preview NOTIFY documentChanged)
    Q_PROPERTY(QString status READ status NOTIFY documentChanged)
    Q_PROPERTY(QString error READ error NOTIFY documentChanged)
    Q_PROPERTY(int offsetMs READ offsetMs NOTIFY offsetChanged)
    Q_PROPERTY(QUrl cover READ cover NOTIFY artworkChanged)
public:
    enum Role { RowKey=Qt::UserRole+1, Timestamp, Original, Translation, Current, Played, HasTranslation };
    LyricsModel(Library *library,PlaybackFacade *facade,QObject *parent=nullptr);
    int rowCount(const QModelIndex &parent={}) const override;
    QVariant data(const QModelIndex &index,int role) const override;
    QHash<int,QByteArray> roleNames() const override;
    int count() const {return m_rows.size();}
    int currentIndex() const {return m_current;}
    QString trackKey() const {return QString::number(m_track.id);}
    bool timed() const {return m_timeline.timed&&!m_timeline.instrumental;}
    bool loading() const {return m_loading;}
    bool preview() const {return m_preview;}
    QString status() const;
    QString error() const {return m_error;}
    int offsetMs() const {return m_offset;}
    QUrl cover() const {return m_cover;}
    Q_INVOKABLE bool setOffsetMs(int value);
    Q_INVOKABLE bool seekRow(int row);
    Q_INVOKABLE qint64 timestampAt(int row) const { return row>=0&&row<m_rows.size()?m_rows[row].time:0; }
    Q_INVOKABLE void reload();
    void presentTranslation(const TranslationSummary &summary);
signals:
    void documentChanged();
    void currentIndexChanged();
    void offsetChanged();
    void artworkChanged();
private:
    struct Row {int documentRow=0;qint64 time=-1;QString original,translation;};
    QPointer<Library> m_library;QPointer<PlaybackFacade> m_facade;
    Track m_track;QByteArray m_source,m_presented;Lyrics m_timeline;QVector<Row> m_rows;
    QTimer m_reload;QUrl m_cover;QString m_error;int m_current=-1,m_offset=0;
    bool m_loading=false,m_preview=false,m_force=false;
    void schedule();void readCurrent();void present(const QByteArray &bytes,bool preview);
    void position(qint64 ms);void clear();
};
}
