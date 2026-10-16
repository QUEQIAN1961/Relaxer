#pragma once
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace relaxer {
class Library;
// Borrowed, uncached access to the Library's existing settings connection.
// The Library must outlive this facade; use it only on the Library's thread.
// Encoding, defaults, transactions and notification timing remain with Library.
class SettingsStore final {
  public:
    explicit SettingsStore(Library *library):m_library(library){}
    QJsonValue get(const QString &key,const QJsonValue &fallback={})const;
    bool set(const QString &key,const QJsonValue &value);
    bool setMany(const QJsonObject &values);
  private:
    Library *m_library;
};
} // namespace relaxer
