#pragma once
#include "library.h"
#include "motion.h"
#include <QVariantAnimation>
namespace relaxer {
class NextQueuePanel : public QWidget {
    Q_OBJECT
public:
    NextQueuePanel(Library *library,bool large,QWidget *parent);
    void setTracks(QVector<Track> tracks);
    void setAnchor(QPoint bottomLeft,int width,int footerHeight=98);
    int preferredWidth() const;
    void present(bool on,bool animate=true);
    void setExpanded(bool on,bool animate=true);
    bool expanded() const {return m_expanded;}
    bool presented() const {return m_presented;}
    void invalidateBackground();
    Track trackAt(int index) const {return m_tracks.value(index);}
    MovingArtwork artwork(int index) const;
    static QString limitedText(const QString &text,const QFont &font,int han);
signals:
    void activated(int index);
    void widthHintChanged();
protected:
    bool event(QEvent *) override;
    bool eventFilter(QObject *,QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void changeEvent(QEvent *) override;
private:
    Library *m_library;bool m_large,m_expanded=false,m_presented=false;
    QVector<Track> m_tracks;QPoint m_anchor;int m_width=310,m_hover=-1,m_footerHeight=98;
    qreal m_scroll=0,m_scrollPosition=0;
    QPixmap m_background;QRect m_backgroundRect;int m_backgroundBuilds=0;
    QVariantAnimation m_geometry,m_scrollSlide;
    int headerHeight() const {return 34;}
    int rowHeight() const;
    int coverSize() const {return m_expanded?(m_large?36:26):m_large?40:24;}
    int shownSongs() const;
    int targetHeight() const;
    QRect targetRect() const;
    QRect rowRect(int index) const;
    QRect arrowRect() const;
    QFont rowFont() const;
    void moveToTarget(bool animate);
    void cacheBackground();
};
class ArtworkFlight : public QWidget {
    Q_OBJECT
public:
    explicit ArtworkFlight(QWidget *parent);
    void start(MovingArtwork art,std::function<void()> finished,int stepDirection=0);
    void stop();
protected:
    void paintEvent(QPaintEvent *) override;
private:
    MovingArtwork m_art;QVariantAnimation m_animation;qreal m_progress=0;
    int m_stepDirection=0;
    std::function<void()> m_finished;
};
}
