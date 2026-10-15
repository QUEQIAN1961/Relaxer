#include "shortcuts.h"
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMap>
#include <QPlainTextEdit>
#include <QSet>
#include <QTextEdit>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace relaxer {
namespace {
class NativeBackend : public GlobalKeyBackend {
  public:
    bool supported() const override {
#ifdef Q_OS_WIN
        return true;
#else
        return false;
#endif
    }
    bool add(int id, NativeKey key, bool repeat) override {
#ifdef Q_OS_WIN
        return RegisterHotKey(nullptr, id, key.modifiers | (repeat ? 0 : MOD_NOREPEAT), key.key);
#else
        Q_UNUSED(id);
        Q_UNUSED(key);
        Q_UNUSED(repeat);
        return false;
#endif
    }
    void remove(int id) override {
#ifdef Q_OS_WIN
        UnregisterHotKey(nullptr, id);
#else
        Q_UNUSED(id);
#endif
    }
};
bool editable(QWidget *widget) {
    for (auto *w = widget; w; w = w->parentWidget())
        if (qobject_cast<QLineEdit *>(w) || qobject_cast<QTextEdit *>(w) ||
            qobject_cast<QPlainTextEdit *>(w) || qobject_cast<QAbstractSpinBox *>(w) ||
            qobject_cast<QComboBox *>(w) || qobject_cast<QKeySequenceEdit *>(w))
            return true;
    return false;
}
bool recording(QWidget *widget) {
    for (auto *w = widget; w; w = w->parentWidget())
        if (qobject_cast<QKeySequenceEdit *>(w))
            return true;
    return false;
}
QKeySequence binding(const QJsonObject &config, const QString &scope, const QString &key) {
    return QKeySequence::fromString(config[scope].toObject()[key].toString(), QKeySequence::PortableText);
}
} // namespace
ShortcutEdit::ShortcutEdit(const QKeySequence &key, QWidget *parent) : QKeySequenceEdit(key, parent) {
    setClearButtonEnabled(true);
}
void ShortcutEdit::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
        QKeySequenceEdit::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift || event->key() == Qt::Key_Alt ||
        event->key() == Qt::Key_Meta || event->isAutoRepeat()) {
        event->accept();
        return;
    }
    if (event->key() != Qt::Key_unknown)
        setKeySequence(QKeySequence(QKeyCombination(event->modifiers(), Qt::Key(event->key()))));
    event->accept();
}
void ShortcutEdit::keyReleaseEvent(QKeyEvent *event) {
    event->accept();
}
QStringList PlayerShortcuts::actionKeys() {
    return {"toggle", "previous", "next", "volumeUp", "volumeDown"};
}
QStringList PlayerShortcuts::actionNames() {
    return {"播放 / 暂停", "上一首", "下一首", "音量增加", "音量减少"};
}
QJsonObject PlayerShortcuts::defaults() {
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
NativeKey PlayerShortcuts::nativeKey(const QKeySequence &sequence) {
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
bool PlayerShortcuts::validate(const QJsonObject &config, QString *error) {
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
PlayerShortcuts::PlayerShortcuts(QWidget *window, std::unique_ptr<GlobalKeyBackend> backend)
    : QObject(window), m_window(window),
      m_backend(backend ? std::move(backend) : std::make_unique<NativeBackend>()) {
    static int nextBase = 0x1000;
    m_base = nextBase;
    nextBase += 8;
    qApp->installNativeEventFilter(this);
    qApp->installEventFilter(this);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        bool active = recording(now);
        if (active == m_recording)
            return;
        m_recording = active;
        releaseGlobals();
        if (active)
            report("录入快捷键时，全局快捷键暂时停用。");
        else {
            QString error;
            if (!registerGlobals(m_config, &error))
                report(error);
            else
                report(m_config["globalEnabled"].toBool() ? "全局快捷键已启用。" : "全局快捷键未启用。");
        }
    });
    apply(defaults());
}
PlayerShortcuts::~PlayerShortcuts() {
    releaseGlobals();
    if (qApp) {
        qApp->removeEventFilter(this);
        qApp->removeNativeEventFilter(this);
    }
}
void PlayerShortcuts::report(const QString &text) {
    m_status = text;
    emit statusChanged(text);
}
void PlayerShortcuts::releaseGlobals() {
    for (int id : m_registered)
        m_backend->remove(id);
    m_registered.clear();
}
bool PlayerShortcuts::registerGlobals(const QJsonObject &config, QString *error) {
    if (!config["globalEnabled"].toBool() || m_recording)
        return true;
    if (!m_backend->supported()) {
        if (error)
            *error = "此系统不支持 Windows 全局快捷键。";
        return false;
    }
    for (int i = 0; i < actionKeys().size(); ++i) {
        auto sequence = binding(config, "global", actionKeys()[i]);
        if (sequence.isEmpty())
            continue;
        if (!m_backend->add(m_base + i, nativeKey(sequence), i >= 3)) {
            releaseGlobals();
            if (error)
                *error = "全局快捷键被系统或其他应用占用：" + sequence.toString(QKeySequence::NativeText) +
                         "。请更换后保存。";
            return false;
        }
        m_registered << m_base + i;
    }
    return true;
}
bool PlayerShortcuts::apply(const QJsonObject &config, QString *error) {
    QString failure;
    if (!validate(config, &failure)) {
        if (error)
            *error = failure;
        report(failure);
        return false;
    }
    releaseGlobals();
    if (!registerGlobals(config, &failure)) {
        QString rollback;
        if (!registerGlobals(m_config, &rollback))
            failure += "\n原全局快捷键也未能恢复，请重新保存。";
        if (error)
            *error = failure;
        report(failure);
        return false;
    }
    for (auto *shortcut : m_local)
        delete shortcut;
    m_local.clear();
    for (int i = 0; i < actionKeys().size(); ++i) {
        auto sequence = binding(config, "app", actionKeys()[i]);
        if (sequence.isEmpty())
            continue;
        auto *shortcut = new QShortcut(sequence, m_window);
        shortcut->setContext(Qt::WindowShortcut);
        shortcut->setAutoRepeat(i >= 3);
        connect(shortcut, &QShortcut::activated, this, [this, i] {
            if (!editable(qApp->focusWidget()))
                emit triggered(PlayerAction(i));
        });
        m_local << shortcut;
    }
    const QVector<Qt::Key> media{Qt::Key_MediaTogglePlayPause, Qt::Key_MediaPrevious, Qt::Key_MediaNext};
    for (int i = 0; i < media.size(); ++i) {
        QKeySequence sequence(media[i]);
        bool assigned = false;
        for (const auto &key : actionKeys())
            assigned |= binding(config, "app", key) == sequence ||
                        (config["globalEnabled"].toBool() && binding(config, "global", key) == sequence);
        if (assigned)
            continue;
        auto *shortcut = new QShortcut(sequence, m_window);
        shortcut->setContext(Qt::WindowShortcut);
        shortcut->setAutoRepeat(false);
        connect(shortcut, &QShortcut::activated, this, [this, i] {
            if (!recording(qApp->focusWidget()))
                emit triggered(PlayerAction(i));
        });
        m_local << shortcut;
    }
    m_config = config;
    report(config["globalEnabled"].toBool() ? "全局快捷键已启用，最小化或切换应用后仍可使用。"
                                            : "应用内快捷键已生效；全局快捷键未启用。");
    return true;
}
bool PlayerShortcuts::eventFilter(QObject *, QEvent *event) {
    if (event->type() == QEvent::ShortcutOverride && editable(qApp->focusWidget())) {
        event->accept();
        // Leave the widget its own ShortcutOverride, including combo Enter.
        return false;
    }
    return false;
}
bool PlayerShortcuts::dispatchGlobal(int id) {
    if (m_recording || !m_registered.contains(id))
        return false;
    emit triggered(PlayerAction(id - m_base));
    return true;
}
bool PlayerShortcuts::nativeEventFilter(const QByteArray &, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    auto *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY && dispatchGlobal(int(msg->wParam))) {
        if (result)
            *result = 0;
        return true;
    }
#else
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}
} // namespace relaxer
