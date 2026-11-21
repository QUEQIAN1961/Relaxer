#include "ui-graphics-policy.h"
#include <QQuickWindow>
#include <QQuickGraphicsConfiguration>
#include <QQuickGraphicsDevice>
#include <QPointer>
#include <QRunnable>
#include <QTimer>
#ifdef Q_OS_WIN
#include <d3d11.h>
#include <dxgi1_6.h>
#endif

namespace relaxer {
#ifdef Q_OS_WIN
namespace {
template<typename T> struct ComHandle {
    T *value=nullptr;
    ~ComHandle(){if(value)value->Release();}
    ComHandle()=default;
    ComHandle(const ComHandle &)=delete;
    ComHandle &operator=(const ComHandle &)=delete;
};
void preferPerformanceAdapter(QQuickWindow *window){
    // Respect explicit diagnostic/device choices; enumerate metadata only.
    // Qt owns the device; the existing hidden-window policy releases graphics.
    if(qEnvironmentVariableIsSet("QT_D3D_ADAPTER_INDEX")||qEnvironmentVariableIsSet("QT_D3D_ADAPTER_LUID"))return;
    ComHandle<IDXGIFactory6> factory;
    if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory6),reinterpret_cast<void **>(&factory.value))))return;
    ComHandle<IDXGIAdapter1> adapter;
    if(FAILED(factory.value->EnumAdapterByGpuPreference(0,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,__uuidof(IDXGIAdapter1),reinterpret_cast<void **>(&adapter.value))))return;
    DXGI_ADAPTER_DESC1 description{};
    if(FAILED(adapter.value->GetDesc1(&description))||(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))return;
    window->setGraphicsDevice(QQuickGraphicsDevice::fromAdapter(description.AdapterLuid.LowPart,description.AdapterLuid.HighPart));
}
void trimDriverCaches(QQuickWindow *window){
    // NoStage runs on the render thread without requesting another frame. Trim
    // only discards driver-owned caches, never live textures, the scene graph,
    // video sinks or audio buffers. No process/device/clock priority is pinned.
    const QPointer<QQuickWindow> guarded=window;
    window->scheduleRenderJob(QRunnable::create([guarded]{
        if(!guarded)return;
        auto *renderer=guarded->rendererInterface();
        if(!renderer||renderer->graphicsApi()!=QSGRendererInterface::Direct3D11)return;
        auto *device=static_cast<ID3D11Device *>(renderer->getResource(guarded,QSGRendererInterface::DeviceResource));
        if(!device)return;
        ComHandle<IDXGIDevice3> dxgi;
        if(SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice3),reinterpret_cast<void **>(&dxgi.value))))dxgi.value->Trim();
    }),QQuickWindow::NoStage);
}
void monitorGraphicsIdle(QQuickWindow *window){
    if(window->findChild<QTimer *>(QStringLiteral("uiGraphicsIdleTrim"),Qt::FindDirectChildrenOnly))return;
    auto *idle=new QTimer(window);idle->setObjectName("uiGraphicsIdleTrim");idle->setSingleShot(true);idle->setInterval(2000);
    QObject::connect(window,&QQuickWindow::frameSwapped,idle,[window,idle]{if(window->isExposed())idle->start();});
    QObject::connect(window,&QQuickWindow::sceneGraphInvalidated,idle,&QTimer::stop);
    QObject::connect(window,&QWindow::visibilityChanged,idle,[idle](QWindow::Visibility state){if(state==QWindow::Hidden||state==QWindow::Minimized)idle->stop();});
    QObject::connect(idle,&QTimer::timeout,window,[window]{if(window->isExposed()&&window->isSceneGraphInitialized())trimDriverCaches(window);});
}
}
#endif
void configureUiGraphics(QQuickWindow *window,bool accelerated){
    if(!window||window->isSceneGraphInitialized())return;
#ifdef Q_OS_WIN
    if(!accelerated){
        auto configuration=window->graphicsConfiguration();configuration.setPreferSoftwareDevice(true);window->setGraphicsConfiguration(configuration);return;
    }
    if(window->rendererInterface()->graphicsApi()!=QSGRendererInterface::Direct3D11||window->graphicsConfiguration().prefersSoftwareDevice())return;
    preferPerformanceAdapter(window);
    monitorGraphicsIdle(window);
#else
    Q_UNUSED(accelerated)
#endif
}
}
