#pragma once
#include "library.h"
#include "storage/appearance-store.h"
#include "appearance-image.h"
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
