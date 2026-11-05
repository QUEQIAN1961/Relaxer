#pragma once
#include <QJsonObject>
#include <QStringList>

namespace relaxer {
inline QStringList uiFontRegions() { return {"songs", "lyrics", "footer", "next", "rest"}; }
inline QJsonObject uiFontFiles(const QJsonObject &configuration) {
    const auto files=configuration.value("files").toObject();
    QJsonObject resolved;
    for(const auto &region:uiFontRegions())
        resolved[region]=files.contains(region)?files.value(region).toString():
            configuration.value(region).toBool()?configuration.value("file").toString():QString();
    return resolved;
}
}
