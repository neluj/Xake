#include <QtTest>

#include "tournament_schedule.h"

namespace {

TournamentConfig tournament(int participantCount,
                            TournamentFormat format,
                            int cycles,
                            int gamesPerPairing)
{
    TournamentConfig config;
    config.match.game.useStartPos = true;
    config.match.game.startPosition = QStringLiteral("startpos");
    config.format = format;
    config.rounds = cycles;
    config.gamesPerPairing = gamesPerPairing;
    for (int index = 0; index < participantCount; ++index) {
        PlayerConfig player;
        player.type = PlayerType::Human;
        player.name = QStringLiteral("Player %1").arg(index + 1);
        config.participants.append({
            QStringLiteral("p%1").arg(index + 1),
            player
        });
    }
    if (!config.participants.isEmpty()) {
        config.gauntletParticipantId = config.participants.first().id;
    }
    return config;
}

QString pairingKey(const TournamentScheduledGame& game)
{
    return game.whiteParticipantId < game.blackParticipantId
        ? game.whiteParticipantId + QLatin1Char('-')
              + game.blackParticipantId
        : game.blackParticipantId + QLatin1Char('-')
              + game.whiteParticipantId;
}

} // namespace

class TestTournamentSchedule : public QObject
{
    Q_OBJECT

private slots:
    void roundRobinSchedulesEveryPairOnce();
    void roundRobinBalancesColorsAndHandlesByes();
    void gauntletOnlyPairsMainParticipant();
    void rejectsInvalidParticipantSets();
};

void TestTournamentSchedule::roundRobinSchedulesEveryPairOnce()
{
    const TournamentScheduleResult result =
        buildTournamentSchedule(
            tournament(4, TournamentFormat::RoundRobin, 1, 1));
    QVERIFY2(result.succeeded(), qPrintable(result.error));
    QCOMPARE(result.games.size(), 6);
    QCOMPARE(result.roundCount, 3);

    QSet<QString> pairings;
    QMap<int, QSet<QString>> playersByRound;
    QMap<QString, int> whiteGames;
    QMap<QString, int> blackGames;
    for (const TournamentScheduledGame& game : result.games) {
        pairings.insert(pairingKey(game));
        QVERIFY(!playersByRound[game.roundNumber].contains(
            game.whiteParticipantId));
        QVERIFY(!playersByRound[game.roundNumber].contains(
            game.blackParticipantId));
        playersByRound[game.roundNumber].insert(
            game.whiteParticipantId);
        playersByRound[game.roundNumber].insert(
            game.blackParticipantId);
        ++whiteGames[game.whiteParticipantId];
        ++blackGames[game.blackParticipantId];
    }
    QCOMPARE(pairings.size(), 6);
    for (int index = 1; index <= 4; ++index) {
        const QString participant =
            QStringLiteral("p%1").arg(index);
        QVERIFY(qAbs(whiteGames[participant]
                     - blackGames[participant]) <= 1);
    }
}

void TestTournamentSchedule::roundRobinBalancesColorsAndHandlesByes()
{
    const TournamentScheduleResult result =
        buildTournamentSchedule(
            tournament(3, TournamentFormat::RoundRobin, 1, 2));
    QVERIFY2(result.succeeded(), qPrintable(result.error));
    QCOMPARE(result.games.size(), 6);
    QCOMPARE(result.roundCount, 6);

    QMap<QString, QVector<TournamentScheduledGame>> gamesByPairing;
    for (const TournamentScheduledGame& game : result.games) {
        gamesByPairing[pairingKey(game)].append(game);
    }
    QCOMPARE(gamesByPairing.size(), 3);
    for (const auto& games : gamesByPairing) {
        QCOMPARE(games.size(), 2);
        QCOMPARE(games.at(0).whiteParticipantId,
                 games.at(1).blackParticipantId);
        QCOMPARE(games.at(0).blackParticipantId,
                 games.at(1).whiteParticipantId);
        QCOMPARE(games.at(0).openingGroup,
                 games.at(1).openingGroup);
    }
}

void TestTournamentSchedule::gauntletOnlyPairsMainParticipant()
{
    TournamentConfig config =
        tournament(4, TournamentFormat::Gauntlet, 2, 2);
    config.gauntletParticipantId = QStringLiteral("p2");
    const TournamentScheduleResult result =
        buildTournamentSchedule(config);
    QVERIFY2(result.succeeded(), qPrintable(result.error));
    QCOMPARE(result.games.size(), 12);

    QMap<QString, int> opponentGames;
    int mainWhiteGames = 0;
    int mainBlackGames = 0;
    for (const TournamentScheduledGame& game : result.games) {
        QVERIFY(game.whiteParticipantId == QStringLiteral("p2")
                || game.blackParticipantId == QStringLiteral("p2"));
        const QString opponent =
            game.whiteParticipantId == QStringLiteral("p2")
            ? game.blackParticipantId
            : game.whiteParticipantId;
        ++opponentGames[opponent];
        mainWhiteGames +=
            game.whiteParticipantId == QStringLiteral("p2") ? 1 : 0;
        mainBlackGames +=
            game.blackParticipantId == QStringLiteral("p2") ? 1 : 0;
    }
    QCOMPARE(opponentGames.size(), 3);
    QCOMPARE(mainWhiteGames, mainBlackGames);
    for (int games : opponentGames) {
        QCOMPARE(games, 4);
    }
}

void TestTournamentSchedule::rejectsInvalidParticipantSets()
{
    TournamentConfig config =
        tournament(1, TournamentFormat::RoundRobin, 1, 1);
    const TournamentScheduleResult result =
        buildTournamentSchedule(config);
    QVERIFY(!result.succeeded());
    QVERIFY(!result.error.isEmpty());
}

QTEST_APPLESS_MAIN(TestTournamentSchedule)

#include "test_tournament_schedule.moc"
