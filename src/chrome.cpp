#include "chrome.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>
namespace relaxer {
CaptionButton::CaptionButton(Action action,QWidget *parent):QAbstractButton(parent),m_action(action) {
    const QStringList names{"windowMinimize","windowMaximize","windowClose"},tips{"最小化","最大化 / 还原","关闭"};
    setObjectName(names[action]);setToolTip(tips[action]);setAccessibleName(tips[action]);setFixedSize(46,34);setFocusPolicy(Qt::NoFocus);
    connect(this,&QAbstractButton::clicked,this,[this]{auto *host=window();if(m_action==Minimize)host->showMinimized();else if(m_action==Close)host->close();else if(host->isMaximized())host->showNormal();else host->showMaximized();});
}
void CaptionButton::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);QColor ink=palette().color(QPalette::WindowText);
    if(underMouse()||property("nativeHover").toBool()||isDown()){p.fillRect(rect(),m_action==Close?QColor("#e81123"):QColor(128,128,128,35));if(m_action==Close)ink=Qt::white;}
    p.setPen(QPen(ink,1.2));const QPointF c=QRectF(rect()).center();
    if(m_action==Minimize)p.drawLine(c+QPointF(-5,2),c+QPointF(5,2));
    else if(m_action==Close){p.drawLine(c+QPointF(-4,-4),c+QPointF(4,4));p.drawLine(c+QPointF(-4,4),c+QPointF(4,-4));}
    else if(window()->isMaximized()){p.drawRect(QRectF(c.x()-3,c.y()-5,8,8));p.fillRect(QRectF(c.x()-5,c.y()-3,8,8),palette().color(QPalette::Window));p.drawRect(QRectF(c.x()-5,c.y()-3,8,8));}
    else p.drawRect(QRectF(c.x()-5,c.y()-5,10,10));
}
TitleBar::TitleBar(QWidget *parent):QWidget(parent){setObjectName("windowControls");setFixedSize(138,34);auto *row=new QHBoxLayout(this);row->setContentsMargins(0,0,0,0);row->setSpacing(0);for(auto a:{CaptionButton::Minimize,CaptionButton::Maximize,CaptionButton::Close})row->addWidget(new CaptionButton(a,this));window()->installEventFilter(this);}
bool TitleBar::eventFilter(QObject *object,QEvent *event){if(object==window()&&event->type()==QEvent::WindowStateChange)for(auto *button:findChildren<CaptionButton *>())button->update();return QWidget::eventFilter(object,event);}
}
