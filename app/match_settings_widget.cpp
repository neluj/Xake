#include "match_settings_widget.h"
#include "./ui_match_settings_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>

MatchSettingsWidget::MatchSettingsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MatchSettingsWidget)
{
    ui->setupUi(this);

    applyPlayerType(ui->player1TypeCombo, ui->player1Stacked,
                    ui->player1NameEdit, ui->player1EnginePathEdit,
                    ui->player1BrowseButton);
    applyPlayerType(ui->player2TypeCombo, ui->player2Stacked,
                    ui->player2NameEdit, ui->player2EnginePathEdit,
                    ui->player2BrowseButton);

    connect(ui->player1TypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        applyPlayerType(ui->player1TypeCombo, ui->player1Stacked,
                        ui->player1NameEdit, ui->player1EnginePathEdit,
                        ui->player1BrowseButton);
    });
    connect(ui->player2TypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        applyPlayerType(ui->player2TypeCombo, ui->player2Stacked,
                        ui->player2NameEdit, ui->player2EnginePathEdit,
                        ui->player2BrowseButton);
    });

    connect(ui->player1BrowseButton, &QPushButton::clicked, this, [this]() {
        browseEngine(ui->player1EnginePathEdit);
    });
    connect(ui->player2BrowseButton, &QPushButton::clicked, this, [this]() {
        browseEngine(ui->player2EnginePathEdit);
    });

    if (ui->useStartPosCheckBox) {
        connect(ui->useStartPosCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
            if (ui->startPositionEdit) {
                ui->startPositionEdit->setEnabled(!checked);
            }
        });
        ui->startPositionEdit->setEnabled(!ui->useStartPosCheckBox->isChecked());
    }
}

MatchSettingsWidget::~MatchSettingsWidget()
{
    delete ui;
}

PlayerConfig MatchSettingsWidget::player1Config() const
{
    return makePlayerConfig(ui->player1TypeCombo,
                            ui->player1NameEdit,
                            ui->player1EnginePathEdit);
}

PlayerConfig MatchSettingsWidget::player2Config() const
{
    return makePlayerConfig(ui->player2TypeCombo,
                            ui->player2NameEdit,
                            ui->player2EnginePathEdit);
}

GameConfig MatchSettingsWidget::gameConfig() const
{
    GameConfig config;
    config.timeControl = ui->timeControlCombo->currentText().trimmed();
    config.baseTimeSeconds = ui->baseTimeSpin->value();
    config.incrementSeconds = ui->incrementSpin->value();
    config.movesToGo = ui->movesToGoSpin->value();
    config.useStartPos = ui->useStartPosCheckBox->isChecked();
    if (config.useStartPos) {
        config.startPosition = QStringLiteral("startpos");
    } else {
        config.startPosition = ui->startPositionEdit->text().trimmed();
    }
    return config;
}

void MatchSettingsWidget::applyPlayerType(QComboBox *typeCombo,
                                          QStackedWidget *stack,
                                          QLineEdit *nameEdit,
                                          QLineEdit *enginePathEdit,
                                          QPushButton *browseButton)
{
    if (!typeCombo || !stack) {
        return;
    }

    const bool isEngine = (typeCombo->currentIndex() == 1);
    stack->setCurrentIndex(isEngine ? 1 : 0);

    if (stack->count() > 1 && stack->widget(1)) {
        stack->widget(1)->setEnabled(isEngine);
    }

    if (nameEdit) {
        nameEdit->setEnabled(!isEngine);
    }
    if (enginePathEdit) {
        enginePathEdit->setEnabled(isEngine);
    }
    if (browseButton) {
        browseButton->setEnabled(isEngine);
    }
}

void MatchSettingsWidget::browseEngine(QLineEdit *targetEdit)
{
    if (!targetEdit) {
        return;
    }

    QString startDir = m_lastEngineDir;
    if (startDir.isEmpty()) {
        startDir = QDir::homePath();
    }

#if defined(Q_OS_WIN)
    const QString filter = tr("Executable files (*.exe);;All files (*)");
#else
    const QString filter = tr("All files (*)");
#endif

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Chess Engine"), startDir, filter);
    if (path.isEmpty()) {
        return;
    }

    m_lastEngineDir = QFileInfo(path).absolutePath();
    targetEdit->setText(path);
}

PlayerConfig MatchSettingsWidget::makePlayerConfig(QComboBox *typeCombo,
                                                   QLineEdit *nameEdit,
                                                   QLineEdit *enginePathEdit) const
{
    PlayerConfig config;
    const bool isEngine = typeCombo && typeCombo->currentIndex() == 1;
    config.type = isEngine ? PlayerType::Engine : PlayerType::Human;
    if (isEngine) {
        if (enginePathEdit) {
            config.enginePath = enginePathEdit->text().trimmed();
        }
    } else {
        if (nameEdit) {
            config.name = nameEdit->text().trimmed();
        }
    }
    return config;
}
