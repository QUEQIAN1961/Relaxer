#pragma once
#include "shortcut-rules.h"
#include <QAbstractNativeEventFilter>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <QShortcut>
#include <memory>

namespace relaxer {
enum class PlayerAction { Toggle, Previous, Next, VolumeUp, VolumeDown };
class GlobalKeyBackend {
  public:
    virtual ~GlobalKeyBackend() = default;
    virtual bool supported() const = 0;
    virtual bool add(int id, NativeKey key, bool repeat) = 0;
    virtual void remove(int id) = 0;
};
class ShortcutEdit : public QKeySequenceEdit {
    Q_OBJECT
  public:
    explicit ShortcutEdit(const QKeySequence &key, QWidget *parent = nullptr);

  protected:
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
};
class PlayerShortcuts : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
  public:
    explicit PlayerShortcuts(QWidget *window, std::unique_ptr<GlobalKeyBackend> backend = {});
    ~PlayerShortcuts() override;
    static QJsonObject defaults();
    static QStringList actionKeys();
    static QStringList actionNames();
    static bool validate(const QJsonObject &, QString *error = nullptr);
    static NativeKey nativeKey(const QKeySequence &);
    bool apply(const QJsonObject &, QString *error = nullptr);
    QJsonObject config() const { return m_config; }
    bool globalSupported() const { return m_backend->supported(); }
    QString status() const { return m_status; }
    bool dispatchGlobal(int id);
    bool nativeEventFilter(const QByteArray &, void *, qintptr *) override;
  signals:
    void triggered(relaxer::PlayerAction action);
    void statusChanged(const QString &);

  protected:
    bool eventFilter(QObject *, QEvent *) override;

  private:
    QWidget *m_window;
    std::unique_ptr<GlobalKeyBackend> m_backend;
    QJsonObject m_config;
    QVector<QShortcut *> m_local;
    QVector<int> m_registered;
    int m_base;
    bool m_recording = false;
    QString m_status;
    void releaseGlobals();
    bool registerGlobals(const QJsonObject &, QString *error);
    void report(const QString &);
};
} // namespace relaxer
Q_DECLARE_METATYPE(relaxer::PlayerAction)
