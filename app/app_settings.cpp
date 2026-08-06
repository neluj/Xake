#include "app_settings.h"

#include <QSettings>

namespace {

constexpr int kSettingsSchemaVersion = 3;

void writePlayer(QSettings& settings,
                 const QString& group,
                 const PlayerConfig& player)
{
    settings.beginGroup(group);
    settings.setValue(QStringLiteral("type"), static_cast<int>(player.type));
    settings.setValue(QStringLiteral("name"), player.name);
    settings.setValue(QStringLiteral("enginePath"), player.enginePath);
    settings.endGroup();
}

PlayerConfig readPlayer(QSettings& settings, const QString& group)
{
    settings.beginGroup(group);
    PlayerConfig player;
    player.type = settings.value(QStringLiteral("type"),
                                 static_cast<int>(PlayerType::Human))
                      .toInt() == static_cast<int>(PlayerType::Engine)
        ? PlayerType::Engine
        : PlayerType::Human;
    player.name = settings.value(QStringLiteral("name")).toString();
    player.enginePath =
        settings.value(QStringLiteral("enginePath")).toString();
    settings.endGroup();
    return player;
}

void writeGame(QSettings& settings, const GameConfig& game)
{
    settings.beginGroup(QStringLiteral("game"));
    settings.setValue(QStringLiteral("timeControl"), game.timeControl);
    settings.setValue(QStringLiteral("baseTimeSeconds"), game.baseTimeSeconds);
    settings.setValue(QStringLiteral("incrementSeconds"), game.incrementSeconds);
    settings.setValue(QStringLiteral("useStartPos"), game.useStartPos);
    settings.setValue(QStringLiteral("startPosition"), game.startPosition);
    settings.setValue(QStringLiteral("useOpeningFile"), game.useOpeningFile);
    settings.setValue(QStringLiteral("openingFilePath"), game.openingFilePath);
    settings.endGroup();
}

GameConfig readGame(QSettings& settings)
{
    settings.beginGroup(QStringLiteral("game"));
    GameConfig game;
    game.timeControl =
        settings.value(QStringLiteral("timeControl")).toString();
    game.baseTimeSeconds =
        settings.value(QStringLiteral("baseTimeSeconds")).toInt();
    game.incrementSeconds =
        settings.value(QStringLiteral("incrementSeconds")).toInt();
    game.useStartPos =
        settings.value(QStringLiteral("useStartPos"), true).toBool();
    game.startPosition =
        settings.value(QStringLiteral("startPosition"),
                       QStringLiteral("startpos")).toString();
    game.useOpeningFile =
        settings.value(QStringLiteral("useOpeningFile"), false).toBool();
    game.openingFilePath =
        settings.value(QStringLiteral("openingFilePath")).toString();
    settings.endGroup();
    return game;
}

void writeMatch(QSettings& settings, const MatchConfig& match)
{
    writePlayer(settings, QStringLiteral("player1"), match.player1);
    writePlayer(settings, QStringLiteral("player2"), match.player2);
    writeGame(settings, match.game);
}

MatchConfig readMatch(QSettings& settings)
{
    MatchConfig match;
    match.player1 = readPlayer(settings, QStringLiteral("player1"));
    match.player2 = readPlayer(settings, QStringLiteral("player2"));
    match.game = readGame(settings);
    return match;
}

void writeTournamentParticipants(
    QSettings& settings,
    const QVector<TournamentParticipant>& participants)
{
    settings.beginWriteArray(QStringLiteral("participants"),
                             participants.size());
    for (qsizetype index = 0; index < participants.size(); ++index) {
        settings.setArrayIndex(index);
        settings.setValue(QStringLiteral("id"),
                          participants.at(index).id);
        writePlayer(settings,
                    QStringLiteral("player"),
                    participants.at(index).player);
    }
    settings.endArray();
}

QVector<TournamentParticipant> readTournamentParticipants(
    QSettings& settings)
{
    QVector<TournamentParticipant> participants;
    const int size =
        settings.beginReadArray(QStringLiteral("participants"));
    participants.reserve(size);
    for (int index = 0; index < size; ++index) {
        settings.setArrayIndex(index);
        participants.append({
            settings.value(QStringLiteral("id")).toString(),
            readPlayer(settings, QStringLiteral("player"))
        });
    }
    settings.endArray();
    return participants;
}

TournamentFormat tournamentFormatFromSettings(
    const QString& value)
{
    return value.compare(QStringLiteral("gauntlet"),
                         Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("Gauntlet"),
                         Qt::CaseInsensitive) == 0
        ? TournamentFormat::Gauntlet
        : TournamentFormat::RoundRobin;
}

}

AppState loadAppState(QSettings& settings)
{
    AppState state;
    settings.beginGroup(QStringLiteral("lastMatch"));
    state.hasLastMatch =
        settings.value(QStringLiteral("available"), false).toBool();
    if (state.hasLastMatch) {
        state.lastMatch = readMatch(settings);
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("lastTournament"));
    state.hasLastTournament =
        settings.value(QStringLiteral("available"), false).toBool();
    if (state.hasLastTournament) {
        state.lastTournament.match = readMatch(settings);
        state.lastTournament.participants =
            readTournamentParticipants(settings);
        state.lastTournament.tournamentType =
            settings.value(QStringLiteral("tournamentType")).toString();
        state.lastTournament.format = tournamentFormatFromSettings(
            settings.value(
                QStringLiteral("format"),
                state.lastTournament.tournamentType).toString());
        state.lastTournament.gauntletParticipantId =
            settings.value(
                QStringLiteral("gauntletParticipantId")).toString();
        state.lastTournament.rounds =
            settings.value(QStringLiteral("rounds"), 1).toInt();
        state.lastTournament.gamesPerPairing =
            settings.value(
                QStringLiteral("gamesPerPairing"), 2).toInt();
        state.lastTournament.maxMoves =
            settings.value(QStringLiteral("maxMoves")).toInt();
        if (state.lastTournament.participants.isEmpty()) {
            state.lastTournament.participants = {
                {QStringLiteral("participant-1"),
                 state.lastTournament.match.player1},
                {QStringLiteral("participant-2"),
                 state.lastTournament.match.player2}
            };
        }
        if (state.lastTournament.gauntletParticipantId.isEmpty()
            && !state.lastTournament.participants.isEmpty()) {
            state.lastTournament.gauntletParticipantId =
                state.lastTournament.participants.first().id;
        }
    }
    settings.endGroup();
    return state;
}

void saveLastMatch(QSettings& settings, const MatchConfig& config)
{
    settings.setValue(QStringLiteral("settingsSchemaVersion"),
                      kSettingsSchemaVersion);
    settings.beginGroup(QStringLiteral("lastMatch"));
    settings.remove(QString());
    settings.setValue(QStringLiteral("available"), true);
    writeMatch(settings, config);
    settings.endGroup();
    settings.sync();
}

void saveLastTournament(QSettings& settings, const TournamentConfig& config)
{
    settings.setValue(QStringLiteral("settingsSchemaVersion"),
                      kSettingsSchemaVersion);
    settings.beginGroup(QStringLiteral("lastTournament"));
    settings.remove(QString());
    settings.setValue(QStringLiteral("available"), true);
    MatchConfig legacyMatch = config.match;
    if (config.participants.size() >= 2) {
        legacyMatch.player1 = config.participants.at(0).player;
        legacyMatch.player2 = config.participants.at(1).player;
    }
    writeMatch(settings, legacyMatch);
    writeTournamentParticipants(settings, config.participants);
    settings.setValue(QStringLiteral("tournamentType"),
                      config.format == TournamentFormat::Gauntlet
                          ? QStringLiteral("Gauntlet")
                          : QStringLiteral("Round-robin"));
    settings.setValue(QStringLiteral("format"),
                      config.format == TournamentFormat::Gauntlet
                          ? QStringLiteral("gauntlet")
                          : QStringLiteral("round_robin"));
    settings.setValue(QStringLiteral("gauntletParticipantId"),
                      config.gauntletParticipantId);
    settings.setValue(QStringLiteral("rounds"), config.rounds);
    settings.setValue(QStringLiteral("gamesPerPairing"),
                      config.gamesPerPairing);
    settings.setValue(QStringLiteral("maxMoves"), config.maxMoves);
    settings.endGroup();
    settings.sync();
}
