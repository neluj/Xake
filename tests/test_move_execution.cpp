#include <QtTest>
#include <QSignalSpy>

#include "game_controller.h"
#include "move.h"
#include "movegen.h"
#include "position.h"

using namespace ChessGame;

namespace {

constexpr char kStartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

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

QTEST_APPLESS_MAIN(TestMoveExecution)

#include "test_move_execution.moc"
