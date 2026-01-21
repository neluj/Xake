#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "single_game_dialog.h"
#include "tournament_dialog.h"
#include "uci_client.h"

#include <QAction>
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

    const auto applyMatchConfig = [this](const MatchConfig& config) {
        if (!ui || !ui->widget) {
            return;
        }
        const std::string fen = resolveStartFen(config.game);
        ui->widget->setFromFenString(fen);
    };

    if (ui->actionSingleGame) {
        connect(ui->actionSingleGame, &QAction::triggered, this, [this, applyMatchConfig]() {
            SingleGameDialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                const MatchConfig config = dialog.config();
                m_state.lastMatch = config;
                m_state.hasLastMatch = true;
                applyMatchConfig(config);
            }
        });
    }

    if (ui->actionTournament) {
        connect(ui->actionTournament, &QAction::triggered, this, [this, applyMatchConfig]() {
            TournamentDialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                const TournamentConfig config = dialog.config();
                m_state.lastTournament = config;
                m_state.hasLastTournament = true;
                applyMatchConfig(config.match);
            }
        });
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
