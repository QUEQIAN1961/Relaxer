#pragma once
#include "library.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <QMutex>
#include <QMutexLocker>
namespace relaxer {
// Only generated cache files are managed. Online account/session files and
// downloaded music remain outside this list, including in older data folders.
class DiskCache {
public:
    inline static QSet<QString> activeFiles;
    inline static QMutex activeMutex;
    static void markActive(const QString &path){QMutexLocker lock(&activeMutex);activeFiles.insert(path);}
    static void releaseActive(const QString &path){QMutexLocker lock(&activeMutex);activeFiles.remove(path);}
    static bool isActive(const QString &path){QMutexLocker lock(&activeMutex);return activeFiles.contains(path);}
    static QString root(const Library *library){return QDir(library->root()).filePath("cache");}
    static QString online(const Library *library){const auto path=QDir(root(library)).filePath("online");QDir().mkpath(path);return path;}
    static QList<QFileInfo> legacyFiles(const Library *library){const auto folder=QDir(library->root()).filePath("Online");const QFileInfo info(folder);if(info.isSymLink()||info.isJunction())return {};return QDir(folder).entryInfoList({"thumbnail-*.png","cover-*.png","qq-cover-*.png","kugou-*.png","qishui-*.png","catalog-*.json","recommendations.json","qq-recommendations.json"},QDir::Files|QDir::NoSymLinks);}
    static QList<QFileInfo> files(const Library *library){
        QList<QFileInfo> entries;
        for(const auto &folder:{root(library),QDir(root(library)).filePath("online")}){const QFileInfo info(folder);if(!info.isSymLink()&&!info.isJunction())entries.append(QDir(folder).entryInfoList(QDir::Files|QDir::NoSymLinks));}
        entries.append(legacyFiles(library));
        return entries;
    }
    static qint64 usage(const Library *library){qint64 total=0;for(const auto &file:files(library))total+=file.size();return total;}
    static qint64 trim(const Library *library,const QSet<QString> &protectedFiles={},qint64 limit=-1){
        if(limit<0)limit=qint64(qBound(256,library->setting("cacheLimitMB",1024).toInt(),40960))*1024*1024;
        auto entries=files(library);qint64 total=0;for(const auto &file:entries)total+=file.size();
        if(total<=limit)return total;
        std::sort(entries.begin(),entries.end(),[](const auto &a,const auto &b){return a.lastModified()<b.lastModified();});
        for(const auto &file:entries){if(total<=limit)break;if(!protectedFiles.contains(file.absoluteFilePath())&&!isActive(file.absoluteFilePath())&&QFile::remove(file.absoluteFilePath()))total-=file.size();}
        return total;
    }
};
}
