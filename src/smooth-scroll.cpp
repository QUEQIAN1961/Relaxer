#include "smooth-scroll.h"
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QApplication>
#include <QHash>
#include <QPointer>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>
#include <cmath>
namespace relaxer {
class SmoothScrolling final : public QObject {
public:
    explicit SmoothScrolling(QObject *parent):QObject(parent){setObjectName("smoothScrollController");parent->installEventFilter(this);}
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()==QEvent::Show){
            if(auto *view=qobject_cast<QAbstractItemView *>(object)){view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);}
        }
        if(event->type()==QEvent::MouseButtonPress){if(auto *bar=qobject_cast<QScrollBar *>(object))if(auto *a=m_animations.value(bar))a->stop();}
        if(event->type()!=QEvent::Wheel)return false;
        auto *widget=qobject_cast<QWidget *>(object);if(!widget)return false;
        QAbstractScrollArea *area=nullptr;
        for(auto *child=widget;child;child=child->parentWidget()){
            if(qobject_cast<QAbstractSpinBox *>(child)||qobject_cast<QComboBox *>(child)||qobject_cast<QAbstractSlider *>(child))return false;
            if(auto *candidate=qobject_cast<QAbstractScrollArea *>(child->parentWidget());candidate&&candidate->viewport()==child){area=candidate;break;}
        }
        if(!area)return false;
        auto *wheel=static_cast<QWheelEvent *>(event);
        if(wheel->modifiers()&Qt::ControlModifier)return false;
        if(auto *view=qobject_cast<QAbstractItemView *>(area)){view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);}
        const bool pixels=!wheel->pixelDelta().isNull();
        const auto delta=pixels?wheel->pixelDelta():wheel->angleDelta();
        const bool horizontal=(wheel->modifiers()&Qt::ShiftModifier)||qAbs(delta.x())>qAbs(delta.y());
        auto *bar=horizontal?area->horizontalScrollBar():area->verticalScrollBar();
        if(bar->maximum()<=bar->minimum())return false;
        const int amount=horizontal?(delta.x()?delta.x():delta.y()):delta.y();if(!amount)return false;
        auto *animation=m_animations.value(bar);
        if(!animation){
            animation=new QPropertyAnimation(bar,"value",bar);animation->setEasingCurve(QEasingCurve::OutCubic);m_animations.insert(bar,animation);
            connect(bar,&QObject::destroyed,this,[this,bar]{m_animations.remove(bar);});
            connect(bar,&QScrollBar::sliderPressed,animation,&QPropertyAnimation::stop);
        }
        const int base=animation->state()==QAbstractAnimation::Running?animation->endValue().toInt():bar->value();
        const qreal distance=pixels?amount:amount/120.0*qMax(16,bar->singleStep())*QApplication::wheelScrollLines();
        const int target=qBound(bar->minimum(),qRound(base-distance),bar->maximum());
        animation->stop();animation->setDuration(pixels?85:170);animation->setStartValue(bar->value());animation->setEndValue(target);
        bar->setProperty("smoothScrollTarget",target);animation->start();wheel->accept();return true;
    }
private:
    QHash<QScrollBar *,QPropertyAnimation *> m_animations;
};
void installSmoothScrolling(QObject *application){if(application&&!application->findChild<QObject *>("smoothScrollController"))new SmoothScrolling(application);}
}
