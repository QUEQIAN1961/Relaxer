#pragma once
#include <QJsonArray>
#include <QWidget>
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
    int m_count = 3;
    void rebuild();
};
} // namespace relaxer
