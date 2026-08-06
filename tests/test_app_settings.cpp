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
    void migratesLegacyTwoParticipantTournament();
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
    PlayerConfig thirdPlayer;
    thirdPlayer.type = PlayerType::Engine;
    thirdPlayer.name = QStringLiteral("Dragon");
    thirdPlayer.enginePath =
        QStringLiteral("C:/engines/dragon.exe");
    tournament.participants = {
        {QStringLiteral("stockfish"), match.player1},
        {QStringLiteral("julen"), match.player2},
        {QStringLiteral("dragon"), thirdPlayer}
    };
    tournament.format = TournamentFormat::Gauntlet;
    tournament.gauntletParticipantId = QStringLiteral("stockfish");
    tournament.tournamentType = QStringLiteral("Gauntlet");
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
    QCOMPARE(restored.lastTournament.format,
             TournamentFormat::Gauntlet);
    QCOMPARE(restored.lastTournament.gauntletParticipantId,
             QStringLiteral("stockfish"));
    QCOMPARE(restored.lastTournament.participants.size(), 3);
    QCOMPARE(restored.lastTournament.participants.at(2).id,
             QStringLiteral("dragon"));
    QCOMPARE(restored.lastTournament.participants.at(2).player.name,
             QStringLiteral("Dragon"));
}

void TestAppSettings::migratesLegacyTwoParticipantTournament()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(
        directory.filePath(QStringLiteral("legacy.ini")),
        QSettings::IniFormat);
    settings.setValue(QStringLiteral("lastTournament/available"), true);
    settings.setValue(QStringLiteral("lastTournament/player1/type"),
                      static_cast<int>(PlayerType::Human));
    settings.setValue(QStringLiteral("lastTournament/player1/name"),
                      QStringLiteral("Alice"));
    settings.setValue(QStringLiteral("lastTournament/player2/type"),
                      static_cast<int>(PlayerType::Human));
    settings.setValue(QStringLiteral("lastTournament/player2/name"),
                      QStringLiteral("Bob"));
    settings.setValue(
        QStringLiteral("lastTournament/game/useStartPos"), true);
    settings.setValue(
        QStringLiteral("lastTournament/game/startPosition"),
        QStringLiteral("startpos"));
    settings.setValue(
        QStringLiteral("lastTournament/tournamentType"),
        QStringLiteral("Round-robin"));
    settings.setValue(QStringLiteral("lastTournament/rounds"), 2);
    settings.setValue(
        QStringLiteral("lastTournament/gamesPerPairing"), 2);
    settings.sync();

    const AppState state = loadAppState(settings);
    QVERIFY(state.hasLastTournament);
    QCOMPARE(state.lastTournament.participants.size(), 2);
    QCOMPARE(state.lastTournament.participants.at(0).player.name,
             QStringLiteral("Alice"));
    QCOMPARE(state.lastTournament.participants.at(1).player.name,
             QStringLiteral("Bob"));
    QCOMPARE(state.lastTournament.format,
             TournamentFormat::RoundRobin);
}


QTEST_APPLESS_MAIN(TestAppSettings)

#include "test_app_settings.moc"
