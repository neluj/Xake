#pragma once

#include "match_settings_types.h"
#include "position.h"

#include <QObject>
#include <QString>
#include <string>

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
    bool m_active = false;
    MatchConfig m_config;
    Position m_position;
};
