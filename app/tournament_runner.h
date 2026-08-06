#pragma once

#include "game_controller.h"
#include "match_settings_types.h"
#include "opening_book.h"
#include "tournament_schedule.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QVector>

#include <string>

struct TournamentStanding {
    QString participantId;
    QString name;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    int whiteGames = 0;
    int blackGames = 0;
    QString sequence;

    int games() const
    {
        return wins + losses + draws;
    }

    double points() const
    {
        return wins + draws * 0.5;
    }
};

struct TournamentSummary {
    int totalGames = 0;
    int completedGames = 0;
    int player1Wins = 0;
    int player2Wins = 0;
    int draws = 0;
    int whiteWins = 0;
    int blackWins = 0;
    QVector<TournamentStanding> standings;
};

Q_DECLARE_METATYPE(TournamentSummary)

struct TournamentGameRecord {
    int gameNumber = 0;
    int roundNumber = 0;
    int cycleNumber = 0;
    int gameInPairing = 0;
    QString whiteParticipantId;
    QString blackParticipantId;
    MatchConfig match;
    QString startedAtIso;
    QString finishedAtIso;
    QVector<MoveRecord> moveRecords;
    QStringList moves;
    bool colorsSwapped = false;
    bool completed = false;
    bool aborted = false;
    GameResult result;
    GameTermination termination = GameTermination::Unknown;
    QString abortTitle;
    QString abortMessage;
    int openingIndex = 0;
    QString openingName;
    QString startFen;
    QStringList openingMoves;
};

class TournamentRunner : public QObject
{
    Q_OBJECT

public:
    explicit TournamentRunner(GameController *gameController, QObject *parent = nullptr);

    bool start(const TournamentConfig& config,
               const QVector<OpeningEntry>& openings,
               const QString& logDir,
               const QString& sessionTag);
    bool pause();
    bool resume();
    bool stop();
    bool isActive() const;
    bool isPaused() const;
    void setHumanGameConfirmationEnabled(bool enabled);
    bool startPendingHumanGame();
    bool isWaitingForHumanGame() const;
    TournamentSummary summary() const;
    QVector<TournamentGameRecord> gameRecords() const;
    QString reportFilePath() const;
    QString pgnFilePath() const;
    int openingCount() const;

signals:
    void tournamentStarted(int totalGames);
    void tournamentGameStarted(int gameNumber, int totalGames, const MatchConfig& match);
    void humanGameReadyRequested(int gameNumber,
                                 int totalGames,
                                 const MatchConfig& match);
    void tournamentGameFinished(int gameNumber, const GameResult& result);
    void tournamentFinished(const TournamentSummary& summary);
    void tournamentAborted(const QString& title, const QString& message);
    void pauseChanged(bool paused);
    void tournamentReportError(const QString& message);

private:
    void startNextGame();
    void launchCurrentGame();
    void handleMovePlayed(int ply, const QString& uciMove);
    void handleGameFinished(const GameResult& result);
    void handleEngineFailure(EngineFailure failure,
                             EngineSide side,
                             const QString& message);
    void handleGameAborted(GameTermination termination,
                           const QString& title,
                           const QString& message);
    void completeCurrentGame(const GameResult& result);
    MatchConfig matchForScheduledGame(
        const TournamentScheduledGame& game) const;
    const OpeningEntry& openingForScheduledGame(
        const TournamentScheduledGame& game) const;
    const TournamentParticipant* participant(
        const QString& participantId) const;
    TournamentStanding* standing(const QString& participantId);
    void finishTournament();
    TournamentGameRecord* currentGameRecord();
    void persistReport();
    void persistPgn();
    bool writeReport(QString* errorOut) const;
    bool writeTournamentPgn(QString* errorOut) const;

    GameController *m_gameController = nullptr;
    TournamentConfig m_config;
    QVector<TournamentScheduledGame> m_schedule;
    QVector<OpeningEntry> m_openings;
    QString m_logDir;
    QString m_sessionTag;
    TournamentSummary m_summary;
    QVector<TournamentGameRecord> m_gameRecords;
    QString m_reportFilePath;
    QString m_pgnFilePath;
    QString m_status;
    QString m_startedAtIso;
    QString m_finishedAtIso;
    QString m_abortTitle;
    QString m_abortMessage;
    int m_nextScheduleIndex = 0;
    int m_currentGameNumber = 0;
    TournamentScheduledGame m_currentScheduledGame;
    bool m_currentColorsSwapped = false;
    bool m_active = false;
    bool m_paused = false;
    bool m_waitingForNextGame = false;
    bool m_waitingForHumanGame = false;
    bool m_humanGameConfirmationEnabled = false;
    int m_failedEngineSide = -1;
    bool m_reportErrorEmitted = false;
    bool m_pgnErrorEmitted = false;
    quint64 m_runGeneration = 0;
};
