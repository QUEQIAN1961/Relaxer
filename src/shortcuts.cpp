#include "shortcuts.h"
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
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
QStringList PlayerShortcuts::actionKeys() { return ShortcutRules::actionKeys(); }
QStringList PlayerShortcuts::actionNames() { return ShortcutRules::actionNames(); }
QJsonObject PlayerShortcuts::defaults() { return ShortcutRules::defaults(); }
NativeKey PlayerShortcuts::nativeKey(const QKeySequence &sequence) { return ShortcutRules::nativeKey(sequence); }
bool PlayerShortcuts::validate(const QJsonObject &config, QString *error) {
    return ShortcutRules::validate(config, error);
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
