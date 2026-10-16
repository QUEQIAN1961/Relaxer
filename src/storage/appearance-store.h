#pragma once
#include "settings-store.h"
#include <QJsonArray>
#include <QJsonObject>
namespace relaxer {
class AppearanceStore {
  public:
    explicit AppearanceStore(Library *library) : m_library(library), m_settings(library) {}
    static QJsonObject defaults();
    QJsonObject state() const;
    QJsonObject active() const;
    QJsonObject draft(QJsonObject theme) const;
    QString imagePath(const QJsonObject &theme, const QString &region) const;
    bool save(QJsonObject theme, const QString &replaceId, QString *error = nullptr);
    QJsonObject withProgressColors(QJsonObject theme, bool refreshRecommendation = false) const;
    bool initializeProgressColors();
    bool use(const QString &id);
    bool rename(const QString &id, const QString &name);
    bool remove(const QString &id);

  private:
    Library *m_library;
    SettingsStore m_settings;
};
} // namespace relaxer
