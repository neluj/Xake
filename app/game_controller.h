#pragma once

#include "match_settings_types.h"
#include "position.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QElapsedTimer>
#include <string>

class UciClient;

enum class EngineSide {
    White,
    Black
};

struct EngineSession {
    UciClient *client = nullptr;
    bool active = false;
    bool uciOk = false;
    bool readyOk = false;
};

class GameController : public QObject
{
    Q_OBJECT

public:
    explicit GameController(QObject *parent = nullptr);

    bool startMatch(const MatchConfig& config,
                    const std::string& fen,
                    const QString& logDir = QString(),
                    const QString& logTag = QString());
    void stopMatch();
    bool applyHumanMove(ChessGame::Move move);

    bool isActive() const;
    MatchConfig matchConfig() const;
    ChessGame::Position currentPosition() const;
    bool timeControlEnabled() const;
    qint64 remainingTimeMs(ChessGame::Color side) const;

signals:
    void positionChanged(const ChessGame::Position& position);
    void matchStarted(const MatchConfig& config);
    void matchStopped();
    void errorOccurred(const QString& title, const QString& message);

private:
    ChessGame::Move moveFromUci(const QString& move) const;
    ChessGame::Move resolveMoveCandidate(ChessGame::Move move) const;
    QString uciFromMove(ChessGame::Move move) const;
    bool applyMove(ChessGame::Move move, ChessGame::Move* appliedMove = nullptr);
    void afterMoveApplied(ChessGame::Move move);
    bool finishGameIfNoLegalMoves();
    bool startEngineForPlayer(EngineSession& session,
                              const PlayerConfig& player,
                              EngineSide side);
    void stopEngines();
    void startSideTimer(ChessGame::Color side);
    void stopSideTimer(ChessGame::Color side);
    void startTurnIfReady();
    void handleUciOk(EngineSide side);
    void handleReadyOk(EngineSide side);
    void handleBestMove(EngineSide side, const QString& move);
    void sendPositionToEngine(EngineSession& session);
    void sendGoForSide(EngineSide side);
    EngineSession& sessionForSide(EngineSide side);

    bool m_active = false;
    MatchConfig m_config;
    ChessGame::Position m_position;
    EngineSession m_whiteSession;
    EngineSession m_blackSession;
    bool m_baseIsStartpos = false;
    std::string m_baseFen;
    QStringList m_uciMoves;
    QVector<ChessGame::Move> m_moveHistory;
    bool m_timeControlEnabled = false;
    qint64 m_whiteTimeMs = 0;
    qint64 m_blackTimeMs = 0;
    qint64 m_incrementMs = 0;
    bool m_timerRunning = false;
    ChessGame::Color m_timedSide = ChessGame::WHITE;
    QElapsedTimer m_turnTimer;
    QString m_logDir;
    QString m_logTag;
};
