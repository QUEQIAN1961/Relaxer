#include "queue.h"
#include <QRandomGenerator>
#include <QSet>

namespace relaxer {
void Queue::set(QVector<qint64> ids, qint64 id) {
    items.clear();
    QSet<qint64> unique;
    for (auto t : ids)
        if (t > 0 && !unique.contains(t)) {
            unique.insert(t);
            items << t;
        }
    index = items.indexOf(id);
    if (index < 0 && !items.isEmpty())
        index = 0;
    bag.clear();
    m_plannedBags.clear();m_generated.clear();
    history = {};
    historyIndex = -1;
    if (current()) {
        history << current();
        historyIndex = 0;
    }
    refill();
}
void Queue::refill(bool full) {
    if(full&&!m_plannedBags.isEmpty()){bag=m_plannedBags.takeFirst();return;}
    bag = items;
    if (!full)
        bag.removeAll(current());
    for (int i = bag.size() - 1; i > 0; --i)
        bag.swapItemsAt(i, int(QRandomGenerator::global()->bounded(i + 1)));
    if (full && bag.size() > 1 && bag.last() == current())
        bag.swapItemsAt(0, bag.size() - 1);
    if(full&&m_previewing)m_generated<<bag;
}
void Queue::setMode(Mode m) {
    mode = m;
    m_plannedBags.clear();m_generated.clear();
    refill();
    history = {};
    historyIndex = -1;
    if (current()) {
        history << current();
        historyIndex = 0;
    }
}
QVector<qint64> Queue::upcoming(int count){
    QVector<qint64> result;if(items.isEmpty())return result;
    const int limit=count<0?int(items.size()):qMin(count,int(items.size()));
    Queue copy=*this;copy.m_previewing=true;copy.m_generated.clear();
    for(int i=0;i<limit;++i){const auto id=copy.next(true);if(!id)break;result<<id;}
    m_plannedBags+=copy.m_generated;return result;
}
qint64 Queue::next(bool manual) {
    if (items.isEmpty())
        return 0;
    if (mode == Mode::SingleLoop && !manual)
        return current();
    if (mode == Mode::Shuffle) {
        if (historyIndex + 1 < history.size()) {
            index = items.indexOf(history[++historyIndex]);
            return current();
        }
        if (bag.isEmpty())
            refill(true);
        qint64 id = bag.isEmpty() ? current() : bag.takeLast();
        index = items.indexOf(id);
        history << id;
        historyIndex = history.size() - 1;
        return id;
    }
    if (index + 1 < items.size()) {
        ++index;
        return current();
    }
    if (mode == Mode::Sequential)
        return 0;
    index = 0;
    return current();
}
qint64 Queue::previous() {
    if (items.isEmpty())
        return 0;
    if (mode == Mode::Shuffle && historyIndex > 0) {
        index = items.indexOf(history[--historyIndex]);
        return current();
    }
    if (mode == Mode::Sequential && index <= 0)
        return current();
    index = (index - 1 + items.size()) % items.size();
    return current();
}
} // namespace relaxer
