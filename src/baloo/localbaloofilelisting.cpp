/*
   SPDX-FileCopyrightText: 2016 (c) Matthieu Gallien <matthieu_gallien@yahoo.fr>

   SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "localbaloofilelisting.h"

#include "baloo/baloocommon.h"

#include "elisa_settings.h"
#include "elisautils.h"

#include "baloo/scheduler.h"
#include "baloo/fileindexer.h"
#include "baloo/main.h"

#include "baloowatcherapplicationadaptor.h"

#include "filescanner.h"

#include <Baloo/Query>
#include <Baloo/File>
#include <Baloo/IndexerConfig>



#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>

#include <QThread>
#include <QFileInfo>
#include <QAtomicInt>
#include <QScopedPointer>

#include <algorithm>

class LocalBalooFileListingPrivate
{
public:

    Baloo::Query mQuery;

    QDBusServiceWatcher mServiceWatcher;

    QScopedPointer<org::kde::baloo::main> mBalooMainInterface;

    QScopedPointer<org::kde::baloo::fileindexer> mBalooIndexer;

    QScopedPointer<org::kde::baloo::scheduler> mBalooScheduler;

    BalooWatcherApplicationAdaptor *mDbusAdaptor = nullptr;

    QAtomicInt mStopRequest = 0;

    bool mIsRegisteredToBaloo = false;

    bool mIsRegisteringToBaloo = false;

    bool mIsRegisteredToBalooWatcher = false;

    bool mIsRegisteringToBalooWatcher = false;

};

LocalBalooFileListing::LocalBalooFileListing(QObject *parent)
    : AbstractFileListing(parent), d(std::make_unique<LocalBalooFileListingPrivate>())
{
    d->mQuery.addType(QStringLiteral("Audio"));
    setHandleNewFiles(false);

    auto sessionBus = QDBusConnection::sessionBus();

    d->mDbusAdaptor = new BalooWatcherApplicationAdaptor(this);

    sessionBus.registerObject(QStringLiteral("/org/kde/BalooWatcherApplication"), d->mDbusAdaptor, QDBusConnection::ExportAllContents);

    connect(&d->mServiceWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, &LocalBalooFileListing::serviceRegistered);
    connect(&d->mServiceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &LocalBalooFileListing::serviceOwnerChanged);
    connect(&d->mServiceWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &LocalBalooFileListing::serviceUnregistered);

    d->mServiceWatcher.setConnection(sessionBus);
    d->mServiceWatcher.addWatchedService(QStringLiteral("org.kde.baloo"));

    if (sessionBus.interface()->isServiceRegistered(QStringLiteral("org.kde.baloo"))) {
        registerToBaloo();
    }
}

LocalBalooFileListing::~LocalBalooFileListing()
{
    d->mDbusAdaptor->setParent(nullptr);
    d->mDbusAdaptor->deleteLater();
}

void LocalBalooFileListing::applicationAboutToQuit()
{
    AbstractFileListing::applicationAboutToQuit();
    d->mStopRequest = 1;
}

bool isSubPath(const QString &subPath, const QString &parentPath)
{
    const auto parentUrl = QUrl::fromLocalFile(parentPath);
    const auto childUrl = QUrl::fromLocalFile(subPath);
    return parentUrl == childUrl || parentUrl.isParentOf(childUrl);
}

QList<QString> filterParentPaths(const QString &child, const QList<QString> &paths)
{
    const auto predicate = [&child](const QString &path) {return isSubPath(child, path);};
    QList<QString> parents;
    std::copy_if(paths.cbegin(), paths.cend(), std::back_inserter(parents), predicate);
    return parents;
}

QList<QString> filterChildPaths(const QString &parent, const QList<QString> &paths)
{
    const auto predicate = [&parent](const QString &path) {return isSubPath(path, parent);};
    QList<QString> children;
    std::copy_if(paths.cbegin(), paths.cend(), std::back_inserter(children), predicate);
    return children;
}

// Assumes all of `paths` are either parents or children of each other
// E.g. deepestPath({"/path/to/my/files", "/path/to/my"}) == "/path/to/my/files"
QString deepestPath(const QList<QString> &paths)
{
    const auto op = [](const QString &acc, const QString &path) {return isSubPath(acc, path) ? acc : path;};
    return std::reduce(++paths.cbegin(), paths.cend(), paths.first(), op);
}

bool canHandlePath(const QString &path, const QList<QString> &includePaths, const QList<QString> &excludePaths)
{
    const auto parentIncludePaths = filterParentPaths(path, includePaths);
    if (parentIncludePaths.isEmpty()) {
        return false;
    }
    // E.g. we want to scan `/home/music` but the path `/home/music/album` is excluded
    const auto childExcludePaths = filterChildPaths(path, excludePaths);
    if (!childExcludePaths.isEmpty()) {
        return false;
    }
    const auto parentExcludePaths = filterParentPaths(path, excludePaths);
    if (parentExcludePaths.isEmpty()) {
        return true;
    }

    const auto closestIncludePath = deepestPath(parentIncludePaths);
    const auto closestExcludePath = deepestPath(parentExcludePaths);
    return isSubPath(closestIncludePath, closestExcludePath);
}

bool LocalBalooFileListing::canHandleRootPaths() const
{
    const Baloo::IndexerConfig balooConfiguration;
    const auto includePaths = balooConfiguration.includeFolders();
    const auto excludePaths = balooConfiguration.excludeFolders();

    return std::all_of(allRootPaths().cbegin(), allRootPaths().cend(),
        [&includePaths, &excludePaths](const auto &path) {
            return canHandlePath(path, includePaths, excludePaths);
        }
    );
}

void LocalBalooFileListing::newBalooFile(const QString &fileName)
{
    qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::newBalooFile" << fileName;

    if (!isActive()) {
        qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::newBalooFile is inactive";
        return;
    }

    auto scanFileInfo = QFileInfo(fileName);

    if (!scanFileInfo.exists()) {
        return;
    }

    if (!fileScanner().shouldScanFile(fileName)) {
        return;
    }

    Q_EMIT indexingStarted();

    auto newFile = QUrl::fromLocalFile(fileName);

    auto newTrack = scanOneFile(newFile, scanFileInfo, DoNotWatchFileSystemChanges);

    if (newTrack.isValid()) {
        QFileInfo newFileInfo(fileName);

        addFileInDirectory(newFile, QUrl::fromLocalFile(newFileInfo.absoluteDir().absolutePath()), DoNotWatchFileSystemChanges);

        emitNewFiles({newTrack});
    }

    Q_EMIT indexingFinished();
}

void LocalBalooFileListing::registeredToBaloo(QDBusPendingCallWatcher *watcher)
{
    qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::registeredToBaloo";

    if (!watcher) {
        return;
    }

    QDBusPendingReply<> reply = *watcher;
    if (reply.isError()) {
        qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::registeredToBaloo" << reply.error().name() << reply.error().message();
        d->mIsRegisteredToBaloo = false;
    } else {
        d->mIsRegisteredToBaloo = true;
    }

    d->mIsRegisteringToBaloo = false;

    watcher->deleteLater();
}

void LocalBalooFileListing::registeredToBalooWatcher(QDBusPendingCallWatcher *watcher)
{
    qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::registeredToBalooWatcher";

    if (!watcher) {
        return;
    }

    QDBusPendingReply<> reply = *watcher;
    if (reply.isError()) {
        qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::registeredToBalooWatcher" << reply.error().name() << reply.error().message();
        d->mIsRegisteredToBalooWatcher = false;
    } else {
        d->mIsRegisteredToBalooWatcher = true;
    }

    d->mIsRegisteringToBalooWatcher = false;

    watcher->deleteLater();
}

void LocalBalooFileListing::registerToBaloo()
{
    if (d->mIsRegisteringToBaloo || d->mIsRegisteringToBalooWatcher) {
        qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::registerToBaloo" << "already registering";
        return;
    }

    qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::registerToBaloo";

    d->mIsRegisteringToBaloo = true;
    d->mIsRegisteringToBalooWatcher = true;

    auto sessionBus = QDBusConnection::sessionBus();

    d->mBalooMainInterface.reset(new org::kde::baloo::main(QStringLiteral("org.kde.baloo"), QStringLiteral("/"),
                                                           sessionBus, this));

    if (!d->mBalooMainInterface->isValid()) {
        qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::registerToBaloo" << "invalid org.kde.baloo/main interface";
        return;
    }

    d->mBalooIndexer.reset(new org::kde::baloo::fileindexer(QStringLiteral("org.kde.baloo"), QStringLiteral("/fileindexer"),
                                                            sessionBus, this));

    if (!d->mBalooIndexer->isValid()) {
        qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::registerToBaloo" << "invalid org.kde.baloo/fileindexer interface";
        return;
    }

    connect(d->mBalooIndexer.data(), &org::kde::baloo::fileindexer::finishedIndexingFile,
            this, &LocalBalooFileListing::newBalooFile);

    d->mBalooScheduler.reset(new org::kde::baloo::scheduler(QStringLiteral("org.kde.baloo"), QStringLiteral("/scheduler"),
                                                            sessionBus, this));

    if (!d->mBalooScheduler->isValid()) {
        qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::registerToBaloo" << "invalid org.kde.baloo/scheduler interface";
        return;
    }

    qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::registerToBaloo" << "call registerMonitor";
    auto answer = d->mBalooIndexer->registerMonitor();

    if (answer.isError()) {
        qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::executeInit" << answer.error().name() << answer.error().message();
    }

    auto pendingCallWatcher = new QDBusPendingCallWatcher(answer);

    connect(pendingCallWatcher, &QDBusPendingCallWatcher::finished, this, &LocalBalooFileListing::registeredToBaloo);
    if (pendingCallWatcher->isFinished()) {
        registeredToBaloo(pendingCallWatcher);
    }
}

void LocalBalooFileListing::renamedFiles(const QString &from, const QString &to, const QStringList &listFiles)
{
    qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::renamedFiles" << from << to << listFiles;
}

void LocalBalooFileListing::serviceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner);
    Q_UNUSED(newOwner);

    qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::serviceOwnerChanged" << serviceName << oldOwner << newOwner;

    if (serviceName == QLatin1String("org.kde.baloo") && !newOwner.isEmpty()) {
        d->mIsRegisteredToBaloo = false;
        d->mIsRegisteredToBalooWatcher = false;
        registerToBaloo();
    }
}

void LocalBalooFileListing::serviceRegistered(const QString &serviceName)
{
    qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::serviceRegistered" << serviceName;

    if (serviceName == QLatin1String("org.kde.baloo")) {
        registerToBaloo();
    }
}

void LocalBalooFileListing::serviceUnregistered(const QString &serviceName)
{
    qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::serviceUnregistered" << serviceName;

    if (serviceName == QLatin1String("org.kde.baloo")) {
        d->mIsRegisteredToBaloo = false;
        d->mIsRegisteredToBalooWatcher = false;
    }
}

void LocalBalooFileListing::executeInit(QHash<QUrl, QDateTime> allFiles)
{
    if (!isActive()) {
        qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::executeInit is inactive";
        return;
    }

    qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::executeInit" << "with" << allFiles.size() << "files";
    AbstractFileListing::executeInit(std::move(allFiles));
}

void LocalBalooFileListing::triggerRefreshOfContent()
{
    qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::triggerRefreshOfContent";

    if (!isActive()) {
        qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::triggerRefreshOfContent is inactive";
        return;
    }

    Q_EMIT indexingStarted();

    AbstractFileListing::triggerRefreshOfContent();

    const auto &rootPaths = allRootPaths();
    bool hasSingleRootPath = (rootPaths.size() == 1);
    auto singleRootPath = rootPaths.at(0);

    auto resultIterator = d->mQuery.exec();
    auto newFiles = DataTypes::ListTrackDataType();

    while(resultIterator.next() && d->mStopRequest == 0) {
        const auto &fileName = resultIterator.filePath();

        if (hasSingleRootPath) {
            if (!fileName.startsWith(singleRootPath)) {
                qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::triggerRefreshOfContent" << fileName << "does not match root paths";
                continue;
            }
        } else {
            bool isIncluded = false;
            for (const auto &oneRootPath : rootPaths) {
                if (fileName.startsWith(oneRootPath)) {
                    isIncluded = true;
                    break;
                }
            }
            if (!isIncluded) {
                qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::triggerRefreshOfContent" << fileName << "does not match root paths";
                continue;
            }
        }

        const auto &newFileUrl = QUrl::fromLocalFile(resultIterator.filePath());

        auto scanFileInfo = QFileInfo(fileName);

        if (!scanFileInfo.exists()) {
            qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::triggerRefreshOfContent" << fileName << "file does not exists";
            continue;
        }

        auto itExistingFile = allFiles().find(newFileUrl);
        if (itExistingFile != allFiles().end()) {
            if (*itExistingFile >= scanFileInfo.metadataChangeTime()) {
                allFiles().erase(itExistingFile);
                qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::triggerRefreshOfContent" << fileName << "file not modified since last scan";
                continue;
            }
        }

        const auto currentDirectory = QUrl::fromLocalFile(scanFileInfo.absoluteDir().absolutePath());

        addFileInDirectory(newFileUrl, currentDirectory, DoNotWatchFileSystemChanges);

        const auto &newTrack = scanOneFile(newFileUrl, scanFileInfo, DoNotWatchFileSystemChanges);

        if (newTrack.isValid()) {
            newFiles.push_back(newTrack);
            if (newFiles.size() > 500 && d->mStopRequest == 0) {
                qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::triggerRefreshOfContent" << "insert new tracks in database" << newFiles.count();
                emitNewFiles(newFiles);
                newFiles.clear();
            }
        } else {
            qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::triggerRefreshOfContent" << fileName << "invalid track" << newTrack;
        }
    }

    if (!newFiles.isEmpty() && d->mStopRequest == 0) {
        qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::triggerRefreshOfContent" << "insert new tracks in database" << newFiles.count();
        emitNewFiles(newFiles);
    }

    setWaitEndTrackRemoval(false);

    checkFilesToRemove();

    if (!waitEndTrackRemoval()) {
        Q_EMIT indexingFinished();
    }
}

void LocalBalooFileListing::triggerStop()
{
    qCDebug(orgKdeElisaBaloo()) << "LocalBalooFileListing::triggerStop";
    AbstractFileListing::triggerStop();
}

DataTypes::TrackDataType LocalBalooFileListing::scanOneFile(const QUrl &scanFile, const QFileInfo &scanFileInfo, FileSystemWatchingModes watchForFileSystemChanges)
{

    auto trackData = fileScanner().scanOneBalooFile(scanFile, scanFileInfo);

    if (!trackData.isValid()) {
        qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::scanOneFile" << scanFile << "falling back to plain file metadata analysis";
        trackData = AbstractFileListing::scanOneFile(scanFile, scanFileInfo, watchForFileSystemChanges);
    }

    if (trackData.isValid()) {
        addCover(trackData);
    } else {
        qCDebug(orgKdeElisaBaloo) << "LocalBalooFileListing::scanOneFile" << scanFile << "invalid track";
    }

    return trackData;
}

#include "moc_localbaloofilelisting.cpp"
