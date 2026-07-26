#include <QElapsedTimer>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QtTest>

#include "perft.h"

using namespace Xake;

class TestPerftSuite : public QObject
{
    Q_OBJECT

private slots:
    void validatesEpdThroughDepthFour();
};

void TestPerftSuite::validatesEpdThroughDepthFour()
{
    QFile suite(QString::fromUtf8(PERFT_SUITE_PATH));
    QVERIFY2(suite.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(suite.errorString()));

    const QRegularExpression depthExpression(
        QStringLiteral("\\bD([1-4])\\s+(\\d+)\\b"));
    QTextStream stream(&suite);
    int lineNumber = 0;
    int positionCount = 0;
    int checkCount = 0;
    NodesSize totalNodes = 0;
    QElapsedTimer timer;
    timer.start();

    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const int operationsStart = line.indexOf(QLatin1Char(';'));
        QVERIFY2(operationsStart > 0,
                 qPrintable(QStringLiteral("Malformed EPD line %1")
                                .arg(lineNumber)));

        const QString fen = line.left(operationsStart).trimmed();
        Position position;
        QVERIFY2(position.set_FEN(fen.toStdString()),
                 qPrintable(QStringLiteral("Invalid FEN at EPD line %1: %2")
                                .arg(lineNumber)
                                .arg(fen)));
        ++positionCount;

        QRegularExpressionMatchIterator matches =
            depthExpression.globalMatch(line.mid(operationsStart));
        int depthsOnLine = 0;
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            const int depth = match.captured(1).toInt();
            bool expectedOk = false;
            const NodesSize expected =
                match.captured(2).toULongLong(&expectedOk);
            QVERIFY2(expectedOk,
                     qPrintable(QStringLiteral(
                         "Invalid node count at EPD line %1, depth %2")
                                    .arg(lineNumber)
                                    .arg(depth)));

            Position testPosition = position;
            const NodesSize actual = perft_root(testPosition, depth);
            if (actual != expected) {
                QFAIL(qPrintable(QStringLiteral(
                    "Perft mismatch at EPD line %1, depth %2: "
                    "expected %3, got %4\nFEN: %5")
                                     .arg(lineNumber)
                                     .arg(depth)
                                     .arg(expected)
                                     .arg(actual)
                                     .arg(fen)));
            }

            ++depthsOnLine;
            ++checkCount;
            totalNodes += actual;
        }
        QCOMPARE(depthsOnLine, 4);
    }

    QCOMPARE(positionCount, 127);
    QCOMPARE(checkCount, positionCount * 4);
    qInfo().noquote()
        << QStringLiteral(
               "Validated %1 EPD positions (%2 checks, %3 nodes) in %4 ms.")
               .arg(positionCount)
               .arg(checkCount)
               .arg(totalNodes)
               .arg(timer.elapsed());
}

QTEST_APPLESS_MAIN(TestPerftSuite)

#include "test_perft_suite.moc"
