#pragma once
#include <QQuickImageProvider>
#include <QCache>
#include <QMutex>
#include <QMutexLocker>
namespace relaxer {
class UiImages final : public QQuickImageProvider {
public:
    UiImages():QQuickImageProvider(Image){}
    QImage requestImage(const QString &id,QSize *size,const QSize &requested) override;
    void clearCache() { QMutexLocker lock(&m_mutex); m_cache.clear(); }
private:
    QCache<QString,QImage> m_cache{32768}; QMutex m_mutex;
};
}
