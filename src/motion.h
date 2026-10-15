#pragma once
#include <QPixmap>
#include <QVariantAnimation>
#include <QWidget>
#include <functional>
namespace relaxer {
struct MovingArtwork {
    QRectF coverFrom, coverTo, titleFrom, titleTo, artistFrom, artistTo;
    QPixmap cover, titleSmall, titleLarge, artistSmall, artistLarge;
    QRectF titleOld,artistOld,coverOld;
    QPixmap titleOutgoing,artistOutgoing,coverOutgoing;
};
class PageMotion : public QWidget {
public:
    explicit PageMotion(QWidget *parent);
    void hold(const QRect &area, const QPixmap &image, const QPixmap &background = {});
    void startPage(const QPixmap &next, const QPixmap &background, std::function<void()> finished);
    void startPlayer(const QPixmap &oldClean, const QPixmap &nextClean, const QPixmap &backdrop,
                     const MovingArtwork &art, bool expanding, std::function<void()> finished, qreal initialReveal=-1, qreal initialDetails=-1);
    void stop();
    QPixmap currentPage() const;
    QPixmap currentContent();
    bool isPlayer() const { return m_player; }
    MovingArtwork currentArtwork() const;
    qreal currentReveal() const;
    qreal currentDetails() const;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QRect m_area;
    QPixmap m_old, m_next, m_background;
    MovingArtwork m_art;
    QVariantAnimation m_animation;
    std::function<void()> m_finished;
    qreal m_progress=0, m_initialReveal=0, m_initialDetails=0;
    bool m_player=false, m_expanding=true, m_contentOnly=false;
};
QPixmap transparentSnapshot(QWidget *widget);
QRect artworkRect(QWidget *widget, QWidget *ancestor);
}
