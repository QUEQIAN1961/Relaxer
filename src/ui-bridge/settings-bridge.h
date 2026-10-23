#pragma once

#include "library.h"
#include "storage/settings-store.h"
#include "storage/appearance-store.h"
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QUrl>
#include <QPointer>
#include <functional>

class QColorDialog;

namespace relaxer {
class SettingsBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList changelog READ changelog CONSTANT)
    Q_PROPERTY(QVariantMap values READ values NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap palette READ palette NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList appearances READ appearances NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap appearanceDraft READ appearanceDraft NOTIFY appearanceChanged)
    Q_PROPERTY(int appearanceMode READ appearanceMode NOTIFY appearanceChanged)
    Q_PROPERTY(bool appearanceDirty READ appearanceDirty NOTIFY appearanceChanged)
    Q_PROPERTY(QString activeAppearanceId READ activeAppearanceId NOTIFY appearanceChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap windowBounds READ windowBounds NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList shortcutActions READ shortcutActions CONSTANT)
    Q_PROPERTY(QString dataRoot READ dataRoot CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    // Explicit C++ instance injection, never a QML or environment override.
    // An empty reader preserves the native system preference in production.
    using SystemThemeReader = std::function<bool()>;
    explicit SettingsBridge(Library *library, QObject *parent = nullptr, SystemThemeReader systemTheme = {});
    ~SettingsBridge() override;
    QVariantList changelog() const;
    QVariantMap values() const;
    QVariantMap windowBounds() const { return values().value("windowBounds").toMap(); }
    QVariantMap draft() const { return m_draft.toVariantMap(); }
    QVariantMap palette() const;
    QVariantList appearances() const;
    QVariantMap appearanceDraft() const { return m_appearanceDraft.toVariantMap(); }
    int appearanceMode() const { return m_appearanceMode; }
    bool appearanceDirty() const { return m_appearanceDirty; }
    QString activeAppearanceId() const { return m_activeAppearanceId; }
    QString fontFamily() const { return m_fontFamily; }
    QVariantList shortcutActions() const;
    QString dataRoot() const { return m_library->root(); }
    QString status() const { return m_status; }
    Q_INVOKABLE void resetDraft();
    Q_INVOKABLE bool edit(const QString &key, const QVariant &value);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool setPreference(const QString &key, const QVariant &value);
    Q_INVOKABLE bool setTheme(int theme);
    Q_INVOKABLE bool saveWindowState(int width, int height, bool maximized);
    Q_INVOKABLE bool openDataRoot();
    Q_INVOKABLE QString fontFamilyFor(const QString &region) const;
    Q_INVOKABLE bool saveFont(const QUrl &source, const QVariantMap &regions);
    Q_INVOKABLE bool resetFont();
    Q_INVOKABLE QString shortcutText(int key, int modifiers) const;
    Q_INVOKABLE QVariantMap defaultShortcuts() const;
    Q_INVOKABLE bool saveShortcuts(const QVariantMap &configuration);
    Q_INVOKABLE void beginAppearance(const QString &id = {});
    Q_INVOKABLE bool editAppearance(const QString &key, const QVariant &value);
    Q_INVOKABLE bool chooseAppearanceColor(const QString &key, QObject *ownerWindow);
    Q_INVOKABLE bool setAppearanceMode(int mode);
    Q_INVOKABLE void resetAppearance();
    Q_INVOKABLE void discardAppearance();
    Q_INVOKABLE bool saveAppearance(bool overwrite = false);
    Q_INVOKABLE bool useAppearance(const QString &id);
    Q_INVOKABLE bool renameAppearance(const QString &id, const QString &name);
    Q_INVOKABLE bool removeAppearance(const QString &id);
    Q_INVOKABLE bool savePalette(const QString &name, const QString &background, const QString &panel, const QString &accent);
signals:
    void settingsChanged();
    void draftChanged();
    void statusChanged();
    void appearanceChanged();
private:
    Library *m_library;
    SettingsStore m_store;
    AppearanceStore m_appearance;
    const SystemThemeReader m_systemTheme;
    QJsonObject m_draft;
    QJsonObject m_appearanceDraft;
    QJsonObject m_preview;
    int m_appearanceMode = 0;
    bool m_appearanceDirty = false;
    QString m_activeAppearanceId;
    QString m_status;
    int m_fontId = -1;
    QString m_fontFamily;
    QPointer<QColorDialog> m_colorDialog;
    quint64 m_appearanceSession = 0;
    void message(const QString &value);
    QString validatePreference(const QString &key, const QJsonValue &value) const;
    QString validate(const QJsonObject &value) const;
    void updateFont();
    void previewAppearance();
};
} // namespace relaxer
