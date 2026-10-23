#pragma once
#include <QSemaphore>
#include <QThreadPool>
#include <QRunnable>
#include <memory>

// Hold real workers at an explicit barrier, while the GUI thread keeps
// processing events. No guessed ranking/audit duration or long test sleep.
class AsyncPoolGate {
    struct State {QSemaphore reached,release;};
    QThreadPool *pool;std::shared_ptr<State> state=std::make_shared<State>();int count;
public:
    explicit AsyncPoolGate(QThreadPool *p):pool(p),count(p->maxThreadCount()) {
        for(int i=0;i<count;++i)pool->start(QRunnable::create([s=state]{s->reached.release();s->release.tryAcquire(1,15000);}));
    }
    bool entered(){return state->reached.tryAcquire(count,5000);}
    void release(){state->release.release(count);}
    ~AsyncPoolGate(){release();pool->waitForDone(5000);}
};
