#pragma once

#include "match_settings_types.h"

#include <QString>
#include <QStringList>

struct SessionRecord {
    QString sessionType;
    QString sessionTag;
    QString startTimeIso;
    QString logDir;
    MatchConfig match;
    bool hasTournament = false;
    TournamentConfig tournament;
    QString startFen;
    int openingCount = 0;
    QString openingName;
    QString finalOpeningFen;
    QStringList openingMoves;
};

QString sessionTagNow();
bool writeSessionRecord(const SessionRecord& record,
                        const QString& filePath,
                        QString* errorOut = nullptr);
