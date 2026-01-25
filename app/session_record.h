#pragma once

#include "match_settings_types.h"

#include <QString>

struct SessionRecord {
    QString sessionType;
    QString sessionTag;
    QString startTimeIso;
    QString logDir;
    MatchConfig match;
    bool hasTournament = false;
    TournamentConfig tournament;
    QString startFen;
};

QString sessionTagNow();
QString defaultSessionDir(const QString& sessionTag, const QString& sessionType);
bool writeSessionRecord(const SessionRecord& record,
                        const QString& filePath,
                        QString* errorOut = nullptr);
