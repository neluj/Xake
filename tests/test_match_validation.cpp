#include "match_settings_validation.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestMatchValidation : public QObject
{
    Q_OBJECT

private slots:
    void normalizeTrimsNames();
    void validateMissingHumanName();
    void validateMissingEngine();
    void validateMissingStartPosition();
    void validateInvalidFen();
    void validateTournamentFields();
    void validateOpeningFile();
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

    const ValidationError error = validateMatchConfig(config);
    QCOMPARE(error, ValidationError::MissingHumanNamePlayer1);
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

void TestMatchValidation::validateInvalidFen()
{
    MatchConfig config;
    config.player1.type = PlayerType::Human;
    config.player1.name = "Human 1";
    config.player2.type = PlayerType::Human;
    config.player2.name = "Human 2";
    config.game.useStartPos = false;
    config.game.startPosition = "8/8/8/8/8/8/8/9 w - - 0 1";

    const ValidationError error = validateMatchConfig(config);
    QCOMPARE(error, ValidationError::InvalidStartPosition);
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
    ValidationError error = validateTournamentConfig(config);
    QCOMPARE(error, ValidationError::InvalidRounds);

    config.rounds = 1;
    config.gamesPerPairing = 0;
    error = validateTournamentConfig(config);
    QCOMPARE(error, ValidationError::InvalidGamesPerPairing);
}

void TestMatchValidation::validateOpeningFile()
{
    MatchConfig config;
    config.player1.type = PlayerType::Human;
    config.player1.name = QStringLiteral("Human 1");
    config.player2.type = PlayerType::Human;
    config.player2.name = QStringLiteral("Human 2");
    config.game.useOpeningFile = true;

    QCOMPARE(validateMatchConfig(config), ValidationError::MissingOpeningFile);

    config.game.openingFilePath = QStringLiteral("missing.pgn");
    QCOMPARE(validateMatchConfig(config), ValidationError::OpeningFileNotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString invalidPath = directory.filePath(QStringLiteral("openings.txt"));
    QFile invalidFile(invalidPath);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly));
    invalidFile.close();
    config.game.openingFilePath = invalidPath;
    QCOMPARE(validateMatchConfig(config), ValidationError::UnsupportedOpeningFile);

    const QString validPath = directory.filePath(QStringLiteral("openings.pgn"));
    QFile validFile(validPath);
    QVERIFY(validFile.open(QIODevice::WriteOnly));
    validFile.close();
    config.game.openingFilePath = validPath;
    QCOMPARE(validateMatchConfig(config), ValidationError::None);
}

void TestMatchValidation::validMatchConfig()
{
    MatchConfig config;
    config.player1.type = PlayerType::Engine;
    config.player1.enginePath = "C:/engines/stockfish.exe";
    config.player2.type = PlayerType::Human;
    config.player2.name = "Human";
    config.game.useStartPos = false;
    config.game.startPosition =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    const ValidationError error = validateMatchConfig(config);
    QCOMPARE(error, ValidationError::None);
}

QTEST_APPLESS_MAIN(TestMatchValidation)

#include "test_match_validation.moc"
