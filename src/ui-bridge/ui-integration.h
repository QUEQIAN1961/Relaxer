#pragma once
#include "library.h"
#include "playback-bridge.h"
#include <QAbstractNativeEventFilter>
#include <QQuickWindow>
#include <QKeySequence>
#include <QSystemTrayIcon>
#include <QVariantMap>
#include <QMenu>
#include <memory>
namespace relaxer {
class UiIntegration final:public QObject,public QAbstractNativeEventFilter {
    Q_OBJECT
    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
    Q_PROPERTY(QString shortcutStatus READ shortcutStatus NOTIFY changed)
public:
    UiIntegration(Library *library,PlaybackBridge *playback,QObject *parent=nullptr);
    ~UiIntegration() override;
    void attach(QQuickWindow *window);
    bool trayAvailable()const;
    QString shortcutStatus()const{return m_status;}
    Q_INVOKABLE void showTray();
    Q_INVOKABLE bool saveShortcuts(const QVariantMap &configuration);
    Q_INVOKABLE void setShortcutRecording(bool recording);
    Q_INVOKABLE void saveMiniWindowPlacement();
    Q_INVOKABLE void restoreMiniWindowPlacement();
    Q_INVOKABLE QPoint clampWindowPosition(int x,int y,int width,int height) const;
    Q_INVOKABLE QPointF pointerPosition() const { return m_pointerPosition; }
    Q_INVOKABLE void setWindowGeometry(qreal x,qreal y,qreal width,qreal height);
    Q_INVOKABLE void beginResizeAnchor(int edges);
    Q_INVOKABLE void endResizeAnchor();
    bool eventFilter(QObject *,QEvent *)override;
    bool nativeEventFilter(const QByteArray &,void *,qintptr *)override;
signals:
    void changed();
    void miniPlayerToggleRequested();
private:
    QPointer<Library> m_library;QPointer<PlaybackBridge> m_playback;QPointer<QQuickWindow> m_window;
    std::unique_ptr<QMenu> m_menu;std::unique_ptr<QSystemTrayIcon> m_tray;
    struct Binding{QKeySequence sequence;int action;bool media;};QList<Binding> m_bindings;QList<int> m_registered;QJsonObject m_config;
    QString m_status;bool m_recording=false;int m_base=0;
    QVariantList m_miniPlacement;
    QKeySequence m_miniSequence;
    QPointF m_pointerPosition;
    int m_resizeEdges=0;
    WId m_resizeWindow=0;
    QPoint m_nativeResizeAnchor;
    void dispatch(int action);bool apply(const QJsonObject &configuration,QString *error);void releaseGlobals();bool registerGlobals(const QJsonObject &,QString *);
    bool editableFocus()const;
    void updateWindowShadow();

};
}
