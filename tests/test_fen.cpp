#include <QtTest>

#include "position.h"
#include "types.h"

using namespace ChessGame;

class TestFen : public QObject
{
    Q_OBJECT

private slots:
    void castlingCompatibility();
    void initialFen();
    void epFen();
    void invalidEpFen();
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

    QVERIFY(pos.set_FEN(fen));
    QCOMPARE(static_cast<int>(pos.get_side_to_move()), static_cast<int>(WHITE));
    QCOMPARE(static_cast<int>(pos.get_castling_right()),
             static_cast<int>(CASTLE_WK | CASTLE_WQ | CASTLE_BK | CASTLE_BQ));
    QCOMPARE(static_cast<int>(pos.get_enpassant_square()), static_cast<int>(SQ64_NO_SQUARE));
    QCOMPARE(pos.get_fifty_moves_counter(), static_cast<unsigned short>(0));
    QCOMPARE(pos.get_moves_counter(), static_cast<unsigned short>(1));
    QCOMPARE(QString::fromStdString(pos.get_FEN()), QString::fromStdString(fen));
}

void TestFen::castlingCompatibility()
{
    QCOMPARE(static_cast<int>(CASTLE_WK), static_cast<int>(WKCA));
    QCOMPARE(static_cast<int>(CASTLE_WQ), static_cast<int>(WQCA));
    QCOMPARE(static_cast<int>(CASTLE_BK), static_cast<int>(BKCA));
    QCOMPARE(static_cast<int>(CASTLE_BQ), static_cast<int>(BQCA));

    const CastlingRight allCurrent = WKCA | WQCA | BKCA | BQCA;
    const CastlingRight allLegacy = CASTLE_WK | CASTLE_WQ | CASTLE_BK | CASTLE_BQ;
    QCOMPARE(static_cast<int>(allCurrent), static_cast<int>(allLegacy));
}

void TestFen::epFen()
{
    const std::string fen = "4k3/8/8/3pP3/8/8/8/4K3 w - d6 12 34";
    Position pos;

    QVERIFY(pos.set_FEN(fen));
    QCOMPARE(static_cast<int>(pos.get_side_to_move()), static_cast<int>(WHITE));
    QCOMPARE(static_cast<int>(pos.get_castling_right()), static_cast<int>(NO_RIGHT));
    QCOMPARE(static_cast<int>(pos.get_enpassant_square()), squareFrom('d', '6'));
    QCOMPARE(pos.get_fifty_moves_counter(), static_cast<unsigned short>(12));
    QCOMPARE(pos.get_moves_counter(), static_cast<unsigned short>(34));
    QCOMPARE(QString::fromStdString(pos.get_FEN()), QString::fromStdString(fen));
}

void TestFen::invalidEpFen()
{
    const std::string fen = "4k3/8/8/3pP3/8/8/8/4K3 b - e6 12 34";
    Position pos;

    QVERIFY(!pos.set_FEN(fen));
}

void TestFen::invalidFen()
{
    const std::string fen = "8/8/8/8/8/8/8/9 w - - 0 1";
    Position pos;

    QVERIFY(!pos.set_FEN(fen));
}

QTEST_APPLESS_MAIN(TestFen)

#include "test_fen.moc"
