#include "match_settings_validation.h"

#include "position.h"

#include <QFileInfo>

using namespace Xake;

void normalizeMatchConfig(MatchConfig &config)
{
    config.player1.name = config.player1.name.trimmed();
    config.player2.name = config.player2.name.trimmed();
    config.player1.enginePath = config.player1.enginePath.trimmed();
    config.player2.enginePath = config.player2.enginePath.trimmed();
    config.game.startPosition = config.game.startPosition.trimmed();
    config.game.openingFilePath = config.game.openingFilePath.trimmed();
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
    if (config.game.useOpeningFile) {
        const QString openingPath = config.game.openingFilePath.trimmed();
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
    } else if (!config.game.useStartPos
               && config.game.startPosition.trimmed().isEmpty()) {
        return ValidationError::MissingStartPosition;
    }
    if (!config.game.useOpeningFile && !config.game.useStartPos) {
        const QString fenText = config.game.startPosition.trimmed();
        if (fenText.compare(QStringLiteral("startpos"), Qt::CaseInsensitive) != 0) {
            Position pos;
            if (!pos.set_FEN(fenText.toStdString())) {
                return ValidationError::InvalidStartPosition;
            }
        }
    }

    return ValidationError::None;
}

void normalizeTournamentConfig(TournamentConfig &config)
{
    normalizeMatchConfig(config.match);
}

ValidationError validateTournamentConfig(const TournamentConfig &config)
{
    const ValidationError matchError = validateMatchConfig(config.match);
    if (matchError != ValidationError::None) {
        return matchError;
    }
    if (config.rounds < 1) {
        return ValidationError::InvalidRounds;
    }
    if (config.gamesPerPairing < 1) {
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
    case ValidationError::None:
        return QString();
    }

    return QString();
}
