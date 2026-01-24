#pragma once

#include "match_settings_types.h"
#include "position.h"

#include <QObject>
#include <QString>
#include <QStringList>
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

    bool startMatch(const MatchConfig& config, const std::string& fen);
    void stopMatch();

    bool isActive() const;
    MatchConfig matchConfig() const;
    Position currentPosition() const;

signals:
    void positionChanged(const Position& position);
    void matchStarted(const MatchConfig& config);
    void matchStopped();
    void errorOccurred(const QString& title, const QString& message);

private:
    bool startEngineForPlayer(EngineSession& session,
                              const PlayerConfig& player);
    void stopEngines();
    bool applyUciMove(const QString& move);
    void handleUciOk(EngineSide side);
    void handleReadyOk(EngineSide side);
    void handleBestMove(EngineSide side, const QString& move);
    void sendPositionToEngine(EngineSession& session);
    void sendGoForSide(EngineSide side);
    EngineSession& sessionForSide(EngineSide side);

    bool m_active = false;
    MatchConfig m_config;
    Position m_position;
    EngineSession m_whiteSession;
    EngineSession m_blackSession;
    bool m_baseIsStartpos = false;
    std::string m_baseFen;
    QStringList m_moves;
};
