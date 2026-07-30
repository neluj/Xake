#include <QtTest>

#include "game_result.h"

class TestGameResult : public QObject
{
    Q_OBJECT

private slots:
    void mapsEveryTermination();
    void formatsResultSummary();
};

void TestGameResult::mapsEveryTermination()
{
    struct TerminationCase {
        GameTermination termination;
        const char *key;
        const char *display;
    };
    const TerminationCase cases[] = {
        {GameTermination::Unknown, "unknown", "Unknown"},
        {GameTermination::Checkmate, "checkmate", "Checkmate"},
        {GameTermination::Stalemate, "stalemate", "Stalemate"},
        {GameTermination::FiftyMoveRule,
         "fifty_move_rule",
         "Fifty-move rule"},
        {GameTermination::ThreefoldRepetition,
         "threefold_repetition",
         "Threefold repetition"},
        {GameTermination::InsufficientMaterial,
         "insufficient_material",
         "Insufficient material"},
        {GameTermination::TimeForfeit, "time_forfeit", "Time forfeit"},
        {GameTermination::TimeExpiredInsufficientMaterial,
         "time_expired_insufficient_material",
         "Draw on time: insufficient mating material"},
        {GameTermination::MoveLimit, "move_limit", "Move limit"},
        {GameTermination::Resignation, "resignation", "Resignation"},
        {GameTermination::EngineFailure,
         "engine_failure",
         "Engine failure"},
        {GameTermination::InvalidPosition,
         "invalid_position",
         "Invalid position"},
        {GameTermination::Stopped, "stopped", "Stopped by user"},
        {GameTermination::StartFailure, "start_failure", "Start failure"}
    };

    for (const TerminationCase& item : cases) {
        QCOMPARE(gameTerminationKey(item.termination),
                 QString::fromLatin1(item.key));
        QCOMPARE(gameTerminationDisplayName(item.termination),
                 QString::fromLatin1(item.display));
    }
}

void TestGameResult::formatsResultSummary()
{
    QCOMPARE(gameResultSummary(QStringLiteral("1-0"),
                               QStringLiteral("checkmate")),
             QStringLiteral("1-0 (Checkmate)"));
    QCOMPARE(
        gameResultSummary(
            QStringLiteral("1/2-1/2"),
            QStringLiteral("time forfeit (insufficient mating material)")),
        QStringLiteral(
            "1/2-1/2 (Draw on time: insufficient mating material)"));
    QCOMPARE(gameResultSummary(QStringLiteral("0-1"), QString()),
             QStringLiteral("0-1"));
}

QTEST_APPLESS_MAIN(TestGameResult)

#include "test_game_result.moc"
