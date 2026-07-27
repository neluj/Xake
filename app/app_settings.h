#pragma once

#include "app_state.h"

class QSettings;

AppState loadAppState(QSettings& settings);
void saveLastMatch(QSettings& settings, const MatchConfig& config);
void saveLastTournament(QSettings& settings, const TournamentConfig& config);
