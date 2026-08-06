#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "game_controller.h"
#include "move.h"
#include "tournament_runner.h"

using namespace Xake;

namespace {

constexpr char kMateInOneFen[] = "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1";
constexpr char kStartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

int squareFrom(char file, char rank)
{
    return (rank - '1') * 8 + (file - 'a');
}

Move makeCandidate(char fromFile, char fromRank, char toFile, char toRank)
{
    return make_quiet_move(Square64(squareFrom(fromFile, fromRank)),
                           Square64(squareFrom(toFile, toRank)),
                           NO_SPECIAL);
}

TournamentConfig tournamentConfig(const QString& fen, int games, int maxMoves = 0)
{
    TournamentConfig config;
    config.match.player1.type = PlayerType::Human;
    config.match.player1.name = QStringLiteral("Player 1");
    config.match.player2.type = PlayerType::Human;
    config.match.player2.name = QStringLiteral("Player 2");
    config.match.game.useStartPos = false;
    config.match.game.startPosition = fen;
    config.tournamentType = QStringLiteral("Round-robin");
    config.rounds = 1;
    config.gamesPerPairing = games;
    config.maxMoves = maxMoves;
    return config;
}

TournamentConfig multiParticipantConfig(const QString& fen)
{
    TournamentConfig config = tournamentConfig(fen, 1);
    config.participants.clear();
    for (int index = 0; index < 3; ++index) {
        PlayerConfig player;
        player.type = PlayerType::Human;
        player.name = QStringLiteral("Player %1").arg(index + 1);
        config.participants.append({
            QStringLiteral("p%1").arg(index + 1),
            player
        });
    }
    config.match.player1 = config.participants.at(0).player;
    config.match.player2 = config.participants.at(1).player;
    config.rounds = 1;
    config.gamesPerPairing = 1;
    return config;
}

QVector<OpeningEntry> singleOpening(const QString& fen)
{
    return {OpeningEntry{1, QStringLiteral("Test opening"), fen, fen, {}}};
}

} // namespace

class TestTournamentRunner : public QObject
{
    Q_OBJECT

private slots:
    void playsAllGamesAndAlternatesColors();
    void drawsAtTournamentMoveLimit();
    void writesTournamentReportWithMoves();
    void persistsReportAtLifecycleBoundaries();
    void reusesEachOpeningWithBothColors();
    void pausesResumesAndStopsTournament();
    void playsRoundRobinWithThreeParticipants();
    void waitsBeforeStartingHumanGamesWhenRequested();
    void continuesAfterEngineFailure();
};

void TestTournamentRunner::playsAllGamesAndAlternatesColors()
{
    GameController controller;
    TournamentRunner runner(&controller);
    int startedGames = 0;
    MatchConfig secondGame;
    bool finished = false;
    TournamentSummary summary;

    connect(&runner, &TournamentRunner::tournamentGameStarted, this,
            [&startedGames, &secondGame](int gameNumber, int, const MatchConfig& match) {
        ++startedGames;
        if (gameNumber == 2) {
            secondGame = match;
        }
    });
    connect(&runner, &TournamentRunner::tournamentFinished, this,
            [&finished, &summary](const TournamentSummary& result) {
        summary = result;
        finished = true;
    });

    QVERIFY(runner.start(tournamentConfig(QString::fromLatin1(kMateInOneFen), 2),
                          singleOpening(QString::fromLatin1(kMateInOneFen)),
                          QString(),
                          QStringLiteral("test")));
    QCOMPARE(startedGames, 1);
    QCOMPARE(controller.matchConfig().player1.name, QStringLiteral("Player 1"));

    QVERIFY(controller.applyHumanMove(makeCandidate('f', '7', 'g', '7')));
    QTRY_COMPARE(startedGames, 2);
    QCOMPARE(runner.summary().completedGames, 1);
    QCOMPARE(runner.summary().player1Wins, 1);
    QCOMPARE(runner.summary().whiteWins, 1);
    QCOMPARE(runner.summary().blackWins, 0);
    QCOMPARE(secondGame.player1.name, QStringLiteral("Player 2"));
    QCOMPARE(secondGame.player2.name, QStringLiteral("Player 1"));
    QCOMPARE(runner.gameRecords().size(), 2);
    QCOMPARE(runner.gameRecords().at(0).colorsSwapped, false);
    QCOMPARE(runner.gameRecords().at(1).colorsSwapped, true);

    QVERIFY(controller.applyHumanMove(makeCandidate('f', '7', 'g', '7')));
    QTRY_VERIFY(finished);

    QCOMPARE(summary.totalGames, 2);
    QCOMPARE(summary.completedGames, 2);
    QCOMPARE(summary.player1Wins, 1);
    QCOMPARE(summary.player2Wins, 1);
    QCOMPARE(summary.draws, 0);
    QCOMPARE(summary.whiteWins, 2);
    QCOMPARE(summary.blackWins, 0);
    QCOMPARE(runner.isActive(), false);
}

void TestTournamentRunner::drawsAtTournamentMoveLimit()
{
    GameController controller;
    TournamentRunner runner(&controller);
    bool finished = false;
    GameResult result;
    TournamentSummary summary;

    connect(&runner, &TournamentRunner::tournamentGameFinished, this,
            [&result](int, const GameResult& gameResult) {
        result = gameResult;
    });
    connect(&runner, &TournamentRunner::tournamentFinished, this,
            [&finished, &summary](const TournamentSummary& tournamentSummary) {
        summary = tournamentSummary;
        finished = true;
    });

    QVERIFY(runner.start(tournamentConfig(QString::fromLatin1(kStartFen), 1, 1),
                          singleOpening(QString::fromLatin1(kStartFen)),
                          QString(),
                          QStringLiteral("test")));
    QVERIFY(controller.applyHumanMove(makeCandidate('e', '2', 'e', '4')));
    QVERIFY(controller.applyHumanMove(makeCandidate('e', '7', 'e', '5')));
    QTRY_VERIFY(finished);

    QCOMPARE(result.outcome, GameOutcome::Draw);
    QCOMPARE(result.termination, GameTermination::MoveLimit);
    QCOMPARE(summary.completedGames, 1);
    QCOMPARE(summary.draws, 1);
    QCOMPARE(summary.whiteWins, 0);
    QCOMPARE(summary.blackWins, 0);
}

void TestTournamentRunner::writesTournamentReportWithMoves()
{
    QTemporaryDir reportDir;
    QVERIFY(reportDir.isValid());

    GameController controller;
    TournamentRunner runner(&controller);
    bool finished = false;
    connect(&runner, &TournamentRunner::tournamentFinished, this,
            [&finished](const TournamentSummary&) {
        finished = true;
    });

    QVERIFY(runner.start(tournamentConfig(QString::fromLatin1(kMateInOneFen), 1),
                         singleOpening(QString::fromLatin1(kMateInOneFen)),
                         reportDir.path(),
                         QStringLiteral("report_test")));
    QVERIFY(controller.applyHumanMove(makeCandidate('f', '7', 'g', '7')));
    QTRY_VERIFY(finished);

    QCOMPARE(runner.reportFilePath(),
             reportDir.filePath(QStringLiteral("tournament_report.json")));
    QCOMPARE(runner.pgnFilePath(),
             reportDir.filePath(QStringLiteral("tournament.pgn")));

    QFile reportFile(runner.reportFilePath());
    QVERIFY(reportFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonDocument document = QJsonDocument::fromJson(reportFile.readAll());
    QVERIFY(document.isObject());

    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("formatVersion")).toInt(), 2);
    QCOMPARE(root.value(QStringLiteral("status")).toString(), QStringLiteral("completed"));
    QCOMPARE(root.value(QStringLiteral("moveFormat")).toString(), QStringLiteral("uci"));

    const QJsonObject tournament = root.value(QStringLiteral("tournament")).toObject();
    QCOMPARE(tournament.value(QStringLiteral("participants"))
                 .toArray().size(), 2);
    const QJsonObject gameSettings = tournament.value(QStringLiteral("game")).toObject();
    QCOMPARE(gameSettings.value(QStringLiteral("baseTimeSeconds")).toInt(),
             tournamentConfig(QString::fromLatin1(kMateInOneFen), 1)
                 .match.game.baseTimeSeconds);
    QVERIFY(!gameSettings.contains(QStringLiteral("movesToGo")));

    const QJsonObject summary = root.value(QStringLiteral("summary")).toObject();
    QCOMPARE(summary.value(QStringLiteral("completedGames")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("whiteWins")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("standings"))
                 .toArray().size(), 2);

    const QJsonArray games = root.value(QStringLiteral("games")).toArray();
    QCOMPARE(games.size(), 1);
    const QJsonObject game = games.at(0).toObject();
    QCOMPARE(game.value(QStringLiteral("status")).toString(), QStringLiteral("completed"));
    QCOMPARE(game.value(QStringLiteral("colorsSwapped")).toBool(), false);
    QCOMPARE(game.value(QStringLiteral("result")).toString(), QStringLiteral("1-0"));
    QCOMPARE(game.value(QStringLiteral("termination")).toString(), QStringLiteral("checkmate"));
    QCOMPARE(game.value(QStringLiteral("whiteParticipantId")).toString(),
             QStringLiteral("participant-1"));
    QCOMPARE(game.value(QStringLiteral("blackParticipantId")).toString(),
             QStringLiteral("participant-2"));
    QCOMPARE(game.value(QStringLiteral("white")).toObject()
                 .value(QStringLiteral("name")).toString(),
             QStringLiteral("Player 1"));
    QCOMPARE(game.value(QStringLiteral("black")).toObject()
                 .value(QStringLiteral("name")).toString(),
             QStringLiteral("Player 2"));

    const QJsonArray moves = game.value(QStringLiteral("moves")).toArray();
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves.at(0).toString(), QStringLiteral("f7g7"));

    const QJsonArray moveRecords =
        game.value(QStringLiteral("moveRecords")).toArray();
    QCOMPARE(moveRecords.size(), 1);
    QCOMPARE(moveRecords.at(0).toObject()
                 .value(QStringLiteral("origin")).toString(),
             QStringLiteral("human"));

    const QJsonObject opening = game.value(QStringLiteral("opening")).toObject();
    QCOMPARE(opening.value(QStringLiteral("index")).toInt(), 1);
    QCOMPARE(opening.value(QStringLiteral("name")).toString(),
             QStringLiteral("Test opening"));

    QFile pgnFile(runner.pgnFilePath());
    QVERIFY(pgnFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString pgn = QString::fromUtf8(pgnFile.readAll());
    QVERIFY(pgn.contains(QStringLiteral("[White \"Player 1\"]")));
    QVERIFY(pgn.contains(QStringLiteral("[Black \"Player 2\"]")));
    QVERIFY(pgn.contains(QStringLiteral("[Result \"1-0\"]")));
    QVERIFY(pgn.contains(QStringLiteral("1. Qg7# 1-0")));
}

void TestTournamentRunner::persistsReportAtLifecycleBoundaries()
{
    QTemporaryDir reportDir;
    QVERIFY(reportDir.isValid());

    GameController controller;
    TournamentRunner runner(&controller);
    QVERIFY(runner.start(tournamentConfig(QString::fromLatin1(kStartFen), 1),
                         singleOpening(QString::fromLatin1(kStartFen)),
                         reportDir.path(),
                         QStringLiteral("report_boundaries")));

    const auto reportMoves = [&runner]() {
        QFile reportFile(runner.reportFilePath());
        if (!reportFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QJsonArray{};
        }
        const QJsonDocument document =
            QJsonDocument::fromJson(reportFile.readAll());
        return document.object()
            .value(QStringLiteral("games")).toArray()
            .at(0).toObject()
            .value(QStringLiteral("moves")).toArray();
    };

    QCOMPARE(reportMoves().size(), 0);
    QVERIFY(controller.applyHumanMove(makeCandidate('e', '2', 'e', '4')));
    QCOMPARE(runner.gameRecords().constLast().moves,
             QStringList({QStringLiteral("e2e4")}));
    QCOMPARE(reportMoves().size(), 0);

    QVERIFY(runner.pause());
    const QJsonArray pausedMoves = reportMoves();
    QCOMPARE(pausedMoves.size(), 1);
    QCOMPARE(pausedMoves.at(0).toString(), QStringLiteral("e2e4"));

    QVERIFY(runner.stop());
}

void TestTournamentRunner::reusesEachOpeningWithBothColors()
{
    GameController controller;
    TournamentRunner runner(&controller);
    bool finished = false;
    int startedGames = 0;
    connect(&runner, &TournamentRunner::tournamentGameStarted, this,
            [&startedGames](int, int, const MatchConfig&) {
        ++startedGames;
    });
    connect(&runner, &TournamentRunner::tournamentFinished, this,
            [&finished](const TournamentSummary&) {
        finished = true;
    });

    const QString fen = QString::fromLatin1(kMateInOneFen);
    const QVector<OpeningEntry> openings{
        OpeningEntry{11, QStringLiteral("Opening A"), fen, fen, {}},
        OpeningEntry{22, QStringLiteral("Opening B"), fen, fen, {}}
    };
    QVERIFY(runner.start(tournamentConfig(fen, 4),
                         openings,
                         QString(),
                         QStringLiteral("opening_pairs")));

    for (int game = 0; game < 4; ++game) {
        QTRY_COMPARE(startedGames, game + 1);
        QVERIFY(controller.applyHumanMove(makeCandidate('f', '7', 'g', '7')));
    }
    QTRY_VERIFY(finished);

    const QVector<TournamentGameRecord> records = runner.gameRecords();
    QCOMPARE(records.size(), 4);
    QCOMPARE(records.at(0).openingIndex, 11);
    QCOMPARE(records.at(1).openingIndex, 11);
    QCOMPARE(records.at(2).openingIndex, 22);
    QCOMPARE(records.at(3).openingIndex, 22);
}

void TestTournamentRunner::pausesResumesAndStopsTournament()
{
    GameController controller;
    TournamentRunner runner(&controller);
    QSignalSpy pauseSpy(&runner, &TournamentRunner::pauseChanged);
    QSignalSpy abortedSpy(&runner, &TournamentRunner::tournamentAborted);

    QVERIFY(runner.start(tournamentConfig(QString::fromLatin1(kStartFen), 2),
                         singleOpening(QString::fromLatin1(kStartFen)),
                         QString(),
                         QStringLiteral("pause_stop")));
    QVERIFY(runner.isActive());
    QVERIFY(controller.isActive());

    QVERIFY(runner.pause());
    QVERIFY(runner.isPaused());
    QVERIFY(controller.isPaused());
    QVERIFY(!controller.applyHumanMove(makeCandidate('e', '2', 'e', '4')));

    QVERIFY(runner.resume());
    QVERIFY(!runner.isPaused());
    QVERIFY(!controller.isPaused());
    QVERIFY(controller.applyHumanMove(makeCandidate('e', '2', 'e', '4')));

    QVERIFY(runner.pause());
    QVERIFY(runner.stop());
    QVERIFY(!runner.isActive());
    QVERIFY(!runner.isPaused());
    QVERIFY(!controller.isActive());
    QCOMPARE(pauseSpy.count(), 4);
    QCOMPARE(abortedSpy.count(), 1);

    const QVector<TournamentGameRecord> records = runner.gameRecords();
    QCOMPARE(records.size(), 1);
    QVERIFY(records.constLast().aborted);
    QCOMPARE(records.constLast().moves, QStringList({QStringLiteral("e2e4")}));
    QCOMPARE(records.constLast().termination,
             GameTermination::Stopped);
    QCOMPARE(records.constLast().abortTitle,
             QStringLiteral("Tournament stopped"));
}

void TestTournamentRunner::playsRoundRobinWithThreeParticipants()
{
    GameController controller;
    TournamentRunner runner(&controller);
    int startedGames = 0;
    bool finished = false;
    QSet<QString> pairings;
    connect(&runner, &TournamentRunner::tournamentGameStarted,
            this,
            [&startedGames, &pairings](
                int, int, const MatchConfig& match) {
        ++startedGames;
        QStringList names{match.player1.name, match.player2.name};
        names.sort();
        pairings.insert(names.join(QLatin1Char('-')));
    });
    connect(&runner, &TournamentRunner::tournamentFinished,
            this, [&finished](const TournamentSummary&) {
        finished = true;
    });

    const QString fen = QString::fromLatin1(kMateInOneFen);
    QVERIFY(runner.start(
        multiParticipantConfig(fen),
        singleOpening(fen),
        QString(),
        QStringLiteral("three_players")));

    for (int game = 0; game < 3; ++game) {
        QTRY_COMPARE(startedGames, game + 1);
        QVERIFY(controller.applyHumanMove(
            makeCandidate('f', '7', 'g', '7')));
    }
    QTRY_VERIFY(finished);

    const TournamentSummary summary = runner.summary();
    QCOMPARE(summary.totalGames, 3);
    QCOMPARE(summary.completedGames, 3);
    QCOMPARE(summary.standings.size(), 3);
    QCOMPARE(pairings.size(), 3);
    for (const TournamentStanding& standing : summary.standings) {
        QCOMPARE(standing.games(), 2);
    }
}

void TestTournamentRunner::waitsBeforeStartingHumanGamesWhenRequested()
{
    GameController controller;
    TournamentRunner runner(&controller);
    runner.setHumanGameConfirmationEnabled(true);
    int readyRequests = 0;
    connect(&runner, &TournamentRunner::humanGameReadyRequested,
            this,
            [&readyRequests](int, int, const MatchConfig&) {
        ++readyRequests;
    });

    const QString fen = QString::fromLatin1(kStartFen);
    QVERIFY(runner.start(
        tournamentConfig(fen, 1),
        singleOpening(fen),
        QString(),
        QStringLiteral("human_ready")));
    QCOMPARE(readyRequests, 1);
    QVERIFY(runner.isWaitingForHumanGame());
    QVERIFY(!controller.isActive());

    QVERIFY(runner.startPendingHumanGame());
    QVERIFY(!runner.isWaitingForHumanGame());
    QVERIFY(controller.isActive());
    QVERIFY(runner.stop());
}

void TestTournamentRunner::continuesAfterEngineFailure()
{
    GameController controller;
    TournamentRunner runner(&controller);
    int startedGames = 0;
    connect(&runner, &TournamentRunner::tournamentGameStarted,
            this, [&startedGames](int, int, const MatchConfig&) {
        ++startedGames;
    });

    const QString fen = QString::fromLatin1(kStartFen);
    QVERIFY(runner.start(
        tournamentConfig(fen, 2),
        singleOpening(fen),
        QString(),
        QStringLiteral("engine_forfeit")));
    QCOMPARE(startedGames, 1);

    emit controller.engineFailureOccurred(
        EngineFailure::ProcessCrashed,
        EngineSide::White,
        QStringLiteral("White engine crashed."));
    emit controller.gameAborted(
        GameTermination::EngineFailure,
        QStringLiteral("Engine error"),
        QStringLiteral("White engine crashed."));
    controller.stopMatch();

    QTRY_COMPARE(startedGames, 2);
    QCOMPARE(runner.summary().completedGames, 1);
    const TournamentGameRecord first =
        runner.gameRecords().first();
    QVERIFY(first.completed);
    QCOMPARE(first.result.outcome, GameOutcome::BlackWin);
    QCOMPARE(first.termination, GameTermination::EngineFailure);
    QVERIFY(runner.stop());
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestTournamentRunner test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_tournament_runner.moc"
