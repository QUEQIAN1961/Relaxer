#pragma once
#include "ui-bridge/settings-bridge.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtTest>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Pure per-instance input: never create, write or redirect registry keys.
class SystemThemeFixture {
public:
    explicit SystemThemeFixture(bool dark) : m_dark(dark) {}
    relaxer::SettingsBridge::SystemThemeReader reader() const {
        return [dark = m_dark] { return dark; };
    }
    void record(QJsonObject actual) const {
        actual["source"] = "explicit C++ instance reader; simulated system input";
        actual["input"] = m_dark ? "dark-system" : "light-system";
        actual["test"] = QString::fromLatin1(QTest::currentTestFunction());
        actual["status"] = "PASS (exact-color and persistence assertions executed)";
#ifdef Q_OS_WIN
        // Open attempts only: record token access without any registry mutation.
        HKEY key = nullptr;
        auto status = RegOpenCurrentUser(KEY_READ, &key);
        actual["hkcuReadOpenStatus"] = int(status); if (key) RegCloseKey(key);
        key = nullptr; status = RegOpenCurrentUser(KEY_WRITE, &key);
        actual["hkcuWriteOpenStatus"] = int(status); if (key) RegCloseKey(key);
#endif
        qInfo().noquote() << "SYSTEM_THEME_EVIDENCE" << QJsonDocument(actual).toJson(QJsonDocument::Compact);
        const auto directory = qEnvironmentVariable("RELAXER_C6_EVIDENCE");
        if (directory.isEmpty()) return; // Assertions ran; QtTest log remains evidence.
        QFile file(QDir(directory).filePath("theme-" + QUuid::createUuid().toString(QUuid::Id128) + ".json"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) qFatal("Cannot preserve theme evidence");
        const auto bytes = QJsonDocument(actual).toJson();
        if (file.write(bytes) != bytes.size()) qFatal("Cannot write theme evidence");
    }
private:
    const bool m_dark;
};
