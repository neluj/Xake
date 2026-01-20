#pragma once

#include <QString>

enum class PlayerType {
    Human = 0,
    Engine = 1
};

struct PlayerConfig {
    PlayerType type = PlayerType::Human;
    QString name;
    QString enginePath;
};

struct GameConfig {
    QString timeControl;
    int baseTimeSeconds = 0;
    int incrementSeconds = 0;
    int movesToGo = 0;
    bool useStartPos = true;
    QString startPosition;
};

struct MatchConfig {
    PlayerConfig player1;
    PlayerConfig player2;
    GameConfig game;
};

struct TournamentConfig {
    MatchConfig match;
    QString tournamentType;
    int rounds = 0;
    int gamesPerPairing = 0;
    int maxMoves = 0;
    bool randomizeColors = false;
};
