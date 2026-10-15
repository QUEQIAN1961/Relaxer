#include "widgets.h"
#include <QScrollBar>
#include <QTextLayout>
#include <QResizeEvent>
#include <QtMath>
#include "library.h"
#include "v14widgets.h"
#include "appearance.h"
#include <QShowEvent>
#include <QSignalBlocker>
#include <QResizeEvent>
#include <QApplication>
#include <QAbstractItemView>
#include <QLayout>
#include <QScreen>
#include <QProxyStyle>
#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QImageReader>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QStyleOptionSlider>
#include <QToolTip>
#include <QWheelEvent>
namespace relaxer {
void applyPopupTheme(const QPalette &palette) {
    // Tooltips are top-level widgets, so a window's stylesheet does not reliably
    // theme their native surface. Keep their opaque colors at application scope.
    QPalette tooltipPalette = palette;
    tooltipPalette.setColor(QPalette::ToolTipBase, palette.color(QPalette::Button));
    tooltipPalette.setColor(QPalette::ToolTipText, palette.color(QPalette::Text));
    QToolTip::setPalette(tooltipPalette);
    const QString tooltipStyle =
        QString("QToolTip{background:%1;color:%2;border:1px solid %3;"
                "border-radius:6px;padding:5px 8px;}")
            .arg(tooltipPalette.color(QPalette::ToolTipBase).name(),
                 tooltipPalette.color(QPalette::ToolTipText).name(),
                 palette.color(QPalette::Mid).name());
    QString applicationStyle = qApp->styleSheet();
    const QString previous = qApp->property("relaxerTooltipStyle").toString();
    if (!previous.isEmpty()) applicationStyle.remove(previous);
    qApp->setProperty("relaxerTooltipStyle", tooltipStyle);
    qApp->setStyleSheet(applicationStyle.trimmed() + "\n" + tooltipStyle);
}
CircleButton::CircleButton(const QString &name,const QString &tip,QWidget *parent):QAbstractButton(parent){setObjectName("playButton");setFixedSize(42,42);setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);setToolTip(tip);setAccessibleName(tip);setCursor(Qt::PointingHandCursor);setFocusPolicy(Qt::StrongFocus);setSymbol(name,Qt::white);}
void CircleButton::setSymbol(const QString &name,const QColor &color){setProperty("symbolName",name);m_icon=symbol(name,color);update();}
void CircleButton::setAccent(const QColor &color){m_accent=color;update();}
void CircleButton::paintEvent(QPaintEvent *){QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const qreal d=qMin(width(),height())-2.;const QRectF circle((width()-d)/2.,(height()-d)/2.,d,d);p.setPen(Qt::NoPen);p.setBrush(isDown()?m_accent.darker(120):underMouse()?m_accent.darker(110):m_accent);p.drawEllipse(circle);m_icon.paint(&p,QRect((width()-22)/2,(height()-22)/2,22,22),Qt::AlignCenter,isEnabled()?QIcon::Normal:QIcon::Disabled);if(hasFocus()){p.setBrush(Qt::NoBrush);p.setPen(QPen(palette().color(QPalette::WindowText),1,Qt::DotLine));p.drawEllipse(circle.adjusted(2,2,-2,-2));}}
bool CircleButton::hitButton(const QPoint &point) const{return QLineF(QPointF(point),QPointF(width()/2.,height()/2.)).length()<=qMin(width(),height())/2.;}
namespace {
class RoundedRowStyle:public QProxyStyle {
public:
    RoundedRowStyle():QProxyStyle("Fusion"){}
    void drawPrimitive(PrimitiveElement element,const QStyleOption *option,QPainter *painter,const QWidget *widget=nullptr)const override{
        if(element==PE_PanelItemViewRow)return;
        QProxyStyle::drawPrimitive(element,option,painter,widget);
    }
};
class ComboPopupStyle : public QProxyStyle {
public:
    ComboPopupStyle():QProxyStyle(qApp->style()->name()){}
    int styleHint(StyleHint hint,const QStyleOption *option=nullptr,const QWidget *widget=nullptr,QStyleHintReturn *data=nullptr)const override{if(hint==SH_ComboBox_Popup)return 0;return QProxyStyle::styleHint(hint,option,widget,data);}
};
}
RoundedComboBox::RoundedComboBox(QWidget *parent):QComboBox(parent){
    // Popup children receive focus events during destruction; keep their base
    // style alive for the application's widget lifetime.
    static QPointer<ComboPopupStyle> popupStyle;
    if(!popupStyle){popupStyle=new ComboPopupStyle;popupStyle->setParent(qApp);}
    setStyle(popupStyle);
}
void RoundedComboBox::showPopup(){auto *list=view();auto *popup=list->window();if(popup!=window()){m_popup=popup;popup->setWindowFlag(Qt::FramelessWindowHint);popup->setAttribute(Qt::WA_TranslucentBackground);popup->setAttribute(Qt::WA_NoSystemBackground);popup->installEventFilter(this);popup->setObjectName("roundedComboPopup");popup->setStyleSheet("QWidget#roundedComboPopup{background:transparent;border:none;}QAbstractItemView{border:none;background:transparent;padding:2px;outline:none;}QAbstractItemView::item{padding:8px 10px;border-radius:6px;}QAbstractItemView::item:selected{background:"+palette().color(QPalette::Highlight).name()+";color:"+palette().color(QPalette::HighlightedText).name()+";}");if(popup->layout())popup->layout()->setContentsMargins(6,6,6,6);list->setFrameShape(QFrame::NoFrame);list->setAutoFillBackground(false);list->viewport()->setAutoFillBackground(false);}QComboBox::showPopup();if(m_popup){int rows=0;for(int i=0;i<qMin(count(),maxVisibleItems());++i)rows+=qMax(24,list->sizeHintForRow(i));const auto available=m_popup->screen()->availableGeometry();m_popup->setFixedSize(qMin(available.width(),qMax(width(),m_popup->width())),qMin(available.height(),rows+16));auto pos=mapToGlobal(QPoint(0,height()+4));if(pos.y()+m_popup->height()>available.bottom())pos.setY(mapToGlobal(QPoint()).y()-m_popup->height()-4);pos.setX(qBound(available.left(),pos.x(),available.right()-m_popup->width()+1));pos.setY(qBound(available.top(),pos.y(),available.bottom()-m_popup->height()+1));m_popup->move(pos);roundedMask(m_popup,10);}}
bool RoundedComboBox::eventFilter(QObject *object,QEvent *event){if(object==m_popup&&event->type()==QEvent::Paint){QPainter p(m_popup);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(qApp->palette().color(QPalette::Mid),1));p.setBrush(qApp->palette().color(QPalette::Button));p.drawRoundedRect(QRectF(m_popup->rect()).adjusted(.5,.5,-.5,-.5),10,10);return true;}return QComboBox::eventFilter(object,event);}
RoundedMenu::RoundedMenu(QWidget *parent):QMenu(parent){setWindowFlag(Qt::NoDropShadowWindowHint);setProperty("roundedPopup",true);setStyleSheet("QMenu[roundedPopup=true]{background:transparent;border:none;}");setAttribute(Qt::WA_TranslucentBackground);setAttribute(Qt::WA_NoSystemBackground);setWindowFlag(Qt::FramelessWindowHint);}
RoundedMenu *RoundedMenu::addMenu(const QString &title) {
    auto *menu = new RoundedMenu(this);
    menu->setTitle(title);
    QMenu::addMenu(menu);
    return menu;
}
RoundedMenu *RoundedMenu::addMenu(const QIcon &icon, const QString &title) {
    auto *menu = addMenu(title);
    menu->setIcon(icon);
    return menu;
}
void RoundedComboBox::resizeEvent(QResizeEvent *e){QComboBox::resizeEvent(e);roundedMask(this,8);}
void RoundedMenu::showEvent(QShowEvent *e){QMenu::showEvent(e);roundedMask(this,10);}
void RoundedMenu::resizeEvent(QResizeEvent *e){QMenu::resizeEvent(e);roundedMask(this,10);}
void RoundedMenu::paintEvent(QPaintEvent *event){QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(qApp->palette().color(QPalette::Mid),1));p.setBrush(qApp->palette().color(QPalette::Button));p.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),10,10);p.end();QMenu::paintEvent(event);}

QString volumeSymbol(int percent){
    percent=qBound(0,percent,100);
    return percent==0?"volumeMuted":percent<=33?"volumeLow":percent<=66?"volumeMedium":"volumeHigh";
}
QIcon symbol(const QString &name, const QColor &c) {
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(2, 2);
    p.setPen(QPen(c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (name == "play") {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(QPolygonF{{11, 7}, {11, 25}, {25, 16}});
    } else if (name == "pause") {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(QRectF(9, 7, 5, 18), 1, 1);
        p.drawRoundedRect(QRectF(19, 7, 5, 18), 1, 1);
    } else if (name == "next" || name == "previous") {
        if (name == "previous") {
            p.translate(32, 0);
            p.scale(-1, 1);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(QPolygonF{{8, 8}, {8, 24}, {21, 16}});
        p.drawRoundedRect(QRectF(22, 8, 3, 16), 1, 1);
    } else if (name == "music") {
        p.drawLine(13, 23, 13, 8);
        p.drawLine(13, 8, 25, 5);
        p.drawLine(25, 5, 25, 20);
        p.setBrush(c);
        p.drawEllipse(QRectF(6, 21, 7, 5));
        p.drawEllipse(QRectF(18, 18, 7, 5));
    } else if (name == "playlist") {
        for (int y : {9, 16, 23})
            p.drawLine(6, y, 19, y);
        p.drawLine(25, 18, 25, 26);
        p.drawLine(21, 22, 29, 22);
    } else if (name == "heart" || name == "liked") {
        QPainterPath path;
        path.moveTo(16, 27);
        path.cubicTo(4, 19, 2, 12, 7, 7);
        path.cubicTo(11, 3, 15, 6, 16, 9);
        path.cubicTo(18, 4, 23, 4, 26, 8);
        path.cubicTo(31, 14, 24, 21, 16, 27);
        if (name == "liked")
            p.setBrush(c);
        p.drawPath(path);
    } else if (name == "person") {
        p.drawEllipse(QRectF(11, 5, 10, 10));
        p.drawArc(QRectF(5, 17, 22, 18), 0, 180 * 16);
    } else if (name == "settings") {
        p.drawEllipse(QRectF(9, 9, 14, 14));
        p.drawEllipse(QRectF(13, 13, 6, 6));
        for (int i = 0; i < 8; ++i) {
            p.save();
            p.translate(16, 16);
            p.rotate(i * 45);
            p.drawLine(0, -10, 0, -13);
            p.restore();
        }
    } else if (name == "search") {
        p.drawEllipse(QRectF(6, 5, 15, 15));
        p.drawLine(20, 20, 27, 27);
    } else if (name == "plus") {
        p.drawLine(16, 7, 16, 25);
        p.drawLine(7, 16, 25, 16);
    } else if (name == "repeat" || name == "single") {
        p.drawLine(7, 10, 25, 10);
        p.drawLine(25, 10, 21, 6);
        p.drawLine(25, 10, 21, 14);
        p.drawLine(25, 22, 7, 22);
        p.drawLine(7, 22, 11, 18);
        p.drawLine(7, 22, 11, 26);
        p.drawLine(6, 10, 6, 17);
        p.drawLine(26, 15, 26, 22);
        if (name == "single") {
            p.setFont(QFont("Segoe UI", 9, QFont::Bold));
            p.drawText(QRectF(10, 10, 12, 12), Qt::AlignCenter, "1");
        }
    } else if (name == "shuffle") {
        p.drawLine(6, 8, 10, 8);
        p.drawLine(10, 8, 22, 24);
        p.drawLine(22, 24, 28, 24);
        p.drawLine(24, 20, 28, 24);
        p.drawLine(24, 28, 28, 24);
        p.drawLine(6, 24, 10, 24);
        p.drawLine(10, 24, 22, 8);
        p.drawLine(22, 8, 28, 8);
        p.drawLine(24, 4, 28, 8);
        p.drawLine(24, 12, 28, 8);
    } else if (name == "volume" || name == "volumeMuted" || name == "volumeLow" ||
               name == "volumeMedium" || name == "volumeHigh") {
        p.drawPolygon(QPolygonF{{4, 12}, {9, 12}, {14, 7}, {14, 25}, {9, 20}, {4, 20}});
        if(name=="volumeMuted"){
            p.drawLine(19,12,26,20);p.drawLine(19,20,26,12);
        }else{
            const int waves=name=="volumeLow"?1:name=="volumeMedium"?2:3;
            for(int i=0;i<waves;++i){
                const qreal x=18+4*i,reach=3+2*i,half=3+3*i;
                QPainterPath wave;wave.moveTo(x,16-half);wave.quadTo(x+reach,16,x,16+half);p.drawPath(wave);
            }
        }
    } else if (name == "expand") {
        p.drawLine(7, 13, 7, 7);
        p.drawLine(7, 7, 13, 7);
        p.drawLine(19, 7, 25, 7);
        p.drawLine(25, 7, 25, 13);
        p.drawLine(25, 19, 25, 25);
        p.drawLine(25, 25, 19, 25);
        p.drawLine(13, 25, 7, 25);
        p.drawLine(7, 25, 7, 19);
    } else if (name == "contract") {
        p.drawLine(7,13,13,13);p.drawLine(13,13,13,7);
        p.drawLine(19,7,19,13);p.drawLine(19,13,25,13);
        p.drawLine(25,19,19,19);p.drawLine(19,19,19,25);
        p.drawLine(13,25,13,19);p.drawLine(13,19,7,19);
    } else if (name == "collapse") {
        p.drawLine(7, 11, 16, 20);
        p.drawLine(16, 20, 25, 11);
    } else if (name == "back") {
        p.drawLine(20, 7, 11, 16);
        p.drawLine(11, 16, 20, 25);
    } else if (name == "folder") {
        p.drawPath([] {
            QPainterPath f;
            f.moveTo(5, 9);
            f.lineTo(13, 9);
            f.lineTo(16, 12);
            f.lineTo(27, 12);
            f.lineTo(27, 26);
            f.lineTo(5, 26);
            f.closeSubpath();
            return f;
        }());
    } else if (name == "up" || name == "down") {
        if (name == "down") {
            p.translate(0, 32);
            p.scale(1, -1);
        }
        p.drawLine(16, 25, 16, 7);
        p.drawLine(9, 14, 16, 7);
        p.drawLine(16, 7, 23, 14);
    } else if (name == "clock") {
        p.drawEllipse(QRectF(5, 5, 22, 22));
        p.drawLine(16, 10, 16, 16);
        p.drawLine(16, 16, 21, 19);
    } else {
        p.setFont(QFont("Segoe UI", 18, QFont::Bold));
        p.drawText(QRectF(0, 0, 32, 32), Qt::AlignCenter, "…");
    }
    return QIcon(pm);
}
QPixmap coverPixmap(const QString &path, const QString &seed, int size) {
    QString key = path + "|" + seed + "|" + QString::number(size) + "|" +
                  QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());
    QPixmap out;
    if (QPixmapCache::find(key, &out))
        return out;
    if (!path.isEmpty()) {
        QImageReader r(path);
        r.setAutoTransform(true);
        auto s = r.size();
        if (s.isValid() && qint64(s.width()) * s.height() <= 120000000) {
            r.setScaledSize(s.scaled(size, size, Qt::KeepAspectRatioByExpanding));
            auto img = r.read();
            if (!img.isNull()) {
                auto scaled = QPixmap::fromImage(img).scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                                             Qt::SmoothTransformation);
                out = scaled.copy((scaled.width() - size) / 2, (scaled.height() - size) / 2, size, size);
            }
        }
    }
    if (out.isNull()) {
        auto h = QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha256);
        int hue = (uchar(h[0]) * 360) / 256;
        out = QPixmap(size, size);
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient g(0, 0, size, size);
        g.setColorAt(0, QColor::fromHsl(hue, 90, 170));
        g.setColorAt(1, QColor::fromHsl((hue + 38) % 360, 105, 85));
        p.fillRect(out.rect(), g);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 22));
        p.drawEllipse(QRectF(size * .15, -size * .25, size * 1.3, size * 1.3));
        p.setBrush(QColor(255, 255, 255, 25));
        p.drawEllipse(QRectF(-size * .3, size * .3, size, size));
        p.setPen(QPen(QColor(255, 255, 255, 180), size * .025, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(size * .46, size * .62), QPointF(size * .46, size * .30));
        p.drawLine(QPointF(size * .46, size * .30), QPointF(size * .68, size * .25));
        p.drawLine(QPointF(size * .68, size * .25), QPointF(size * .68, size * .55));
        p.setBrush(QColor(255, 255, 255, 180));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(size * .32, size * .59, size * .14, size * .10));
        p.drawEllipse(QRectF(size * .54, size * .52, size * .14, size * .10));
        p.setPen(QColor(255, 255, 255, 165));
        QFont f("Segoe UI");
        f.setPixelSize(qMax(10, size / 28));
        f.setLetterSpacing(QFont::AbsoluteSpacing, size / 100.);
        p.setFont(f);
        p.drawText(QRectF(size * .1, size * .86, size * .8, size * .06), Qt::AlignCenter, "RELAXER");
    }
    QPixmapCache::insert(key, out);
    return out;
}
Cover::Cover(QWidget *p) : QWidget(p) {
    setMinimumSize(32, 32);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::PointingHandCursor);
}
void Cover::setCover(const QString &p, const QString &s) {
    m_path = p;
    m_seed = s;
    update();
}
void Cover::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int s = qMin(width(), height());
    QRectF rect((width() - s) / 2., (height() - s) / 2., s, s);
    QPainterPath clip;
    clip.addRoundedRect(rect, m_radius, m_radius);
    p.setClipPath(clip);
    p.drawPixmap(rect.toRect(), coverPixmap(m_path, m_seed, qMin(900, qMax(100, s))));
}
void Cover::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton)
        emit clicked();
}
void Backdrop::setArtwork(const QString &p, const QString &s, bool d) {
    if(m_path!=p||m_seed!=s||m_image.isNull()){
        m_path=p;m_seed=s;m_image=backgroundImage(p,640);
        if(m_image.isNull())m_image=coverPixmap({},s,64).toImage();
        m_image=gaussianBlur(m_image,m_blur);m_scaled={};
    }
    m_dark = d;
    update();
}
void Backdrop::setCustomBackground(const QColor &color,const QString &path,double blur){
    m_customColor=color;
    blur=qBound(0.,blur,50.);
    if(path!=m_customPath||blur!=m_customBlur){
        m_customPath=path;m_customBlur=blur;
        m_customImage=gaussianBlur(backgroundImage(path,1600),blur);m_customScaled={};
    }
    update();
}
void Backdrop::paintEvent(QPaintEvent *) {
    if(m_custom){
        QPainter p(this);p.fillRect(rect(),m_customColor);
        if(m_customImage.isNull())return;
        const auto canvas=m_customImage.size().scaled(size(),Qt::KeepAspectRatioByExpanding);
        if(m_customScaled.size()!=canvas)m_customScaled=m_customImage.scaled(canvas,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        p.drawImage(QPoint((width()-m_customScaled.width())/2,(height()-m_customScaled.height())/2),m_customScaled);
        return;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (m_scaled.isNull() || m_scaled.size() != m_image.size().scaled(size(), Qt::KeepAspectRatioByExpanding))
        m_scaled = m_image.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    p.drawImage(QPoint((width() - m_scaled.width()) / 2, (height() - m_scaled.height()) / 2), m_scaled);
    p.fillRect(rect(), m_dark ? QColor(15, 15, 19, 220) : QColor(252, 249, 247, 226));
}
IconButton::IconButton(const QString &n, const QString &t, QWidget *p) : QPushButton(p) {
    setToolTip(t);
    setAccessibleName(t);
    setObjectName("iconButton");
    setFixedSize(36, 36);
    setIconSize(QSize(22, 22));
    setSymbol(n);
}
void IconButton::setSymbol(const QString &n, const QColor &c) {
    setProperty("symbolName", n);
    setIcon(symbol(n, c));
}
void IconButton::paintEvent(QPaintEvent *e) {
    if (objectName() != "playButton") {
        QPushButton::paintEvent(e);
        return;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    QColor accent = property("accentColor").value<QColor>();
    if (!accent.isValid()) accent = QColor("#e5455f");
    p.setBrush(underMouse() ? accent.darker(110) : accent);
    p.drawEllipse(rect().adjusted(1, 1, -1, -1));
    // QIcon paints in logical coordinates, including fractional Windows display scaling.
    const QRect target(QPoint((width() - iconSize().width()) / 2, (height() - iconSize().height()) / 2),
                       iconSize());
    icon().paint(&p, target, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);
}
void Slider::wheelEvent(QWheelEvent *e) { e->ignore(); }
void Slider::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        QStyleOptionSlider option;
        initStyleOption(&option);
        const auto handle =
            style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
        const bool onHandle = handle.contains(e->position().toPoint());
        m_grabOffset = onHandle ? (orientation() == Qt::Horizontal ? e->position().x() - handle.center().x()
                                                                   : e->position().y() - handle.center().y())
                                : 0;
        m_dragging = true;
        m_pointerPosition = e->position();
        setSliderDown(true);
        if (!onHandle)
            setSliderPosition(valueAt(e->position()));
        e->accept();
    } else
        QSlider::mousePressEvent(e);
}
int Slider::valueAt(const QPointF &position) const {
    QStyleOptionSlider option;
    initStyleOption(&option);
    auto centerAt = [&](int value) {
        option.sliderPosition = value;
        const auto handle =
            style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
        return orientation() == Qt::Horizontal ? handle.center().x() : handle.center().y();
    };
    const int start = centerAt(minimum()), end = centerAt(maximum());
    const int span = qAbs(end - start);
    if (!span)
        return minimum();
    const qreal coordinate = orientation() == Qt::Horizontal ? position.x() : position.y();
    const int pixel = qBound(0, qRound(coordinate - m_grabOffset - qMin(start, end)), span);
    return QStyle::sliderValueFromPosition(minimum(), maximum(), pixel, span, start > end);
}
void Slider::mouseMoveEvent(QMouseEvent *e) {
    if (m_dragging) {
        if (e->position() != m_pointerPosition)
            setSliderPosition(valueAt(e->position()));
        m_pointerPosition = e->position();
        e->accept();
    } else
        QSlider::mouseMoveEvent(e);
}
void Slider::mouseReleaseEvent(QMouseEvent *e) {
    if (m_dragging && e->button() == Qt::LeftButton) {
        if (e->position() != m_pointerPosition)
            setSliderPosition(valueAt(e->position()));
        setValue(sliderPosition());
        m_dragging = false;
        setSliderDown(false);
        e->accept();
    } else
        QSlider::mouseReleaseEvent(e);
}
void PercentSpin::wheelEvent(QWheelEvent *e) { e->ignore(); }
void BlurPercentSpin::wheelEvent(QWheelEvent *e){e->ignore();}
void BlurPercentSpin::resizeEvent(QResizeEvent *e){QDoubleSpinBox::resizeEvent(e);roundedMask(this,8);}
void PercentSpin::resizeEvent(QResizeEvent *e){QSpinBox::resizeEvent(e);roundedMask(this,8);}
void SongDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const {
    auto o=opt;initStyleOption(&o,idx);p->save();
    const bool selected=o.state & QStyle::State_Selected;
    const QColor foreground=selected?o.palette.color(QPalette::HighlightedText):o.palette.color(QPalette::Text);
    const QColor muted=selected?foreground:o.palette.color(QPalette::PlaceholderText);
    const auto *table=qobject_cast<const SongTable *>(parent());
    if(selected||(o.state & QStyle::State_MouseOver)){
        const QRectF row(0,o.rect.y()+1,table?table->viewport()->width()-1:o.rect.right(),o.rect.height()-2);
        p->save();p->setClipRect(o.rect,Qt::IntersectClip);p->setRenderHint(QPainter::Antialiasing);p->setPen(Qt::NoPen);p->setBrush(selected?o.palette.color(QPalette::Highlight):QColor(128,128,128,12));p->drawRoundedRect(row,9,9);p->restore();
    }
    if(idx.column()==SongTable::NumberColumn&&table&&table->multiSelect()){
        const QRectF box(o.rect.center().x()-8,o.rect.center().y()-8,16,16);p->setRenderHint(QPainter::Antialiasing);p->setPen(QPen(selected?foreground:muted,1.3));p->setBrush(Qt::NoBrush);p->drawRoundedRect(box,4,4);
        if(selected){QPainterPath tick;tick.moveTo(box.left()+4,box.top()+8);tick.lineTo(box.left()+7,box.top()+11);tick.lineTo(box.left()+12,box.top()+5);p->drawPath(tick);}
    }else if(idx.column()==SongTable::TitleColumn){
        const int y=o.rect.center().y();QRect art(o.rect.x()+10,y-23,46,46);
        QPainterPath clip;clip.addRoundedRect(art,8,8);p->setClipPath(clip);
        p->drawPixmap(art,coverPixmap(idx.data(Qt::UserRole+1).toString(),idx.data().toString(),100));
        p->setClipping(false);
        const QRect text(art.right()+13,o.rect.y(),qMax(0,o.rect.width()-art.width()-34),o.rect.height());
        QFont f=o.font;f.setWeight(QFont::DemiBold);p->setFont(f);
        const bool playing=idx.data(Qt::UserRole+2).toBool();
        p->setPen(playing?(selected?QColor(Qt::white):o.palette.color(QPalette::Highlight)):foreground);
        p->drawText(text,Qt::AlignVCenter|Qt::AlignLeft,p->fontMetrics().elidedText(idx.data().toString(),Qt::ElideRight,text.width()));
    }else if(idx.column()==SongTable::FavoriteColumn){
        const bool liked=idx.data(Qt::UserRole+3).toBool();
        const QColor tint=selected?foreground:(liked?o.palette.color(QPalette::Highlight):muted);
        symbol(liked?"liked":"heart",tint).paint(p,QRect(o.rect.center()-QPoint(9,9),QSize(18,18)));
    }else{
        p->setFont(o.font);p->setPen(muted);const QRect r=o.rect.adjusted(8,0,-8,0);
        p->drawText(r,Qt::AlignVCenter|(idx.column()==SongTable::NumberColumn||idx.column()>=SongTable::DurationColumn?Qt::AlignCenter:Qt::AlignLeft),
                    p->fontMetrics().elidedText(idx.data().toString(),Qt::ElideRight,r.width()));
    }
    p->restore();
}
SongTable::SongTable(QWidget *p):QTableWidget(p){
    static QPointer<RoundedRowStyle> rowStyle;if(!rowStyle){rowStyle=new RoundedRowStyle;rowStyle->setParent(qApp);}setStyle(rowStyle);
    setShowGrid(false);setFrameShape(QFrame::NoFrame);setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);setMouseTracking(true);setItemDelegate(new SongDelegate(this));
    verticalHeader()->hide();verticalHeader()->setDefaultSectionSize(66);
    horizontalHeader()->setHighlightSections(false);horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    horizontalHeader()->setFixedHeight(38);setContextMenuPolicy(Qt::CustomContextMenu);
    setColumnsMode(Statistics);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_columnSave.setSingleShot(true);m_columnSave.setInterval(150);
    connect(&m_columnSave,&QTimer::timeout,this,&SongTable::saveColumnWidths);
    connect(verticalScrollBar(),&QScrollBar::rangeChanged,this,[this]{QTimer::singleShot(0,this,&SongTable::fitTitleColumn);});
    connect(horizontalHeader(),&QHeaderView::sectionResized,this,[this](int section,int,int width){if(m_columnInternal||section<TitleColumn)return;m_customColumns.insert(section);m_requestedWidths[section]=width;const int minimum=section==TitleColumn?96:40;if(width<minimum){m_columnInternal=true;setColumnWidth(section,minimum);m_columnInternal=false;}fitTitleColumn();m_columnSave.start();});
    connect(horizontalHeader(),&QHeaderView::sectionClicked,this,[this](int section){if(m_multiSelect&&section==NumberColumn){if(selectedIds().size()==rowCount())clearSelection();else selectAll();}});
}
SongTable::~SongTable(){if(m_columnSave.isActive())saveColumnWidths();}
void SongTable::setColumnsMode(ColumnsMode mode){
    if(mode==m_columnsMode&&columnCount())return;
    if(m_columnSave.isActive()){m_columnSave.stop();saveColumnWidths();}
    m_columnInternal=true;m_customColumns.clear();m_requestedWidths.clear();m_columnsMode=mode;setColumnCount(mode==LocalMetadata?8:7);
    QStringList headers={"","","歌曲","歌手","专辑","时长",mode==LocalMetadata?"文件大小":"播放次数"};
    if(mode==LocalMetadata)headers<<"导入时间";
    setHorizontalHeaderLabels(headers);
    const int widths[]={34,32,260,120,120,64,88,104};
    for(int c=0;c<columnCount();++c){m_requestedWidths[c]=widths[c];setColumnWidth(c,widths[c]);horizontalHeader()->setSectionResizeMode(c,c>=TitleColumn?QHeaderView::Interactive:QHeaderView::Fixed);}
    horizontalHeader()->setMinimumSectionSize(24);
    if(m_multiSelect){horizontalHeaderItem(NumberColumn)->setText("选择");setColumnWidth(NumberColumn,48);}
    m_columnInternal=false;restoreColumnWidths();fitTitleColumn();
}
void SongTable::setMultiSelect(bool enabled){m_multiSelect=enabled;setSelectionMode(enabled?QAbstractItemView::MultiSelection:QAbstractItemView::ExtendedSelection);horizontalHeaderItem(NumberColumn)->setText(enabled?QString("选择"):QString());setColumnWidth(NumberColumn,enabled?48:34);viewport()->update();}
QString SongTable::widthKey(int column) const {
    static const QStringList keys={"number","favorite","title","artist","album","duration","plays","importDate"};
    return column==DetailsColumn&&m_columnsMode==LocalMetadata?QString("fileSize"):keys.value(column);
}
void SongTable::setColumnLibrary(Library *library) {
    m_columnLibrary=library;restoreColumnWidths();
}
void SongTable::restoreColumnWidths() {
    if(!m_columnLibrary)return;m_columnInternal=true;m_customColumns.clear();
    const auto saved=m_columnLibrary->setting("songColumnWidths").toObject();
    for(int c=TitleColumn;c<columnCount();++c)if(saved.contains(widthKey(c))){
        const int width=saved[widthKey(c)].toInt();
        if(width>=40&&width<=2400){m_customColumns.insert(c);m_requestedWidths[c]=qMax(c==TitleColumn?96:40,width);setColumnWidth(c,m_requestedWidths[c]);}
    }
    m_columnInternal=false;fitTitleColumn();
}
void SongTable::saveColumnWidths() {
    if(!m_columnLibrary)return;auto saved=m_columnLibrary->setting("songColumnWidths").toObject();
    for(int c:m_customColumns)if(c>=TitleColumn&&c<columnCount())saved[widthKey(c)]=m_requestedWidths.value(c,columnWidth(c));
    m_columnLibrary->setSetting("songColumnWidths",saved);
}
void SongTable::fitTitleColumn() {
    if(m_columnInternal||!isVisible()||columnCount()<=TitleColumn)return;const int available=viewport()->width(),last=columnCount()-1;
    int minimum=0;for(int c=0;c<columnCount();++c)minimum+=c<TitleColumn?columnWidth(c):c==TitleColumn?96:40;
    if(available<minimum)return;m_columnInternal=true;
    for(int c=TitleColumn;c<last;++c)setColumnWidth(c,qMax(c==TitleColumn?96:40,m_requestedWidths.value(c,columnWidth(c))));
    int other=0;for(int c=0;c<last;++c)if(c!=TitleColumn)other+=columnWidth(c);
    if(!m_customColumns.contains(TitleColumn))setColumnWidth(TitleColumn,qMax(96,available-other-qMax(40,m_requestedWidths.value(last,last==ImportDateColumn?104:88))));
    int used=0;for(int c=0;c<last;++c)used+=columnWidth(c);
    int shortage=qMax(0,used+40-available);
    for(int c=TitleColumn;c<last&&shortage>0;++c){const int low=c==TitleColumn?96:40,reduce=qMin(shortage,qMax(0,columnWidth(c)-low));setColumnWidth(c,columnWidth(c)-reduce);shortage-=reduce;used-=reduce;}
    setColumnWidth(last,qMax(40,available-used));setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);m_columnInternal=false;
}
void SongTable::resizeEvent(QResizeEvent *e) {
    QTableWidget::resizeEvent(e);fitTitleColumn();
}
void SongTable::showEvent(QShowEvent *e) {
    QTableWidget::showEvent(e);fitTitleColumn();
}
bool SongDelegate::helpEvent(QHelpEvent *event,QAbstractItemView *view,const QStyleOptionViewItem &option,const QModelIndex &idx) {
    if(event->type()!=QEvent::ToolTip)return QStyledItemDelegate::helpEvent(event,view,option,idx);
    QString text;
    if(idx.column()==SongTable::FavoriteColumn)text=idx.data(Qt::ToolTipRole).toString();
    else if(idx.column()>=SongTable::TitleColumn&&idx.column()<=SongTable::AlbumColumn){
        QFont font=option.font;if(idx.column()==SongTable::TitleColumn)font.setWeight(QFont::DemiBold);
        const int available=option.rect.width()-(idx.column()==SongTable::TitleColumn?80:16);
        if(QFontMetrics(font).horizontalAdvance(idx.data().toString())>available)text=idx.data().toString();
    }
    if(text.isEmpty())QToolTip::hideText();else QToolTip::showText(event->globalPos(),text,view);
    return true;
}
QString SongTable::fileSizeText(qint64 bytes){return bytes<0?QString("—"):QString::number(double(bytes)/1000000.0,'f',2)+"MB";}
QString SongTable::importDateText(const QString &timestamp){
    auto utc=QDateTime::fromString(timestamp,"yyyy-MM-dd HH:mm:ss");
    if(utc.isValid()){utc.setTimeSpec(Qt::UTC);return utc.toLocalTime().date().toString("yyyy-MM-dd");}
    const auto iso=QDateTime::fromString(timestamp,Qt::ISODate);
    if(iso.isValid())return iso.toLocalTime().date().toString("yyyy-MM-dd");
    const auto date=QDate::fromString(timestamp,Qt::ISODate);
    return date.isValid()?date.toString("yyyy-MM-dd"):QString("—");
}
QVector<qint64> SongTable::ids() const {
    QVector<qint64> out;for(int r=0;r<rowCount();++r)if(auto *it=item(r,TitleColumn))out<<it->data(Qt::UserRole).toLongLong();return out;
}
QVector<qint64> SongTable::selectedIds() const {
    QVector<qint64> out;auto rows=selectionModel()->selectedRows();
    std::sort(rows.begin(),rows.end(),[](auto a,auto b){return a.row()<b.row();});
    for(auto idx:rows)if(auto *it=item(idx.row(),TitleColumn))out<<it->data(Qt::UserRole).toLongLong();
    return out;
}
void SongTable::setSongs(const QVector<Track> &songs,const QString &root,qint64 current){
    const auto selected=selectedIds();QSignalBlocker selectionBlock(selectionModel());setUpdatesEnabled(false);clearSelection();setRowCount(songs.size());
    m_totalFileSize=0;
    for(int r=0;r<songs.size();++r){const auto &t=songs[r];
        QStringList text={QString::number(r+1),t.favorite?"♥":"♡",t.title,t.artist,t.album,durationText(t.duration),QString::number(t.plays)};
        if(m_columnsMode==LocalMetadata){
            const QString path=QDir(root).filePath(t.file);
            if(!m_fileSizes.contains(path)){const QFileInfo info(path);if(info.isFile())m_fileSizes.insert(path,info.size());}
            m_totalFileSize+=qMax(qint64(0),m_fileSizes.value(path,-1));
            text[DetailsColumn]=fileSizeText(m_fileSizes.value(path,-1));text<<importDateText(t.importedAt);
        }
        for(int c=0;c<columnCount();++c){auto *it=new QTableWidgetItem(text[c]);it->setData(Qt::UserRole,t.id);
            if(c==TitleColumn){it->setData(Qt::UserRole+1,t.cover.isEmpty()?QString():QDir(root).filePath(t.cover));it->setData(Qt::UserRole+2,t.id==current);}
            if(c==FavoriteColumn){it->setData(Qt::UserRole+3,t.favorite);it->setToolTip(t.favorite?"取消收藏":"收藏歌曲");}

            setItem(r,c,it);
        }
        if(selected.contains(t.id))selectionModel()->select(model()->index(r,TitleColumn),QItemSelectionModel::Select|QItemSelectionModel::Rows);
    }
    setUpdatesEnabled(true);selectionBlock.unblock();emit itemSelectionChanged();
}
void SongTable::updateStats(const QVector<Track> &songs){
    if(m_columnsMode==LocalMetadata)return;
    QHash<qint64,qint64> plays;for(const auto &t:songs)plays[t.id]=t.plays;
    for(int r=0;r<rowCount();++r)item(r,DetailsColumn)->setText(QString::number(plays.value(item(r,TitleColumn)->data(Qt::UserRole).toLongLong())));
}
void SongTable::keyPressEvent(QKeyEvent *e) { QTableWidget::keyPressEvent(e); }
LyricView::LyricView(QWidget *p) : QWidget(p) {
    setObjectName("lyricView");
    setMinimumSize(260, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_resume.setSingleShot(true);m_resume.setInterval(3000);
    connect(&m_resume,&QTimer::timeout,this,[this]{m_manualIndex=-1;setProperty("manualLyric",-1);scrollTo(qMax(0,m_index));update();});
    m_slide.setDuration(360);m_slide.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_slide,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){m_center=v.toReal();publishScroll();update();});
}
void LyricView::setLyrics(Lyrics l) {
    m_slide.stop();m_resume.stop();m_manualIndex=-1;setProperty("manualLyric",-1);
    m_lyrics = std::move(l);
    m_index = -1;setProperty("activeLyric",-1);
    m_blocks.clear();
    bool pair=false;
    for(const auto &line:m_lyrics.lines){
        if(m_lyrics.timed&&!m_blocks.isEmpty()&&m_blocks.last().time==line.time&&m_blocks.last().translation.isEmpty())m_blocks.last().translation=line.text;
        else if(!m_lyrics.timed&&m_lyrics.bilingual&&pair&&!m_blocks.isEmpty()){m_blocks.last().translation=line.text;pair=false;}
        else{m_blocks.push_back({line.time,line.text,{},0,0});pair=!m_lyrics.timed&&m_lyrics.bilingual;}
    }
    rebuild();scrollTo(0,false);
    update();
}
void LyricView::rebuild(){
    QFont normal=font();normal.setPixelSize(26);normal.setWeight(QFont::DemiBold);
    QFont enlarged=normal;enlarged.setPointSizeF(26.*1.2*72./logicalDpiY());
    QFont translation=normal;translation.setPixelSize(qMax(1,qRound(26*.8)));
    QFont translationActive=normal;translationActive.setPixelSize(qMax(1,qRound(26*1.2*.8)));
    const qreal available=qMax(40,width()-112);
    const auto makeLayout=[&](const QString &source,const QFont &font,qreal &height){
        auto layout=std::make_shared<QTextLayout>(QString(source).replace(QChar('\n'),QChar(0x2028)),font,this);
        QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout->setTextOption(option);
        layout->beginLayout();qreal h=0;
        while(true){auto line=layout->createLine();if(!line.isValid())break;line.setLineWidth(available);line.setPosition(QPointF(0,h));h+=line.height();}
        layout->endLayout();height=qMax(qreal(36),h);return layout;
    };
    for(auto &block:m_blocks){
        block.layout=makeLayout(block.text,normal,block.primaryNormal);
        const bool show=m_showTranslation&&!block.translation.isEmpty();
        qreal translationHeight=0,activeTranslationHeight=0;
        if(hasTimeline()){
            block.activeLayout=makeLayout(block.text,enlarged,block.primaryActive);
            if(show){block.translationLayout=makeLayout(block.translation,translation,translationHeight);block.activeTranslationLayout=makeLayout(block.translation,translationActive,activeTranslationHeight);}
        }else{
            block.activeLayout=block.layout;block.primaryActive=block.primaryNormal;
            if(show){block.translationLayout=makeLayout(block.translation,translation,translationHeight);block.activeTranslationLayout=block.translationLayout;activeTranslationHeight=translationHeight;}
        }
        block.normalHeight=block.primaryNormal+(show?translationHeight+6:0);
        block.activeHeight=block.primaryActive+(show?activeTranslationHeight+6:0);
    }
    arrangeBlocks();setProperty("lyricBlockCount",int(m_blocks.size()));publishScroll();
}
void LyricView::arrangeBlocks(){
    qreal top=0;
    for(int i=0;i<m_blocks.size();++i){auto &block=m_blocks[i];block.top=top;
        block.height=hasTimeline()&&i==m_index?block.activeHeight:block.normalHeight;
        top+=block.height+30;
    }
}
void LyricView::scrollTo(int index,bool animate){
    if(m_blocks.isEmpty())return;
    index=qBound(0,index,int(m_blocks.size())-1);const auto &b=m_blocks[index];const qreal target=b.top+b.height/2;
    m_slide.stop();if(!animate||!isVisible()){m_center=target;publishScroll();update();return;}
    m_slide.setStartValue(m_center);m_slide.setEndValue(target);m_slide.start();
}
void LyricView::publishScroll(){
    if(m_blocks.size()<2||m_lyrics.instrumental){emit scrollMetricsChanged(0,10000,false);return;}
    const auto &first=m_blocks.first(),&last=m_blocks.last();const qreal low=first.top+first.height/2,span=last.top+last.height/2-low;
    emit scrollMetricsChanged(qBound(0,qRound((m_center-low)/qMax(1.,span)*10000),10000),qBound(400,qRound(height()/qMax(1.,span)*10000),10000),true);
}
void LyricView::browseFraction(qreal fraction,bool dragging){
    if(m_blocks.isEmpty()||m_lyrics.instrumental)return;m_slide.stop();m_resume.stop();
    const auto &first=m_blocks.first(),&last=m_blocks.last();const qreal low=first.top+first.height/2,high=last.top+last.height/2;
    m_center=low+qBound(0.,fraction,1.)*(high-low);int closest=0;qreal distance=1e30;
    for(int i=0;i<m_blocks.size();++i){const auto &b=m_blocks[i];const auto d=qAbs(b.top+b.height/2-m_center);if(d<distance){distance=d;closest=i;}}
    m_manualIndex=closest;setProperty("manualLyric",closest);publishScroll();update();if(hasTimeline()&&!dragging)m_resume.start();
}
void LyricView::setBrowsingDrag(bool dragging){if(dragging)m_resume.stop();else if(hasTimeline()&&m_manualIndex>=0)m_resume.start();}
void LyricView::setOffset(int ms){m_offset=qBound(-5000,ms,5000);setProperty("lyricOffsetMs",m_offset);setPosition(m_position);}
void LyricView::setShowTranslation(bool on){if(m_showTranslation==on)return;m_showTranslation=on;rebuild();update();}
void LyricView::setPosition(qint64 ms) {
    m_position=ms;int i=-1;
    if(hasTimeline())for(int n=0;n<m_blocks.size();++n){if(m_blocks[n].time<=ms-m_offset)i=n;else break;}
    if (i != m_index) {
        const int anchor=qBound(0,m_manualIndex>=0?m_manualIndex:qMax(0,m_index),qMax(0,int(m_blocks.size())-1));
        const qreal before=m_blocks.isEmpty()?0:m_blocks[anchor].top+m_blocks[anchor].height/2;
        m_index=i;arrangeBlocks();
        if(!m_blocks.isEmpty())m_center+=m_blocks[anchor].top+m_blocks[anchor].height/2-before;
        setProperty("activeLyric",m_index);
        if(m_manualIndex<0)scrollTo(qMax(0,m_index));
        else if(m_slide.state()==QAbstractAnimation::Running)scrollTo(m_manualIndex);
        update();
    }
}
void LyricView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    m_hits.clear();
    QColor active=m_color.isValid()?m_color:palette().color(QPalette::Text);
    QColor normal=active;normal.setAlpha(130);
    if(m_lyrics.instrumental){
        QFont f=font();f.setPixelSize(26);f.setWeight(QFont::DemiBold);p.setFont(f);p.setPen(active);
        p.drawText(rect().adjusted(24,24,-24,-24),Qt::AlignCenter,"纯音乐，请欣赏");return;
    }
    if (m_lyrics.lines.isEmpty()) {
        QFont f = font();
        f.setPixelSize(22);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(normal);
        p.drawText(rect().adjusted(25, 0, -25, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   "让音乐，留一点空白。\n\n暂无歌词\n可在歌曲菜单中导入本地歌词");
        return;
    }
    QFont f=font();f.setPixelSize(26);f.setWeight(QFont::DemiBold);
    for(int i=0;i<m_blocks.size();++i){
        const auto &b=m_blocks[i];const qreal top=height()/2.+b.top-m_center;
        if(top+b.height<0||top>height())continue;
        const auto layout=hasTimeline()&&i==m_index?b.activeLayout:b.layout;
        if(!layout)continue;const auto &text=*layout;
        for(int n=0;n<text.lineCount();++n){auto line=text.lineAt(n);const qreal cy=top+line.y()+line.height()/2;
            const qreal edge=qBound(0.,qMin(cy-8,height()-8-cy)/42.,1.);
            p.setOpacity(edge);p.setPen(i==m_index?active:normal);line.draw(&p,QPointF(84,top));
        }
        const auto translation=m_showTranslation?(hasTimeline()&&i==m_index?b.activeTranslationLayout:b.translationLayout):std::shared_ptr<QTextLayout>();
        if(translation){const qreal shift=(hasTimeline()&&i==m_index?b.primaryActive:b.primaryNormal)+6;const auto &extra=*translation;
            for(int n=0;n<extra.lineCount();++n){auto line=extra.lineAt(n);const qreal cy=top+shift+line.y()+line.height()/2;
                const qreal edge=qBound(0.,qMin(cy-8,height()-8-cy)/42.,1.);
                p.setOpacity(edge*.86);p.setPen(i==m_index?active:normal);line.draw(&p,QPointF(84,top+shift));}}
        p.setOpacity(1);
        if(hasTimeline())m_hits.push_back({QRect(78,qRound(top),width()-90,qCeil(b.height)),qMax<qint64>(0,b.time+m_offset)});
        if(hasTimeline()&&i==m_manualIndex){QFont small=font();small.setPixelSize(11);p.setFont(small);p.setPen(active);
            p.drawText(QRect(0,qRound(top+b.height/2-10),76,20),Qt::AlignRight|Qt::AlignVCenter,durationText(qMax<qint64>(0,b.time+m_offset)));
        }
    }
}
void LyricView::mouseReleaseEvent(QMouseEvent *e) {
    if (hasTimeline()&&e->button() == Qt::LeftButton)
        for (auto &h : m_hits)
            if (h.first.contains(e->pos())) {
                emit seekRequested(h.second);
                break;
            }
}
void LyricView::wheelEvent(QWheelEvent *e) {
    if(m_lyrics.instrumental||m_blocks.isEmpty()){e->accept();return;}
    const int delta=e->angleDelta().y()?e->angleDelta().y():e->pixelDelta().y();if(!delta){e->accept();return;}
    const int current=m_manualIndex<0?qMax(0,m_index):m_manualIndex;
    m_manualIndex=qBound(0,current+(delta<0?1:-1),int(m_blocks.size())-1);
    setProperty("manualLyric",m_manualIndex);scrollTo(m_manualIndex);if(hasTimeline())m_resume.start();
    e->accept();
}
void LyricView::changeEvent(QEvent *e){QWidget::changeEvent(e);if(e->type()==QEvent::FontChange){rebuild();scrollTo(m_manualIndex>=0?m_manualIndex:qMax(0,m_index),false);}}
void LyricView::resizeEvent(QResizeEvent *e){QWidget::resizeEvent(e);rebuild();scrollTo(m_manualIndex>=0?m_manualIndex:qMax(0,m_index),false);}
} // namespace relaxer
