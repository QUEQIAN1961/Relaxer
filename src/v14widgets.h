#pragma once
#include "widgets.h"
#include "playlist-ui.h"
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVariantAnimation>
namespace relaxer {
void roundedMask(QWidget *widget,int radius);
class FilterButton:public QPushButton {
    Q_OBJECT
public:
    FilterButton(const QString &caption,QWidget *parent=nullptr);
    QString value()const{return m_value;}
    void setOptions(QStringList options);
    void reset();
signals:
    void filterChanged();
private:
    QString m_caption,m_value;
    QStringList m_options;
    void choose();
    void select(const QString &value);
};
class ProfileHeader:public QWidget {
    Q_OBJECT
public:
    explicit ProfileHeader(QWidget *parent=nullptr);
    Cover *avatar;
    QLabel *name,*songs,*plays,*time;
    QPushButton *edit;
    void collapseTo(qreal progress);
    qreal collapse()const{return m_progress;}
    qreal target()const{return m_target;}
    void setDescription(const QString &text){m_tagline->setText(text);arrange();}
protected:
    void paintEvent(QPaintEvent *)override;
    void resizeEvent(QResizeEvent *)override;
private:
    DescriptionPreview *m_tagline;
    qreal m_progress=0,m_target=0;
    QVariantAnimation m_animation;
    void arrange();
};
}
