#include "session_record.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
    obj["movesToGo"] = game.movesToGo;
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

} // namespace

QString sessionTagNow()
{
    return QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
}

QString defaultSessionDir(const QString& sessionTag, const QString& sessionType)
{
    const QString baseDir = QDir::current().filePath("logs");
    const QString dirName = QString("%1_%2").arg(sessionTag, sessionType);
    return QDir(baseDir).filePath(dirName);
}

bool writeSessionRecord(const SessionRecord& record,
                        const QString& filePath,
                        QString* errorOut)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return false;
    }

    QJsonObject root;
    root["sessionType"] = record.sessionType;
    root["sessionTag"] = record.sessionTag;
    root["startTime"] = record.startTimeIso;
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

    const QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}
