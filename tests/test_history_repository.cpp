#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "history_repository.h"

namespace {

bool writeTextFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Text)
        && file.write(contents) == contents.size();
}

}

class TestHistoryRepository : public QObject
{
    Q_OBJECT

private slots:
    void loadsMatchesAndTournamentsNewestFirst();
    void skipsCorruptRecordsWithoutLosingValidHistory();
    void fallsBackToTournamentSessionRecord();
    void deletesManagedSessionDirectory();
    void rejectsUnsafeSessionDirectories();
};

void TestHistoryRepository::loadsMatchesAndTournamentsNewestFirst()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const QString matchDir = root.filePath(QStringLiteral("match"));
    const QString tournamentDir = root.filePath(QStringLiteral("tournament"));
    QVERIFY(QDir().mkpath(matchDir));
    QVERIFY(QDir().mkpath(tournamentDir));

    const QByteArray matchJson = R"json({
        "sessionType": "match",
        "sessionTag": "match_1",
        "status": "completed",
        "startTime": "2026-07-25T10:00:00.000+02:00",
        "finishedAt": "2026-07-25T10:05:00.000+02:00",
        "startFen": "start",
        "finalFen": "final",
        "match": {
            "player1": {"name": "Alice"},
            "player2": {"enginePath": "C:/engines/Dragon.exe"},
            "game": {
                "timeControl": "Blitz",
                "baseTimeSeconds": 180,
                "incrementSeconds": 2
            }
        },
        "opening": {"name": "Sicilian", "moves": ["e2e4", "c7c5"]},
        "moves": ["e2e4", "c7c5", "g1f3"],
        "result": {
            "notation": "1-0",
            "termination": "checkmate",
            "message": "White wins."
        }
    })json";
    QVERIFY(writeTextFile(
        QDir(matchDir).filePath(QStringLiteral("session_match_1.json")),
        matchJson));
    QVERIFY(writeTextFile(
        QDir(matchDir).filePath(QStringLiteral("game.pgn")),
        QByteArrayLiteral("[Result \"1-0\"]\n")));

    const QByteArray tournamentJson = R"json({
        "sessionTag": "tournament_1",
        "status": "completed",
        "startedAt": "2026-07-26T11:00:00.000+02:00",
        "finishedAt": "2026-07-26T12:00:00.000+02:00",
        "tournament": {
            "format": "round_robin",
            "participants": [
                {"id": "stockfish", "player": {"name": "Stockfish"}},
                {"id": "dragon", "player": {"name": "Dragon"}},
                {"id": "leela", "player": {"name": "Leela"}}
            ],
            "player1": {"name": "Stockfish"},
            "player2": {"name": "Dragon"},
            "game": {
                "timeControl": "Bullet",
                "baseTimeSeconds": 60,
                "incrementSeconds": 1
            }
        },
        "summary": {
            "totalGames": 2,
            "completedGames": 2,
            "player1Wins": 1,
            "player2Wins": 0,
            "draws": 1,
            "standings": [
                {"participantId": "stockfish", "name": "Stockfish", "wins": 1, "losses": 0, "draws": 1, "points": 1.5, "sequence": "1="},
                {"participantId": "dragon", "name": "Dragon", "wins": 0, "losses": 1, "draws": 1, "points": 0.5, "sequence": "0="},
                {"participantId": "leela", "name": "Leela", "wins": 0, "losses": 0, "draws": 0, "points": 0.0, "sequence": ""}
            ]
        },
        "games": [{
            "gameNumber": 1,
            "status": "completed",
            "startedAt": "2026-07-26T11:00:00.000+02:00",
            "finishedAt": "2026-07-26T11:10:00.000+02:00",
            "white": {"name": "Stockfish"},
            "black": {"name": "Dragon"},
            "opening": {
                "name": "Italian Game",
                "startFen": "start",
                "moves": ["e2e4", "e7e5"]
            },
            "moves": ["e2e4", "e7e5", "g1f3"],
            "result": "1-0",
            "termination": "checkmate"
        }]
    })json";
    QVERIFY(writeTextFile(
        QDir(tournamentDir).filePath(
            QStringLiteral("tournament_report.json")),
        tournamentJson));
    QVERIFY(writeTextFile(
        QDir(tournamentDir).filePath(QStringLiteral("tournament.pgn")),
        QByteArrayLiteral("[Event \"Xake tournament\"]\n")));

    const HistoryLoadResult history = loadSessionHistory(root.path());
    QVERIFY(history.warnings.isEmpty());
    QCOMPARE(history.entries.size(), 2);

    const HistoryEntry& tournament = history.entries.at(0);
    QCOMPARE(tournament.type, HistorySessionType::Tournament);
    QCOMPARE(tournament.player1, QStringLiteral("Stockfish"));
    QCOMPARE(tournament.player2, QStringLiteral("Dragon"));
    QCOMPARE(tournament.participants,
             QStringList({QStringLiteral("Stockfish"),
                          QStringLiteral("Dragon"),
                          QStringLiteral("Leela")}));
    QCOMPARE(tournament.tournamentFormat,
             QStringLiteral("Round-robin"));
    QCOMPARE(tournament.totalGames, 2);
    QCOMPARE(tournament.completedGames, 2);
    QCOMPARE(tournament.standings.size(), 3);
    QCOMPARE(tournament.standings.first().points, 1.5);
    QCOMPARE(tournament.games.size(), 1);
    QCOMPARE(tournament.games.first().openingName,
             QStringLiteral("Italian Game"));
    QCOMPARE(tournament.games.first().openingMoveCount, 2);
    QCOMPARE(tournament.games.first().termination,
             QStringLiteral("checkmate"));
    QVERIFY(!tournament.pgnFilePath.isEmpty());

    const HistoryEntry& match = history.entries.at(1);
    QCOMPARE(match.type, HistorySessionType::Match);
    QCOMPARE(match.player1, QStringLiteral("Alice"));
    QCOMPARE(match.player2, QStringLiteral("Dragon"));
    QCOMPARE(match.result, QStringLiteral("1-0"));
    QCOMPARE(match.termination, QStringLiteral("checkmate"));
    QCOMPARE(match.openingName, QStringLiteral("Sicilian"));
    QCOMPARE(match.openingMoveCount, 2);
    QCOMPARE(match.moves.size(), 3);
    QVERIFY(!match.pgnFilePath.isEmpty());
}

void TestHistoryRepository::skipsCorruptRecordsWithoutLosingValidHistory()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const QString corruptDir = root.filePath(QStringLiteral("corrupt"));
    const QString validDir = root.filePath(QStringLiteral("valid"));
    QVERIFY(QDir().mkpath(corruptDir));
    QVERIFY(QDir().mkpath(validDir));
    QVERIFY(writeTextFile(
        QDir(corruptDir).filePath(QStringLiteral("session_bad.json")),
        QByteArrayLiteral("{ invalid")));
    QVERIFY(writeTextFile(
        QDir(validDir).filePath(QStringLiteral("session_ok.json")),
        QByteArrayLiteral(R"json({
            "sessionType": "match",
            "sessionTag": "ok",
            "status": "stopped",
            "termination": "stopped",
            "startTime": "2026-07-26T09:00:00Z",
            "match": {
                "player1": {"name": "White"},
                "player2": {"name": "Black"},
                "game": {}
            }
        })json")));

    const HistoryLoadResult history = loadSessionHistory(root.path());
    QCOMPARE(history.entries.size(), 1);
    QCOMPARE(history.entries.first().sessionTag, QStringLiteral("ok"));
    QCOMPARE(history.entries.first().termination,
             QStringLiteral("stopped"));
    QCOMPARE(history.warnings.size(), 1);
    QVERIFY(history.warnings.first().contains(QStringLiteral("session_bad.json")));
}

void TestHistoryRepository::fallsBackToTournamentSessionRecord()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const QString sessionDir = root.filePath(QStringLiteral("active_tournament"));
    QVERIFY(QDir().mkpath(sessionDir));
    QVERIFY(writeTextFile(
        QDir(sessionDir).filePath(QStringLiteral("tournament_report.json")),
        QByteArrayLiteral("{ invalid")));
    QVERIFY(writeTextFile(
        QDir(sessionDir).filePath(QStringLiteral("session_active.json")),
        QByteArrayLiteral(R"json({
            "sessionType": "tournament",
            "sessionTag": "active",
            "status": "in_progress",
            "startTime": "2026-07-26T12:00:00Z",
            "tournament": {
                "rounds": 2,
                "gamesPerPairing": 3,
                "match": {
                    "player1": {"name": "Engine A"},
                    "player2": {"name": "Engine B"},
                    "game": {"baseTimeSeconds": 60, "incrementSeconds": 1}
                }
            }
        })json")));

    const HistoryLoadResult history = loadSessionHistory(root.path());
    QCOMPARE(history.entries.size(), 1);
    QCOMPARE(history.entries.first().type, HistorySessionType::Tournament);
    QCOMPARE(history.entries.first().totalGames, 6);
    QCOMPARE(history.entries.first().status, QStringLiteral("in_progress"));
    QCOMPARE(history.warnings.size(), 1);
}

void TestHistoryRepository::deletesManagedSessionDirectory()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const QString sessionsDirectory =
        root.filePath(QStringLiteral("sessions"));
    const QString sessionDirectory =
        QDir(sessionsDirectory).filePath(QStringLiteral("session_1"));
    const QString nestedDirectory =
        QDir(sessionDirectory).filePath(QStringLiteral("nested"));
    QVERIFY(QDir().mkpath(nestedDirectory));
    QVERIFY(writeTextFile(
        QDir(sessionDirectory).filePath(QStringLiteral("game.pgn")),
        QByteArrayLiteral("[Result \"1-0\"]\n")));
    QVERIFY(writeTextFile(
        QDir(nestedDirectory).filePath(QStringLiteral("engine.log")),
        QByteArrayLiteral("uci\n")));

    QVERIFY(isManagedHistorySessionDirectory(sessionsDirectory,
                                             sessionDirectory));
    const HistorySessionDeletionResult result =
        deleteHistorySession(sessionsDirectory, sessionDirectory);
    QVERIFY2(result.succeeded(), qPrintable(result.error));
    QVERIFY(!QDir(sessionDirectory).exists());
    QVERIFY(QDir(sessionsDirectory).exists());
}

void TestHistoryRepository::rejectsUnsafeSessionDirectories()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const QString sessionsDirectory =
        root.filePath(QStringLiteral("sessions"));
    const QString sessionDirectory =
        QDir(sessionsDirectory).filePath(QStringLiteral("session_1"));
    const QString nestedDirectory =
        QDir(sessionDirectory).filePath(QStringLiteral("nested"));
    const QString outsideDirectory =
        root.filePath(QStringLiteral("outside"));
    QVERIFY(QDir().mkpath(nestedDirectory));
    QVERIFY(QDir().mkpath(outsideDirectory));

    QVERIFY(!isManagedHistorySessionDirectory(sessionsDirectory,
                                              sessionsDirectory));
    QVERIFY(!isManagedHistorySessionDirectory(sessionsDirectory,
                                              nestedDirectory));
    QVERIFY(!isManagedHistorySessionDirectory(sessionsDirectory,
                                              outsideDirectory));
    QVERIFY(!isManagedHistorySessionDirectory(
        sessionsDirectory,
        QDir(sessionsDirectory).filePath(QStringLiteral("missing"))));

    QVERIFY(!deleteHistorySession(sessionsDirectory,
                                  sessionsDirectory).succeeded());
    QVERIFY(!deleteHistorySession(sessionsDirectory,
                                  nestedDirectory).succeeded());
    QVERIFY(!deleteHistorySession(sessionsDirectory,
                                  outsideDirectory).succeeded());
    QVERIFY(QDir(sessionsDirectory).exists());
    QVERIFY(QDir(nestedDirectory).exists());
    QVERIFY(QDir(outsideDirectory).exists());
}

QTEST_APPLESS_MAIN(TestHistoryRepository)

#include "test_history_repository.moc"
