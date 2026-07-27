#include <QtTest>

#include "boardwidget.h"
#include "move.h"
#include "position.h"

using namespace Xake;

namespace {

constexpr char kStartFen[] =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

int squareFrom(char file, char rank)
{
    return (rank - '1') * 8 + (file - 'a');
}

bool containsDestination(const QVector<Move>& moves, int destination)
{
    for (const Move move : moves) {
        if (move_to(move) == destination) {
            return true;
        }
    }
    return false;
}

} // namespace

class TestBoardWidget : public QObject
{
    Q_OBJECT

private slots:
    void selectionProvidesLegalDestinations();
    void illegalDestinationIsIgnored();
    void disabledInputIgnoresClicks();

private:
    static QPoint squareCenter(BoardWidget& board, int square);
    static void setStartPosition(BoardWidget& board);
};

QPoint TestBoardWidget::squareCenter(BoardWidget& board, int square)
{
    QRect boardRect;
    int cellSize = 0;
    int labelMargin = 0;
    const bool hasGeometry = board.boardGeometry(boardRect, cellSize, labelMargin);
    Q_ASSERT(hasGeometry);
    Q_UNUSED(labelMargin);

    const int file = square % 8;
    const int rank = square / 8;
    return QPoint(boardRect.left() + file * cellSize + cellSize / 2,
                  boardRect.top() + (7 - rank) * cellSize + cellSize / 2);
}

void TestBoardWidget::setStartPosition(BoardWidget& board)
{
    Position position;
    QVERIFY(position.set_FEN(kStartFen));
    board.resize(400, 400);
    board.setPosition(position);
    board.setMoveInputEnabled(true);
}

void TestBoardWidget::selectionProvidesLegalDestinations()
{
    BoardWidget board;
    setStartPosition(board);

    QVector<Move> requestedMoves;
    connect(&board, &BoardWidget::moveRequested, this,
            [&requestedMoves](Move move) {
        requestedMoves.append(move);
    });

    const int e2 = squareFrom('e', '2');
    const int e3 = squareFrom('e', '3');
    const int e4 = squareFrom('e', '4');
    QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier,
                      squareCenter(board, e2));

    QCOMPARE(board.m_selectedSq, e2);
    QCOMPARE(board.m_legalMoves.size(), 2);
    QVERIFY(containsDestination(board.m_legalMoves, e3));
    QVERIFY(containsDestination(board.m_legalMoves, e4));

    QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier,
                      squareCenter(board, e4));

    QCOMPARE(requestedMoves.size(), 1);
    QCOMPARE(move_from(requestedMoves.constFirst()), e2);
    QCOMPARE(move_to(requestedMoves.constFirst()), e4);
    QCOMPARE(move_special(requestedMoves.constFirst()), PAWN_START);
    QCOMPARE(board.m_selectedSq, -1);
    QVERIFY(board.m_legalMoves.isEmpty());
}

void TestBoardWidget::illegalDestinationIsIgnored()
{
    BoardWidget board;
    setStartPosition(board);

    int requestCount = 0;
    connect(&board, &BoardWidget::moveRequested, this,
            [&requestCount](Move) {
        ++requestCount;
    });

    const int b1 = squareFrom('b', '1');
    QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier,
                      squareCenter(board, b1));
    QCOMPARE(board.m_selectedSq, b1);

    QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier,
                      squareCenter(board, squareFrom('b', '3')));

    QCOMPARE(requestCount, 0);
    QCOMPARE(board.m_selectedSq, b1);
    QVERIFY(containsDestination(board.m_legalMoves, squareFrom('a', '3')));
    QVERIFY(containsDestination(board.m_legalMoves, squareFrom('c', '3')));
}

void TestBoardWidget::disabledInputIgnoresClicks()
{
    BoardWidget board;
    setStartPosition(board);
    board.setMoveInputEnabled(false);

    int requestCount = 0;
    connect(&board, &BoardWidget::moveRequested, this,
            [&requestCount](Move) {
        ++requestCount;
    });

    QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier,
                      squareCenter(board, squareFrom('e', '2')));

    QCOMPARE(requestCount, 0);
    QCOMPARE(board.m_selectedSq, -1);
    QVERIFY(board.m_legalMoves.isEmpty());
}

QTEST_MAIN(TestBoardWidget)

#include "test_boardwidget.moc"
