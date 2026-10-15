#pragma once
#include "lyrics.h"
#include <QTimer>
#include <QVariantAnimation>
#include <memory>
class QTextLayout;
#include "model.h"
#include <QColor>
#include <QComboBox>
#include <QMenu>
#include <QPointer>
#include <QAbstractButton>
#include <QIcon>
#include <QImage>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QWidget>
#include <QSet>
#include <QTimer>
#include <QHelpEvent>
namespace relaxer {
class Library;
QIcon symbol(const QString &name, const QColor &color = QColor("#555560"));
QString volumeSymbol(int percent);
QPixmap coverPixmap(const QString &path, const QString &seed, int size = 400);
void applyPopupTheme(const QPalette &palette);
class CircleButton : public QAbstractButton {
    Q_OBJECT
public:
    CircleButton(const QString &name,const QString &tip,QWidget *parent=nullptr);
    void setSymbol(const QString &,const QColor &);
    void setAccent(const QColor &);
    QSize sizeHint() const override { return {42,42}; }
protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &) const override;
private:
    QIcon m_icon;
    QColor m_accent=QColor("#e5455f");
};
class RoundedComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit RoundedComboBox(QWidget *parent=nullptr);
    void showPopup() override;
    void resizeEvent(QResizeEvent *) override;
protected:
    bool eventFilter(QObject *,QEvent *) override;
private:
    QPointer<QWidget> m_popup;
};
class RoundedMenu : public QMenu {
    Q_OBJECT
public:
    explicit RoundedMenu(QWidget *parent=nullptr);
    using QMenu::addMenu;
    RoundedMenu *addMenu(const QString &title);
    RoundedMenu *addMenu(const QIcon &icon, const QString &title);
protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;
    void resizeEvent(QResizeEvent *) override;
};
class Cover : public QWidget {
    Q_OBJECT
  public:
    explicit Cover(QWidget *p = nullptr);
    QSize sizeHint() const override { return {360, 360}; }
    void setCover(const QString &path, const QString &seed);
    void setRadius(int r) {
        m_radius = r;
        update();
    }
  signals:
    void clicked();

  protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

  private:
    QString m_path, m_seed = "Relaxer";
    int m_radius = 16;
};
class Backdrop : public QWidget {
    Q_OBJECT
  public:
    using QWidget::QWidget;
    void setArtwork(const QString &path, const QString &seed, bool dark);
    void setBlur(double value) { if(m_blur!=value){m_blur=value;m_image={};m_scaled={};} }
    void useCustomBackground(bool enabled) { m_custom = enabled; update(); }
    void setCustomBackground(const QColor &color, const QString &path, double blur);

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    QImage m_image, m_scaled;
    QString m_path,m_seed;
    double m_blur=25;
    bool m_dark = false;
    bool m_custom = false;
    QColor m_customColor;
    QString m_customPath;
    double m_customBlur = -1;
    QImage m_customImage, m_customScaled;
};
class IconButton : public QPushButton {
    Q_OBJECT
  public:
    IconButton(const QString &name, const QString &tip, QWidget *p = nullptr);
    void setSymbol(const QString &name, const QColor &color = QColor("#555560"));

  protected:
    void paintEvent(QPaintEvent *) override;
};
class Slider : public QSlider {
    Q_OBJECT
  public:
    explicit Slider(QWidget *p = nullptr) : QSlider(Qt::Horizontal, p) {}

  protected:
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

  private:
    int valueAt(const QPointF &position) const;
    bool m_dragging = false;
    qreal m_grabOffset = 0;
    QPointF m_pointerPosition;
};
class PercentSpin : public QSpinBox {
    Q_OBJECT
  public:
    using QSpinBox::QSpinBox;

  protected:
    void wheelEvent(QWheelEvent *) override;
    void resizeEvent(QResizeEvent *) override;
};
class BlurPercentSpin : public QDoubleSpinBox {
    Q_OBJECT
public: using QDoubleSpinBox::QDoubleSpinBox;
protected: void wheelEvent(QWheelEvent *e) override;void resizeEvent(QResizeEvent *e) override;
};
class SongDelegate : public QStyledItemDelegate {
  public:
    explicit SongDelegate(QObject *p = nullptr) : QStyledItemDelegate(p) {}
    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    bool helpEvent(QHelpEvent *,QAbstractItemView *,const QStyleOptionViewItem &,const QModelIndex &) override;
};
class SongTable : public QTableWidget {
    Q_OBJECT
  public:
    explicit SongTable(QWidget *p = nullptr);
    ~SongTable() override;
    enum Column { NumberColumn, FavoriteColumn, TitleColumn, ArtistColumn, AlbumColumn,
                  DurationColumn, DetailsColumn, ImportDateColumn };
    enum ColumnsMode { Statistics, LocalMetadata };
    void setColumnsMode(ColumnsMode mode);
    void setColumnLibrary(Library *library);
    ColumnsMode columnsMode() const { return m_columnsMode; }
    static QString fileSizeText(qint64 bytes);
    qint64 totalFileSize() const {return m_totalFileSize;}
    static QString importDateText(const QString &timestamp);
    QVector<qint64> ids() const;
    QVector<qint64> selectedIds() const;
    void setSongs(const QVector<Track> &songs, const QString &root, qint64 playing = 0);
    void updateStats(const QVector<Track> &songs);
    bool reorderEnabled = false;
    void setMultiSelect(bool enabled);
    bool multiSelect() const { return m_multiSelect; }
  signals:
    void reordered();

  protected:
    void keyPressEvent(QKeyEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void showEvent(QShowEvent *) override;
  private:
    QPointer<Library> m_columnLibrary;
    QTimer m_columnSave;
    bool m_columnInternal=false;
    QSet<int> m_customColumns;
    QHash<int,int> m_requestedWidths;
    QString widthKey(int) const;
    void restoreColumnWidths();void saveColumnWidths();void fitTitleColumn();
    bool m_multiSelect=false;
    ColumnsMode m_columnsMode=Statistics;
    QHash<QString,qint64> m_fileSizes;
    qint64 m_totalFileSize=0;
};
class LyricView : public QWidget {
    Q_OBJECT
  public:
    explicit LyricView(QWidget *p = nullptr);
    void setLyrics(Lyrics lyrics);
    void setPosition(qint64 ms);
    void setOffset(int ms);
    void setShowTranslation(bool on);
    void setLyricColor(QColor color) { m_color=color;update(); }
    bool hasTimeline() const { return m_lyrics.timed&&!m_lyrics.instrumental; }
    void browseFraction(qreal fraction,bool dragging=false);
    void setBrowsingDrag(bool dragging);
    bool dark = false;
  signals:
    void seekRequested(qint64 ms);
    void scrollMetricsChanged(int value,int page,bool scrollable);

  protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void changeEvent(QEvent *) override;

  private:
    Lyrics m_lyrics;
    struct Block {
        qint64 time;QString text,translation;qreal top=0,height=0;
        std::shared_ptr<QTextLayout> layout,activeLayout,translationLayout,activeTranslationLayout;
        qreal normalHeight=0,activeHeight=0,primaryNormal=0,primaryActive=0;
    };
    QVector<Block> m_blocks;
    int m_index=-1,m_manualIndex=-1,m_offset=0;
    bool m_showTranslation=true;
    qint64 m_position=0;
    qreal m_center=0;
    QColor m_color;
    QTimer m_resume;
    QVariantAnimation m_slide;
    void rebuild();
    void arrangeBlocks();
    void scrollTo(int index,bool animate=true);
    void publishScroll();
    QVector<QPair<QRect, qint64>> m_hits;
};
} // namespace relaxer
