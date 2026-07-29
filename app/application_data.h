#pragma once

#include <QStringList>
#include <QtGlobal>

class QSettings;

struct ApplicationDataCategorySummary {
    int fileCount = 0;
    qint64 byteCount = 0;
};

struct ApplicationDataSummary {
    ApplicationDataCategorySummary records;
    ApplicationDataCategorySummary pgnFiles;
    ApplicationDataCategorySummary communicationLogs;
    ApplicationDataCategorySummary otherFiles;

    int totalFileCount() const;
    qint64 totalByteCount() const;
};

struct ApplicationDataSelection {
    bool records = false;
    bool pgnFiles = false;
    bool communicationLogs = false;
    bool settings = false;

    bool anySelected() const;
    bool allSelected() const;
};

struct ApplicationDataDeletionResult {
    int deletedFiles = 0;
    int deletedDirectories = 0;
    bool settingsCleared = false;
    QStringList errors;

    bool succeeded() const;
};

ApplicationDataSummary inspectApplicationData(const QString& dataDirectory);

ApplicationDataDeletionResult deleteApplicationData(
    const QString& dataDirectory,
    const ApplicationDataSelection& selection,
    QSettings& settings,
    bool removeEmptyOrganizationDirectory = false);
