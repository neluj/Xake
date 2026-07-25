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

} // namespace

class TestTournamentRunner : public QObject
{
    Q_OBJECT

private slots:
    void playsAllGamesAndAlternatesColors();
    void drawsAtTournamentMoveLimit();
    void writesTournamentReportWithMoves();
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
                          kMateInOneFen,
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
                          kStartFen,
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
                         kMateInOneFen,
                         reportDir.path(),
                         QStringLiteral("report_test")));
    QVERIFY(controller.applyHumanMove(makeCandidate('f', '7', 'g', '7')));
    QTRY_VERIFY(finished);

    QCOMPARE(runner.reportFilePath(),
             reportDir.filePath(QStringLiteral("tournament_report.json")));

    QFile reportFile(runner.reportFilePath());
    QVERIFY(reportFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonDocument document = QJsonDocument::fromJson(reportFile.readAll());
    QVERIFY(document.isObject());

    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("status")).toString(), QStringLiteral("completed"));
    QCOMPARE(root.value(QStringLiteral("moveFormat")).toString(), QStringLiteral("uci"));

    const QJsonObject tournament = root.value(QStringLiteral("tournament")).toObject();
    const QJsonObject gameSettings = tournament.value(QStringLiteral("game")).toObject();
    QCOMPARE(gameSettings.value(QStringLiteral("baseTimeSeconds")).toInt(),
             tournamentConfig(QString::fromLatin1(kMateInOneFen), 1)
                 .match.game.baseTimeSeconds);

    const QJsonObject summary = root.value(QStringLiteral("summary")).toObject();
    QCOMPARE(summary.value(QStringLiteral("completedGames")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("whiteWins")).toInt(), 1);

    const QJsonArray games = root.value(QStringLiteral("games")).toArray();
    QCOMPARE(games.size(), 1);
    const QJsonObject game = games.at(0).toObject();
    QCOMPARE(game.value(QStringLiteral("status")).toString(), QStringLiteral("completed"));
    QCOMPARE(game.value(QStringLiteral("result")).toString(), QStringLiteral("1-0"));
    QCOMPARE(game.value(QStringLiteral("termination")).toString(), QStringLiteral("checkmate"));
    QCOMPARE(game.value(QStringLiteral("white")).toObject()
                 .value(QStringLiteral("name")).toString(),
             QStringLiteral("Player 1"));
    QCOMPARE(game.value(QStringLiteral("black")).toObject()
                 .value(QStringLiteral("name")).toString(),
             QStringLiteral("Player 2"));

    const QJsonArray moves = game.value(QStringLiteral("moves")).toArray();
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves.at(0).toString(), QStringLiteral("f7g7"));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestTournamentRunner test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_tournament_runner.moc"
