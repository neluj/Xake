#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "app_state.h"
#include "game_controller.h"

#include <QMainWindow>
#include <QString>
#include <QTimer>

class UciClient;

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
    void updateClockUi();
    void updateSideToMoveLabel(const Xake::Position& position);

    Ui::MainWindow *ui;
    UciClient *m_uciClient;
    QString m_enginePath;
    AppState m_state;
    GameController *m_gameController;
    QTimer *m_clockUiTimer = nullptr;
};
#endif // MAINWINDOW_H
