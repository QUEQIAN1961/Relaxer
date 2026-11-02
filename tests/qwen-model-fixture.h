#pragma once
#include "ai/ai-model.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace qwen_fixture {
inline const QStringList CurrentModels{"qwen3.8-max", "qwen3.8-max-0902", "qwen3.7-max", "qwen3.7-plus", "qwen3.8-flash"};
inline QJsonObject model(const QString &id) {
    return {{"model",id},{"provider","qwen"},{"published_time","2026-09-02 00:00:00"},
            {"features",QJsonArray{"structured-outputs"}},{"capabilities",QJsonArray{"TG","Reasoning"}},
            {"inference_metadata",QJsonObject{{"request_modality",QJsonArray{"Text"}},{"response_modality",QJsonArray{"Text"}}}},
            {"model_info",QJsonObject{{"context_window",1000000},{"max_output_tokens",131072}}}};
}
inline QByteArray catalog(const QStringList &ids, int page = 1, int size = 100, int total = -1) {
    QJsonArray models;
    for (const auto &id : ids) models.append(model(id));
    return QJsonDocument(QJsonObject{{"success",true},{"output",QJsonObject{{"total",total < 0 ? ids.size() : total},
        {"page_no",page},{"page_size",size},{"models",models}}}}).toJson(QJsonDocument::Compact);
}
inline QStringList ids(const QList<relaxer::ai::ModelInfo> &models) {
    QStringList result; for (const auto &model : models) result.append(model.id); return result;
}
}
