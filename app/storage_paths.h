#pragma once

#include <QString>

QString applicationDataDir();
QString sessionsRootDir();
QString defaultSessionDir(const QString& sessionTag, const QString& sessionType);
