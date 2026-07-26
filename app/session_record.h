#pragma once

#include "game_controller.h"
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
    QString status = QStringLiteral("in_progress");
    QString updatedAtIso;
    QString finishedAtIso;
    QStringList moves;
    QString finalFen;
    qint64 whiteTimeMs = -1;
    qint64 blackTimeMs = -1;
    bool hasResult = false;
    GameResult result;
    QString abortTitle;
    QString abortMessage;
};

QString sessionTagNow();
bool writeSessionRecord(const SessionRecord& record,
                        const QString& filePath,
                        QString* errorOut = nullptr);
