#include "single_game_dialog.h"
#include "./ui_single_game_dialog.h"

#include "match_settings_validation.h"

#include <QMessageBox>

SingleGameDialog::SingleGameDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SingleGameDialog)
{
    ui->setupUi(this);
}

SingleGameDialog::~SingleGameDialog()
{
    delete ui;
}

MatchConfig SingleGameDialog::config() const
{
    return m_config;
}

void SingleGameDialog::setConfig(const MatchConfig& config)
{
    if (ui && ui->matchSettingsWidget) {
        ui->matchSettingsWidget->setConfig(config);
    }
}

void SingleGameDialog::accept()
{
    if (!ui || !ui->matchSettingsWidget) {
        QDialog::accept();
        return;
    }

    MatchConfig config;
    config.player1 = ui->matchSettingsWidget->player1Config();
    config.player2 = ui->matchSettingsWidget->player2Config();
    config.game = ui->matchSettingsWidget->gameConfig();

    normalizeMatchConfig(config);
    const ValidationError error = validateMatchConfig(config);
    if (error != ValidationError::None) {
        QMessageBox::warning(this, validationErrorTitle(error),
                             validationErrorMessage(error));
        return;
    }

    m_config = config;
    QDialog::accept();
}
