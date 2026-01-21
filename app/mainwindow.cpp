#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "single_game_dialog.h"
#include "tournament_dialog.h"
#include "uci_client.h"

#include <QAction>
#include <QMessageBox>
#include <QString>

#include <string>

namespace {

const char kStartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

std::string resolveStartFen(const GameConfig& gameConfig)
{
    const QString start = gameConfig.startPosition.trimmed();
    if (start.compare(QStringLiteral("startpos"), Qt::CaseInsensitive) == 0) {
        return std::string(kStartFen);
    }
    return start.toStdString();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_uciClient(new UciClient(this))
    , m_gameController(new GameController(this))
{
    // Build the widget tree from the .ui description.
    ui->setupUi(this);

    if (ui->actionSingleGame) {
        connect(ui->actionSingleGame, &QAction::triggered, this, [this]() {
            SingleGameDialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                const MatchConfig config = dialog.config();
                m_state.lastMatch = config;
                m_state.hasLastMatch = true;
                startMatch(config);
            }
        });
    }

    if (ui->actionTournament) {
        connect(ui->actionTournament, &QAction::triggered, this, [this]() {
            TournamentDialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                const TournamentConfig config = dialog.config();
                m_state.lastTournament = config;
                m_state.hasLastTournament = true;
                startMatch(config.match);
            }
        });
    }

    connect(m_gameController, &GameController::positionChanged, this, [this](const Position& position) {
        if (ui && ui->widget) {
            ui->widget->setPosition(position);
        }
    });

    connect(m_gameController, &GameController::errorOccurred, this,
            [this](const QString& title, const QString& message) {
        QMessageBox::warning(this, title, message);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::startMatch(const MatchConfig& config)
{
    const std::string fen = resolveStartFen(config.game);
    return m_gameController->startMatch(config, fen);
}
