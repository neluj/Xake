#pragma once

#include "match_settings_types.h"

struct AppState {
    bool hasLastMatch = false;
    bool hasLastTournament = false;
    MatchConfig lastMatch;
    TournamentConfig lastTournament;
};
