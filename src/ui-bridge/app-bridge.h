#pragma once
#include "library.h"
#include <QPointer>
#include <QTimer>

namespace relaxer {
class AppBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString legalNotice READ legalNotice CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString windowTitle READ windowTitle CONSTANT)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY trackCountChanged)
    Q_PROPERTY(int page READ page NOTIFY pageChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
  public:
    QString legalNotice() const;
    explicit AppBridge(Library *library, QObject *parent = nullptr);
    QString version() const;
    QString windowTitle() const;
    int trackCount() const { return m_count; }
    int page() const { return m_page; }
    bool available() const { return !m_library.isNull(); }
    Q_INVOKABLE bool navigate(int page);
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE QVariantList playbackHistory(const QString &query={},int offset=0,const QString &sortKey="endedAt",bool descending=true) const {return m_library?m_library->playbackHistory(query,offset,sortKey,descending):QVariantList{};}
    void refresh();
  signals:
    void trackCountChanged();
    void pageChanged();
    void historyChanged();
    void availableChanged();
  private:
    QPointer<Library> m_library;
    QTimer m_refresh;
    int m_count = 0, m_page = 0;
};
}
