#include "game_controller.h"

#include "fen.h"
#include "match_settings_validation.h"

#include <QString>

GameController::GameController(QObject *parent)
    : QObject(parent)
{
}

bool GameController::startMatch(const MatchConfig& config, const std::string& fen)
{
    MatchConfig normalized = config;
    normalizeMatchConfig(normalized);
    const ValidationError error = validateMatchConfig(normalized);
    if (error != ValidationError::None) {
        emit errorOccurred(validationErrorTitle(error),
                           validationErrorMessage(error));
        return false;
    }

    Position position;
    if (!setFromFen(position, fen)) {
        emit errorOccurred(tr("Invalid start position"),
                           tr("Start position is not a valid FEN."));
        return false;
    }

    m_config = normalized;
    m_position = position;
    m_active = true;
    emit matchStarted(m_config);
    emit positionChanged(m_position);
    return true;
}

void GameController::stopMatch()
{
    if (!m_active) {
        return;
    }
    m_active = false;
    emit matchStopped();
}

bool GameController::isActive() const
{
    return m_active;
}

MatchConfig GameController::matchConfig() const
{
    return m_config;
}

Position GameController::currentPosition() const
{
    return m_position;
}
