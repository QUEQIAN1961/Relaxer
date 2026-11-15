#pragma once
#include <QVariantAnimation>
#include <functional>
namespace relaxer {
class AudioFader {
public:
    explicit AudioFader(std::function<void(qreal)> apply):m_apply(std::move(apply)){
        m_animation.setDuration(220);
        m_animation.setEasingCurve(QEasingCurve::InOutSine);
        QObject::connect(&m_animation,&QVariantAnimation::valueChanged,[this](const QVariant &value){m_gain=value.toReal();m_apply(m_gain);});
        QObject::connect(&m_animation,&QVariantAnimation::finished,[this]{auto done=std::move(m_done);m_done={};if(done)done();});
    }
    qreal gain()const{return m_gain;}
    bool pausing()const{return m_animation.state()==QAbstractAnimation::Running&&m_target==0;}
    void to(qreal target,bool animate,std::function<void()> done={}){
        m_animation.stop();m_done=std::move(done);m_target=target;
        if(!animate||qFuzzyCompare(m_gain,target)){
            m_gain=target;m_apply(m_gain);auto callback=std::move(m_done);m_done={};if(callback)callback();return;
        }
        m_animation.setStartValue(m_gain);m_animation.setEndValue(target);m_animation.start();
    }
    void reset(qreal gain=1){to(gain,false);}
    void finish(){to(m_target,false,std::move(m_done));}
private:
    QVariantAnimation m_animation;
    std::function<void(qreal)> m_apply;
    std::function<void()> m_done;
    qreal m_gain=1,m_target=1;
};
}
