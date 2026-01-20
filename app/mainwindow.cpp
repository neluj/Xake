#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "single_game_dialog.h"
#include "tournament_dialog.h"
#include "uci_client.h"

#include <QAction>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_uciClient(new UciClient(this))
{
    // Build the widget tree from the .ui description.
    ui->setupUi(this);

    if (ui->actionSingleGame) {
        connect(ui->actionSingleGame, &QAction::triggered, this, [this]() {
            SingleGameDialog dialog(this);
            dialog.exec();
        });
    }

    if (ui->actionTournament) {
        connect(ui->actionTournament, &QAction::triggered, this, [this]() {
            TournamentDialog dialog(this);
            dialog.exec();
        });
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
