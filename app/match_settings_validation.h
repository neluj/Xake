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
    MissingOpeningFile,
    OpeningFileNotFound,
    UnsupportedOpeningFile,
    InvalidRounds,
    InvalidGamesPerPairing,
    TooFewTournamentParticipants,
    MissingTournamentParticipantName,
    MissingTournamentParticipantEngine,
    DuplicateTournamentParticipantName,
    InvalidGauntletParticipant
};

void normalizeMatchConfig(MatchConfig &config);
ValidationError validateMatchConfig(const MatchConfig &config);
void normalizeTournamentConfig(TournamentConfig &config);
ValidationError validateTournamentConfig(const TournamentConfig &config);

QString validationErrorTitle(ValidationError error);
QString validationErrorMessage(ValidationError error);
