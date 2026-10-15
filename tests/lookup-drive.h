#pragma once
#include <QAbstractButton>
#include <QApplication>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
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
