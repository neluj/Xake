#include "move_record.h"

#include <QJsonObject>

namespace {

bool isValidPiece(int value)
{
    return value == Xake::NO_PIECE
        || (value >= Xake::W_PAWN && value <= Xake::W_KING)
        || (value >= Xake::B_PAWN && value <= Xake::B_KING);
}

qint64 integerOrMissing(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toInteger(-1) : -1;
}

} // namespace

QString moveOriginName(MoveOrigin origin)
{
    switch (origin) {
    case MoveOrigin::Opening:
        return QStringLiteral("opening");
    case MoveOrigin::Human:
        return QStringLiteral("human");
    case MoveOrigin::Engine:
        return QStringLiteral("engine");
    case MoveOrigin::Imported:
        return QStringLiteral("imported");
    }
    return QStringLiteral("imported");
}

MoveOrigin moveOriginFromName(const QString& name)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("opening")) {
        return MoveOrigin::Opening;
    }
    if (normalized == QStringLiteral("human")) {
        return MoveOrigin::Human;
    }
    if (normalized == QStringLiteral("engine")) {
        return MoveOrigin::Engine;
    }
    return MoveOrigin::Imported;
}

QJsonArray moveRecordsToJson(const QVector<MoveRecord>& records)
{
    QJsonArray values;
    for (const MoveRecord& record : records) {
        QJsonObject object;
        object.insert(QStringLiteral("uci"), record.uci);
        object.insert(QStringLiteral("movedPiece"),
                      static_cast<int>(record.movedPiece));
        object.insert(QStringLiteral("capturedPiece"),
                      static_cast<int>(record.capturedPiece));
        object.insert(QStringLiteral("origin"),
                      moveOriginName(record.origin));
        if (record.whiteTimeBeforeMs >= 0) {
            object.insert(QStringLiteral("whiteTimeBeforeMs"),
                          record.whiteTimeBeforeMs);
        }
        if (record.blackTimeBeforeMs >= 0) {
            object.insert(QStringLiteral("blackTimeBeforeMs"),
                          record.blackTimeBeforeMs);
        }
        if (record.whiteTimeAfterMs >= 0) {
            object.insert(QStringLiteral("whiteTimeAfterMs"),
                          record.whiteTimeAfterMs);
        }
        if (record.blackTimeAfterMs >= 0) {
            object.insert(QStringLiteral("blackTimeAfterMs"),
                          record.blackTimeAfterMs);
        }
        values.append(object);
    }
    return values;
}

QVector<MoveRecord> moveRecordsFromJson(const QJsonArray& values)
{
    QVector<MoveRecord> records;
    records.reserve(values.size());
    for (const QJsonValue& value : values) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        MoveRecord record;
        record.uci = object.value(QStringLiteral("uci"))
                         .toString()
                         .trimmed()
                         .toLower();
        if (record.uci.isEmpty()) {
            continue;
        }

        const int movedPiece =
            object.value(QStringLiteral("movedPiece")).toInt();
        const int capturedPiece =
            object.value(QStringLiteral("capturedPiece")).toInt();
        record.movedPiece = isValidPiece(movedPiece)
            ? static_cast<Xake::Piece>(movedPiece)
            : Xake::NO_PIECE;
        record.capturedPiece = isValidPiece(capturedPiece)
            ? static_cast<Xake::Piece>(capturedPiece)
            : Xake::NO_PIECE;
        record.origin = moveOriginFromName(
            object.value(QStringLiteral("origin")).toString());
        record.whiteTimeBeforeMs =
            integerOrMissing(object, QStringLiteral("whiteTimeBeforeMs"));
        record.blackTimeBeforeMs =
            integerOrMissing(object, QStringLiteral("blackTimeBeforeMs"));
        record.whiteTimeAfterMs =
            integerOrMissing(object, QStringLiteral("whiteTimeAfterMs"));
        record.blackTimeAfterMs =
            integerOrMissing(object, QStringLiteral("blackTimeAfterMs"));
        records.append(record);
    }
    return records;
}

QVector<Xake::Piece> capturedPiecesFromMoveRecords(
    const QVector<MoveRecord>& records,
    int plyCount)
{
    const int count = plyCount < 0
        ? static_cast<int>(records.size())
        : qBound(0, plyCount, static_cast<int>(records.size()));
    QVector<Xake::Piece> pieces;
    for (int index = 0; index < count; ++index) {
        const Xake::Piece captured = records.at(index).capturedPiece;
        if (captured != Xake::NO_PIECE) {
            pieces.append(captured);
        }
    }
    return pieces;
}
