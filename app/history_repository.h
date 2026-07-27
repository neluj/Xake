#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

enum class HistorySessionType {
    Match,
    Tournament
};

struct HistoryGame {
    int gameNumber = 0;
    QString status;
    QDateTime startedAt;
    QDateTime finishedAt;
    QString white;
    QString black;
    QString result;
    QString termination;
    QString message;
    QString openingName;
    QString startFen;
    QStringList moves;
    int openingMoveCount = 0;
};

struct HistoryEntry {
    HistorySessionType type = HistorySessionType::Match;
    QString sessionTag;
    QString directoryPath;
    QString recordFilePath;
    QString pgnFilePath;
    QString status;
    QDateTime startedAt;
    QDateTime finishedAt;
    QString player1;
    QString player2;
    QString timeControl;
    int baseTimeSeconds = 0;
    int incrementSeconds = 0;
    QString result;
    QString termination;
    QString message;
    QString openingName;
    QString startFen;
    QString finalFen;
    QStringList moves;
    int openingMoveCount = 0;
    int totalGames = 0;
    int completedGames = 0;
    int player1Wins = 0;
    int player2Wins = 0;
    int draws = 0;
    QVector<HistoryGame> games;
};

struct HistoryLoadResult {
    QVector<HistoryEntry> entries;
    QStringList warnings;
};

HistoryLoadResult loadSessionHistory(const QString& sessionsDirectory);
