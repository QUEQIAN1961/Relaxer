#pragma once
#include "library.h"
#include <QPointer>
#include <QTimer>

namespace relaxer {
class AppBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString windowTitle READ windowTitle CONSTANT)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY trackCountChanged)
    Q_PROPERTY(int page READ page NOTIFY pageChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
  public:
    explicit AppBridge(Library *library, QObject *parent = nullptr);
    QString version() const;
    QString windowTitle() const;
    int trackCount() const { return m_count; }
    int page() const { return m_page; }
    bool available() const { return !m_library.isNull(); }
    Q_INVOKABLE bool navigate(int page);
    void refresh();
  signals:
    void trackCountChanged();
    void pageChanged();
    void availableChanged();
  private:
    QPointer<Library> m_library;
    QTimer m_refresh;
    int m_count = 0, m_page = 0;
};
}
