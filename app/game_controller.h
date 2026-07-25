#pragma once

#include "match_settings_types.h"
#include "position.h"

#include <QFile>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QElapsedTimer>
#include <string>

class UciClient;
class QTimer;
class TestMoveExecution;

enum class EngineSide {
    White,
    Black
};

Q_DECLARE_METATYPE(EngineSide)

struct EngineSession {
    UciClient *client = nullptr;
    bool active = false;
    bool uciOk = false;
    bool readyOk = false;
    bool searching = false;
    QString lastErrorLine;
};

enum class GameOutcome {
    WhiteWin,
    BlackWin,
    Draw
};

enum class GameTermination {
    Checkmate,
    Stalemate,
    FiftyMoveRule,
    ThreefoldRepetition,
    InsufficientMaterial,
    TimeForfeit,
    MoveLimit
};

struct GameResult {
    GameOutcome outcome = GameOutcome::Draw;
    GameTermination termination = GameTermination::Stalemate;
    QString message;
};

Q_DECLARE_METATYPE(GameResult)

class GameController : public QObject
{
    Q_OBJECT
    friend class TestMoveExecution;

public:
    explicit GameController(QObject *parent = nullptr);
    ~GameController() override;

    bool startMatch(const MatchConfig& config,
                    const std::string& fen,
                    const QString& logDir = QString(),
                    const QString& logTag = QString(),
                    int maxFullMoves = 0);
    void stopMatch();
    bool applyHumanMove(Xake::Move move);

    bool isActive() const;
    MatchConfig matchConfig() const;
    Xake::Position currentPosition() const;
    QStringList moveHistoryUci() const;
    QStringList communicationHistory() const;
    QString communicationLogFilePath() const;
    bool timeControlEnabled() const;
    qint64 remainingTimeMs(Xake::Color side) const;

signals:
    void positionChanged(const Xake::Position& position);
    void movePlayed(int ply, const QString& uciMove);
    void matchStarted(const MatchConfig& config);
    void matchStopped();
    void gameFinished(const GameResult& result);
    void gameAborted(const QString& title, const QString& message);
    void errorOccurred(const QString& title, const QString& message);
    void engineSearchStarted(EngineSide side);
    void engineOutputReceived(EngineSide side, const QString& line);
    void communicationHistoryReset();
    void communicationLogged(const QString& line);
    void communicationLogError(const QString& message);

private slots:
    void handleTurnTimeout();

private:
    Xake::Move moveFromUci(const QString& move) const;
    Xake::Move resolveMoveCandidate(Xake::Move move) const;
    QString uciFromMove(Xake::Move move) const;
    bool applyMove(Xake::Move move, Xake::Move* appliedMove = nullptr);
    void afterMoveApplied(Xake::Move move);
    bool finishGameIfNoLegalMoves();
    bool finishGameIfDraw();
    bool finishGameIfMoveLimit();
    bool finishGameAsDraw(GameTermination termination, const QString& message);
    bool finishGame(GameOutcome outcome,
                    GameTermination termination,
                    const QString& title,
                    const QString& message);
    bool finishGameIfTimeExpired();
    bool finishGameOnTime(Xake::Color flaggedSide);
    bool startEngineForPlayer(EngineSession& session,
                              const PlayerConfig& player);
    void stopEngines();
    void startSideTimer(Xake::Color side);
    void stopSideTimer(Xake::Color side);
    void startTurnIfReady();
    void handleEngineError(EngineSide side, const QString& line);
    void handleEngineExited(EngineSide side, int exitCode, QProcess::ExitStatus status);
    void handleEngineCommunication(EngineSide side,
                                   const QString& prefix,
                                   const QString& line);
    void prepareCommunicationLog();
    void writeCommunicationLog(const QString& source,
                               const QString& prefix,
                               const QString& line);
    QString engineDisplayName(EngineSide side) const;
    void handleUciOk(EngineSide side);
    void handleReadyOk(EngineSide side);
    void handleBestMove(EngineSide side, const QString& move);
    void sendPositionToEngine(EngineSession& session);
    void sendGoForSide(EngineSide side);
    EngineSession& sessionForSide(EngineSide side);

    bool m_active = false;
    MatchConfig m_config;
    Xake::Position m_position;
    EngineSession m_whiteSession;
    EngineSession m_blackSession;
    bool m_baseIsStartpos = false;
    std::string m_baseFen;
    QStringList m_uciMoves;
    QVector<Xake::Move> m_moveHistory;
    bool m_timeControlEnabled = false;
    qint64 m_whiteTimeMs = 0;
    qint64 m_blackTimeMs = 0;
    qint64 m_incrementMs = 0;
    bool m_timerRunning = false;
    Xake::Color m_timedSide = Xake::WHITE;
    QElapsedTimer m_turnTimer;
    QTimer *m_flagTimer = nullptr;
    QString m_logDir;
    QString m_logTag;
    QFile m_communicationLogFile;
    QStringList m_communicationHistory;
    bool m_communicationLogErrorReported = false;
    int m_maxFullMoves = 0;
};
