#pragma once
#include "library.h"
#include <QColor>
#include <QWidget>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QVBoxLayout>
#include <functional>
class QPushButton;
class QTimer;
namespace relaxer {
QImage gaussianBlur(QImage image, double strength);
QImage fixedGaussianBlur(QImage image,int strength);
double blurRadius(double percent);
QColor autoInk(const QColor &color);
QImage backgroundImage(const QString &path, int maximum = 1600);
class Surface : public QWidget {
    Q_OBJECT
  public:
    using QWidget::QWidget;
    void setLook(const QColor &color, const QString &image = {}, double blur = 0, QWidget *shared = nullptr);

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    QColor m_color;
    QString m_file;
    double m_blur = -1;
    QImage m_image;
    QPixmap m_scaled;
    QSize m_canvas;
    QPointer<QWidget> m_shared;
};
class AppearanceStore {
  public:
    explicit AppearanceStore(Library *library) : m_library(library) {}
    static QJsonObject defaults();
    QJsonObject state() const;
    QJsonObject active() const;
    QJsonObject draft(QJsonObject theme) const;
    QString imagePath(const QJsonObject &theme, const QString &region) const;
    bool save(QJsonObject theme, const QString &replaceId, QString *error = nullptr);
    QJsonObject withProgressColors(QJsonObject theme, bool refreshRecommendation = false) const;
    bool initializeProgressColors();
    bool use(const QString &id);
    bool rename(const QString &id, const QString &name);
    bool remove(const QString &id);

  private:
    Library *m_library;
};
class AppearancePage : public QWidget {
    Q_OBJECT
  public:
    AppearancePage(Library *, std::function<void(const QJsonObject &)> preview,
                   std::function<void()> restore, QWidget *parent = nullptr, QJsonObject initial = {});
    bool confirmLeave();
  signals:
    void finished();

  private:
    Library *m_library;
    AppearanceStore m_store;
    QJsonObject m_draft;
    QString m_active;
    std::function<void(const QJsonObject &)> m_preview;
    std::function<void()> m_restore;
    QVBoxLayout *m_historyLayout = nullptr;
    QPushButton *m_newSave = nullptr, *m_overwrite = nullptr, *m_savePreset = nullptr;
    int m_mode = 0;
    QTimer *m_previewTimer = nullptr;
    bool m_dirty = false;
    void previewDraft();
    bool save(const QString &replaceId = {});
    void rebuildHistory();
};
} // namespace relaxer
