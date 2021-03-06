#include "DebugOperatorNew.h"
#include <boost/algorithm/string.hpp>
#include <QList>

#include "AssetCache.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "Framework.h"
#include "LoggingFunctions.h"

#include <QUrl>
#include <QFile>
#include <QDataStream>
#include <QFileInfo>
#include <QScopedPointer>

#include "MemoryLeakCheck.h"

AssetCache::AssetCache(AssetAPI *owner, QString assetCacheDirectory) : 
#ifndef DISABLE_QNETWORKDISKCACHE
    QNetworkDiskCache(0),
#endif
    assetAPI(owner),
    cacheDirectory(GuaranteeTrailingSlash(QDir::fromNativeSeparators(assetCacheDirectory)))
{
    LogInfo("* Asset cache directory: " + cacheDirectory);  

    // Check that the main directory exists
    QDir assetDir(cacheDirectory);
    if (!assetDir.exists())
    {
        QString dirName = cacheDirectory.split("/", QString::SkipEmptyParts).last();
        QString parentPath = cacheDirectory;
        parentPath.chop(dirName.length()+1);
        QDir parentDir(parentPath);
        parentDir.mkdir(dirName);
    }

    // Check that the needed subfolders exist
    if (!assetDir.exists("data"))
        assetDir.mkdir("data");
    assetDataDir = QDir(cacheDirectory + "data");
    if (!assetDir.exists("metadata"))
        assetDir.mkdir("metadata");

#ifndef DISABLE_QNETWORKDISKCACHE
    assetMetaDataDir = QDir(cacheDirectory + "metadata");

    // Set for QNetworkDiskCache
    setCacheDirectory(cacheDirectory);
#endif

    // Check --clear-asset-cache start param
    if (owner->GetFramework()->HasCommandLineParameter("--clear-asset-cache"))
    {
        LogInfo("AssetCache: Removing all data and metadata files from cache, found 'clear-asset-cache' from start params!");
        ClearAssetCache();
    }
}

#ifndef DISABLE_QNETWORKDISKCACHE
QIODevice* AssetCache::data(const QUrl &url)
{
    QScopedPointer<QFile> dataFile;
    QString absoluteDataFile = GetAbsoluteFilePath(false, url);
    if (QFile::exists(absoluteDataFile))
    {
        dataFile.reset(new QFile(absoluteDataFile));
        if (!dataFile->open(QIODevice::ReadWrite))
        {
            dataFile.reset();
            return 0;
        }
    }
    // It is the callers responsibility to delete this ptr as said by the Qt docs.
    // This will most likely happen when QNetworkReply->deleteLater() is called, meaning next qt mainloop cycle from that call.
    return dataFile.take();
}

void AssetCache::insert(QIODevice* device)
{
    // We own this ptr from prepare()
    QHashIterator<QString, QFile*> it(preparedItems);
    while(it.hasNext())
    {
        it.next();
        if (it.value() == device)
        {
            preparedItems.remove(it.key());
            break;
        }
    }
    // Delete later, meaning next qt mainloop cycle, because the asset will 
    // use this ptr to deserialize the content to and IAsset after this call return.
    device->close();
    device->deleteLater();
}

QIODevice* AssetCache::prepare(const QNetworkCacheMetaData &metaData)
{
    if (!WriteMetadata(GetAbsoluteFilePath(true, metaData.url()), metaData))
        return 0;
    QScopedPointer<QFile> dataFile(new QFile(GetAbsoluteFilePath(false, metaData.url())));
    if (!dataFile->open(QIODevice::ReadWrite))
    {
        LogError("AssetCache: Failed not open data file QIODevice::ReadWrite mode for " + metaData.url().toString().toStdString());
        dataFile.reset();
        remove(metaData.url());
        return 0;
    }
    if (dataFile->bytesAvailable() > 0)
    {
        if (!dataFile->resize(0))
        {
            LogError("AssetCache: Failed not reset existing data from cache entry. Skipping cache store for " + metaData.url().toString().toStdString());
            dataFile->close();
            dataFile.reset();
            remove(metaData.url());
            return 0;
        }
    }
    // Take ownership of the ptr
    QFile *dataPtr = dataFile.take();
    preparedItems[metaData.url().toString()] = dataPtr;
    return dataPtr;
}

bool AssetCache::remove(const QUrl &url)
{
    // remove() is also used for canceling insertion after prepare()
    // we need to delete the QFile* ptr also in these cases
    // note: this is not a common operation
    QHashIterator<QString, QFile*> it(preparedItems);
    while(it.hasNext())
    {
        it.next();
        if (it.key() == url.toString())
        {
            delete it.value();
            preparedItems.remove(it.key());
            break;
        }
    }

    // Remove the actual files related to this url
    bool success = true;
    QString absoluteMetaDataFile = GetAbsoluteFilePath(true,url);
    if (QFile::exists(absoluteMetaDataFile))
        success = QFile::remove(absoluteMetaDataFile);
    if (!success)
        return false;
    QString absoluteDataFile = GetAbsoluteFilePath(false, url);
    if (QFile::exists(absoluteDataFile))
        success = QFile::remove(absoluteDataFile);
    return success;
}

QNetworkCacheMetaData AssetCache::metaData(const QUrl &url)
{
    QNetworkCacheMetaData resultMetaData;
    QString absoluteMetaDataFile = GetAbsoluteFilePath(true, url);
    if (QFile::exists(absoluteMetaDataFile))
    {
        QFile metaDataFile(absoluteMetaDataFile);
        if (metaDataFile.open(QIODevice::ReadOnly))
        {
            QDataStream metaDataStream(&metaDataFile);
            metaDataStream >> resultMetaData;
            metaDataFile.close();
        }
    }
    return resultMetaData;
}

void AssetCache::updateMetaData(const QNetworkCacheMetaData &metaData)
{
    const QNetworkCacheMetaData oldMetaData = this->metaData(metaData.url());
    if (oldMetaData.isValid())
        if (oldMetaData != metaData)
            WriteMetadata(GetAbsoluteFilePath(true, metaData.url()), metaData);
}

void AssetCache::clear()
{
    ClearAssetCache();
}

qint64 AssetCache::expire()
{
    // Skip keeping cache at some static size, unlimited for now.
    return maximumCacheSize() / 2;
}
#endif

QString AssetCache::FindInCache(const QString &assetRef)
{
    // Deny http:// and https:// asset references to be gotten from cache
    // as the QAccessManager will request it from the overrides above later!
    // You can get the path if you ask directly as a url.
    if (assetRef.startsWith("http://") || assetRef.startsWith("https://")) ///\todo Remove this. The Asset Cache needs to be protocol agnostic. -jj.
        return "";

    QString absolutePath = assetDataDir.absolutePath() + "/" + AssetAPI::SanitateAssetRef(assetRef);
    if (QFile::exists(absolutePath))
        return absolutePath;
    return "";
}

QString AssetCache::GetDiskSourceByRef(const QString &assetRef)
{
    QString absolutePath = assetDataDir.absolutePath() + "/" + AssetAPI::SanitateAssetRef(assetRef);
    if (QFile::exists(absolutePath))
        return absolutePath;
    return "";
}

QString AssetCache::GetCacheDirectory() const
{
    return GuaranteeTrailingSlash(assetDataDir.absolutePath());
}

QString AssetCache::StoreAsset(AssetPtr asset)
{
    std::vector<u8> data;
    asset->SerializeTo(data);
    return StoreAsset(&data[0], data.size(), asset->Name());
}

QString AssetCache::StoreAsset(const u8 *data, size_t numBytes, const QString &assetName)
{
    QString absolutePath = GetAbsoluteDataFilePath(assetName);
#ifdef DISABLE_QNETWORKDISKCACHE // Don't store duplicate assets, but only if not using QNetworkDiskCache.
    if (QFile::exists(absolutePath))
        return absolutePath;
#endif
    bool success = SaveAssetFromMemoryToFile(data, numBytes, absolutePath.toStdString().c_str());
    if (success)
        return absolutePath;
    return "";
}

void AssetCache::DeleteAsset(const QString &assetRef)
{
    DeleteAsset(QUrl(assetRef, QUrl::TolerantMode));
}

void AssetCache::DeleteAsset(const QUrl &assetUrl)
{
#ifndef DISABLE_QNETWORKDISKCACHE
    if (!remove(assetUrl))
        LogWarning("AssetCache: AssetCache::DeleteAsset Failed to delete asset " + assetUrl.toString().toStdString());
#endif
}

void AssetCache::ClearAssetCache()
{
    ClearDirectory(assetDataDir.absolutePath());
#ifndef DISABLE_QNETWORKDISKCACHE
    ClearDirectory(assetMetaDataDir.absolutePath());
#endif
}

#ifndef DISABLE_QNETWORKDISKCACHE
bool AssetCache::WriteMetadata(const QString &filePath, const QNetworkCacheMetaData &metaData)
{
    QFile metaDataFile(filePath);
    if (!metaDataFile.open(QIODevice::WriteOnly))
    {
        LogError("AssetCache::WriteMetadata Could not open metadata file: " + filePath);
        return false;
    }
    if (!metaDataFile.resize(0))
    {
        LogError("AssetCache::WriteMetadata Could not reset metadata file: " + filePath);
        return false;
    }

    QDataStream metaDataStream(&metaDataFile);
    metaDataStream << metaData;
    metaDataFile.close();
    return true;
}
#endif

QString AssetCache::GetAbsoluteFilePath(bool isMetaData, const QUrl &url)
{
    QString subDir = isMetaData ? "metadata" : "data";
    QDir assetDir(cacheDirectory + subDir);
    QString absolutePath = assetDir.absolutePath() + "/" + AssetAPI::SanitateAssetRef(url.toString());
    if (isMetaData)
        absolutePath.append(".metadata");
    return absolutePath;
}

QString AssetCache::GetAbsoluteDataFilePath(const QString &filename)
{
    return assetDataDir.absolutePath() + "/" + AssetAPI::SanitateAssetRef(filename);
}

void AssetCache::ClearDirectory(const QString &absoluteDirPath)
{
    QDir targetDir(absoluteDirPath);
    if (!targetDir.exists())
    {
        LogWarning("AssetCache::ClearDirectory called with non existing directory path.");
        return;
    }
    QFileInfoList entries = targetDir.entryInfoList(QDir::Files|QDir::NoSymLinks|QDir::NoDotAndDotDot);
    foreach(QFileInfo entry, entries)
    {
        if (entry.isFile())
        {
            if (!targetDir.remove(entry.fileName()))
                LogWarning("AssetCache::ClearDirectory could not remove file " + entry.absoluteFilePath());
        }
    }
}
