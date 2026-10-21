#pragma once
#include <QGuiApplication>
#include <QPalette>
#include <QUuid>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Exercise the real system-theme reader against a process-local registry root.
// The desktop's Personalize key is never written; restore before removing our
// uniquely owned volatile fixture key. No child process inherits this override.
class SystemThemeFixture {
public:
    explicit SystemThemeFixture(bool dark) {
#ifdef Q_OS_WIN
        m_path = (QStringLiteral("Software\\Relaxer\\TestThemes\\") +
                  QUuid::createUuid().toString(QUuid::Id128)).toStdWString();
        if (RegOpenCurrentUser(KEY_ALL_ACCESS, &m_user) != ERROR_SUCCESS) return;
        if (RegCreateKeyExW(m_user, m_path.c_str(), 0, nullptr, REG_OPTION_VOLATILE,
                            KEY_ALL_ACCESS, nullptr, &m_root, nullptr) != ERROR_SUCCESS) return;
        HKEY personalize = nullptr;
        if (RegCreateKeyExW(m_root, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                            0, nullptr, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, nullptr,
                            &personalize, nullptr) != ERROR_SUCCESS) return;
        const DWORD light = dark ? 0 : 1;
        const auto written = RegSetValueExW(personalize, L"AppsUseLightTheme", 0, REG_DWORD,
                                            reinterpret_cast<const BYTE *>(&light), sizeof(light));
        RegCloseKey(personalize);
        if (written != ERROR_SUCCESS) return;
        m_active = RegOverridePredefKey(HKEY_CURRENT_USER, m_root) == ERROR_SUCCESS;
#else
        m_original = QGuiApplication::palette();
        auto palette = m_original;
        palette.setColor(QPalette::Window, dark ? QColor("#19191e") : QColor("#ffffff"));
        QGuiApplication::setPalette(palette);
        m_active = true;
#endif
    }
    ~SystemThemeFixture() {
#ifdef Q_OS_WIN
        if (m_active && RegOverridePredefKey(HKEY_CURRENT_USER, nullptr) != ERROR_SUCCESS)
            qFatal("Cannot restore process-local HKCU theme fixture");
        if (m_root) { RegCloseKey(m_root); RegDeleteTreeW(m_user, m_path.c_str()); }
        if (m_user) RegCloseKey(m_user);
#else
        QGuiApplication::setPalette(m_original);
#endif
    }
    bool ok() const { return m_active; }
    SystemThemeFixture(const SystemThemeFixture &) = delete;
    SystemThemeFixture &operator=(const SystemThemeFixture &) = delete;
private:
    bool m_active = false;
#ifdef Q_OS_WIN
    HKEY m_user = nullptr, m_root = nullptr;
    std::wstring m_path;
#else
    QPalette m_original;
#endif
};
