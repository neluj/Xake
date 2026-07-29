#pragma once

#include "move_record.h"
#include "position.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

struct ReplayGame {
    int gameNumber = 1;
    QString sourcePath;
    QString title;
    QString event;
    QString white;
    QString black;
    QString result;
    QString openingName;
    QString startFen;
    int openingMoveCount = 0;
    QStringList movesUci;
    QVector<MoveRecord> moveRecords;
};

struct ReplayLoadResult {
    QVector<ReplayGame> games;
    QString error;

    bool success() const
    {
        return error.isEmpty() && !games.isEmpty();
    }
};

ReplayLoadResult loadReplayFile(const QString& filePath);

class GameReplay
{
public:
    GameReplay();

    bool load(const ReplayGame& game, QString* errorOut = nullptr);
    void clear();
    bool goToPly(int ply);

    const ReplayGame& game() const;
    const Xake::Position& position() const;
    Xake::Move lastMove() const;
    int currentPly() const;
    int totalPly() const;
    QStringList visibleMoves() const;
    QVector<Xake::Piece> capturedPieces() const;
    qint64 whiteTimeMs() const;
    qint64 blackTimeMs() const;

private:
    qint64 clockAtCurrentPly(bool white) const;

    ReplayGame m_game;
    Xake::Position m_position;
    QVector<MoveRecord> m_records;
    int m_currentPly = 0;
};
