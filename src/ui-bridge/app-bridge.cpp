#include "app-bridge.h"
#include "phase-c-version.h"
#include <QThread>

namespace relaxer {
AppBridge::AppBridge(Library *library, QObject *parent) : QObject(parent), m_library(library) {
    Q_ASSERT(library && library->thread() == thread());
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(0);
    connect(&m_refresh, &QTimer::timeout, this, &AppBridge::refresh);
    connect(library, &Library::changed, this, [this] {
        if (!m_refresh.isActive()) m_refresh.start();
    });
    connect(library, &QObject::destroyed, this, [this] {
        m_library = nullptr;
        m_refresh.stop();
        refresh();
        emit availableChanged();
    });
    refresh();
}
QString AppBridge::version() const { return QString::fromLatin1(phaseC::Display); }
QString AppBridge::windowTitle() const { return QString::fromLatin1(phaseC::WindowTitle); }
bool AppBridge::navigate(int page) {
    Q_ASSERT(QThread::currentThread() == thread());
    if (page < 0 || page > 3) return false;
    if (m_page != page) { m_page = page; emit pageChanged(); }
    return true;
}
void AppBridge::refresh() {
    Q_ASSERT(QThread::currentThread() == thread());
    const int count = m_library ? m_library->tracks().size() : 0;
    if (m_count != count) { m_count = count; emit trackCountChanged(); }
}
}
