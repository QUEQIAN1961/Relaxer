#pragma once
#include <QVector>
#include <QtGlobal>

namespace relaxer {
enum class Mode { Sequential, ListLoop, SingleLoop, Shuffle };
// A shuffle bag visits every available track once before opening a new round.
class Queue {
  public:
    QVector<qint64> items;
    int index = -1;
    Mode mode = Mode::ListLoop;
    void set(QVector<qint64> ids, qint64 current = 0);
    qint64 current() const { return index >= 0 && index < items.size() ? items[index] : 0; }
    qint64 next(bool manual = false);
    qint64 previous();
    void setMode(Mode m);
    QVector<qint64> upcoming(int count=-1);

  private:
    QVector<qint64> bag, history;
    int historyIndex = -1;
    QVector<QVector<qint64>> m_plannedBags,m_generated;
    bool m_previewing=false;
    void refill(bool full = false);
};
} // namespace relaxer
