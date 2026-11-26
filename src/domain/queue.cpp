#include "queue.h"
#include <QRandomGenerator>
#include <QSet>

namespace relaxer {
void Queue::set(QVector<qint64> ids, qint64 id) {
    pendingEdited=false;
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
    m_priority.clear();
    m_plannedBags.clear();m_generated.clear();
    history = {};
    historyIndex = -1;
    if (current()) {
        history << current();
        historyIndex = 0;
    }
    refill();
}
bool Queue::enqueue(const QVector<qint64> &ids,bool next){
    const auto playing=current();QVector<qint64> selected;QSet<qint64> seen;
    for(auto id:ids)if(id>0&&id!=playing&&!seen.contains(id)){seen.insert(id);selected.append(id);}
    if(selected.isEmpty())return false;
    if(!playing){set(selected);return true;}
    for(auto id:selected){items.removeAll(id);bag.removeAll(id);m_priority.removeAll(id);}
    index=items.indexOf(playing);const int insertion=next?index+1:int(items.size());
    for(int i=0;i<selected.size();++i)items.insert(insertion+i,selected[i]);
    if(next){m_priority=selected+m_priority;if(historyIndex+1<history.size())history.resize(historyIndex+1);}
    else for(auto it=selected.crbegin();it!=selected.crend();++it)bag.prepend(*it);
    m_plannedBags.clear();m_generated.clear();return true;
}
bool Queue::editUpcoming(const QVector<qint64> &order) {
    const auto playing=current();if(!playing)return false;
    auto pending=upcoming();pending.removeAll(playing);QSet<qint64> eligible(pending.begin(),pending.end()),seen;
    if(mode!=Mode::Sequential)for(auto id:items)if(id!=playing)eligible.insert(id);
    for(auto id:order)if(!eligible.contains(id)||seen.contains(id))return false;else seen.insert(id);
    QVector<qint64> kept;for(auto id:items)if(!eligible.contains(id))kept.append(id);
    const int at=kept.indexOf(playing);if(at<0)return false;
    for(int i=0;i<order.size();++i)kept.insert(at+1+i,order[i]);
    items=kept;index=items.indexOf(playing);m_priority=order;pendingEdited=true;
    m_plannedBags.clear();m_generated.clear();refill();
    QVector<qint64> keptHistory;int keptIndex=-1;
    for(int i=0;i<history.size();++i)if(items.contains(history[i])){keptHistory.append(history[i]);if(i<=historyIndex)keptIndex=keptHistory.size()-1;}
    history=keptHistory;historyIndex=keptIndex;
    if(historyIndex+1<history.size())history.resize(historyIndex+1);
    if(historyIndex<0||history.value(historyIndex)!=playing){history.append(playing);historyIndex=history.size()-1;}
    return true;
}
void Queue::restoreUpcomingState(bool edited,const QVector<qint64> &priority){
    pendingEdited=edited;m_priority.clear();QSet<qint64> seen;
    for(auto id:priority)if(id!=current()&&items.contains(id)&&!seen.contains(id)){seen.insert(id);m_priority.append(id);bag.removeAll(id);}
    m_plannedBags.clear();m_generated.clear();
}
void Queue::refill(bool full) {
    if(full&&!m_plannedBags.isEmpty()){bag=m_plannedBags.takeFirst();return;}
    bag = items;
    if (!full)
        bag.removeAll(current());
    for(auto id:m_priority)bag.removeAll(id);
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
    if(pendingEdited){
        const int wanted=count<0?items.size()-1:qMin(count,int(items.size())-1);
        Queue copy=*this;copy.m_previewing=true;copy.m_generated.clear();QSet<qint64> seen;
        for(int i=0;i<items.size()*2&&result.size()<wanted;++i){const auto id=copy.next(true);if(!id)break;if(id!=current()&&!seen.contains(id)){seen.insert(id);result.append(id);}}
        m_plannedBags+=copy.m_generated;return result;
    }
    const int limit=count<0?int(items.size()):qMin(count,int(items.size()));
    Queue copy=*this;copy.m_previewing=true;copy.m_generated.clear();
    for(int i=0;i<limit;++i){const auto id=copy.next(true);if(!id)break;result<<id;}
    m_plannedBags+=copy.m_generated;return result;
}
qint64 Queue::next(bool manual) {
    if (items.isEmpty())
        return 0;
    if(mode==Mode::Shuffle&&historyIndex+1<history.size()){
        index=items.indexOf(history[++historyIndex]);return current();
    }
    if(!m_priority.isEmpty()){
        const auto id=m_priority.takeFirst();index=items.indexOf(id);bag.removeAll(id);
        if(historyIndex+1<history.size())history.resize(historyIndex+1);
        history.append(id);historyIndex=int(history.size())-1;return id;
    }
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
