#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "session_record.h"

class TestSessionRecord : public QObject
{
    Q_OBJECT

private slots:
    void writesCompletedMatchState();
    void writesTournamentParticipants();
};

void TestSessionRecord::writesCompletedMatchState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    SessionRecord record;
    record.sessionType = QStringLiteral("match");
    record.sessionTag = QStringLiteral("record_test");
    record.startTimeIso = QStringLiteral("2026-07-26T12:00:00");
    record.updatedAtIso = QStringLiteral("2026-07-26T12:03:00");
    record.finishedAtIso = record.updatedAtIso;
    record.status = QStringLiteral("completed");
    record.startFen = QStringLiteral(
        "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1");
    record.finalFen = QStringLiteral(
        "7k/6Q1/6K1/8/8/8/8/8 b - - 1 1");
    record.moves = {QStringLiteral("f7g7")};
    MoveRecord move;
    move.uci = QStringLiteral("f7g7");
    move.movedPiece = Xake::W_QUEEN;
    move.origin = MoveOrigin::Human;
    move.whiteTimeBeforeMs = 1500;
    move.whiteTimeAfterMs = 1234;
    record.moveRecords = {move};
    record.whiteTimeMs = 1234;
    record.blackTimeMs = 5678;
    record.hasResult = true;
    record.result = GameResult{GameOutcome::WhiteWin,
                               GameTermination::Checkmate,
                               QStringLiteral("White wins by checkmate.")};

    const QString path = directory.filePath(QStringLiteral("session.json"));
    QString error;
    QVERIFY2(writeSessionRecord(record, path, &error), qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();

    QCOMPARE(root.value(QStringLiteral("status")).toString(),
             QStringLiteral("completed"));
    QCOMPARE(root.value(QStringLiteral("finalFen")).toString(), record.finalFen);
    QCOMPARE(root.value(QStringLiteral("moves")).toArray().size(), 1);
    QCOMPARE(root.value(QStringLiteral("moves")).toArray().at(0).toString(),
             QStringLiteral("f7g7"));
    const QJsonArray moveRecords =
        root.value(QStringLiteral("moveRecords")).toArray();
    QCOMPARE(moveRecords.size(), 1);
    QCOMPARE(moveRecords.at(0).toObject()
                 .value(QStringLiteral("origin")).toString(),
             QStringLiteral("human"));
    const QJsonObject gameSettings = root.value(QStringLiteral("match"))
                                         .toObject()
                                         .value(QStringLiteral("game"))
                                         .toObject();
    QVERIFY(!gameSettings.contains(QStringLiteral("movesToGo")));

    const QJsonObject clocks = root.value(QStringLiteral("clocks")).toObject();
    QCOMPARE(clocks.value(QStringLiteral("whiteMs")).toInteger(), qint64(1234));
    QCOMPARE(clocks.value(QStringLiteral("blackMs")).toInteger(), qint64(5678));

    const QJsonObject result = root.value(QStringLiteral("result")).toObject();
    QCOMPARE(result.value(QStringLiteral("notation")).toString(),
             QStringLiteral("1-0"));
    QCOMPARE(result.value(QStringLiteral("outcome")).toString(),
             QStringLiteral("white_win"));
    QCOMPARE(result.value(QStringLiteral("termination")).toString(),
             QStringLiteral("checkmate"));
    QCOMPARE(root.value(QStringLiteral("termination")).toString(),
             QStringLiteral("checkmate"));
}

void TestSessionRecord::writesTournamentParticipants()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    SessionRecord record;
    record.sessionType = QStringLiteral("tournament");
    record.sessionTag = QStringLiteral("multi_participant");
    record.hasTournament = true;
    record.tournament.format = TournamentFormat::Gauntlet;
    record.tournament.tournamentType = QStringLiteral("Gauntlet");
    record.tournament.gauntletParticipantId =
        QStringLiteral("human");
    for (int index = 0; index < 3; ++index) {
        PlayerConfig player;
        player.type = PlayerType::Human;
        player.name = QStringLiteral("Player %1").arg(index + 1);
        record.tournament.participants.append({
            index == 0
                ? QStringLiteral("human")
                : QStringLiteral("engine-%1").arg(index),
            player
        });
    }
    record.tournament.match.player1 =
        record.tournament.participants.at(0).player;
    record.tournament.match.player2 =
        record.tournament.participants.at(1).player;

    const QString path =
        directory.filePath(QStringLiteral("tournament.json"));
    QString error;
    QVERIFY2(writeSessionRecord(record, path, &error),
             qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonObject root =
        QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(root.value(QStringLiteral("formatVersion")).toInt(), 2);
    const QJsonObject tournament =
        root.value(QStringLiteral("tournament")).toObject();
    QCOMPARE(tournament.value(QStringLiteral("format")).toString(),
             QStringLiteral("gauntlet"));
    QCOMPARE(tournament.value(
                 QStringLiteral("gauntletParticipantId")).toString(),
             QStringLiteral("human"));
    QCOMPARE(tournament.value(
                 QStringLiteral("participants")).toArray().size(), 3);
}

QTEST_APPLESS_MAIN(TestSessionRecord)

#include "test_session_record.moc"
