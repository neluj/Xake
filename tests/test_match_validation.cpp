#include "match_settings_validation.h"

#include <QtTest/QtTest>

class TestMatchValidation : public QObject
{
    Q_OBJECT

private slots:
    void normalizeTrimsNames();
    void validateMissingHumanName();
    void validateMissingEngine();
    void validateMissingStartPosition();
    void validateTournamentFields();
    void validMatchConfig();
};

void TestMatchValidation::normalizeTrimsNames()
{
    MatchConfig config;
    config.player1.type = PlayerType::Human;
    config.player2.type = PlayerType::Human;
    config.player1.name = " ";
    config.player2.name = "  ";

    normalizeMatchConfig(config);

    QCOMPARE(config.player1.name, QString());
    QCOMPARE(config.player2.name, QString());
}

void TestMatchValidation::validateMissingHumanName()
{
    MatchConfig config;
    config.player1.type = PlayerType::Human;
    config.player1.name = " ";
    config.player2.type = PlayerType::Human;
    config.player2.name = "Name";
    config.game.useStartPos = true;
    config.game.startPosition = "startpos";

    QCOMPARE(validateMatchConfig(config), ValidationError::MissingHumanNamePlayer1);
}

void TestMatchValidation::validateMissingEngine()
{
    MatchConfig config;
    config.player1.type = PlayerType::Engine;
    config.player2.type = PlayerType::Human;
    config.player2.name = "Human";
    config.game.useStartPos = true;
    config.game.startPosition = "startpos";

    const ValidationError error = validateMatchConfig(config);
    QCOMPARE(error, ValidationError::MissingEnginePlayer1);
}

void TestMatchValidation::validateMissingStartPosition()
{
    MatchConfig config;
    config.player1.type = PlayerType::Human;
    config.player1.name = "Human 1";
    config.player2.type = PlayerType::Human;
    config.player2.name = "Human 2";
    config.game.useStartPos = false;
    config.game.startPosition = " ";

    const ValidationError error = validateMatchConfig(config);
    QCOMPARE(error, ValidationError::MissingStartPosition);
}

void TestMatchValidation::validateTournamentFields()
{
    TournamentConfig config;
    config.match.player1.type = PlayerType::Human;
    config.match.player1.name = "Human 1";
    config.match.player2.type = PlayerType::Human;
    config.match.player2.name = "Human 2";
    config.match.game.useStartPos = true;
    config.match.game.startPosition = "startpos";

    config.rounds = 0;
    config.gamesPerPairing = 1;
    QCOMPARE(validateTournamentConfig(config), ValidationError::InvalidRounds);

    config.rounds = 1;
    config.gamesPerPairing = 0;
    QCOMPARE(validateTournamentConfig(config), ValidationError::InvalidGamesPerPairing);
}

void TestMatchValidation::validMatchConfig()
{
    MatchConfig config;
    config.player1.type = PlayerType::Engine;
    config.player1.enginePath = "C:/engines/stockfish.exe";
    config.player2.type = PlayerType::Human;
    config.player2.name = "Human";
    config.game.useStartPos = true;
    config.game.startPosition = "startpos";

    QCOMPARE(validateMatchConfig(config), ValidationError::None);
}

QTEST_APPLESS_MAIN(TestMatchValidation)

#include "test_match_validation.moc"
