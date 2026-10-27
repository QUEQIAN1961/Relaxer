#pragma once

#include <QJsonObject>
#include <QKeySequence>
#include <QStringList>

namespace relaxer {
struct NativeKey {
    unsigned modifiers = 0, key = 0;
};

// Shared value rules. This type owns no UI object or global hotkey lifecycle.
class ShortcutRules {
  public:
    static QJsonObject defaults();
    static QStringList actionKeys();
    static QStringList actionNames();
    static bool validate(const QJsonObject &, QString *error = nullptr);
    static NativeKey nativeKey(const QKeySequence &);
};
} // namespace relaxer
