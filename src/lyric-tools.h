#pragma once
#include "widgets.h"
#include <QCheckBox>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QScrollBar>
#include <QPropertyAnimation>
namespace relaxer {
class FrostedLyricScroll : public QScrollBar {
public:
    explicit FrostedLyricScroll(QWidget *parent);
    void invalidateBackdrop(){m_backdrop={};}
    void cacheBackdrop();
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QPixmap m_backdrop;
};
class LyricTools : public QWidget {
    Q_OBJECT
public:
    LyricTools(LyricView *lyrics,QWidget *delayControls,QWidget *parent=nullptr);
    void setAvailable(bool available,bool timeline);
    void setShowTranslation(bool on);
    void invalidateBackdrop();
    void closeTools();
signals:
    void timedLyricsRequested();
    void translationDisplayChanged(bool on);
protected:
    void resizeEvent(QResizeEvent *) override;
    bool eventFilter(QObject *,QEvent *) override;
private:
    LyricView *m_lyrics;QWidget *m_delayControls;
    QPushButton *m_question,*m_delayEntry,*m_back;
    QCheckBox *m_translation=nullptr;
    QFrame *m_menu;FrostedLyricScroll *m_scroll;
    QTimer m_hideTimer,m_cacheTimer;
    bool m_available=true,m_timeline=false,m_pinned=false,m_controlsVisible=false;
    void reveal();void hideControls();void arrange();
    void animateControl(QWidget *,bool);
};
}
