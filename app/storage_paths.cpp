#include "storage_paths.h"

#include <QDir>
#include <QStandardPaths>

QString applicationDataDir()
{
    const QString standardPath =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!standardPath.isEmpty()) {
        return QDir::cleanPath(standardPath);
    }

    return QDir::home().filePath(QStringLiteral(".xake"));
}

QString sessionsRootDir()
{
    return QDir(applicationDataDir()).filePath(QStringLiteral("sessions"));
}

QString defaultSessionDir(const QString& sessionTag, const QString& sessionType)
{
    const QString dirName = QStringLiteral("%1_%2").arg(sessionTag, sessionType);
    return QDir(sessionsRootDir()).filePath(dirName);
}
