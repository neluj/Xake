#include "match_settings_validation.h"

#include "position.h"

#include <QFileInfo>
#include <QSet>

using namespace Xake;

namespace {

void normalizePlayer(PlayerConfig& player)
{
    player.name = player.name.trimmed();
    player.enginePath = player.enginePath.trimmed();
}

void normalizeGame(GameConfig& game)
{
    game.startPosition = game.startPosition.trimmed();
    game.openingFilePath = game.openingFilePath.trimmed();
}

ValidationError validateGame(const GameConfig& game)
{
    if (game.useOpeningFile) {
        const QString openingPath = game.openingFilePath.trimmed();
        if (openingPath.isEmpty()) {
            return ValidationError::MissingOpeningFile;
        }
        const QFileInfo openingFile(openingPath);
        if (!openingFile.exists() || !openingFile.isFile()) {
            return ValidationError::OpeningFileNotFound;
        }
        const QString suffix = openingFile.suffix().toLower();
        if (suffix != QStringLiteral("pgn")
            && suffix != QStringLiteral("epd")
            && suffix != QStringLiteral("edp")) {
            return ValidationError::UnsupportedOpeningFile;
        }
    } else if (!game.useStartPos
               && game.startPosition.trimmed().isEmpty()) {
        return ValidationError::MissingStartPosition;
    }
    if (!game.useOpeningFile && !game.useStartPos) {
        const QString fenText = game.startPosition.trimmed();
        if (fenText.compare(QStringLiteral("startpos"), Qt::CaseInsensitive) != 0) {
            Position pos;
            if (!pos.set_FEN(fenText.toStdString())) {
                return ValidationError::InvalidStartPosition;
            }
        }
    }

    return ValidationError::None;
}

QString participantDisplayName(const PlayerConfig& player)
{
    if (!player.name.trimmed().isEmpty()) {
        return player.name.trimmed();
    }
    return QFileInfo(player.enginePath).completeBaseName().trimmed();
}

bool legacyPlayerConfigured(const PlayerConfig& player)
{
    return player.type == PlayerType::Engine
        ? !player.enginePath.trimmed().isEmpty()
        : !player.name.trimmed().isEmpty();
}

} // namespace

void normalizeMatchConfig(MatchConfig &config)
{
    normalizePlayer(config.player1);
    normalizePlayer(config.player2);
    normalizeGame(config.game);
}

ValidationError validateMatchConfig(const MatchConfig &config)
{
    if (config.player1.type == PlayerType::Human
        && config.player1.name.trimmed().isEmpty()) {
        return ValidationError::MissingHumanNamePlayer1;
    }
    if (config.player2.type == PlayerType::Human
        && config.player2.name.trimmed().isEmpty()) {
        return ValidationError::MissingHumanNamePlayer2;
    }
    if (config.player1.type == PlayerType::Engine
        && config.player1.enginePath.trimmed().isEmpty()) {
        return ValidationError::MissingEnginePlayer1;
    }
    if (config.player2.type == PlayerType::Engine
        && config.player2.enginePath.trimmed().isEmpty()) {
        return ValidationError::MissingEnginePlayer2;
    }
    return validateGame(config.game);
}

void normalizeTournamentConfig(TournamentConfig &config)
{
    normalizeMatchConfig(config.match);
    if (config.participants.isEmpty()
        && legacyPlayerConfigured(config.match.player1)
        && legacyPlayerConfigured(config.match.player2)) {
        config.participants = {
            {QStringLiteral("participant-1"), config.match.player1},
            {QStringLiteral("participant-2"), config.match.player2}
        };
    }

    QSet<QString> participantIds;
    for (qsizetype index = 0; index < config.participants.size(); ++index) {
        TournamentParticipant& participant = config.participants[index];
        normalizePlayer(participant.player);
        QString id = participant.id.trimmed();
        if (id.isEmpty() || participantIds.contains(id)) {
            const QString prefix = QStringLiteral("participant-%1").arg(index + 1);
            id = prefix;
            int suffix = 2;
            while (participantIds.contains(id)) {
                id = QStringLiteral("%1-%2").arg(prefix).arg(suffix++);
            }
        }
        participant.id = id;
        participantIds.insert(id);
    }

    if (config.tournamentType.compare(QStringLiteral("Gauntlet"),
                                      Qt::CaseInsensitive) == 0) {
        config.format = TournamentFormat::Gauntlet;
    } else if (config.tournamentType.compare(QStringLiteral("Round-robin"),
                                             Qt::CaseInsensitive) == 0) {
        config.format = TournamentFormat::RoundRobin;
    }
    config.tournamentType =
        config.format == TournamentFormat::Gauntlet
        ? QStringLiteral("Gauntlet")
        : QStringLiteral("Round-robin");
    config.gauntletParticipantId = config.gauntletParticipantId.trimmed();
    if (config.format == TournamentFormat::Gauntlet
        && config.gauntletParticipantId.isEmpty()
        && !config.participants.isEmpty()) {
        config.gauntletParticipantId = config.participants.first().id;
    }

    if (config.participants.size() >= 2) {
        config.match.player1 = config.participants.at(0).player;
        config.match.player2 = config.participants.at(1).player;
    }
}

ValidationError validateTournamentConfig(const TournamentConfig &config)
{
    TournamentConfig normalized = config;
    normalizeTournamentConfig(normalized);

    const ValidationError gameError = validateGame(normalized.match.game);
    if (gameError != ValidationError::None) {
        return gameError;
    }
    if (normalized.participants.size() < 2) {
        return ValidationError::TooFewTournamentParticipants;
    }

    QSet<QString> displayNames;
    bool gauntletParticipantFound =
        normalized.format != TournamentFormat::Gauntlet;
    for (const TournamentParticipant& participant : normalized.participants) {
        if (participant.player.type == PlayerType::Human
            && participant.player.name.trimmed().isEmpty()) {
            return ValidationError::MissingTournamentParticipantName;
        }
        if (participant.player.type == PlayerType::Engine
            && participant.player.enginePath.trimmed().isEmpty()) {
            return ValidationError::MissingTournamentParticipantEngine;
        }

        const QString displayName =
            participantDisplayName(participant.player).toCaseFolded();
        if (displayName.isEmpty()) {
            return ValidationError::MissingTournamentParticipantName;
        }
        if (displayNames.contains(displayName)) {
            return ValidationError::DuplicateTournamentParticipantName;
        }
        displayNames.insert(displayName);
        gauntletParticipantFound = gauntletParticipantFound
            || participant.id == normalized.gauntletParticipantId;
    }
    if (!gauntletParticipantFound) {
        return ValidationError::InvalidGauntletParticipant;
    }
    if (normalized.rounds < 1) {
        return ValidationError::InvalidRounds;
    }
    if (normalized.gamesPerPairing < 1) {
        return ValidationError::InvalidGamesPerPairing;
    }

    return ValidationError::None;
}

QString validationErrorTitle(ValidationError error)
{
    switch (error) {
    case ValidationError::MissingHumanNamePlayer1:
    case ValidationError::MissingHumanNamePlayer2:
        return QStringLiteral("Missing player name");
    case ValidationError::MissingEnginePlayer1:
    case ValidationError::MissingEnginePlayer2:
        return QStringLiteral("Missing engine");
    case ValidationError::MissingStartPosition:
        return QStringLiteral("Missing start position");
    case ValidationError::InvalidStartPosition:
        return QStringLiteral("Invalid start position");
    case ValidationError::MissingOpeningFile:
        return QStringLiteral("Missing opening file");
    case ValidationError::OpeningFileNotFound:
        return QStringLiteral("Opening file not found");
    case ValidationError::UnsupportedOpeningFile:
        return QStringLiteral("Unsupported opening file");
    case ValidationError::InvalidRounds:
        return QStringLiteral("Invalid rounds");
    case ValidationError::InvalidGamesPerPairing:
        return QStringLiteral("Invalid games per pairing");
    case ValidationError::TooFewTournamentParticipants:
        return QStringLiteral("Not enough participants");
    case ValidationError::MissingTournamentParticipantName:
        return QStringLiteral("Missing participant name");
    case ValidationError::MissingTournamentParticipantEngine:
        return QStringLiteral("Missing participant engine");
    case ValidationError::DuplicateTournamentParticipantName:
        return QStringLiteral("Duplicate participant");
    case ValidationError::InvalidGauntletParticipant:
        return QStringLiteral("Invalid main participant");
    case ValidationError::None:
        return QString();
    }

    return QString();
}

QString validationErrorMessage(ValidationError error)
{
    switch (error) {
    case ValidationError::MissingHumanNamePlayer1:
        return QStringLiteral("Enter a name for Player 1.");
    case ValidationError::MissingHumanNamePlayer2:
        return QStringLiteral("Enter a name for Player 2.");
    case ValidationError::MissingEnginePlayer1:
        return QStringLiteral("Select an engine for Player 1.");
    case ValidationError::MissingEnginePlayer2:
        return QStringLiteral("Select an engine for Player 2.");
    case ValidationError::MissingStartPosition:
        return QStringLiteral("Enter a start position or enable startpos.");
    case ValidationError::InvalidStartPosition:
        return QStringLiteral("Start position is not a valid FEN.");
    case ValidationError::MissingOpeningFile:
        return QStringLiteral("Select a PGN or EPD opening file.");
    case ValidationError::OpeningFileNotFound:
        return QStringLiteral("The selected opening file does not exist.");
    case ValidationError::UnsupportedOpeningFile:
        return QStringLiteral("Opening files must use the .pgn, .epd or .edp extension.");
    case ValidationError::InvalidRounds:
        return QStringLiteral("Rounds must be at least 1.");
    case ValidationError::InvalidGamesPerPairing:
        return QStringLiteral("Games per pairing must be at least 1.");
    case ValidationError::TooFewTournamentParticipants:
        return QStringLiteral("Add at least two tournament participants.");
    case ValidationError::MissingTournamentParticipantName:
        return QStringLiteral("Every human participant must have a name.");
    case ValidationError::MissingTournamentParticipantEngine:
        return QStringLiteral("Select an executable for every engine participant.");
    case ValidationError::DuplicateTournamentParticipantName:
        return QStringLiteral("Tournament participant names must be unique.");
    case ValidationError::InvalidGauntletParticipant:
        return QStringLiteral("Select a valid main participant for the gauntlet.");
    case ValidationError::None:
        return QString();
    }

    return QString();
}
