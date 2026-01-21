#pragma once

#include "match_settings_types.h"

enum class ValidationError {
    None = 0,
    MissingHumanNamePlayer1,
    MissingHumanNamePlayer2,
    MissingEnginePlayer1,
    MissingEnginePlayer2,
    MissingStartPosition,
    InvalidStartPosition,
    InvalidRounds,
    InvalidGamesPerPairing
};

void normalizeMatchConfig(MatchConfig &config);
ValidationError validateMatchConfig(const MatchConfig &config);
void normalizeTournamentConfig(TournamentConfig &config);
ValidationError validateTournamentConfig(const TournamentConfig &config);

QString validationErrorTitle(ValidationError error);
QString validationErrorMessage(ValidationError error);
