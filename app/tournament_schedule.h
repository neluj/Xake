#pragma once

#include "match_settings_types.h"

#include <QString>
#include <QVector>

struct TournamentScheduledGame {
    int gameNumber = 0;
    int roundNumber = 0;
    int cycleNumber = 0;
    int gameInPairing = 0;
    int openingGroup = 0;
    QString whiteParticipantId;
    QString blackParticipantId;
};

struct TournamentScheduleResult {
    QVector<TournamentScheduledGame> games;
    int roundCount = 0;
    QString error;

    bool succeeded() const
    {
        return error.isEmpty() && !games.isEmpty();
    }
};

TournamentScheduleResult buildTournamentSchedule(
    const TournamentConfig& config);
