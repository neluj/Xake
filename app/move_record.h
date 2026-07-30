#pragma once

#include "move.h"

#include <QJsonArray>
#include <QString>
#include <QVector>
#include <QtGlobal>

enum class MoveOrigin {
    Opening,
    Human,
    Engine,
    Imported
};

struct MoveRecord {
    Xake::Move move = Xake::NOMOVE;
    QString uci;
    Xake::Piece movedPiece = Xake::NO_PIECE;
    Xake::Piece capturedPiece = Xake::NO_PIECE;
    MoveOrigin origin = MoveOrigin::Imported;
    qint64 whiteTimeBeforeMs = -1;
    qint64 blackTimeBeforeMs = -1;
    qint64 whiteTimeAfterMs = -1;
    qint64 blackTimeAfterMs = -1;
};

QString moveOriginName(MoveOrigin origin);
MoveOrigin moveOriginFromName(const QString& name);

QJsonArray moveRecordsToJson(const QVector<MoveRecord>& records);
QVector<MoveRecord> moveRecordsFromJson(const QJsonArray& values);

QVector<Xake::Piece> capturedPiecesFromMoveRecords(
    const QVector<MoveRecord>& records,
    int plyCount = -1);
