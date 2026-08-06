#include "match_settings_widget.h"
#include "./ui_match_settings_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>

MatchSettingsWidget::MatchSettingsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MatchSettingsWidget)
{
    ui->setupUi(this);

    if (ui->baseTimeSpin) {
        m_customBaseTimeSeconds = ui->baseTimeSpin->value();
    }
    if (ui->incrementSpin) {
        m_customIncrementSeconds = ui->incrementSpin->value();
    }

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

    if (ui->timeControlCombo) {
        connect(ui->timeControlCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
            applyTimeControlSelection();
        });
    }
    if (ui->baseTimeSpin) {
        connect(ui->baseTimeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) {
            rememberCustomTimeSettings();
        });
    }
    if (ui->incrementSpin) {
        connect(ui->incrementSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) {
            rememberCustomTimeSettings();
        });
    }
    if (ui->useStartPosCheckBox) {
        connect(ui->useStartPosCheckBox, &QCheckBox::toggled, this, [this](bool) {
            applyOpeningFileSelection();
        });
    }
    if (ui->useOpeningFileCheckBox) {
        connect(ui->useOpeningFileCheckBox, &QCheckBox::toggled, this, [this](bool) {
            applyOpeningFileSelection();
        });
    }
    if (ui->openingFileBrowseButton) {
        connect(ui->openingFileBrowseButton, &QPushButton::clicked, this, [this]() {
            browseOpeningFile();
        });
    }

    applyOpeningFileSelection();
    applyTimeControlSelection();
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
    config.useOpeningFile = ui->useOpeningFileCheckBox->isChecked();
    config.openingFilePath = config.useOpeningFile
        ? ui->openingFileEdit->text().trimmed()
        : QString();
    config.useStartPos = ui->useStartPosCheckBox->isChecked();
    if (config.useStartPos) {
        config.startPosition = QStringLiteral("startpos");
    } else {
        config.startPosition = ui->startPositionEdit->text().trimmed();
    }
    return config;
}

void MatchSettingsWidget::setConfig(const MatchConfig& config)
{
    const auto setPlayer = [this](const PlayerConfig& player,
                                  QComboBox *typeCombo,
                                  QStackedWidget *stack,
                                  QLineEdit *nameEdit,
                                  QLineEdit *enginePathEdit,
                                  QPushButton *browseButton) {
        typeCombo->setCurrentIndex(
            player.type == PlayerType::Engine ? 1 : 0);
        nameEdit->setText(player.name);
        enginePathEdit->setText(player.enginePath);
        applyPlayerType(typeCombo, stack, nameEdit, enginePathEdit,
                        browseButton);
        if (!player.enginePath.isEmpty()) {
            m_lastEngineDir = QFileInfo(player.enginePath).absolutePath();
        }
    };

    setPlayer(config.player1,
              ui->player1TypeCombo,
              ui->player1Stacked,
              ui->player1NameEdit,
              ui->player1EnginePathEdit,
              ui->player1BrowseButton);
    setPlayer(config.player2,
              ui->player2TypeCombo,
              ui->player2Stacked,
              ui->player2NameEdit,
              ui->player2EnginePathEdit,
              ui->player2BrowseButton);

    const GameConfig& game = config.game;
    int timeControlIndex = ui->timeControlCombo->findText(
        game.timeControl,
        Qt::MatchFixedString);
    if (timeControlIndex < 0) {
        timeControlIndex = ui->timeControlCombo->findText(
            QStringLiteral("Custom"),
            Qt::MatchFixedString);
    }
    if (timeControlIndex >= 0) {
        if (ui->timeControlCombo->itemText(timeControlIndex).compare(
                QStringLiteral("Custom"), Qt::CaseInsensitive) == 0) {
            m_customBaseTimeSeconds = game.baseTimeSeconds;
            m_customIncrementSeconds = game.incrementSeconds;
        }
        ui->timeControlCombo->setCurrentIndex(timeControlIndex);
        applyTimeControlSelection();
    }

    ui->useOpeningFileCheckBox->setChecked(game.useOpeningFile);
    ui->openingFileEdit->setText(game.openingFilePath);
    ui->useStartPosCheckBox->setChecked(game.useStartPos);
    ui->startPositionEdit->setText(
        game.useStartPos ? QStringLiteral("startpos")
                         : game.startPosition);
    if (!game.openingFilePath.isEmpty()) {
        m_lastOpeningDir =
            QFileInfo(game.openingFilePath).absolutePath();
    }
    applyOpeningFileSelection();
}

void MatchSettingsWidget::setPlayersVisible(bool visible)
{
    if (ui && ui->groupBoxPlayers) {
        ui->groupBoxPlayers->setVisible(visible);
    }
}

void MatchSettingsWidget::applyOpeningFileSelection()
{
    if (!ui || !ui->useOpeningFileCheckBox || !ui->useStartPosCheckBox
        || !ui->startPositionEdit || !ui->openingFileEdit
        || !ui->openingFileBrowseButton) {
        return;
    }

    const bool useOpeningFile = ui->useOpeningFileCheckBox->isChecked();
    ui->useStartPosCheckBox->setEnabled(!useOpeningFile);
    ui->startPositionEdit->setEnabled(
        !useOpeningFile && !ui->useStartPosCheckBox->isChecked());
    ui->openingFileEdit->setEnabled(useOpeningFile);
    ui->openingFileBrowseButton->setEnabled(useOpeningFile);
}

void MatchSettingsWidget::applyTimeControlSelection()
{
    if (!ui->timeControlCombo || !ui->baseTimeSpin || !ui->incrementSpin) {
        return;
    }

    const QString selection = ui->timeControlCombo->currentText().trimmed();
    const bool isCustom = selection.compare(QStringLiteral("Custom"), Qt::CaseInsensitive) == 0;

    ui->baseTimeSpin->setEnabled(isCustom);
    ui->incrementSpin->setEnabled(isCustom);

    if (isCustom) {
        const QSignalBlocker baseBlocker(ui->baseTimeSpin);
        const QSignalBlocker incrementBlocker(ui->incrementSpin);
        ui->baseTimeSpin->setValue(m_customBaseTimeSeconds);
        ui->incrementSpin->setValue(m_customIncrementSeconds);
        return;
    }

    int baseTimeSeconds = 0;
    int incrementSeconds = 0;

    if (selection.compare(QStringLiteral("Bullet"), Qt::CaseInsensitive) == 0) {
        baseTimeSeconds = 60;
    } else if (selection.compare(QStringLiteral("Blitz"), Qt::CaseInsensitive) == 0) {
        baseTimeSeconds = 300;
    } else if (selection.compare(QStringLiteral("Rapid"), Qt::CaseInsensitive) == 0) {
        baseTimeSeconds = 600;
    } else {
        ui->baseTimeSpin->setEnabled(true);
        ui->incrementSpin->setEnabled(true);
        return;
    }

    const QSignalBlocker baseBlocker(ui->baseTimeSpin);
    const QSignalBlocker incrementBlocker(ui->incrementSpin);
    ui->baseTimeSpin->setValue(baseTimeSeconds);
    ui->incrementSpin->setValue(incrementSeconds);
}

void MatchSettingsWidget::rememberCustomTimeSettings()
{
    if (!ui->timeControlCombo || !ui->baseTimeSpin || !ui->incrementSpin) {
        return;
    }
    if (ui->timeControlCombo->currentText().trimmed().compare(QStringLiteral("Custom"), Qt::CaseInsensitive) != 0) {
        return;
    }

    m_customBaseTimeSeconds = ui->baseTimeSpin->value();
    m_customIncrementSeconds = ui->incrementSpin->value();
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

void MatchSettingsWidget::browseOpeningFile()
{
    QString startDir = m_lastOpeningDir;
    if (startDir.isEmpty()) {
        startDir = QDir::homePath();
    }

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select Opening File"),
        startDir,
        tr("Opening files (*.pgn *.epd *.edp);;PGN files (*.pgn);;"
           "EPD files (*.epd *.edp);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    m_lastOpeningDir = QFileInfo(path).absolutePath();
    ui->openingFileEdit->setText(path);
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
