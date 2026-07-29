#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "game_replay.h"

namespace {

constexpr char kStartFen[] =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

bool writeTextFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Text)
        && file.write(contents) == contents.size();
}

} // namespace

class TestGameReplay : public QObject
{
    Q_OBJECT

private slots:
    void navigatesMovesCapturesAndClocks();
    void loadsLegacySessionJson();
    void loadsTournamentReport();
    void loadsMultiplePgnGames();
    void reconstructsEnPassantCapture();
    void rejectsIllegalRecordedMove();
};

void TestGameReplay::navigatesMovesCapturesAndClocks()
{
    ReplayGame game;
    game.white = QStringLiteral("White");
    game.black = QStringLiteral("Black");
    game.startFen = QString::fromLatin1(kStartFen);
    game.openingMoveCount = 2;
    game.movesUci = {
        QStringLiteral("e2e4"),
        QStringLiteral("d7d5"),
        QStringLiteral("e4d5"),
        QStringLiteral("d8d5")
    };

    for (qsizetype index = 0; index < game.movesUci.size(); ++index) {
        MoveRecord record;
        record.uci = game.movesUci.at(index);
        record.origin = index < game.openingMoveCount
            ? MoveOrigin::Opening
            : MoveOrigin::Human;
        game.moveRecords.append(record);
    }
    game.moveRecords[2].whiteTimeBeforeMs = 60000;
    game.moveRecords[2].blackTimeBeforeMs = 60000;
    game.moveRecords[2].whiteTimeAfterMs = 59000;
    game.moveRecords[2].blackTimeAfterMs = 60000;

    GameReplay replay;
    QString error;
    QVERIFY2(replay.load(game, &error), qPrintable(error));
    QCOMPARE(replay.currentPly(), 0);
    QCOMPARE(replay.totalPly(), 4);
    QCOMPARE(replay.position().get_FEN(), std::string(kStartFen));
    QCOMPARE(replay.whiteTimeMs(), qint64(60000));

    QVERIFY(replay.goToPly(3));
    QCOMPARE(replay.visibleMoves().size(), 3);
    QCOMPARE(replay.capturedPieces(),
             QVector<Xake::Piece>({Xake::B_PAWN}));
    QCOMPARE(replay.whiteTimeMs(), qint64(59000));

    const std::string positionAfterWhiteCapture =
        replay.position().get_FEN();
    QVERIFY(replay.goToPly(4));
    QCOMPARE(replay.capturedPieces(),
             QVector<Xake::Piece>({Xake::B_PAWN, Xake::W_PAWN}));
    QVERIFY(replay.goToPly(3));
    QCOMPARE(replay.position().get_FEN(), positionAfterWhiteCapture);
    QVERIFY(replay.goToPly(0));
    QCOMPARE(replay.position().get_FEN(), std::string(kStartFen));
}

void TestGameReplay::loadsLegacySessionJson()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        directory.filePath(QStringLiteral("session_legacy.json"));
    QVERIFY(writeTextFile(path, QByteArrayLiteral(R"json({
        "sessionType": "match",
        "startFen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "match": {
            "player1": {"name": "Alice"},
            "player2": {"name": "Engine"}
        },
        "opening": {
            "name": "King Pawn",
            "moves": ["e2e4", "e7e5"]
        },
        "moves": ["e2e4", "e7e5", "g1f3"],
        "clocks": {"whiteMs": 58000, "blackMs": 59000},
        "result": {
            "notation": "1-0",
            "termination": "checkmate"
        }
    })json")));

    const ReplayLoadResult loaded = loadReplayFile(path);
    QVERIFY2(loaded.success(), qPrintable(loaded.error));
    QCOMPARE(loaded.games.size(), 1);
    const ReplayGame& game = loaded.games.first();
    QCOMPARE(game.white, QStringLiteral("Alice"));
    QCOMPARE(game.black, QStringLiteral("Engine"));
    QCOMPARE(game.termination, QStringLiteral("checkmate"));
    QCOMPARE(game.openingMoveCount, 2);
    QCOMPARE(game.moveRecords.at(0).origin, MoveOrigin::Opening);
    QCOMPARE(game.moveRecords.at(2).origin, MoveOrigin::Imported);

    GameReplay replay;
    QString error;
    QVERIFY2(replay.load(game, &error), qPrintable(error));
    QVERIFY(replay.goToPly(replay.totalPly()));
    QCOMPARE(replay.whiteTimeMs(), qint64(58000));
    QCOMPARE(replay.blackTimeMs(), qint64(59000));
}

void TestGameReplay::loadsTournamentReport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        directory.filePath(QStringLiteral("tournament_report.json"));
    QVERIFY(writeTextFile(path, QByteArrayLiteral(R"json({
        "startFen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "games": [
            {
                "gameNumber": 1,
                "white": {"name": "Engine A"},
                "black": {"name": "Engine B"},
                "opening": {
                    "name": "Open Game",
                    "startFen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                    "moves": ["e2e4", "e7e5"]
                },
                "moves": ["e2e4", "e7e5"],
                "result": "1/2-1/2",
                "termination": "fifty_move_rule"
            },
            {
                "gameNumber": 2,
                "white": {"name": "Engine B"},
                "black": {"name": "Engine A"},
                "opening": {
                    "name": "Queen Pawn",
                    "startFen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                    "moves": []
                },
                "moves": ["d2d4"],
                "result": "*"
            }
        ]
    })json")));

    const ReplayLoadResult loaded = loadReplayFile(path);
    QVERIFY2(loaded.success(), qPrintable(loaded.error));
    QCOMPARE(loaded.games.size(), 2);
    QCOMPARE(loaded.games.at(0).gameNumber, 1);
    QCOMPARE(loaded.games.at(0).termination,
             QStringLiteral("fifty_move_rule"));
    QCOMPARE(loaded.games.at(1).white, QStringLiteral("Engine B"));
    QCOMPARE(loaded.games.at(1).openingName,
             QStringLiteral("Queen Pawn"));
}

void TestGameReplay::loadsMultiplePgnGames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("games.pgn"));
    QVERIFY(writeTextFile(path, QByteArrayLiteral(
        "[Event \"Match\"]\n"
        "[Round \"1\"]\n"
        "[White \"Alpha\"]\n"
        "[Black \"Beta\"]\n"
        "[Result \"1-0\"]\n"
        "[Termination \"checkmate\"]\n"
        "[Opening \"Italian Game\"]\n"
        "[XakeOpeningPlyCount \"2\"]\n\n"
        "1. e4 e5 2. Nf3 1-0\n\n"
        "[Event \"Match\"]\n"
        "[Round \"2\"]\n"
        "[White \"Beta\"]\n"
        "[Black \"Alpha\"]\n"
        "[Result \"*\"]\n\n"
        "1. d4 *\n")));

    const ReplayLoadResult loaded = loadReplayFile(path);
    QVERIFY2(loaded.success(), qPrintable(loaded.error));
    QCOMPARE(loaded.games.size(), 2);
    QCOMPARE(loaded.games.at(0).white, QStringLiteral("Alpha"));
    QCOMPARE(loaded.games.at(0).black, QStringLiteral("Beta"));
    QCOMPARE(loaded.games.at(0).result, QStringLiteral("1-0"));
    QCOMPARE(loaded.games.at(0).termination, QStringLiteral("checkmate"));
    QCOMPARE(loaded.games.at(0).openingMoveCount, 2);
    QCOMPARE(loaded.games.at(1).movesUci,
             QStringList({QStringLiteral("d2d4")}));
}

void TestGameReplay::reconstructsEnPassantCapture()
{
    ReplayGame game;
    game.startFen =
        QStringLiteral("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    game.movesUci = {QStringLiteral("e5d6")};

    GameReplay replay;
    QString error;
    QVERIFY2(replay.load(game, &error), qPrintable(error));
    QVERIFY(replay.goToPly(1));
    QCOMPARE(replay.capturedPieces(),
             QVector<Xake::Piece>({Xake::B_PAWN}));
    QVERIFY(replay.goToPly(0));
    QVERIFY(replay.capturedPieces().isEmpty());
}

void TestGameReplay::rejectsIllegalRecordedMove()
{
    ReplayGame game;
    game.startFen = QString::fromLatin1(kStartFen);
    game.movesUci = {QStringLiteral("e2e5")};

    GameReplay replay;
    QString error;
    QVERIFY(!replay.load(game, &error));
    QVERIFY(error.contains(QStringLiteral("move 1"),
                           Qt::CaseInsensitive));
    QVERIFY(error.contains(QStringLiteral("e2e5")));
}

QTEST_APPLESS_MAIN(TestGameReplay)

#include "test_game_replay.moc"
