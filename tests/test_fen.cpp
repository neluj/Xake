#include <QtTest>

#include "fen.h"
#include "position.h"
#include "types.h"

class TestFen : public QObject
{
    Q_OBJECT

private slots:
    void initialFen();
    void epFen();
    void invalidFen();
};

static int squareFrom(char file, char rank)
{
    return (rank - '1') * 8 + (file - 'a');
}

void TestFen::initialFen()
{
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Position pos;

    QVERIFY(setFromFen(pos, fen));
    QCOMPARE(static_cast<int>(pos.stm), static_cast<int>(WHITE));
    QCOMPARE(pos.castling, CASTLE_WK | CASTLE_WQ | CASTLE_BK | CASTLE_BQ);
    QCOMPARE(pos.epSquare, -1);
    QCOMPARE(pos.halfmove, 0);
    QCOMPARE(pos.fullmove, 1);
    QCOMPARE(QString::fromStdString(toFen(pos)), QString::fromStdString(fen));
}

void TestFen::epFen()
{
    const std::string fen = "8/8/8/3pP3/8/8/8/8 b - e6 12 34";
    Position pos;

    QVERIFY(setFromFen(pos, fen));
    QCOMPARE(static_cast<int>(pos.stm), static_cast<int>(BLACK));
    QCOMPARE(pos.castling, 0);
    QCOMPARE(pos.epSquare, squareFrom('e', '6'));
    QCOMPARE(pos.halfmove, 12);
    QCOMPARE(pos.fullmove, 34);
    QCOMPARE(QString::fromStdString(toFen(pos)), QString::fromStdString(fen));
}

void TestFen::invalidFen()
{
    const std::string fen = "8/8/8/8/8/8/8/9 w - - 0 1";
    Position pos;

    QVERIFY(!setFromFen(pos, fen));
}

QTEST_APPLESS_MAIN(TestFen)

#include "test_fen.moc"
