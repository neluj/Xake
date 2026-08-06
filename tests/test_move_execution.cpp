#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "game_controller.h"
#include "move.h"
#include "movegen.h"
#include "position.h"

using namespace Xake;

namespace {

constexpr char kStartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr char kInsufficientMaterialAfterCaptureFen[] = "4k3/8/8/8/8/8/3b4/2N1K3 w - - 0 1";
constexpr char kInsufficientMaterialAfterCaptureResultFen[] = "4k3/8/8/8/8/8/3K4/2N5 b - - 0 1";
constexpr char kTwoKnightMateFen[] = "7k/5K2/5N2/8/7N/8/8/8 w - - 0 1";
constexpr char kTimeoutWithoutMatingMaterialFen[] = "7k/8/8/8/8/8/8/R3K3 w - - 0 1";

int squareFrom(char file, char rank)
{
    return (rank - '1') * 8 + (file - 'a');
}

SpecialMove promotionMove(PieceType pieceType)
{
    switch (pieceType) {
    case KNIGHT:
        return PROMOTION_KNIGHT;
    case BISHOP:
        return PROMOTION_BISHOP;
    case ROOK:
        return PROMOTION_ROOK;
    case QUEEN:
        return PROMOTION_QUEEN;
    default:
        return NO_SPECIAL;
    }
}

Move makeCandidate(char fromFile,
                   char fromRank,
                   char toFile,
                   char toRank,
                   PieceType promotion = NO_PIECE_TYPE)
{
    return make_quiet_move(Square64(squareFrom(fromFile, fromRank)),
                           Square64(squareFrom(toFile, toRank)),
                           promotionMove(promotion));
}

Move findGeneratedMove(const Position& position,
                       char fromFile,
                       char fromRank,
                       char toFile,
                       char toRank,
                       PieceType promotion = NO_PIECE_TYPE)
{
    MoveGen::MoveList moveList;
    MoveGen::generate_pseudo_moves(position, moveList);

    const int fromSq = squareFrom(fromFile, fromRank);
    const int toSq = squareFrom(toFile, toRank);
    for (int i = 0; i < moveList.size; ++i) {
        const Move move = moveList.moves[i];
        if (move_from(move) == fromSq
            && move_to(move) == toSq
            && promoted_piece(move) == promotion) {
            return move;
        }
    }

    return NOMOVE;
}

MatchConfig humanVsHumanConfig(const QString& fen)
{
    MatchConfig config;
    config.player1.type = PlayerType::Human;
    config.player1.name = QStringLiteral("White");
    config.player2.type = PlayerType::Human;
    config.player2.name = QStringLiteral("Black");
    config.game.useStartPos = false;
    config.game.startPosition = fen;
    return config;
}

MatchConfig timedHumanVsHumanConfig(const QString& fen, int baseTimeSeconds, int incrementSeconds = 0)
{
    MatchConfig config = humanVsHumanConfig(fen);
    config.game.baseTimeSeconds = baseTimeSeconds;
    config.game.incrementSeconds = incrementSeconds;
    return config;
}

} // namespace

class TestMoveExecution : public QObject
{
    Q_OBJECT

private slots:
    void controllerTracksExecutedMoves();
    void controllerTracksCapturedPiecesInOrder();
    void controllerTracksEnPassantCapture();
    void controllerTracksCaptureFromOpeningMoves();
    void controllerClearsFinishedSessionData();
    void moveGenIncludesKnightMovesFromStart();
    void doMoveAppliesLegalGeneratedMove();
    void doMoveRejectsPseudoIllegalPinnedMove();
    void controllerRejectsMoveOutsideGeneratedList();
    void controllerRejectsPseudoIllegalMove();
    void controllerAppliesPromotionMove();
    void controllerStopsOnCheckmate();
    void controllerStopsBeforeResultNotification();
    void controllerStopsOnThreefoldRepetition();
    void controllerStopsOnFiftyMoveRule();
    void controllerStopsOnInsufficientMaterialAfterCapture();
    void controllerAllowsMateWithTwoKnights();
    void controllerStopsOnTimeout();
    void controllerDrawsOnTimeoutWithoutMatingMaterial();
    void controllerRejectsMoveAfterTimeout();
    void controllerFreezesClockWhenStopped();
    void controllerStopsWhenEngineExits();
    void controllerReportsEngineCrash();
    void controllerKeepsRunningOnEngineStandardError();
    void controllerReportsEngineWriteError();
    void controllerReportsMalformedEngineBestMove();
    void controllerReportsIllegalEngineBestMove();
    void controllerReportsNoMoveBestMove();
    void controllerReportsUnexpectedEngineBestMove();
    void controllerReportsHandshakeTimeout();
    void controllerWaitsForReadyAfterNewGame();
    void engineFailureMessages_data();
    void engineFailureMessages();
    void controllerIgnoresDuplicateEngineBestMove();
    void controllerWritesUnifiedEngineCommunicationLog();
    void controllerBoundsCommunicationHistoryWithoutTruncatingLog();
    void controllerAnnouncesEachEngineSearch();
    void controllerStartsAfterOpeningMoves();
    void controllerPauseFreezesClockAndRejectsMoves();
    void controllerAllowsHumanResignation();
    void controllerDiscardsBestMoveFromPausedSearch();

private:
    static void prepareWhiteEngineSearch(GameController& controller);
};

void TestMoveExecution::prepareWhiteEngineSearch(GameController& controller)
{
    controller.m_config.player1.type = PlayerType::Engine;
    controller.m_config.player1.name = QStringLiteral("TestEngine");
    controller.m_whiteSession.active = true;
    controller.m_whiteSession.readyOk = true;
    controller.m_whiteSession.searching = true;
    controller.m_whiteSession.failureReported = false;
}

void TestMoveExecution::controllerTracksExecutedMoves()
{
    GameController controller;
    QSignalSpy moveSpy(&controller, &GameController::movePlayed);
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)), kStartFen));

    QVERIFY(controller.applyHumanMove(makeCandidate('e', '2', 'e', '4')));
    QVERIFY(controller.applyHumanMove(makeCandidate('e', '7', 'e', '5')));

    QCOMPARE(controller.moveHistoryUci(),
             QStringList({QStringLiteral("e2e4"), QStringLiteral("e7e5")}));
    const QVector<MoveRecord> records = controller.moveRecords();
    QCOMPARE(records.size(), 2);
    QCOMPARE(records.at(0).origin, MoveOrigin::Human);
    QCOMPARE(records.at(0).movedPiece, W_PAWN);
    QCOMPARE(records.at(0).capturedPiece, NO_PIECE);
    QCOMPARE(records.at(1).origin, MoveOrigin::Human);
    QCOMPARE(records.at(1).movedPiece, B_PAWN);
    QCOMPARE(moveSpy.count(), 2);
    QCOMPARE(moveSpy.at(0).at(0).toInt(), 1);
    QCOMPARE(moveSpy.at(0).at(1).toString(), QStringLiteral("e2e4"));
    QCOMPARE(moveSpy.at(1).at(0).toInt(), 2);
    QCOMPARE(moveSpy.at(1).at(1).toString(), QStringLiteral("e7e5"));
}

void TestMoveExecution::controllerTracksCapturedPiecesInOrder()
{
    GameController controller;
    QVERIFY(controller.startMatch(
        humanVsHumanConfig(QString::fromLatin1(kStartFen)),
        kStartFen));

    QVERIFY(controller.applyHumanMove(makeCandidate('e', '2', 'e', '4')));
    QVERIFY(controller.applyHumanMove(makeCandidate('d', '7', 'd', '5')));
    QVERIFY(controller.applyHumanMove(makeCandidate('e', '4', 'd', '5')));
    QVERIFY(controller.applyHumanMove(makeCandidate('d', '8', 'd', '5')));

    QCOMPARE(controller.capturedPieces(),
             QVector<Piece>({B_PAWN, W_PAWN}));
}

void TestMoveExecution::controllerTracksEnPassantCapture()
{
    constexpr char kEnPassantFen[] =
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1";
    GameController controller;
    QVERIFY(controller.startMatch(
        humanVsHumanConfig(QString::fromLatin1(kEnPassantFen)),
        kEnPassantFen));

    QVERIFY(controller.applyHumanMove(makeCandidate('e', '5', 'd', '6')));
    QCOMPARE(controller.capturedPieces(), QVector<Piece>({B_PAWN}));
}

void TestMoveExecution::controllerTracksCaptureFromOpeningMoves()
{
    GameController controller;
    QVERIFY(controller.startMatch(
        humanVsHumanConfig(QString::fromLatin1(kStartFen)),
        kStartFen,
        QString(),
        QString(),
        0,
        QStringList({"e2e4", "d7d5", "e4d5"})));

    QCOMPARE(controller.capturedPieces(), QVector<Piece>({B_PAWN}));
}

void TestMoveExecution::controllerClearsFinishedSessionData()
{
    GameController controller;
    QVERIFY(controller.startMatch(
        timedHumanVsHumanConfig(QString::fromLatin1(kStartFen), 60, 1),
        kStartFen));
    QVERIFY(controller.applyHumanMove(makeCandidate('e', '2', 'e', '4')));
    QVERIFY(!controller.clearFinishedSessionData());

    controller.stopMatch();
    QVERIFY(controller.clearFinishedSessionData());

    QVERIFY(controller.moveHistoryUci().isEmpty());
    QVERIFY(controller.capturedPieces().isEmpty());
    QCOMPARE(controller.currentPosition().get_FEN(), std::string(kStartFen));
    QVERIFY(!controller.timeControlEnabled());
    QCOMPARE(controller.remainingTimeMs(WHITE), qint64(0));
    QCOMPARE(controller.remainingTimeMs(BLACK), qint64(0));
}

void TestMoveExecution::doMoveAppliesLegalGeneratedMove()
{
    Position position;
    QVERIFY(position.set_FEN(kStartFen));

    const Move move = findGeneratedMove(position, 'e', '2', 'e', '4');
    QVERIFY(move != NOMOVE);
    QCOMPARE(move_special(move), PAWN_START);

    QVERIFY(position.do_move(move));
    QCOMPARE(QString::fromStdString(position.get_FEN()),
             QStringLiteral("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"));
}

void TestMoveExecution::moveGenIncludesKnightMovesFromStart()
{
    Position position;
    QVERIFY(position.set_FEN(kStartFen));

    const Move b1a3 = findGeneratedMove(position, 'b', '1', 'a', '3');
    const Move b1c3 = findGeneratedMove(position, 'b', '1', 'c', '3');
    const Move g1f3 = findGeneratedMove(position, 'g', '1', 'f', '3');
    const Move g1h3 = findGeneratedMove(position, 'g', '1', 'h', '3');

    QVERIFY(b1a3 != NOMOVE);
    QVERIFY(b1c3 != NOMOVE);
    QVERIFY(g1f3 != NOMOVE);
    QVERIFY(g1h3 != NOMOVE);
}

void TestMoveExecution::doMoveRejectsPseudoIllegalPinnedMove()
{
    const std::string fen = "k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1";
    Position position;
    QVERIFY(position.set_FEN(fen));

    const Move move = findGeneratedMove(position, 'e', '2', 'd', '2');
    QVERIFY(move != NOMOVE);
    QCOMPARE(QString::fromStdString(position.get_FEN()), QString::fromStdString(fen));

    QVERIFY(!position.do_move(move));
    QCOMPARE(QString::fromStdString(position.get_FEN()), QString::fromStdString(fen));
}

void TestMoveExecution::controllerRejectsMoveOutsideGeneratedList()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)), kStartFen));

    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);

    QVERIFY(!controller.applyHumanMove(makeCandidate('e', '2', 'e', '5')));
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QString::fromLatin1(kStartFen));
}

void TestMoveExecution::controllerRejectsPseudoIllegalMove()
{
    const QString fen = QStringLiteral("k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(fen), fen.toStdString()));

    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);

    QVERIFY(!controller.applyHumanMove(makeCandidate('e', '2', 'd', '2')));
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()), fen);
}

void TestMoveExecution::controllerAppliesPromotionMove()
{
    const QString fen = QStringLiteral("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(fen), fen.toStdString()));

    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);

    QVERIFY(controller.applyHumanMove(makeCandidate('a', '7', 'a', '8', QUEEN)));
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QStringLiteral("Q3k3/8/8/8/8/8/8/4K3 b - - 0 1"));
}

void TestMoveExecution::controllerStopsOnCheckmate()
{
    const QString fen = QStringLiteral("7k/5Q2/6K1/8/8/8/8/8 w - - 0 1");
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(fen), fen.toStdString()));

    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    QSignalSpy stoppedSpy(&controller, &GameController::matchStopped);

    QVERIFY(controller.applyHumanMove(makeCandidate('f', '7', 'g', '7')));
    QCOMPARE(controller.isActive(), false);
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);

    const QList<QVariant> message = errorSpy.takeFirst();
    QCOMPARE(message[0].toString(), QStringLiteral("Checkmate"));
    QCOMPARE(message[1].toString(), QStringLiteral("White wins by checkmate."));
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QStringLiteral("7k/6Q1/6K1/8/8/8/8/8 b - - 1 1"));
}

void TestMoveExecution::controllerStopsBeforeResultNotification()
{
    const QString fen = QStringLiteral(
        "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1");
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(fen), fen.toStdString()));

    QStringList events;
    connect(&controller, &GameController::gameFinished, this,
            [&events](const GameResult&) {
        events.append(QStringLiteral("finished"));
    });
    connect(&controller, &GameController::matchStopped, this,
            [&events]() {
        events.append(QStringLiteral("stopped"));
    });
    connect(&controller, &GameController::errorOccurred, this,
            [&events](const QString&, const QString&) {
        events.append(QStringLiteral("notification"));
    });

    QVERIFY(controller.applyHumanMove(makeCandidate('f', '7', 'g', '7')));
    QCOMPARE(events,
             QStringList({QStringLiteral("finished"),
                          QStringLiteral("stopped"),
                          QStringLiteral("notification")}));
}

void TestMoveExecution::controllerStopsOnThreefoldRepetition()
{
    const QString fen = QStringLiteral("4k1n1/8/8/8/8/8/8/1N2K3 w - - 0 1");
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(fen), fen.toStdString()));

    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    QSignalSpy stoppedSpy(&controller, &GameController::matchStopped);

    QVERIFY(controller.applyHumanMove(makeCandidate('b', '1', 'a', '3')));
    QVERIFY(controller.applyHumanMove(makeCandidate('g', '8', 'h', '6')));
    QVERIFY(controller.applyHumanMove(makeCandidate('a', '3', 'b', '1')));
    QVERIFY(controller.applyHumanMove(makeCandidate('h', '6', 'g', '8')));
    QVERIFY(controller.applyHumanMove(makeCandidate('b', '1', 'a', '3')));
    QVERIFY(controller.applyHumanMove(makeCandidate('g', '8', 'h', '6')));
    QVERIFY(controller.applyHumanMove(makeCandidate('a', '3', 'b', '1')));
    QVERIFY(controller.applyHumanMove(makeCandidate('h', '6', 'g', '8')));

    QCOMPARE(controller.isActive(), false);
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QStringLiteral("4k1n1/8/8/8/8/8/8/1N2K3 w - - 8 5"));

    const QList<QVariant> message = errorSpy.takeFirst();
    QCOMPARE(message[0].toString(), QStringLiteral("Draw"));
    QCOMPARE(message[1].toString(), QStringLiteral("Draw by repetition."));
}

void TestMoveExecution::controllerStopsOnFiftyMoveRule()
{
    const QString fen = QStringLiteral("4k3/8/8/8/8/8/4Q3/4K3 w - - 99 1");
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(fen), fen.toStdString()));

    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    QSignalSpy stoppedSpy(&controller, &GameController::matchStopped);

    QVERIFY(controller.applyHumanMove(makeCandidate('e', '2', 'e', '3')));

    QCOMPARE(controller.isActive(), false);
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QStringLiteral("4k3/8/8/8/8/4Q3/8/4K3 b - - 100 1"));

    const QList<QVariant> message = errorSpy.takeFirst();
    QCOMPARE(message[0].toString(), QStringLiteral("Draw"));
    QCOMPARE(message[1].toString(), QStringLiteral("Draw by fifty-move rule."));
}

void TestMoveExecution::controllerStopsOnInsufficientMaterialAfterCapture()
{
    const QString fen = QString::fromLatin1(kInsufficientMaterialAfterCaptureFen);
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(fen), fen.toStdString()));

    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    QSignalSpy stoppedSpy(&controller, &GameController::matchStopped);

    // Kxd2 leaves king + knight versus king, which is insufficient material.
    QVERIFY(controller.applyHumanMove(makeCandidate('e', '1', 'd', '2')));

    QCOMPARE(controller.isActive(), false);
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QString::fromLatin1(kInsufficientMaterialAfterCaptureResultFen));

    const QList<QVariant> message = errorSpy.takeFirst();
    QCOMPARE(message[0].toString(), QStringLiteral("Draw"));
    QCOMPARE(message[1].toString(), QStringLiteral("Draw by insufficient material."));
}

void TestMoveExecution::controllerAllowsMateWithTwoKnights()
{
    const QString fen = QString::fromLatin1(kTwoKnightMateFen);
    GameController controller;
    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    QSignalSpy stoppedSpy(&controller, &GameController::matchStopped);

    QVERIFY(controller.startMatch(humanVsHumanConfig(fen), fen.toStdString()));
    QVERIFY(controller.isActive());
    QVERIFY(controller.applyHumanMove(makeCandidate('h', '4', 'g', '6')));

    QCOMPARE(controller.isActive(), false);
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);

    const QList<QVariant> message = errorSpy.takeFirst();
    QCOMPARE(message[0].toString(), QStringLiteral("Checkmate"));
    QCOMPARE(message[1].toString(), QStringLiteral("White wins by checkmate."));
}

void TestMoveExecution::controllerStopsOnTimeout()
{
    GameController controller;
    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    QSignalSpy stoppedSpy(&controller, &GameController::matchStopped);
    QVERIFY(controller.startMatch(timedHumanVsHumanConfig(QString::fromLatin1(kStartFen), 1), kStartFen));

    QTRY_VERIFY_WITH_TIMEOUT(controller.remainingTimeMs(WHITE) == 0, 3000);
    QVERIFY(QMetaObject::invokeMethod(&controller, "handleTurnTimeout", Qt::DirectConnection));

    QCOMPARE(controller.isActive(), false);
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(controller.remainingTimeMs(WHITE), 0);
    QCOMPARE(controller.remainingTimeMs(BLACK), 1000);

    const QList<QVariant> message = errorSpy.takeFirst();
    QCOMPARE(message[0].toString(), QStringLiteral("Time"));
    QCOMPARE(message[1].toString(), QStringLiteral("Black wins on time."));
}

void TestMoveExecution::controllerDrawsOnTimeoutWithoutMatingMaterial()
{
    const QString fen = QString::fromLatin1(kTimeoutWithoutMatingMaterialFen);
    GameController controller;
    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    QSignalSpy stoppedSpy(&controller, &GameController::matchStopped);
    QSignalSpy finishedSpy(&controller, &GameController::gameFinished);

    QVERIFY(controller.startMatch(timedHumanVsHumanConfig(fen, 1), fen.toStdString()));
    controller.m_whiteTimeMs = 0;
    controller.handleTurnTimeout();

    QCOMPARE(controller.isActive(), false);
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    const GameResult result = finishedSpy.takeFirst().at(0).value<GameResult>();
    QCOMPARE(result.outcome, GameOutcome::Draw);
    QCOMPARE(result.termination,
             GameTermination::TimeExpiredInsufficientMaterial);

    const QList<QVariant> message = errorSpy.takeFirst();
    QCOMPARE(message[0].toString(), QStringLiteral("Draw"));
    QCOMPARE(message[1].toString(),
             QStringLiteral("Draw on time: Black has no mating material."));
}

void TestMoveExecution::controllerRejectsMoveAfterTimeout()
{
    GameController controller;
    QVERIFY(controller.startMatch(timedHumanVsHumanConfig(QString::fromLatin1(kStartFen), 1), kStartFen));

    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    QSignalSpy stoppedSpy(&controller, &GameController::matchStopped);

    QTRY_VERIFY_WITH_TIMEOUT(controller.remainingTimeMs(WHITE) == 0, 3000);

    QVERIFY(!controller.applyHumanMove(makeCandidate('e', '2', 'e', '4')));
    QCOMPARE(controller.isActive(), false);
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QString::fromLatin1(kStartFen));
}

void TestMoveExecution::controllerFreezesClockWhenStopped()
{
    GameController controller;
    QVERIFY(controller.startMatch(
        timedHumanVsHumanConfig(QString::fromLatin1(kStartFen), 2),
        kStartFen));

    QTest::qWait(30);
    controller.stopMatch();
    const qint64 stoppedTime = controller.remainingTimeMs(WHITE);

    QVERIFY(stoppedTime < 2000);
    QTest::qWait(30);
    QCOMPARE(controller.remainingTimeMs(WHITE), stoppedTime);
}

void TestMoveExecution::controllerStopsWhenEngineExits()
{
    GameController controller;
    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    QSignalSpy failureSpy(&controller, &GameController::engineFailureOccurred);
    QSignalSpy stoppedSpy(&controller, &GameController::matchStopped);
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)), kStartFen));

    controller.m_config.player1.type = PlayerType::Engine;
    controller.m_config.player1.name = QStringLiteral("TestEngine");
    controller.m_whiteSession.active = true;
    controller.handleEngineExited(EngineSide::White, 17, QProcess::NormalExit);

    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(failureSpy.count(), 1);
    QCOMPARE(controller.isActive(), false);

    const QList<QVariant> failure = failureSpy.takeFirst();
    QCOMPARE(failure[0].value<EngineFailure>(), EngineFailure::UnexpectedExit);
    QCOMPARE(failure[1].value<EngineSide>(), EngineSide::White);

    const QList<QVariant> message = errorSpy.takeFirst();
    QCOMPARE(message[0].toString(), QStringLiteral("Engine error"));
    QVERIFY(message[1].toString().contains(QStringLiteral("White engine (TestEngine)")));
    QVERIFY(message[1].toString().contains(QStringLiteral("exited unexpectedly")));
    QVERIFY(message[1].toString().contains(QStringLiteral("exit code 17")));
}

void TestMoveExecution::controllerReportsEngineCrash()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)),
                                  kStartFen));
    prepareWhiteEngineSearch(controller);

    QSignalSpy failureSpy(&controller, &GameController::engineFailureOccurred);
    controller.m_whiteSession.lastErrorLine = QStringLiteral("access violation");
    controller.handleEngineExited(EngineSide::White, -1, QProcess::CrashExit);

    QCOMPARE(failureSpy.count(), 1);
    const QList<QVariant> failure = failureSpy.takeFirst();
    QCOMPARE(failure[0].value<EngineFailure>(), EngineFailure::ProcessCrashed);
    QVERIFY(failure[2].toString().contains(QStringLiteral("access violation")));
    QVERIFY(!controller.isActive());
}

void TestMoveExecution::controllerKeepsRunningOnEngineStandardError()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)),
                                  kStartFen));
    prepareWhiteEngineSearch(controller);

    QSignalSpy failureSpy(&controller, &GameController::engineFailureOccurred);
    QSignalSpy outputSpy(&controller, &GameController::engineOutputReceived);
    controller.handleEngineStandardError(
        EngineSide::White,
        QStringLiteral("diagnostic message"));

    QCOMPARE(failureSpy.count(), 0);
    QCOMPARE(outputSpy.count(), 1);
    QCOMPARE(outputSpy.takeFirst()[1].toString(),
             QStringLiteral("[stderr] diagnostic message"));
    QCOMPARE(controller.m_whiteSession.lastErrorLine,
             QStringLiteral("diagnostic message"));
    QVERIFY(controller.isActive());
}

void TestMoveExecution::controllerReportsEngineWriteError()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)),
                                  kStartFen));
    prepareWhiteEngineSearch(controller);

    QSignalSpy failureSpy(&controller, &GameController::engineFailureOccurred);
    controller.handleEngineProcessError(EngineSide::White,
                                        QProcess::WriteError,
                                        QStringLiteral("broken pipe"));

    QCOMPARE(failureSpy.count(), 1);
    const QList<QVariant> failure = failureSpy.takeFirst();
    QCOMPARE(failure[0].value<EngineFailure>(), EngineFailure::WriteError);
    QVERIFY(failure[2].toString().contains(QStringLiteral("broken pipe")));
    QVERIFY(!controller.isActive());
}

void TestMoveExecution::controllerReportsMalformedEngineBestMove()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)),
                                  kStartFen));
    prepareWhiteEngineSearch(controller);

    QSignalSpy failureSpy(&controller, &GameController::engineFailureOccurred);
    controller.handleBestMove(EngineSide::White, QStringLiteral("e2e4qz"));

    QCOMPARE(failureSpy.count(), 1);
    const QList<QVariant> failure = failureSpy.takeFirst();
    QCOMPARE(failure[0].value<EngineFailure>(), EngineFailure::MalformedBestMove);
    QVERIFY(failure[2].toString().contains(QStringLiteral("e2e4qz")));
    QVERIFY(!controller.isActive());
}

void TestMoveExecution::controllerReportsIllegalEngineBestMove()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)),
                                  kStartFen));
    prepareWhiteEngineSearch(controller);

    QSignalSpy failureSpy(&controller, &GameController::engineFailureOccurred);
    controller.handleBestMove(EngineSide::White, QStringLiteral("e2e5"));

    QCOMPARE(failureSpy.count(), 1);
    const QList<QVariant> failure = failureSpy.takeFirst();
    QCOMPARE(failure[0].value<EngineFailure>(), EngineFailure::IllegalBestMove);
    QVERIFY(failure[2].toString().contains(QStringLiteral("e2e5")));
    QVERIFY(!controller.isActive());
}

void TestMoveExecution::controllerReportsNoMoveBestMove()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)),
                                  kStartFen));
    prepareWhiteEngineSearch(controller);

    QSignalSpy failureSpy(&controller, &GameController::engineFailureOccurred);
    controller.handleBestMove(EngineSide::White, QStringLiteral("0000"));

    QCOMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.takeFirst()[0].value<EngineFailure>(),
             EngineFailure::NoMoveBestMove);
    QVERIFY(!controller.isActive());
}

void TestMoveExecution::controllerReportsUnexpectedEngineBestMove()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)),
                                  kStartFen));
    controller.m_config.player2.type = PlayerType::Engine;
    controller.m_config.player2.name = QStringLiteral("BlackEngine");
    controller.m_blackSession.active = true;
    controller.m_blackSession.readyOk = true;
    controller.m_blackSession.searching = true;

    QSignalSpy failureSpy(&controller, &GameController::engineFailureOccurred);
    controller.handleBestMove(EngineSide::Black, QStringLiteral("e7e5"));

    QCOMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.takeFirst()[0].value<EngineFailure>(),
             EngineFailure::UnexpectedBestMove);
    QVERIFY(!controller.isActive());
}

void TestMoveExecution::controllerReportsHandshakeTimeout()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)),
                                  kStartFen));
    controller.m_config.player1.type = PlayerType::Engine;
    controller.m_config.player1.name = QStringLiteral("SilentEngine");
    controller.m_whiteSession.active = true;
    controller.m_engineResponseTimeoutMs = 1;

    QSignalSpy failureSpy(&controller, &GameController::engineFailureOccurred);
    controller.armEngineResponseTimeout(EngineSide::White,
                                        EngineFailure::UciHandshakeTimeout);

    QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 1, 100);
    QCOMPARE(failureSpy.takeFirst()[0].value<EngineFailure>(),
             EngineFailure::UciHandshakeTimeout);
    QVERIFY(!controller.isActive());
}

void TestMoveExecution::controllerWaitsForReadyAfterNewGame()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)),
                                  kStartFen));

    EngineSession& session = controller.m_whiteSession;
    session.active = true;
    session.uciOk = true;

    controller.handleReadyOk(EngineSide::White);
    QVERIFY(session.newGameSent);
    QVERIFY(!session.readyOk);

    controller.handleReadyOk(EngineSide::White);
    QVERIFY(session.readyOk);
}

void TestMoveExecution::engineFailureMessages_data()
{
    QTest::addColumn<EngineFailure>("failure");
    QTest::addColumn<QString>("expectedText");

    QTest::newRow("client unavailable")
        << EngineFailure::ClientUnavailable << QStringLiteral("client is not available");
    QTest::newRow("start failed")
        << EngineFailure::StartFailed << QStringLiteral("could not be started");
    QTest::newRow("uci timeout")
        << EngineFailure::UciHandshakeTimeout << QStringLiteral("uciok");
    QTest::newRow("ready timeout")
        << EngineFailure::ReadyHandshakeTimeout << QStringLiteral("readyok");
    QTest::newRow("crash")
        << EngineFailure::ProcessCrashed << QStringLiteral("process crashed");
    QTest::newRow("unexpected exit")
        << EngineFailure::UnexpectedExit << QStringLiteral("exited unexpectedly");
    QTest::newRow("read")
        << EngineFailure::ReadError << QStringLiteral("read data");
    QTest::newRow("write")
        << EngineFailure::WriteError << QStringLiteral("send data");
    QTest::newRow("process timeout")
        << EngineFailure::ProcessTimeout << QStringLiteral("timed out");
    QTest::newRow("unknown process")
        << EngineFailure::UnknownProcessError << QStringLiteral("unknown process error");
    QTest::newRow("empty bestmove")
        << EngineFailure::EmptyBestMove << QStringLiteral("empty bestmove");
    QTest::newRow("no move")
        << EngineFailure::NoMoveBestMove << QStringLiteral("bestmove 0000");
    QTest::newRow("malformed bestmove")
        << EngineFailure::MalformedBestMove << QStringLiteral("malformed bestmove");
    QTest::newRow("illegal bestmove")
        << EngineFailure::IllegalBestMove << QStringLiteral("illegal bestmove");
    QTest::newRow("unexpected bestmove")
        << EngineFailure::UnexpectedBestMove << QStringLiteral("not that engine's turn");
}

void TestMoveExecution::engineFailureMessages()
{
    QFETCH(EngineFailure, failure);
    QFETCH(QString, expectedText);

    GameController controller;
    controller.m_config.player1.name = QStringLiteral("TestEngine");
    const QString message = controller.engineFailureMessage(
        EngineSide::White,
        failure,
        QStringLiteral("test detail"),
        QStringLiteral("e2e5"),
        17);

    QVERIFY2(message.contains(expectedText, Qt::CaseInsensitive),
             qPrintable(message));
}

void TestMoveExecution::controllerIgnoresDuplicateEngineBestMove()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)), kStartFen));

    controller.m_config.player1.type = PlayerType::Engine;
    controller.m_whiteSession.active = true;
    controller.m_whiteSession.readyOk = true;
    controller.m_whiteSession.searching = true;

    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    controller.handleBestMove(EngineSide::White, QStringLiteral("e2e4"));

    const QString positionAfterFirstMove =
        QString::fromStdString(controller.currentPosition().get_FEN());
    QCOMPARE(positionAfterFirstMove,
             QStringLiteral("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"));
    QCOMPARE(controller.m_whiteSession.searching, false);

    controller.handleBestMove(EngineSide::White, QStringLiteral("e2e4"));

    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             positionAfterFirstMove);
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(controller.isActive(), true);
}

void TestMoveExecution::controllerWritesUnifiedEngineCommunicationLog()
{
    QTemporaryDir logDir;
    QVERIFY(logDir.isValid());

    GameController controller;
    QSignalSpy logSpy(&controller, &GameController::communicationLogged);
    MatchConfig config = humanVsHumanConfig(QString::fromLatin1(kStartFen));
    config.player1.name = QStringLiteral("Alpha");
    config.player2.name = QStringLiteral("Beta");

    QVERIFY(controller.startMatch(config,
                                  kStartFen,
                                  logDir.path(),
                                  QStringLiteral("test_game")));
    controller.handleEngineCommunication(EngineSide::White,
                                         QStringLiteral(">>"),
                                         QStringLiteral("uci"));
    controller.handleEngineCommunication(EngineSide::Black,
                                         QStringLiteral("<<"),
                                         QStringLiteral("id name Beta"));

    const QString expectedPath = QDir(logDir.path())
        .filePath(QStringLiteral("uci_communication.log"));
    QCOMPARE(controller.communicationLogFilePath(), expectedPath);
    QCOMPARE(logSpy.count(), 3);

    QFile logFile(expectedPath);
    QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QStringList fileLines = QString::fromUtf8(logFile.readAll())
        .split('\n', Qt::SkipEmptyParts);
    QCOMPARE(fileLines, controller.communicationHistory());
    QCOMPARE(fileLines.size(), 3);
    QVERIFY(fileLines.at(0).contains(QStringLiteral("[Session] ## match test_game started")));
    QVERIFY(fileLines.at(1).contains(QStringLiteral("[White: Alpha] >> uci")));
    QVERIFY(fileLines.at(2).contains(QStringLiteral("[Black: Beta] << id name Beta")));
}

void TestMoveExecution::controllerBoundsCommunicationHistoryWithoutTruncatingLog()
{
    QTemporaryDir logDir;
    QVERIFY(logDir.isValid());

    GameController controller;
    QVERIFY(controller.startMatch(
        humanVsHumanConfig(QString::fromLatin1(kStartFen)),
        kStartFen,
        logDir.path(),
        QStringLiteral("bounded_history")));

    constexpr int extraLines = 25;
    for (int line = 0;
         line < GameController::kCommunicationHistoryLimit + extraLines;
         ++line) {
        controller.handleEngineCommunication(
            EngineSide::White,
            QStringLiteral("<<"),
            QStringLiteral("line %1").arg(line));
    }

    const QStringList history = controller.communicationHistory();
    QCOMPARE(history.size(), GameController::kCommunicationHistoryLimit);
    QVERIFY(history.first().endsWith(QStringLiteral("line 25")));
    QVERIFY(history.last().endsWith(
        QStringLiteral("line %1")
            .arg(GameController::kCommunicationHistoryLimit + extraLines - 1)));

    QFile logFile(controller.communicationLogFilePath());
    QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QStringList fileLines = QString::fromUtf8(logFile.readAll())
                                      .split('\n', Qt::SkipEmptyParts);
    QCOMPARE(fileLines.size(),
             GameController::kCommunicationHistoryLimit + extraLines + 1);
    QVERIFY(fileLines.first().contains(
        QStringLiteral("[Session] ## match bounded_history started")));
}

void TestMoveExecution::controllerAnnouncesEachEngineSearch()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)), kStartFen));

    controller.m_config.player1.type = PlayerType::Engine;
    controller.m_whiteSession.active = true;
    controller.m_whiteSession.readyOk = true;

    QSignalSpy searchSpy(&controller, &GameController::engineSearchStarted);
    controller.sendGoForSide(EngineSide::White);

    QCOMPARE(searchSpy.count(), 1);
    QCOMPARE(searchSpy.at(0).at(0).value<EngineSide>(), EngineSide::White);
    QVERIFY(controller.m_whiteSession.searching);

    controller.sendGoForSide(EngineSide::White);
    QCOMPARE(searchSpy.count(), 1);
}

void TestMoveExecution::controllerStartsAfterOpeningMoves()
{
    GameController controller;
    MatchConfig config = humanVsHumanConfig(QString::fromLatin1(kStartFen));

    QVERIFY(controller.startMatch(
        config,
        kStartFen,
        QString(),
        QString(),
        0,
        QStringList({"e2e4", "e7e5", "g1f3"})));

    QCOMPARE(controller.moveHistoryUci(),
             QStringList({"e2e4", "e7e5", "g1f3"}));
    QCOMPARE(controller.initialMoveCount(), 3);
    QCOMPARE(controller.moveRecords().size(), 3);
    for (const MoveRecord& record : controller.moveRecords()) {
        QCOMPARE(record.origin, MoveOrigin::Opening);
    }
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QStringLiteral(
                 "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2"));
}

void TestMoveExecution::controllerPauseFreezesClockAndRejectsMoves()
{
    GameController controller;
    QSignalSpy pauseSpy(&controller, &GameController::pauseChanged);
    QVERIFY(controller.startMatch(timedHumanVsHumanConfig(
                                      QString::fromLatin1(kStartFen), 2),
                                  kStartFen));

    QTest::qWait(30);
    QVERIFY(controller.pauseMatch());
    QVERIFY(controller.isPaused());
    const qint64 pausedTime = controller.remainingTimeMs(WHITE);

    QTest::qWait(50);
    QCOMPARE(controller.remainingTimeMs(WHITE), pausedTime);
    QVERIFY(!controller.applyHumanMove(makeCandidate('e', '2', 'e', '4')));
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QString::fromLatin1(kStartFen));

    QVERIFY(controller.resumeMatch());
    QVERIFY(!controller.isPaused());
    QTest::qWait(30);
    QVERIFY(controller.remainingTimeMs(WHITE) < pausedTime);
    QCOMPARE(pauseSpy.count(), 2);
}

void TestMoveExecution::controllerAllowsHumanResignation()
{
    GameController controller;
    QSignalSpy finishedSpy(&controller, &GameController::gameFinished);
    QVERIFY(controller.startMatch(
        humanVsHumanConfig(QString::fromLatin1(kStartFen)),
        kStartFen));

    QVERIFY(controller.canHumanResign());
    QVERIFY(controller.resignHumanPlayer());
    QCOMPARE(finishedSpy.count(), 1);

    const GameResult result =
        finishedSpy.takeFirst().at(0).value<GameResult>();
    QCOMPARE(result.outcome, GameOutcome::BlackWin);
    QCOMPARE(result.termination, GameTermination::Resignation);
    QVERIFY(!controller.isActive());
}

void TestMoveExecution::controllerDiscardsBestMoveFromPausedSearch()
{
    GameController controller;
    QVERIFY(controller.startMatch(humanVsHumanConfig(
                                      QString::fromLatin1(kStartFen)),
                                  kStartFen));

    controller.m_config.player1.type = PlayerType::Engine;
    controller.m_whiteSession.active = true;
    controller.m_whiteSession.readyOk = true;
    controller.m_whiteSession.searching = true;

    QVERIFY(controller.pauseMatch());
    QVERIFY(controller.m_whiteSession.discardBestMove);
    QVERIFY(controller.resumeMatch());

    controller.handleBestMove(EngineSide::White, QStringLiteral("e2e4"));
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QString::fromLatin1(kStartFen));
    QVERIFY(controller.m_whiteSession.searching);
    QVERIFY(!controller.m_whiteSession.discardBestMove);

    controller.handleBestMove(EngineSide::White, QStringLiteral("e2e4"));
    QCOMPARE(QString::fromStdString(controller.currentPosition().get_FEN()),
             QStringLiteral(
                 "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    qRegisterMetaType<EngineFailure>();
    qRegisterMetaType<EngineSide>();
    TestMoveExecution test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_move_execution.moc"
