#include "session_record.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QJsonObject playerToJson(const PlayerConfig& player)
{
    QJsonObject obj;
    obj["type"] = (player.type == PlayerType::Engine) ? "engine" : "human";
    obj["name"] = player.name;
    obj["enginePath"] = player.enginePath;
    return obj;
}

QJsonObject gameToJson(const GameConfig& game)
{
    QJsonObject obj;
    obj["timeControl"] = game.timeControl;
    obj["baseTimeSeconds"] = game.baseTimeSeconds;
    obj["incrementSeconds"] = game.incrementSeconds;
    obj["useStartPos"] = game.useStartPos;
    obj["startPosition"] = game.startPosition;
    obj["useOpeningFile"] = game.useOpeningFile;
    obj["openingFilePath"] = game.openingFilePath;
    return obj;
}

QJsonObject matchToJson(const MatchConfig& match)
{
    QJsonObject obj;
    obj["player1"] = playerToJson(match.player1);
    obj["player2"] = playerToJson(match.player2);
    obj["game"] = gameToJson(match.game);
    return obj;
}

QJsonObject tournamentToJson(const TournamentConfig& tournament)
{
    QJsonObject obj;
    obj["tournamentType"] = tournament.tournamentType;
    obj["rounds"] = tournament.rounds;
    obj["gamesPerPairing"] = tournament.gamesPerPairing;
    obj["maxMoves"] = tournament.maxMoves;
    obj["match"] = matchToJson(tournament.match);
    return obj;
}

QString resultNotation(GameOutcome outcome)
{
    switch (outcome) {
    case GameOutcome::WhiteWin:
        return QStringLiteral("1-0");
    case GameOutcome::BlackWin:
        return QStringLiteral("0-1");
    case GameOutcome::Draw:
        return QStringLiteral("1/2-1/2");
    }
    return QStringLiteral("*");
}

QString outcomeName(GameOutcome outcome)
{
    switch (outcome) {
    case GameOutcome::WhiteWin:
        return QStringLiteral("white_win");
    case GameOutcome::BlackWin:
        return QStringLiteral("black_win");
    case GameOutcome::Draw:
        return QStringLiteral("draw");
    }
    return QStringLiteral("unknown");
}

QString terminationName(GameTermination termination)
{
    switch (termination) {
    case GameTermination::Checkmate:
        return QStringLiteral("checkmate");
    case GameTermination::Stalemate:
        return QStringLiteral("stalemate");
    case GameTermination::FiftyMoveRule:
        return QStringLiteral("fifty_move_rule");
    case GameTermination::ThreefoldRepetition:
        return QStringLiteral("threefold_repetition");
    case GameTermination::InsufficientMaterial:
        return QStringLiteral("insufficient_material");
    case GameTermination::TimeForfeit:
        return QStringLiteral("time_forfeit");
    case GameTermination::MoveLimit:
        return QStringLiteral("move_limit");
    }
    return QStringLiteral("unknown");
}

} // namespace

QString sessionTagNow()
{
    return QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
}

bool writeSessionRecord(const SessionRecord& record,
                        const QString& filePath,
                        QString* errorOut)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return false;
    }

    QJsonObject root;
    root["sessionType"] = record.sessionType;
    root["sessionTag"] = record.sessionTag;
    root["startTime"] = record.startTimeIso;
    root["status"] = record.status;
    root["updatedAt"] = record.updatedAtIso;
    root["logDir"] = record.logDir;
    root["match"] = matchToJson(record.match);
    root["startFen"] = record.startFen;
    if (record.openingCount > 0) {
        QJsonObject opening;
        opening["count"] = record.openingCount;
        opening["name"] = record.openingName;
        opening["finalFen"] = record.finalOpeningFen;
        QJsonArray moves;
        for (const QString& move : record.openingMoves) {
            moves.append(move);
        }
        opening["moves"] = moves;
        root["opening"] = opening;
    }
    if (record.hasTournament) {
        root["tournament"] = tournamentToJson(record.tournament);
    }
    if (!record.finishedAtIso.isEmpty()) {
        root["finishedAt"] = record.finishedAtIso;
    }
    if (!record.finalFen.isEmpty()) {
        root["finalFen"] = record.finalFen;
    }
    if (!record.moveRecords.isEmpty()) {
        root["moveRecords"] = moveRecordsToJson(record.moveRecords);
    }
    if (!record.moves.isEmpty()) {
        root["moveFormat"] = QStringLiteral("uci");
        QJsonArray moves;
        for (const QString& move : record.moves) {
            moves.append(move);
        }
        root["moves"] = moves;
    }
    if (record.whiteTimeMs >= 0 && record.blackTimeMs >= 0) {
        QJsonObject clocks;
        clocks["whiteMs"] = record.whiteTimeMs;
        clocks["blackMs"] = record.blackTimeMs;
        root["clocks"] = clocks;
    }
    if (record.hasResult) {
        QJsonObject result;
        result["notation"] = resultNotation(record.result.outcome);
        result["outcome"] = outcomeName(record.result.outcome);
        result["termination"] = terminationName(record.result.termination);
        result["message"] = record.result.message;
        root["result"] = result;
    }
    if (!record.abortTitle.isEmpty() || !record.abortMessage.isEmpty()) {
        QJsonObject abort;
        abort["title"] = record.abortTitle;
        abort["message"] = record.abortMessage;
        root["abort"] = abort;
    }

    const QJsonDocument doc(root);
    const QByteArray json = doc.toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return false;
    }
    if (!file.commit()) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return false;
    }
    return true;
}
