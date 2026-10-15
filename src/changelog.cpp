#include "changelog.h"
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
namespace relaxer {
ChangeLog::ChangeLog(QWidget *parent, QJsonArray entries) : QWidget(parent), m_entries(entries) {
    setObjectName("versionLog");
    if (m_entries.isEmpty()) {
        QFile file(":/changelog.json");
        if (file.open(QIODevice::ReadOnly))
            m_entries = QJsonDocument::fromJson(file.readAll()).array();
    }
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 16, 0, 0);
    m_layout->setSpacing(12);
    rebuild();
}
void ChangeLog::rebuild() {
    QPointer<QScrollArea> scroll;
    for (auto *parent = parentWidget(); parent; parent = parent->parentWidget())
        if (auto *area = qobject_cast<QScrollArea *>(parent)) {
            scroll = area;
            break;
        }
    int position = scroll ? scroll->verticalScrollBar()->value() : 0;
    while (auto *item = m_layout->takeAt(0)) {
        if (item->widget())
            delete item->widget();
        delete item;
    }
    for (int i = 0; i < qMin(m_count, int(m_entries.size())); ++i) {
        auto entry = m_entries[i].toArray();
        if (entry.size() < 3)
            continue;
        auto *title = new QLabel(entry.at(0).toString() + "  " + entry.at(1).toString());
        title->setObjectName("versionLogTitle");
        title->setTextFormat(Qt::PlainText);
        auto font = title->font();
        font.setBold(true);
        title->setFont(font);
        m_layout->addWidget(title);
        auto *detail = new QLabel(entry.at(2).toString());
        detail->setTextFormat(Qt::PlainText);
        detail->setWordWrap(true);
        detail->setObjectName("muted");
        m_layout->addWidget(detail);
    }
    auto *host = new QWidget;
    auto *row = new QHBoxLayout(host);
    row->setContentsMargins(0, 0, 0, 0);
    row->addStretch();
    if (m_count > 3) {
        auto *collapse = new QPushButton("收起");
        collapse->setObjectName("versionLogCollapse");
        row->addWidget(collapse);
        connect(collapse, &QPushButton::clicked, this, [this] {
            m_count = 3;
            rebuild();
        });
    }
    if (m_count < m_entries.size()) {
        auto *expand = new QPushButton("展开更多");
        expand->setObjectName("versionLogExpand");
        row->addWidget(expand);
        connect(expand, &QPushButton::clicked, this, [this] {
            m_count += 5;
            rebuild();
        });
    }
    m_layout->addWidget(host);
    if (scroll)
        QTimer::singleShot(0, this, [scroll, position] {
            if (scroll)
                scroll->verticalScrollBar()->setValue(position);
        });
}
} // namespace relaxer
