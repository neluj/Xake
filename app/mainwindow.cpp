#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "single_game_dialog.h"
#include "tournament_dialog.h"
#include "uci_client.h"

#include "fen.h"

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
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::startMatch(const MatchConfig& config)
{
    if (!ui || !ui->widget) {
        return false;
    }

    const std::string fen = resolveStartFen(config.game);
    Position position;
    if (!setFromFen(position, fen)) {
        QMessageBox::warning(this, tr("Invalid start position"),
                             tr("Start position is not a valid FEN."));
        return false;
    }

    m_state.currentPosition = position;
    m_state.hasCurrentPosition = true;
    ui->widget->setPosition(position);
    return true;
}
