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
    obj["format"] =
        tournament.format == TournamentFormat::Gauntlet
        ? QStringLiteral("gauntlet")
        : QStringLiteral("round_robin");
    obj["gauntletParticipantId"] =
        tournament.gauntletParticipantId;
    obj["rounds"] = tournament.rounds;
    obj["gamesPerPairing"] = tournament.gamesPerPairing;
    obj["maxMoves"] = tournament.maxMoves;
    obj["match"] = matchToJson(tournament.match);
    QJsonArray participants;
    for (const TournamentParticipant& participant :
         tournament.participants) {
        QJsonObject participantObject;
        participantObject["id"] = participant.id;
        participantObject["player"] =
            playerToJson(participant.player);
        participants.append(participantObject);
    }
    obj["participants"] = participants;
    return obj;
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
    root["formatVersion"] = 2;
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
    const GameTermination termination =
        record.hasResult ? record.result.termination
                         : record.termination;
    if (termination != GameTermination::Unknown) {
        root["termination"] = gameTerminationKey(termination);
    }
    if (record.hasResult) {
        QJsonObject result;
        result["notation"] = gameResultNotation(record.result.outcome);
        result["outcome"] = gameOutcomeKey(record.result.outcome);
        result["termination"] =
            gameTerminationKey(record.result.termination);
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
