#pragma once

#include "match_settings_types.h"
#include "position.h"

struct AppState {
    bool hasLastMatch = false;
    bool hasLastTournament = false;
    bool hasCurrentPosition = false;
    MatchConfig lastMatch;
    TournamentConfig lastTournament;
    Position currentPosition;
};
