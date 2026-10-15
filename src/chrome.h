#pragma once
#include <QAbstractButton>
#include <QWidget>
namespace relaxer {
class CaptionButton : public QAbstractButton {
    Q_OBJECT
public:
    enum Action { Minimize, Maximize, Close };
    CaptionButton(Action action,QWidget *parent);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    Action m_action;
};
class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent);
protected:
    bool eventFilter(QObject *,QEvent *) override;
};
}
