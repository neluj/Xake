#pragma once

#include "game_controller.h"
#include "match_settings_types.h"

#include <QObject>
#include <QString>

#include <string>

struct TournamentSummary {
    int totalGames = 0;
    int completedGames = 0;
    int player1Wins = 0;
    int player2Wins = 0;
    int draws = 0;
};

Q_DECLARE_METATYPE(TournamentSummary)

class TournamentRunner : public QObject
{
    Q_OBJECT

public:
    explicit TournamentRunner(GameController *gameController, QObject *parent = nullptr);

    bool start(const TournamentConfig& config,
               const std::string& startFen,
               const QString& logDir,
               const QString& sessionTag);
    bool isActive() const;

signals:
    void tournamentStarted(int totalGames);
    void tournamentGameStarted(int gameNumber, int totalGames, const MatchConfig& match);
    void tournamentGameFinished(int gameNumber, const GameResult& result);
    void tournamentFinished(const TournamentSummary& summary);
    void tournamentAborted(const QString& title, const QString& message);

private:
    void startNextGame();
    void handleGameFinished(const GameResult& result);
    void handleGameAborted(const QString& title, const QString& message);
    MatchConfig matchForCurrentGame() const;
    bool colorsAreSwappedForCurrentGame() const;
    void finishTournament();

    GameController *m_gameController = nullptr;
    TournamentConfig m_config;
    std::string m_startFen;
    QString m_logDir;
    QString m_sessionTag;
    TournamentSummary m_summary;
    int m_nextGameNumber = 1;
    int m_currentGameNumber = 0;
    bool m_currentColorsSwapped = false;
    bool m_active = false;
};
