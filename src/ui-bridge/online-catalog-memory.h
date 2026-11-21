#pragma once
#include <QHash>
#include <QSet>
#include <QVariantMap>
namespace relaxer {
// The active page and PlaybackFacade own the full live data. These lookup maps
// need only the visible rows plus a small reserve; old pages restore from cache.
template<typename Key>
void trimCatalogMap(QHash<Key,QVariantMap> &rows,const QSet<Key> &visible,int reserve=256){
    const auto limit=visible.size()+reserve;
    for(auto it=rows.begin();rows.size()>limit&&it!=rows.end();)
        if(!visible.contains(it.key()))it=rows.erase(it);else ++it;
}
}
