#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "app_state.h"
#include "game_controller.h"
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
    void setTournamentTabActive(bool active);
    void clearTournamentPanel();
    void resetTournamentPanel(int totalGames);
    void updateTournamentStandings();
    void updateTournamentHistory();
    void updateGameMoveList();
    void updateGameOpeningLabel();
    void updatePlayerNames(const MatchConfig& match);
    void updateEngineOutputPanels(const MatchConfig& match);
    void updateDebugLogPath();
    void openDebugWindow();
    void updateClockUi();
    void updateSideToMoveLabel(const Xake::Position& position);
    void finalizeMatchRecord(const QString& status,
                             const GameResult* result = nullptr,
                             const QString& abortTitle = QString(),
                             const QString& abortMessage = QString());

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
    QPointer<QDialog> m_debugDialog;
    QPointer<QLabel> m_debugPathLabel;
    QPointer<QPlainTextEdit> m_debugText;
    QPointer<QPushButton> m_debugOpenLogButton;
    SessionRecord m_matchRecord;
    QString m_matchRecordPath;
    bool m_hasActiveMatchRecord = false;
};
#endif // MAINWINDOW_H
