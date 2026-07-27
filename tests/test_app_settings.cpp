#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "app_settings.h"

namespace {

MatchConfig sampleMatch()
{
    MatchConfig match;
    match.player1.type = PlayerType::Engine;
    match.player1.enginePath = QStringLiteral("C:/engines/stockfish.exe");
    match.player2.type = PlayerType::Human;
    match.player2.name = QStringLiteral("Julen");
    match.game.timeControl = QStringLiteral("Custom");
    match.game.baseTimeSeconds = 90;
    match.game.incrementSeconds = 2;
    match.game.useStartPos = false;
    match.game.startPosition =
        QStringLiteral("8/8/8/8/8/8/4K3/7k w - - 0 1");
    match.game.useOpeningFile = true;
    match.game.openingFilePath =
        QStringLiteral("C:/openings/test.pgn");
    return match;
}

void compareMatch(const MatchConfig& actual, const MatchConfig& expected)
{
    QCOMPARE(actual.player1.type, expected.player1.type);
    QCOMPARE(actual.player1.name, expected.player1.name);
    QCOMPARE(actual.player1.enginePath, expected.player1.enginePath);
    QCOMPARE(actual.player2.type, expected.player2.type);
    QCOMPARE(actual.player2.name, expected.player2.name);
    QCOMPARE(actual.player2.enginePath, expected.player2.enginePath);
    QCOMPARE(actual.game.timeControl, expected.game.timeControl);
    QCOMPARE(actual.game.baseTimeSeconds, expected.game.baseTimeSeconds);
    QCOMPARE(actual.game.incrementSeconds, expected.game.incrementSeconds);
    QCOMPARE(actual.game.useStartPos, expected.game.useStartPos);
    QCOMPARE(actual.game.startPosition, expected.game.startPosition);
    QCOMPARE(actual.game.useOpeningFile, expected.game.useOpeningFile);
    QCOMPARE(actual.game.openingFilePath, expected.game.openingFilePath);
}

}

class TestAppSettings : public QObject
{
    Q_OBJECT

private slots:
    void emptySettingsHaveNoPreviousSessions();
    void persistsMatchAndTournamentConfigurations();
};

void TestAppSettings::emptySettingsHaveNoPreviousSessions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("settings.ini")),
                       QSettings::IniFormat);

    const AppState state = loadAppState(settings);
    QVERIFY(!state.hasLastMatch);
    QVERIFY(!state.hasLastTournament);
}

void TestAppSettings::persistsMatchAndTournamentConfigurations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        directory.filePath(QStringLiteral("settings.ini"));
    const MatchConfig match = sampleMatch();
    TournamentConfig tournament;
    tournament.match = match;
    tournament.tournamentType = QStringLiteral("Round-robin");
    tournament.rounds = 3;
    tournament.gamesPerPairing = 2;
    tournament.maxMoves = 150;

    {
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue(QStringLiteral("lastMatch/game/movesToGo"), 40);
        settings.setValue(QStringLiteral("lastTournament/game/movesToGo"), 40);
        saveLastMatch(settings, match);
        saveLastTournament(settings, tournament);
        QCOMPARE(settings.status(), QSettings::NoError);
        QVERIFY(!settings.contains(QStringLiteral("lastMatch/game/movesToGo")));
        QVERIFY(!settings.contains(QStringLiteral("lastTournament/game/movesToGo")));
    }

    QSettings restoredSettings(path, QSettings::IniFormat);
    const AppState restored = loadAppState(restoredSettings);
    QVERIFY(restored.hasLastMatch);
    QVERIFY(restored.hasLastTournament);
    compareMatch(restored.lastMatch, match);
    compareMatch(restored.lastTournament.match, match);
    QCOMPARE(restored.lastTournament.tournamentType,
             tournament.tournamentType);
    QCOMPARE(restored.lastTournament.rounds, tournament.rounds);
    QCOMPARE(restored.lastTournament.gamesPerPairing,
             tournament.gamesPerPairing);
    QCOMPARE(restored.lastTournament.maxMoves, tournament.maxMoves);
}

QTEST_APPLESS_MAIN(TestAppSettings)

#include "test_app_settings.moc"
