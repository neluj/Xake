#pragma once

#include <QString>
#include <QVector>

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
    bool useStartPos = true;
    QString startPosition;
    bool useOpeningFile = false;
    QString openingFilePath;
};

struct MatchConfig {
    PlayerConfig player1;
    PlayerConfig player2;
    GameConfig game;
};

enum class TournamentFormat {
    RoundRobin = 0,
    Gauntlet = 1
};

struct TournamentParticipant {
    QString id;
    PlayerConfig player;
};

struct TournamentConfig {
    // Retained for 0.1-0.3 settings compatibility. Participants are canonical.
    MatchConfig match;
    QVector<TournamentParticipant> participants;
    TournamentFormat format = TournamentFormat::RoundRobin;
    QString gauntletParticipantId;
    QString tournamentType;
    int rounds = 1;
    int gamesPerPairing = 2;
    int maxMoves = 0;
};
