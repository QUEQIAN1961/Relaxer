#include "settings-store.h"
#include "library.h"
namespace relaxer {
QJsonValue SettingsStore::get(const QString &key,const QJsonValue &fallback)const {
    return m_library->setting(key,fallback);
}
bool SettingsStore::set(const QString &key,const QJsonValue &value) {
    return m_library->setSetting(key,value);
}
bool SettingsStore::setMany(const QJsonObject &values) {
    return m_library->setSettings(values);
}
} // namespace relaxer
