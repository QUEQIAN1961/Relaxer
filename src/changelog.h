#pragma once
#include <QJsonArray>
#include <QWidget>
class QHBoxLayout;
class QPushButton;
class QVBoxLayout;
namespace relaxer {
class ChangeLog : public QWidget {
    Q_OBJECT
  public:
    explicit ChangeLog(QWidget *parent = nullptr, QJsonArray entries = {});
    int visibleCount() const { return m_count; }
    void reset() { m_count = 3; rebuild(); }

  private:
    QJsonArray m_entries;
    QVBoxLayout *m_layout;
    QHBoxLayout *m_actions;
    QPushButton *m_expand = nullptr;
    QPushButton *m_collapse = nullptr;
    int m_count = 3;
    int m_renderedEntries = 0;
    void rebuild();
};
} // namespace relaxer
