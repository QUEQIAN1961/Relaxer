#include "v14widgets.h"
#include "appearance.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QResizeEvent>
#include <QTimer>
#include <algorithm>
namespace relaxer {
void roundedMask(QWidget *w,int radius){
    if(w->width()<2||w->height()<2)return;
    QPainterPath shape;shape.addRoundedRect(QRectF(w->rect()),radius,radius);
    w->setMask(QRegion(shape.toFillPolygon().toPolygon()));
}
namespace {
class FilterPopup:public QWidget {
public:
    FilterPopup(QWidget *parent):QWidget(parent,Qt::Popup|Qt::FramelessWindowHint|Qt::NoDropShadowWindowHint){
        setObjectName("filterPopup");setAttribute(Qt::WA_TranslucentBackground);
    }
    void captureBackground(QWidget *window) {
        setProperty("blurPercent",25);
        const QPoint offset=window->mapFromGlobal(pos());
        const QRect region(offset,size());
        const auto snapshot=window->grab();
        const qreal dpr=snapshot.devicePixelRatio();
        const int margin=24;
        const QRect expanded=region.adjusted(-margin,-margin,margin,margin);
        QImage canvas(qRound(expanded.width()*dpr),qRound(expanded.height()*dpr),QImage::Format_ARGB32_Premultiplied);
        canvas.fill(window->palette().color(QPalette::Window));
        QPainter painter(&canvas);
        painter.scale(dpr,dpr);painter.drawPixmap(QPointF(-expanded.x(),-expanded.y()),snapshot);
        painter.end();
        canvas.setDevicePixelRatio(1);
        m_background=fixedGaussianBlur(canvas,13);
        m_source=QRectF(qreal(margin)/expanded.width()*m_background.width(),qreal(margin)/expanded.height()*m_background.height(),
                       qreal(width())/expanded.width()*m_background.width(),qreal(height())/expanded.height()*m_background.height());
    }
    void paintEvent(QPaintEvent *)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath shape;shape.addRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),10,10);
        p.setClipPath(shape);
        p.fillRect(rect(),qApp->palette().color(QPalette::Button));
        if(!m_background.isNull())p.drawImage(QRectF(rect()),m_background,m_source);
        QColor tint=qApp->palette().color(QPalette::Button);tint.setAlpha(220);
        p.fillRect(rect(),tint);
        p.setClipping(false);p.setPen(QPen(qApp->palette().color(QPalette::Mid),1));p.setBrush(Qt::NoBrush);p.drawPath(shape);
    }
    void resizeEvent(QResizeEvent *e)override{QWidget::resizeEvent(e);roundedMask(this,10);}
private:
    QImage m_background;
    QRectF m_source;
};
}
FilterButton::FilterButton(const QString &caption,QWidget *parent):QPushButton(parent),m_caption(caption){
    setObjectName(caption=="歌手"?"artistFilter":"albumFilter");setCursor(Qt::PointingHandCursor);setMinimumWidth(108);setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);setMaximumWidth(190);reset();
    connect(this,&QPushButton::clicked,this,&FilterButton::choose);
}
void FilterButton::select(const QString &value){m_value=value;setText(m_caption+"："+(value.isEmpty()?QString("全部"):fontMetrics().elidedText(value,Qt::ElideRight,130)));setToolTip(!value.isEmpty()&&fontMetrics().elidedText(value,Qt::ElideRight,130)!=value?value:QString());emit filterChanged();}
void FilterButton::reset(){select({});}
void FilterButton::setOptions(QStringList options){options.removeAll(QString());options.removeDuplicates();options.sort(Qt::CaseInsensitive);m_options=std::move(options);if(!m_value.isEmpty()&&!m_options.contains(m_value))select({});}
void FilterButton::choose(){
    auto *popup=new FilterPopup(this);popup->setAttribute(Qt::WA_DeleteOnClose);auto *layout=new QVBoxLayout(popup);layout->setContentsMargins(10,10,10,10);
    auto *search=new QLineEdit(popup);search->setObjectName("filterSearch");search->setPlaceholderText("搜索"+m_caption+"名称");search->setClearButtonEnabled(true);layout->addWidget(search);
    auto *list=new QListWidget(popup);list->setObjectName("filterOptions");list->setFrameShape(QFrame::NoFrame);layout->addWidget(list,1);
    auto *empty=new QLabel("没有匹配的"+m_caption,popup);empty->setObjectName("muted");empty->hide();layout->addWidget(empty);
    const auto options=m_options;const auto fill=[list,empty,options](const QString &text){list->clear();auto *all=new QListWidgetItem("全部",list);all->setData(Qt::UserRole,QString());int found=0;for(const auto &option:options)if(option.contains(text,Qt::CaseInsensitive)){auto *item=new QListWidgetItem(option,list);item->setData(Qt::UserRole,option);++found;}empty->setVisible(found==0&&!text.isEmpty());};
    fill({});connect(search,&QLineEdit::textChanged,popup,fill);connect(list,&QListWidget::itemClicked,popup,[this,popup](QListWidgetItem *item){const auto value=item->data(Qt::UserRole).toString();popup->close();select(value);});
    popup->resize(330,qMin(380,96+qMax(2,qMin(9,m_options.size()+1))*30));const auto available=screen()->availableGeometry();auto point=mapToGlobal(QPoint(0,height()+5));if(point.y()+popup->height()>available.bottom())point.setY(mapToGlobal(QPoint()).y()-popup->height()-5);point.setX(qBound(available.left(),point.x(),qMax(available.left(),available.right()-popup->width()+1)));popup->move(point);popup->captureBackground(window());popup->show();search->setFocus();
}
ProfileHeader::ProfileHeader(QWidget *parent):QWidget(parent){
    setObjectName("profileHeader");setFixedHeight(192);
    avatar=new Cover(this);avatar->setObjectName("profileAvatar");avatar->setFixedSize(76,76);avatar->setRadius(38);
    name=new QLabel(this);name->setObjectName("profileName");name->setTextFormat(Qt::PlainText);
    m_tagline=new DescriptionPreview(this);m_tagline->setObjectName("profileDescription");m_tagline->setProperty("emptyText",QString());m_tagline->setText("收藏、回忆，还有反复听过的那些歌。");
    edit=new QPushButton("编辑资料",this);edit->setObjectName("profileEdit");edit->setCursor(Qt::PointingHandCursor);
    songs=new QLabel("0",this);plays=new QLabel("0",this);time=new QLabel("0 分钟",this);
    for(auto *value:{songs,plays,time}){value->setObjectName("statValue");value->hide();}
    connect(&m_animation,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){m_progress=value.toReal();setFixedHeight(qRound(192-112*m_progress));arrange();update();});
}
void ProfileHeader::collapseTo(qreal progress){progress=progress>0?1:0;if(qFuzzyCompare(m_target+1,progress+1))return;m_target=progress;m_animation.stop();m_animation.setStartValue(m_progress);m_animation.setEndValue(progress);m_animation.setDuration(180);m_animation.setEasingCurve(QEasingCurve::OutCubic);m_animation.start();}
void ProfileHeader::resizeEvent(QResizeEvent *e){QWidget::resizeEvent(e);arrange();}
void ProfileHeader::arrange(){
    avatar->move(0,0);const int editWidth=qMax(92,edit->sizeHint().width());edit->setGeometry(width()-editWidth,18,editWidth,edit->sizeHint().height());
    const int textWidth=qMax(120,qRound(width()-96-editWidth-18-238*m_progress));name->setGeometry(96,4,textWidth,34);const int bioWidth=qMin(textWidth,m_tagline->fontMetrics().horizontalAdvance(QString(25,QChar(0x6b4c))));m_tagline->setGeometry(96,48,bioWidth,m_tagline->fontMetrics().height()*2);
}
void ProfileHeader::paintEvent(QPaintEvent *){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const qreal t=m_progress;const int editWidth=qMax(92,edit->sizeHint().width());const qreal compactX=width()-editWidth-18-220;
    const QStringList captions{"资料库歌曲","有效播放次数","累计聆听时长"};const QList<QLabel *> values{songs,plays,time};
    for(int i=0;i<3;++i){const qreal cardWidth=(width()-40)/3.;const QRectF large(i*(cardWidth+20),96,cardWidth,96),small(compactX,5+i*24,220,22);
        const QRectF box(large.topLeft()+(small.topLeft()-large.topLeft())*t,large.size()+(small.size()-large.size())*t);
        QColor fill=qApp->palette().color(QPalette::Button);fill.setAlphaF(1-t);p.setPen(Qt::NoPen);p.setBrush(fill);p.drawRoundedRect(box,14*(1-t),14*(1-t));
        QFont valueFont=font();valueFont.setPixelSize(qRound(28-16*t));valueFont.setWeight(t<.5?QFont::Bold:QFont::Normal);p.setFont(valueFont);p.setPen(palette().color(QPalette::Text));
        const qreal valueX=box.x()+22*(1-t)+108*t,valueY=box.y()+45*(1-t)+16*t;
        p.drawText(QPointF(valueX,valueY),QFontMetrics(valueFont).elidedText(values[i]->text(),Qt::ElideRight,qRound(box.right()-valueX-10*(1-t))));
        QFont captionFont=font();captionFont.setPixelSize(12);p.setFont(captionFont);p.setPen(palette().color(QPalette::PlaceholderText));p.drawText(QPointF(box.x()+22*(1-t),box.y()+77*(1-t)+16*t),captions[i]+(t>.5?QString("："):QString()));
    }
}
}
