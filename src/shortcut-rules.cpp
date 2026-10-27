#include "shortcut-rules.h"
#include <QMap>
#include <QSet>

namespace relaxer {
QStringList ShortcutRules::actionKeys() {
    return {"toggle", "previous", "next", "volumeUp", "volumeDown"};
}
QStringList ShortcutRules::actionNames() {
    return {"播放 / 暂停", "上一首", "下一首", "音量增加", "音量减少"};
}
QJsonObject ShortcutRules::defaults() {
    return {{"app", QJsonObject{{"toggle", "Space"},
                                {"previous", "Ctrl+Left"},
                                {"next", "Ctrl+Right"},
                                {"volumeUp", "Ctrl+Up"},
                                {"volumeDown", "Ctrl+Down"}}},
            {"global", QJsonObject{{"toggle", "Ctrl+Alt+Space"},
                                   {"previous", "Ctrl+Alt+Left"},
                                   {"next", "Ctrl+Alt+Right"},
                                   {"volumeUp", "Ctrl+Alt+Up"},
                                   {"volumeDown", "Ctrl+Alt+Down"}}},
            {"globalEnabled", false}};
}
NativeKey ShortcutRules::nativeKey(const QKeySequence &sequence) {
    if (sequence.count() != 1)
        return {};
    auto combination = sequence[0];
    auto modifiers = combination.keyboardModifiers();
    if (modifiers & ~(Qt::AltModifier | Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier))
        return {};
    unsigned flags = 0;
    if (modifiers & Qt::AltModifier)
        flags |= 1;
    if (modifiers & Qt::ControlModifier)
        flags |= 2;
    if (modifiers & Qt::ShiftModifier)
        flags |= 4;
    if (modifiers & Qt::MetaModifier)
        flags |= 8;
    int key = combination.key();
    unsigned value = 0;
    if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9))
        value = key;
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        value = 0x70 + key - Qt::Key_F1;
    else {
        static const QMap<int, unsigned> values{
            {Qt::Key_Space, 0x20},     {Qt::Key_Left, 0x25},          {Qt::Key_Up, 0x26},
            {Qt::Key_Right, 0x27},     {Qt::Key_Down, 0x28},          {Qt::Key_Home, 0x24},
            {Qt::Key_End, 0x23},       {Qt::Key_PageUp, 0x21},        {Qt::Key_PageDown, 0x22},
            {Qt::Key_Insert, 0x2d},    {Qt::Key_Delete, 0x2e},        {Qt::Key_Return, 0x0d},
            {Qt::Key_Enter, 0x0d},     {Qt::Key_Tab, 0x09},           {Qt::Key_MediaTogglePlayPause, 0xb3},
            {Qt::Key_MediaNext, 0xb0}, {Qt::Key_MediaPrevious, 0xb1}, {Qt::Key_VolumeUp, 0xaf},
            {Qt::Key_VolumeDown, 0xae}};
        value = values.value(key);
    }
    return {flags, value};
}
bool ShortcutRules::validate(const QJsonObject &config, QString *error) {
    auto fail = [error](const QString &text) {
        if (error)
            *error = text;
        return false;
    };
    QSet<QString> seen;
    QStringList reserved{"Ctrl+O", "Ctrl+Shift+O", "Ctrl+F", "Esc"};
    reserved << QKeySequence(Qt::Key_MediaPlay).toString(QKeySequence::PortableText)
             << QKeySequence(Qt::Key_MediaPause).toString(QKeySequence::PortableText);
    for (const auto &scope : {QString("app"), QString("global")}) {
        auto map = config[scope].toObject();
        for (int i = 0; i < actionKeys().size(); ++i) {
            auto value = map[actionKeys()[i]];
            if (!value.isString())
                return fail("快捷键配置不完整，请恢复默认后重试。");
            auto sequence = QKeySequence::fromString(value.toString(), QKeySequence::PortableText);
            if (value.toString().isEmpty())
                continue;
            if (sequence.count() != 1 || sequence[0].key() == Qt::Key_unknown)
                return fail("每项只支持一个组合键：" + actionNames()[i]);
            auto text = sequence.toString(QKeySequence::PortableText);
            if (reserved.contains(text))
                return fail("此组合键已用于导入、搜索或返回：" + text);
            if (scope == "global") {
                auto native = nativeKey(sequence);
                if (!native.key || (native.modifiers & 8) || sequence[0].key() == Qt::Key_F12 ||
                    (!native.modifiers && native.key < 0x70))
                    return fail("全局快捷键请使用 Ctrl / Alt / Shift 组合、功能键或媒体键：" +
                                actionNames()[i]);
            }
            // Bindings within each scope must be unique even when global is off.
            auto token = scope + ':' + text;
            if (seen.contains(token))
                return fail("同一组中不能重复使用快捷键：" + text);
            seen.insert(token);
            if (config["globalEnabled"].toBool() && scope == "global" && seen.contains("app:" + text))
                return fail("应用内与已启用的全局快捷键不能相同：" + text);
        }
    }
    return true;
}
} // namespace relaxer
