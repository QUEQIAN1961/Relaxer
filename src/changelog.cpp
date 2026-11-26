#include "changelog.h"
#include "changelog-summary.h"
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
namespace relaxer {
ChangeLog::ChangeLog(QWidget *parent, QJsonArray entries) : QWidget(parent), m_entries(entries) {
    setObjectName("versionLog");
    if (m_entries.isEmpty()) {
        QFile file(":/changelog.json");
        if (file.open(QIODevice::ReadOnly))
            m_entries = summarizedChangelog(QJsonDocument::fromJson(file.readAll()).array());
    }
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 16, 0, 0);
    m_layout->setSpacing(12);
    m_actions = new QHBoxLayout;
    m_actions->setContentsMargins(0, 0, 0, 0);
    m_actions->addStretch();
    m_layout->addLayout(m_actions);
    rebuild();
}
void ChangeLog::rebuild() {
    const int count = qMin(m_count, int(m_entries.size()));
    while (m_renderedEntries > count) {
        if (m_entries[--m_renderedEntries].toArray().size() < 3)
            continue;
        for (int label = 0; label < 2; ++label) {
            auto *item = m_layout->takeAt(m_layout->count() - 2);
            delete item->widget();
            delete item;
        }
    }
    for (int i = m_renderedEntries; i < count; ++i) {
        auto entry = m_entries[i].toArray();
        if (entry.size() < 3)
            continue;
        auto *title = new QLabel(entry.at(0).toString() + "  " + entry.at(1).toString());
        title->setObjectName("versionLogTitle");
        title->setTextFormat(Qt::PlainText);
        auto font = title->font();
        font.setBold(true);
        title->setFont(font);
        m_layout->insertWidget(m_layout->count() - 1, title);
        auto *detail = new QLabel(entry.at(2).toString());
        detail->setTextFormat(Qt::PlainText);
        detail->setWordWrap(true);
        detail->setObjectName("muted");
        m_layout->insertWidget(m_layout->count() - 1, detail);
    }
    m_renderedEntries = count;
    if (m_count > 3 && !m_collapse) {
        m_collapse = new QPushButton("收起");
        m_collapse->setObjectName("versionLogCollapse");
        m_actions->insertWidget(1, m_collapse);
        connect(m_collapse, &QPushButton::clicked, this, &ChangeLog::reset);
    } else if (m_count <= 3 && m_collapse) {
        delete m_collapse;
        m_collapse = nullptr;
    }
    if (m_count < m_entries.size() && !m_expand) {
        m_expand = new QPushButton("展开更多");
        m_expand->setObjectName("versionLogExpand");
        m_actions->addWidget(m_expand);
        connect(m_expand, &QPushButton::clicked, this, [this] {
            m_count += 5;
            rebuild();
        });
    } else if (m_count >= m_entries.size() && m_expand) {
        delete m_expand;
        m_expand = nullptr;
    }
}
} // namespace relaxer
