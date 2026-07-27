#include <QTemporaryDir>
#include <QtTest>

#include "pgn_export.h"

namespace {

constexpr char kStartFen[] =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

} // namespace

class TestPgnExport : public QObject
{
    Q_OBJECT

private slots:
    void exportsSanMoveText();
    void exportsCustomFenAndCheckmate();
    void exportsCastlingPromotionAndDisambiguation();
    void writesMultipleGames();
};

void TestPgnExport::exportsSanMoveText()
{
    PgnGameRecord game;
    game.event = QStringLiteral("Test");
    game.date = QStringLiteral("2026.07.26");
    game.white = QStringLiteral("White");
    game.black = QStringLiteral("Black");
    game.result = QStringLiteral("*");
    game.startFen = QString::fromLatin1(kStartFen);
    game.movesUci = {
        QStringLiteral("e2e4"),
        QStringLiteral("e7e5"),
        QStringLiteral("g1f3"),
        QStringLiteral("b8c6"),
        QStringLiteral("f1b5")
    };

    QString error;
    const QString pgn = pgnText({game}, &error);
    QVERIFY2(!pgn.isEmpty(), qPrintable(error));
    QVERIFY(pgn.contains(QStringLiteral("1. e4 e5 2. Nf3 Nc6 3. Bb5 *")));
    QVERIFY(!pgn.contains(QStringLiteral("[SetUp")));
}

void TestPgnExport::exportsCustomFenAndCheckmate()
{
    PgnGameRecord game;
    game.white = QStringLiteral("Player 1");
    game.black = QStringLiteral("Player 2");
    game.result = QStringLiteral("1-0");
    game.startFen = QStringLiteral(
        "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1");
    game.movesUci = {QStringLiteral("f7g7")};

    QString error;
    const QString pgn = pgnText({game}, &error);
    QVERIFY2(!pgn.isEmpty(), qPrintable(error));
    QVERIFY(pgn.contains(QStringLiteral("[SetUp \"1\"]")));
    QVERIFY(pgn.contains(QStringLiteral("[FEN \"7k/5Q2/6K1/8/8/8/8/8 w - - 0 1\"]")));
    QVERIFY(pgn.contains(QStringLiteral("1. Qg7# 1-0")));
}

void TestPgnExport::exportsCastlingPromotionAndDisambiguation()
{
    PgnGameRecord castling;
    castling.startFen = QStringLiteral(
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    castling.movesUci = {
        QStringLiteral("e1g1"),
        QStringLiteral("e8c8")
    };

    PgnGameRecord promotion;
    promotion.startFen = QStringLiteral(
        "7k/P7/8/8/8/8/8/7K w - - 0 1");
    promotion.movesUci = {QStringLiteral("a7a8q")};

    PgnGameRecord disambiguation;
    disambiguation.startFen = QStringLiteral(
        "4k3/8/8/8/8/8/3N3N/4K3 w - - 0 1");
    disambiguation.movesUci = {QStringLiteral("d2f3")};

    QString error;
    const QString pgn = pgnText(
        {castling, promotion, disambiguation}, &error);
    QVERIFY2(!pgn.isEmpty(), qPrintable(error));
    QVERIFY(pgn.contains(QStringLiteral("1. O-O O-O-O *")));
    QVERIFY(pgn.contains(QStringLiteral("1. a8=Q+ *")));
    QVERIFY(pgn.contains(QStringLiteral("1. Ndf3 *")));
}

void TestPgnExport::writesMultipleGames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    PgnGameRecord first;
    first.white = QStringLiteral("A");
    first.black = QStringLiteral("B");
    first.startFen = QString::fromLatin1(kStartFen);

    PgnGameRecord second = first;
    second.white = QStringLiteral("B");
    second.black = QStringLiteral("A");

    QString error;
    const QString path = directory.filePath(QStringLiteral("games.pgn"));
    QVERIFY2(writePgnFile({first, second}, path, &error), qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(file.readAll());
    QCOMPARE(contents.count(QStringLiteral("[Event ")), 2);
}

QTEST_APPLESS_MAIN(TestPgnExport)

#include "test_pgn_export.moc"
