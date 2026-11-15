#include "app-bridge.h"
#include <QFile>
#include "phase-c-version.h"
#include <QThread>
#include <QGuiApplication>
#include <QClipboard>
#include <climits>

namespace relaxer {
QString AppBridge::legalNotice() const {
    QFile license(":/qml/legal/GPL-3.0.txt"),references(":/qml/legal/REFERENCES.md");
    license.open(QIODevice::ReadOnly);references.open(QIODevice::ReadOnly);
    auto notice=QString::fromUtf8(references.readAll())+QStringLiteral("\n\n----- GNU GPL v3 -----\n\n")+QString::fromUtf8(license.readAll());
    for(const auto &name:{"kugou-MIT.txt","qishui-MIT.txt","qishui-car-MIT.txt"}){
        QFile file(QStringLiteral(":/qml/legal/")+QString::fromLatin1(name));
        if(file.open(QIODevice::ReadOnly))notice+=QStringLiteral("\n\n----- ")+QString::fromLatin1(name)+QStringLiteral(" -----\n\n")+QString::fromUtf8(file.readAll());
    }
    return notice;
}
void AppBridge::copyText(const QString &text){if(!text.isEmpty())QGuiApplication::clipboard()->setText(text);}
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
    if(library)connect(library,&Library::playbackHistoryChanged,this,&AppBridge::historyChanged);
    refresh();
}
QString AppBridge::version() const { return QString::fromLatin1(phaseC::Display); }
QString AppBridge::windowTitle() const { return QString::fromLatin1(phaseC::WindowTitle); }
bool AppBridge::navigate(int page) {
    Q_ASSERT(QThread::currentThread() == thread());
    if (page < 0 || page > 5) return false;
    if (m_page != page) { m_page = page; emit pageChanged(); }
    return true;
}
void AppBridge::refresh() {
    Q_ASSERT(QThread::currentThread() == thread());
    const int count = m_library ? int(qMin<qint64>(m_library->trackCount(), INT_MAX)) : 0;
    if (m_count != count) { m_count = count; emit trackCountChanged(); }
}
}
