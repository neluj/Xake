#include "application_data.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>

namespace {

enum class FileCategory {
    Records,
    Pgn,
    CommunicationLog,
    Other
};

constexpr QDir::Filters kStoredFileFilters =
    QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks;
constexpr QDir::Filters kStoredDirectoryFilters =
    QDir::Dirs | QDir::Hidden | QDir::System
    | QDir::NoDotAndDotDot | QDir::NoSymLinks;

QString safeDataDirectory(const QString& dataDirectory)
{
    if (dataDirectory.trimmed().isEmpty()
        || !QDir::isAbsolutePath(dataDirectory)) {
        return {};
    }

    const QString path =
        QDir::cleanPath(QFileInfo(dataDirectory).absoluteFilePath());
    const QDir directory(path);
    if (directory.isRoot()
        || path.compare(QDir::cleanPath(QDir::homePath()),
                        Qt::CaseInsensitive) == 0) {
        return {};
    }
    return path;
}

FileCategory fileCategory(const QFileInfo& fileInfo)
{
    const QString suffix = fileInfo.suffix().toLower();
    if (suffix == QStringLiteral("json")) {
        return FileCategory::Records;
    }
    if (suffix == QStringLiteral("pgn")) {
        return FileCategory::Pgn;
    }
    if (suffix == QStringLiteral("log")) {
        return FileCategory::CommunicationLog;
    }
    return FileCategory::Other;
}

void addFile(ApplicationDataCategorySummary& category,
             const QFileInfo& fileInfo)
{
    ++category.fileCount;
    category.byteCount += fileInfo.size();
}

bool categorySelected(FileCategory category,
                      const ApplicationDataSelection& selection)
{
    switch (category) {
    case FileCategory::Records:
        return selection.records;
    case FileCategory::Pgn:
        return selection.pgnFiles;
    case FileCategory::CommunicationLog:
        return selection.communicationLogs;
    case FileCategory::Other:
        return false;
    }
    return false;
}

QStringList childDirectoriesDeepestFirst(const QString& rootPath)
{
    QStringList directories;
    QDirIterator iterator(
        rootPath,
        kStoredDirectoryFilters,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        directories.append(QDir::cleanPath(iterator.next()));
    }

    std::sort(directories.begin(),
              directories.end(),
              [](const QString& left, const QString& right) {
                  if (left.size() != right.size()) {
                      return left.size() > right.size();
                  }
                  return left > right;
              });
    return directories;
}

int directoryCount(const QString& rootPath)
{
    int count = 0;
    QDirIterator iterator(
        rootPath,
        kStoredDirectoryFilters,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        ++count;
    }
    return count;
}

void removeEmptyChildDirectories(const QString& rootPath,
                                 ApplicationDataDeletionResult& result)
{
    const QStringList directories = childDirectoriesDeepestFirst(rootPath);
    for (const QString& path : directories) {
        const QFileInfo info(path);
        QDir parent = info.dir();
        if (parent.rmdir(info.fileName())) {
            ++result.deletedDirectories;
        }
    }
}

void removeEmptyOrganizationDirectory(
    const QString& dataPath,
    ApplicationDataDeletionResult& result)
{
    const QFileInfo dataInfo(dataPath);
    QDir organizationDirectory = dataInfo.dir();
    const QString organizationName =
        QCoreApplication::organizationName().trimmed();
    const QString applicationName =
        QCoreApplication::applicationName().trimmed();

    if (organizationName.isEmpty()
        || applicationName.isEmpty()
        || dataInfo.fileName().compare(applicationName,
                                       Qt::CaseInsensitive) != 0
        || organizationDirectory.dirName().compare(
               organizationName,
               Qt::CaseInsensitive) != 0
        || !organizationDirectory.entryList(
                QDir::AllEntries | QDir::Hidden | QDir::System
                | QDir::NoDotAndDotDot).isEmpty()) {
        return;
    }

    const QString directoryName = organizationDirectory.dirName();
    if (organizationDirectory.cdUp()
        && organizationDirectory.rmdir(directoryName)) {
        ++result.deletedDirectories;
    }
}

void clearSettings(QSettings& settings,
                   ApplicationDataDeletionResult& result)
{
    settings.clear();
    settings.sync();
    if (settings.status() == QSettings::NoError) {
        result.settingsCleared = true;
        return;
    }

    result.errors.append(
        QCoreApplication::translate(
            "ApplicationData",
            "Could not remove the saved application settings."));
}

} // namespace

int ApplicationDataSummary::totalFileCount() const
{
    return records.fileCount
        + pgnFiles.fileCount
        + communicationLogs.fileCount
        + otherFiles.fileCount;
}

qint64 ApplicationDataSummary::totalByteCount() const
{
    return records.byteCount
        + pgnFiles.byteCount
        + communicationLogs.byteCount
        + otherFiles.byteCount;
}

bool ApplicationDataSelection::anySelected() const
{
    return records || pgnFiles || communicationLogs || settings;
}

bool ApplicationDataSelection::allSelected() const
{
    return records && pgnFiles && communicationLogs && settings;
}

bool ApplicationDataDeletionResult::succeeded() const
{
    return errors.isEmpty();
}

ApplicationDataSummary inspectApplicationData(const QString& dataDirectory)
{
    ApplicationDataSummary summary;
    const QString dataPath = safeDataDirectory(dataDirectory);
    if (dataPath.isEmpty() || !QDir(dataPath).exists()) {
        return summary;
    }

    QDirIterator iterator(
        dataPath,
        kStoredFileFilters,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QFileInfo fileInfo(iterator.next());
        switch (fileCategory(fileInfo)) {
        case FileCategory::Records:
            addFile(summary.records, fileInfo);
            break;
        case FileCategory::Pgn:
            addFile(summary.pgnFiles, fileInfo);
            break;
        case FileCategory::CommunicationLog:
            addFile(summary.communicationLogs, fileInfo);
            break;
        case FileCategory::Other:
            addFile(summary.otherFiles, fileInfo);
            break;
        }
    }
    return summary;
}

ApplicationDataDeletionResult deleteApplicationData(
    const QString& dataDirectory,
    const ApplicationDataSelection& selection,
    QSettings& settings,
    bool removeEmptyOrganizationDirectoryRequested)
{
    ApplicationDataDeletionResult result;
    if (!selection.anySelected()) {
        return result;
    }

    const QString dataPath = safeDataDirectory(dataDirectory);
    if (dataPath.isEmpty()) {
        result.errors.append(
            QCoreApplication::translate(
                "ApplicationData",
                "The application data directory is not safe to remove."));
        return result;
    }

    if (QDir(dataPath).exists()) {
        if (selection.allSelected()) {
            const ApplicationDataSummary summary =
                inspectApplicationData(dataPath);
            const int directories = directoryCount(dataPath) + 1;
            if (QDir(dataPath).removeRecursively()) {
                result.deletedFiles = summary.totalFileCount();
                result.deletedDirectories = directories;
            } else {
                result.errors.append(
                    QCoreApplication::translate(
                        "ApplicationData",
                        "Could not completely remove the application data directory: %1")
                        .arg(QDir::toNativeSeparators(dataPath)));
            }
        } else {
            QStringList filesToRemove;
            QDirIterator iterator(
                dataPath,
                kStoredFileFilters,
                QDirIterator::Subdirectories);
            while (iterator.hasNext()) {
                const QString filePath = iterator.next();
                if (categorySelected(fileCategory(QFileInfo(filePath)),
                                     selection)) {
                    filesToRemove.append(filePath);
                }
            }

            for (const QString& filePath : filesToRemove) {
                if (QFile::remove(filePath)) {
                    ++result.deletedFiles;
                } else {
                    result.errors.append(
                        QCoreApplication::translate(
                            "ApplicationData",
                            "Could not remove: %1")
                            .arg(QDir::toNativeSeparators(filePath)));
                }
            }
            removeEmptyChildDirectories(dataPath, result);
        }
    }

    if (selection.settings) {
        clearSettings(settings, result);
    }

    if (removeEmptyOrganizationDirectoryRequested
        && selection.allSelected()
        && !QDir(dataPath).exists()) {
        removeEmptyOrganizationDirectory(dataPath, result);
    }
    return result;
}
