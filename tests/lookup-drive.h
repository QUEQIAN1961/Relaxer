#pragma once
#include <QAbstractButton>
#include <QApplication>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include "lyrics-search-coordinator.h"
#include <QSignalSpy>
#include <QtTest>
inline void relaxerClickSearch(QWidget *dialog) {
    auto *tracks = dialog->findChild<QListWidget *>("lyricsTracks");
    auto *start = dialog->findChild<QPushButton *>("lyricsStart");
    if (!tracks || !start) return;
    int visible = 0, selected = 0;
    for (int r = 0; r < tracks->count(); ++r) {
        auto *item = tracks->item(r);
        if (!item || item->isHidden()) continue;
        ++visible;
        if (item->isSelected()) ++selected;
    }
    if (visible == 0) return;
    if (selected == 0) {
        QTimer::singleShot(0, dialog, [] {
            for (auto *widget : QApplication::topLevelWidgets()) {
                if (widget->objectName() != "lyricsSearchAllConfirm") continue;
                if (auto *button = widget->findChild<QAbstractButton *>("lyricsSearchAll")) {
                    button->click();
                    return;
                }
            }
        });
    }
    start->click();
}

// Called immediately after starting an asynchronous lookup. Require a clean
// NoResult terminal event, rather than mistaking a failure/cancellation for empty.
inline bool relaxerWaitForNoResult(QWidget *dialog,qint64 trackId,int row,int timeout=8000) {
    auto *service=dialog->findChild<relaxer::LyricsSearchCoordinator *>();
    auto *tracks=dialog->findChild<QListWidget *>("lyricsTracks");
    if(!service||!tracks||row<0||row>=tracks->count())return false;
    QSignalSpy finished(service,&relaxer::LyricsSearchCoordinator::taskFinished);
    return QTest::qWaitFor([&]{
        if(service->summary().running)return false;
        for(const auto &args:finished){
            const auto event=qvariant_cast<relaxer::LyricsSearchEvent>(args.first());
            if(event.trackId==trackId&&event.generation==service->summary().generation&&
               event.state==relaxer::LyricsSearchState::NoResult&&event.sourceFailures.isEmpty())
                return tracks->item(row)->data(Qt::UserRole+2).toString().contains(QStringLiteral("所选来源均无结果"))&&
                    service->candidates(trackId).records.isEmpty();
        }
        return false;
    },timeout);
}
