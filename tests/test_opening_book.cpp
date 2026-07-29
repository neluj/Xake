#include <QtTest>

#include "opening_book.h"

class TestOpeningBook : public QObject
{
    Q_OBJECT

private slots:
    void parsesPgnMainlines();
    void parsesPgnFenAndVariations();
    void rejectsIllegalPgnMove();
    void parsesEpdPositions();
    void parsesExternalBookWhenConfigured();
};

void TestOpeningBook::parsesPgnMainlines()
{
    const QString pgn = QStringLiteral(
        "[Event \"King side\"]\n"
        "[Opening \"Ruy Lopez\"]\n"
        "[Variation \"Berlin Defence\"]\n"
        "[ECO \"C60\"]\n"
        "[White \"Engine A\"]\n"
        "[Black \"Engine B\"]\n"
        "[Result \"1/2-1/2\"]\n\n"
        "1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. O-O Nf6 1/2-1/2\n\n"
        "[Event \"Queen side\"]\n\n"
        "1. d4 d5 2. c4 e6 3. Nc3 Nf6 4. Bg5 Be7 5. e3 O-O 6. Qd2 Nbd7 *\n");

    QVector<OpeningEntry> openings;
    QString error;
    QVERIFY2(parsePgnOpenings(pgn, &openings, &error), qPrintable(error));
    QCOMPARE(openings.size(), 2);
    QCOMPARE(openings.at(0).name,
             QStringLiteral("Ruy Lopez - Berlin Defence [C60]"));
    QCOMPARE(openings.at(0).movesUci,
             QStringList({"e2e4", "e7e5", "g1f3", "b8c6",
                          "f1b5", "a7a6", "e1g1", "g8f6"}));
    QCOMPARE(openings.at(0).white, QStringLiteral("Engine A"));
    QCOMPARE(openings.at(0).black, QStringLiteral("Engine B"));
    QCOMPARE(openings.at(0).result, QStringLiteral("1/2-1/2"));
    QCOMPARE(openings.at(1).movesUci.constLast(), QStringLiteral("b8d7"));
}

void TestOpeningBook::parsesPgnFenAndVariations()
{
    const QString pgn = QStringLiteral(
        "[Event \"Promotion\"]\n"
        "[SetUp \"1\"]\n"
        "[FEN \"4k3/P7/8/8/8/8/8/4K3 w - - 0 1\"]\n\n"
        "1. a8=Q+ (1. a8=N) Kf7 {main line} 2. Qe4 *\n");

    QVector<OpeningEntry> openings;
    QString error;
    QVERIFY2(parsePgnOpenings(pgn, &openings, &error), qPrintable(error));
    QCOMPARE(openings.size(), 1);
    QCOMPARE(openings.first().movesUci,
             QStringList({"a7a8q", "e8f7", "a8e4"}));
    QCOMPARE(openings.first().finalFen,
             QStringLiteral("8/5k2/8/8/4Q3/8/8/4K3 b - - 2 2"));
}

void TestOpeningBook::rejectsIllegalPgnMove()
{
    QVector<OpeningEntry> openings;
    QString error;
    QVERIFY(!parsePgnOpenings(
        QStringLiteral("[Event \"Invalid\"]\n\n1. e5 *\n"),
        &openings,
        &error));
    QVERIFY(error.contains(QStringLiteral("ply 1")));
    QVERIFY(error.contains(QStringLiteral("e5")));
}

void TestOpeningBook::parsesEpdPositions()
{
    const QString epd = QStringLiteral(
        "# opening positions\n"
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - id \"Start\";\n"
        "4k3/8/8/8/8/8/8/4K3 b - - hmvc 12; id \"Kings\";\n");

    QVector<OpeningEntry> openings;
    QString error;
    QVERIFY2(parseEpdOpenings(epd, &openings, &error), qPrintable(error));
    QCOMPARE(openings.size(), 2);
    QCOMPARE(openings.at(0).name, QStringLiteral("Start"));
    QCOMPARE(openings.at(0).finalFen,
             QStringLiteral("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
    QCOMPARE(openings.at(1).name, QStringLiteral("Kings"));
    QCOMPARE(openings.at(1).finalFen,
             QStringLiteral("4k3/8/8/8/8/8/8/4K3 b - - 0 1"));
}

void TestOpeningBook::parsesExternalBookWhenConfigured()
{
    const QString filePath =
        qEnvironmentVariable("XAKE_TEST_OPENING_FILE").trimmed();
    if (filePath.isEmpty()) {
        QSKIP("Set XAKE_TEST_OPENING_FILE to validate an external opening book.");
    }

    QVector<OpeningEntry> openings;
    QString error;
    QVERIFY2(loadOpeningFile(filePath, &openings, &error), qPrintable(error));
    QVERIFY(!openings.isEmpty());
}

QTEST_APPLESS_MAIN(TestOpeningBook)

#include "test_opening_book.moc"
