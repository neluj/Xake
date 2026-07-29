#pragma once

#include <QCoreApplication>
#include <QMetaType>
#include <QString>

enum class GameOutcome {
    WhiteWin,
    BlackWin,
    Draw
};

enum class GameTermination {
    Unknown,
    Checkmate,
    Stalemate,
    FiftyMoveRule,
    ThreefoldRepetition,
    InsufficientMaterial,
    TimeForfeit,
    TimeExpiredInsufficientMaterial,
    MoveLimit,
    Resignation,
    EngineFailure,
    InvalidPosition,
    Stopped,
    StartFailure
};

Q_DECLARE_METATYPE(GameOutcome)
Q_DECLARE_METATYPE(GameTermination)

struct GameResult {
    GameOutcome outcome = GameOutcome::Draw;
    GameTermination termination = GameTermination::Unknown;
    QString message;
};

Q_DECLARE_METATYPE(GameResult)

inline QString gameResultNotation(GameOutcome outcome)
{
    switch (outcome) {
    case GameOutcome::WhiteWin:
        return QStringLiteral("1-0");
    case GameOutcome::BlackWin:
        return QStringLiteral("0-1");
    case GameOutcome::Draw:
        return QStringLiteral("1/2-1/2");
    }
    return QStringLiteral("*");
}

inline QString gameOutcomeKey(GameOutcome outcome)
{
    switch (outcome) {
    case GameOutcome::WhiteWin:
        return QStringLiteral("white_win");
    case GameOutcome::BlackWin:
        return QStringLiteral("black_win");
    case GameOutcome::Draw:
        return QStringLiteral("draw");
    }
    return QStringLiteral("unknown");
}

inline QString gameTerminationKey(GameTermination termination)
{
    switch (termination) {
    case GameTermination::Unknown:
        return QStringLiteral("unknown");
    case GameTermination::Checkmate:
        return QStringLiteral("checkmate");
    case GameTermination::Stalemate:
        return QStringLiteral("stalemate");
    case GameTermination::FiftyMoveRule:
        return QStringLiteral("fifty_move_rule");
    case GameTermination::ThreefoldRepetition:
        return QStringLiteral("threefold_repetition");
    case GameTermination::InsufficientMaterial:
        return QStringLiteral("insufficient_material");
    case GameTermination::TimeForfeit:
        return QStringLiteral("time_forfeit");
    case GameTermination::TimeExpiredInsufficientMaterial:
        return QStringLiteral("time_expired_insufficient_material");
    case GameTermination::MoveLimit:
        return QStringLiteral("move_limit");
    case GameTermination::Resignation:
        return QStringLiteral("resignation");
    case GameTermination::EngineFailure:
        return QStringLiteral("engine_failure");
    case GameTermination::InvalidPosition:
        return QStringLiteral("invalid_position");
    case GameTermination::Stopped:
        return QStringLiteral("stopped");
    case GameTermination::StartFailure:
        return QStringLiteral("start_failure");
    }
    return QStringLiteral("unknown");
}

inline QString gameTerminationPgn(GameTermination termination)
{
    switch (termination) {
    case GameTermination::Unknown:
        return QString();
    case GameTermination::Checkmate:
        return QStringLiteral("checkmate");
    case GameTermination::Stalemate:
        return QStringLiteral("stalemate");
    case GameTermination::FiftyMoveRule:
        return QStringLiteral("fifty-move rule");
    case GameTermination::ThreefoldRepetition:
        return QStringLiteral("threefold repetition");
    case GameTermination::InsufficientMaterial:
        return QStringLiteral("insufficient material");
    case GameTermination::TimeForfeit:
        return QStringLiteral("time forfeit");
    case GameTermination::TimeExpiredInsufficientMaterial:
        return QStringLiteral("time forfeit (insufficient mating material)");
    case GameTermination::MoveLimit:
        return QStringLiteral("move limit");
    case GameTermination::Resignation:
        return QStringLiteral("resignation");
    case GameTermination::EngineFailure:
        return QStringLiteral("engine failure");
    case GameTermination::InvalidPosition:
        return QStringLiteral("invalid position");
    case GameTermination::Stopped:
        return QStringLiteral("abandoned");
    case GameTermination::StartFailure:
        return QStringLiteral("start failure");
    }
    return QString();
}

inline QString normalizedGameTerminationKey(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QLatin1Char('-'), QLatin1Char('_'));
    value.replace(QLatin1Char(' '), QLatin1Char('_'));
    while (value.contains(QStringLiteral("__"))) {
        value.replace(QStringLiteral("__"), QStringLiteral("_"));
    }
    return value;
}

inline QString gameTerminationDisplayName(const QString& value)
{
    const QString key = normalizedGameTerminationKey(value);
    if (key.isEmpty()) {
        return QString();
    }

    const auto text = [](const char *source) {
        return QCoreApplication::translate("GameResult", source);
    };
    if (key == QStringLiteral("unknown")) {
        return text("Unknown");
    }
    if (key == QStringLiteral("checkmate")) {
        return text("Checkmate");
    }
    if (key == QStringLiteral("stalemate")) {
        return text("Stalemate");
    }
    if (key == QStringLiteral("fifty_move_rule")) {
        return text("Fifty-move rule");
    }
    if (key == QStringLiteral("threefold_repetition")
        || key == QStringLiteral("repetition")) {
        return text("Threefold repetition");
    }
    if (key == QStringLiteral("insufficient_material")) {
        return text("Insufficient material");
    }
    if (key == QStringLiteral("time_forfeit")) {
        return text("Time forfeit");
    }
    if (key == QStringLiteral("time_expired_insufficient_material")
        || key == QStringLiteral(
            "time_forfeit_(insufficient_mating_material)")) {
        return text("Draw on time: insufficient mating material");
    }
    if (key == QStringLiteral("move_limit")) {
        return text("Move limit");
    }
    if (key == QStringLiteral("resignation")
        || key == QStringLiteral("resigned")) {
        return text("Resignation");
    }
    if (key == QStringLiteral("engine_failure")
        || key == QStringLiteral("engine_error")) {
        return text("Engine failure");
    }
    if (key == QStringLiteral("invalid_position")) {
        return text("Invalid position");
    }
    if (key == QStringLiteral("stopped")) {
        return text("Stopped by user");
    }
    if (key == QStringLiteral("start_failure")) {
        return text("Start failure");
    }
    if (key == QStringLiteral("abandoned")) {
        return text("Abandoned");
    }
    if (key == QStringLiteral("aborted")) {
        return text("Aborted");
    }
    if (key == QStringLiteral("normal")) {
        return text("Normal");
    }

    QString label = key;
    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    label[0] = label.at(0).toUpper();
    return label;
}

inline QString gameTerminationDisplayName(GameTermination termination)
{
    return gameTerminationDisplayName(gameTerminationKey(termination));
}

inline QString gameResultSummary(const QString& result,
                                 const QString& termination)
{
    const QString notation = result.trimmed();
    const QString reason = gameTerminationDisplayName(termination);
    if (notation.isEmpty()) {
        return reason;
    }
    if (reason.isEmpty()) {
        return notation;
    }
    return QStringLiteral("%1 (%2)").arg(notation, reason);
}
