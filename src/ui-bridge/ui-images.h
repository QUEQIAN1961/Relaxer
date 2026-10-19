#pragma once
#include <QQuickImageProvider>
#include <QCache>
#include <QMutex>
namespace relaxer {
class UiImages final : public QQuickImageProvider {
public:
    UiImages():QQuickImageProvider(Image){}
    QImage requestImage(const QString &id,QSize *size,const QSize &requested) override;
private:
    QCache<QString,QImage> m_cache{32768}; QMutex m_mutex;
};
}
