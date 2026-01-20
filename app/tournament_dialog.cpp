#include "tournament_dialog.h"
#include "./ui_tournament_dialog.h"

#include "match_settings_validation.h"

#include <QMessageBox>

TournamentDialog::TournamentDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TournamentDialog)
{
    ui->setupUi(this);

    if (ui->roundsSpin) {
        ui->roundsSpin->setMinimum(1);
    }
    if (ui->gamesPerPairingSpin) {
        ui->gamesPerPairingSpin->setMinimum(1);
    }
}

TournamentDialog::~TournamentDialog()
{
    delete ui;
}

TournamentConfig TournamentDialog::config() const
{
    return m_config;
}

void TournamentDialog::accept()
{
    if (!ui || !ui->matchSettingsWidget) {
        QDialog::accept();
        return;
    }

    TournamentConfig config;
    config.match.player1 = ui->matchSettingsWidget->player1Config();
    config.match.player2 = ui->matchSettingsWidget->player2Config();
    config.match.game = ui->matchSettingsWidget->gameConfig();
    config.tournamentType = ui->tournamentTypeCombo->currentText().trimmed();
    config.rounds = ui->roundsSpin->value();
    config.gamesPerPairing = ui->gamesPerPairingSpin->value();
    config.maxMoves = ui->maxMovesSpin->value();
    config.randomizeColors = ui->randomizeColorsCheck->isChecked();

    normalizeTournamentConfig(config);
    const ValidationError error = validateTournamentConfig(config);
    if (error != ValidationError::None) {
        QMessageBox::warning(this, validationErrorTitle(error), validationErrorMessage(error));
        return;
    }

    m_config = config;
    QDialog::accept();
}
