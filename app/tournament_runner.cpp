#include "tournament_runner.h"

#include "match_settings_validation.h"

#include <QRandomGenerator>
#include <QTimer>

#include <limits>
#include <utility>

TournamentRunner::TournamentRunner(GameController *gameController, QObject *parent)
    : QObject(parent)
    , m_gameController(gameController)
{
    Q_ASSERT(m_gameController);

    connect(m_gameController, &GameController::gameFinished,
            this, &TournamentRunner::handleGameFinished);
    connect(m_gameController, &GameController::gameAborted,
            this, &TournamentRunner::handleGameAborted);
}

bool TournamentRunner::start(const TournamentConfig& config,
                             const std::string& startFen,
                             const QString& logDir,
                             const QString& sessionTag)
{
    if (m_active || !m_gameController) {
        return false;
    }

    TournamentConfig normalized = config;
    normalizeTournamentConfig(normalized);
    if (validateTournamentConfig(normalized) != ValidationError::None) {
        return false;
    }

    const qint64 totalGames = static_cast<qint64>(normalized.rounds)
        * normalized.gamesPerPairing;
    if (totalGames < 1 || totalGames > std::numeric_limits<int>::max()) {
        return false;
    }

    m_config = normalized;
    m_startFen = startFen;
    m_logDir = logDir;
    m_sessionTag = sessionTag;
    m_summary = TournamentSummary{static_cast<int>(totalGames)};
    m_nextGameNumber = 1;
    m_currentGameNumber = 0;
    m_currentColorsSwapped = false;
    m_active = true;

    emit tournamentStarted(m_summary.totalGames);
    startNextGame();
    return true;
}

bool TournamentRunner::isActive() const
{
    return m_active;
}

TournamentSummary TournamentRunner::summary() const
{
    return m_summary;
}

void TournamentRunner::startNextGame()
{
    if (!m_active) {
        return;
    }
    if (m_nextGameNumber > m_summary.totalGames) {
        finishTournament();
        return;
    }

    m_currentGameNumber = m_nextGameNumber++;
    m_currentColorsSwapped = colorsAreSwappedForCurrentGame();
    const MatchConfig match = matchForCurrentGame();
    emit tournamentGameStarted(m_currentGameNumber, m_summary.totalGames, match);

    const QString gameTag = QStringLiteral("%1_game%2")
        .arg(m_sessionTag)
        .arg(m_currentGameNumber, 3, 10, QChar('0'));
    if (!m_gameController->startMatch(match,
                                      m_startFen,
                                      m_logDir,
                                      gameTag,
                                      m_config.maxMoves)) {
        m_active = false;
        emit tournamentAborted(tr("Tournament error"),
                               tr("Could not start game %1.").arg(m_currentGameNumber));
    }
}

void TournamentRunner::handleGameFinished(const GameResult& result)
{
    if (!m_active || m_currentGameNumber == 0) {
        return;
    }

    ++m_summary.completedGames;
    switch (result.outcome) {
    case GameOutcome::WhiteWin:
        ++m_summary.whiteWins;
        if (m_currentColorsSwapped) {
            ++m_summary.player2Wins;
        } else {
            ++m_summary.player1Wins;
        }
        break;
    case GameOutcome::BlackWin:
        ++m_summary.blackWins;
        if (m_currentColorsSwapped) {
            ++m_summary.player1Wins;
        } else {
            ++m_summary.player2Wins;
        }
        break;
    case GameOutcome::Draw:
        ++m_summary.draws;
        break;
    }

    emit tournamentGameFinished(m_currentGameNumber, result);
    QTimer::singleShot(0, this, [this]() {
        startNextGame();
    });
}

void TournamentRunner::handleGameAborted(const QString& title, const QString& message)
{
    if (!m_active) {
        return;
    }

    m_active = false;
    emit tournamentAborted(title, message);
}

MatchConfig TournamentRunner::matchForCurrentGame() const
{
    MatchConfig match = m_config.match;
    if (m_currentColorsSwapped) {
        std::swap(match.player1, match.player2);
    }
    return match;
}

bool TournamentRunner::colorsAreSwappedForCurrentGame() const
{
    if (m_config.randomizeColors) {
        return QRandomGenerator::global()->bounded(2) == 1;
    }

    return (m_currentGameNumber % 2) == 0;
}

void TournamentRunner::finishTournament()
{
    if (!m_active) {
        return;
    }

    m_active = false;
    emit tournamentFinished(m_summary);
}
