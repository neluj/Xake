#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "app_state.h"
#include "game_replay.h"
#include "game_controller.h"
#include "history_repository.h"
#include "session_record.h"

#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QTimer>

class QDialog;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class UciClient;
class TournamentRunner;
struct TournamentSummary;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class SessionKind {
        None,
        Match,
        Tournament
    };

    void configureEngine();
    bool startMatch(const MatchConfig& config, const TournamentConfig* tournament);
    bool stopActiveSession();
    void togglePause();
    void stopCurrentSession();
    void restartLastSession();
    void updateSessionControls();
    void clearTournamentPanel();
    void clearSessionPanels();
    void resetTournamentPanel(int totalGames);
    void updateTournamentStandings();
    void updateTournamentHistory();
    void updateGameMoveList();
    void updateCapturedPieces();
    void clearGameResult();
    void showGameResult(const QString& result,
                        const QString& termination,
                        const QString& message = QString());
    void updateGameOpeningLabel();
    void updatePlayerNames(const MatchConfig& match);
    void updateEngineOutputPanels(const MatchConfig& match);
    void updateDebugLogPath();
    void openDebugWindow();
    void manageApplicationData();
    void showAboutDialog();
    void refreshHistory();
    void populateHistoryTree();
    void updateHistoryDetails();
    void updateHistoryDeleteButton();
    void openSelectedHistoryPgn();
    void openSelectedHistoryDirectory();
    void deleteSelectedHistorySession();
    void openReplayFile();
    void replaySelectedHistory();
    bool beginReplay(const QVector<ReplayGame>& games,
                     int initialGameIndex = 0);
    bool loadReplayGame(int index);
    void navigateReplayTo(int ply);
    void updateReplayUi(Xake::Move animatedMove = Xake::NOMOVE);
    void updateReplayControls();
    void leaveReplay(bool resetGamePanel);
    void updateClockUi();
    void updateSideToMoveLabel(const Xake::Position& position);
    void finalizeMatchRecord(const QString& status,
                             const GameResult* result = nullptr,
                             const QString& abortTitle = QString(),
                             const QString& abortMessage = QString(),
                             GameTermination termination = GameTermination::Unknown);

    Ui::MainWindow *ui;
    UciClient *m_uciClient;
    QString m_enginePath;
    QString m_currentOpeningName;
    int m_currentOpeningIndex = 0;
    int m_currentOpeningCount = 0;
    SessionKind m_lastSessionKind = SessionKind::None;
    AppState m_state;
    GameController *m_gameController;
    TournamentRunner *m_tournamentRunner;
    QTimer *m_clockUiTimer = nullptr;
    QTimer *m_gameMovesLayoutTimer = nullptr;
    QPointer<QDialog> m_debugDialog;
    QPointer<QLabel> m_debugPathLabel;
    QPointer<QPlainTextEdit> m_debugText;
    QPointer<QPushButton> m_debugOpenLogButton;
    SessionRecord m_matchRecord;
    QString m_matchRecordPath;
    bool m_hasActiveMatchRecord = false;
    QVector<HistoryEntry> m_historyEntries;
    QVector<ReplayGame> m_replayGames;
    GameReplay m_replay;
    bool m_replayActive = false;
};
#endif // MAINWINDOW_H
