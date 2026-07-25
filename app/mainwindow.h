#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "app_state.h"
#include "game_controller.h"

#include <QMainWindow>
#include <QString>
#include <QTimer>

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
    void configureEngine();
    bool startMatch(const MatchConfig& config, const TournamentConfig* tournament);
    void setTournamentTabActive(bool active);
    void resetTournamentPanel(int totalGames);
    void updateTournamentStandings();
    void updateTournamentHistory();
    void updateGameMoveList();
    void updatePlayerNames(const MatchConfig& match);
    void updateClockUi();
    void updateSideToMoveLabel(const Xake::Position& position);

    Ui::MainWindow *ui;
    UciClient *m_uciClient;
    QString m_enginePath;
    AppState m_state;
    GameController *m_gameController;
    TournamentRunner *m_tournamentRunner;
    QTimer *m_clockUiTimer = nullptr;
};
#endif // MAINWINDOW_H
