#pragma once

#include "game_controller.h"
#include "match_settings_types.h"
#include "opening_book.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QVector>

#include <string>

struct TournamentSummary {
    int totalGames = 0;
    int completedGames = 0;
    int player1Wins = 0;
    int player2Wins = 0;
    int draws = 0;
    int whiteWins = 0;
    int blackWins = 0;
};

Q_DECLARE_METATYPE(TournamentSummary)

struct TournamentGameRecord {
    int gameNumber = 0;
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
    TournamentSummary summary() const;
    QVector<TournamentGameRecord> gameRecords() const;
    QString reportFilePath() const;
    QString pgnFilePath() const;
    int openingCount() const;

signals:
    void tournamentStarted(int totalGames);
    void tournamentGameStarted(int gameNumber, int totalGames, const MatchConfig& match);
    void tournamentGameFinished(int gameNumber, const GameResult& result);
    void tournamentFinished(const TournamentSummary& summary);
    void tournamentAborted(const QString& title, const QString& message);
    void pauseChanged(bool paused);
    void tournamentReportError(const QString& message);

private:
    void startNextGame();
    void handleMovePlayed(int ply, const QString& uciMove);
    void handleGameFinished(const GameResult& result);
    void handleGameAborted(GameTermination termination,
                           const QString& title,
                           const QString& message);
    MatchConfig matchForCurrentGame() const;
    const OpeningEntry& openingForCurrentGame() const;
    bool colorsAreSwappedForCurrentGame() const;
    void finishTournament();
    TournamentGameRecord* currentGameRecord();
    void persistReport();
    void persistPgn();
    bool writeReport(QString* errorOut) const;
    bool writeTournamentPgn(QString* errorOut) const;

    GameController *m_gameController = nullptr;
    TournamentConfig m_config;
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
    int m_nextGameNumber = 1;
    int m_currentGameNumber = 0;
    bool m_currentColorsSwapped = false;
    bool m_active = false;
    bool m_paused = false;
    bool m_waitingForNextGame = false;
    bool m_reportErrorEmitted = false;
    bool m_pgnErrorEmitted = false;
    quint64 m_runGeneration = 0;
};
