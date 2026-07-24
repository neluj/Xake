#include <QCoreApplication>
#include <QtTest>
#include <QSignalSpy>

#include "game_controller.h"
#include "move.h"
#include "movegen.h"
#include "position.h"

using namespace Xake;

namespace {

constexpr char kStartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr char kInsufficientMaterialAfterCaptureFen[] = "4k3/8/8/8/8/8/3b4/2N1K3 w - - 0 1";
constexpr char kInsufficientMaterialAfterCaptureResultFen[] = "4k3/8/8/8/8/8/3K4/2N5 b - - 0 1";

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
    void moveGenIncludesKnightMovesFromStart();
    void doMoveAppliesLegalGeneratedMove();
    void doMoveRejectsPseudoIllegalPinnedMove();
    void controllerRejectsMoveOutsideGeneratedList();
    void controllerRejectsPseudoIllegalMove();
    void controllerAppliesPromotionMove();
    void controllerStopsOnCheckmate();
    void controllerStopsOnThreefoldRepetition();
    void controllerStopsOnFiftyMoveRule();
    void controllerStopsOnInsufficientMaterialAfterCapture();
    void controllerStopsOnTimeout();
    void controllerRejectsMoveAfterTimeout();
    void controllerStopsWhenEngineExits();
};

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
    QCOMPARE(errorSpy.count(), 1);
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
    QCOMPARE(errorSpy.count(), 1);
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

void TestMoveExecution::controllerStopsWhenEngineExits()
{
    GameController controller;
    QSignalSpy errorSpy(&controller, &GameController::errorOccurred);
    QSignalSpy stoppedSpy(&controller, &GameController::matchStopped);
    QVERIFY(controller.startMatch(humanVsHumanConfig(QString::fromLatin1(kStartFen)), kStartFen));

    controller.m_whiteSession.active = true;
    controller.handleEngineError(EngineSide::White, QStringLiteral("simulated engine failure"));
    controller.handleEngineExited(EngineSide::White, 17, QProcess::NormalExit);

    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(controller.isActive(), false);

    const QList<QVariant> message = errorSpy.takeFirst();
    QCOMPARE(message[0].toString(), QStringLiteral("Engine error"));
    QVERIFY(message[1].toString().contains(QStringLiteral("White engine exited")));
    QVERIFY(message[1].toString().contains(QStringLiteral("code 17")));
    QVERIFY(message[1].toString().contains(QStringLiteral("simulated engine failure")));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestMoveExecution test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_move_execution.moc"
