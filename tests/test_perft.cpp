#include <QtTest>

#include "perft.h"

using namespace Xake;

namespace {

constexpr char kStartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr char kKiwipeteFen[] = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
constexpr char kEnPassantFen[] = "1k6/8/8/5pP1/4K1P1/8/8/8 w - f6 0 1";

} // namespace

class TestPerft : public QObject
{
    Q_OBJECT

private slots:
    void startPosition();
    void divideMatchesTotal();
    void kiwipete();
    void enPassantRegression();
};

void TestPerft::startPosition()
{
    Position position;
    QVERIFY(position.set_FEN(kStartFen));

    QCOMPARE(qulonglong(perft_root(position, 1)), qulonglong(20));
    QCOMPARE(qulonglong(perft_root(position, 2)), qulonglong(400));
    QCOMPARE(qulonglong(perft_root(position, 3)), qulonglong(8902));
}

void TestPerft::divideMatchesTotal()
{
    Position position;
    QVERIFY(position.set_FEN(kStartFen));

    std::vector<PerftDivideEntry> divide;
    const NodesSize total = perft_root(position, 2, &divide);

    QCOMPARE(divide.size(), size_t(20));

    NodesSize sum = 0;
    for (const PerftDivideEntry& entry : divide) {
        sum += entry.nodes;
    }

    QCOMPARE(qulonglong(sum), qulonglong(total));
    QCOMPARE(qulonglong(total), qulonglong(400));
}

void TestPerft::kiwipete()
{
    Position position;
    QVERIFY(position.set_FEN(kKiwipeteFen));

    QCOMPARE(qulonglong(perft_root(position, 1)), qulonglong(48));
    QCOMPARE(qulonglong(perft_root(position, 2)), qulonglong(2039));
    QCOMPARE(qulonglong(perft_root(position, 3)), qulonglong(97862));
}

void TestPerft::enPassantRegression()
{
    Position position;
    QVERIFY(position.set_FEN(kEnPassantFen));

    QCOMPARE(qulonglong(perft_root(position, 1)), qulonglong(10));
    QCOMPARE(qulonglong(perft_root(position, 2)), qulonglong(63));
    QCOMPARE(qulonglong(perft_root(position, 3)), qulonglong(533));
}

QTEST_APPLESS_MAIN(TestPerft)

#include "test_perft.moc"
