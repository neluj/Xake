#include "tournament_dialog.h"
#include "./ui_tournament_dialog.h"

#include "match_settings_validation.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QUuid>

namespace {

QString participantName(const PlayerConfig& player)
{
    if (!player.name.trimmed().isEmpty()) {
        return player.name.trimmed();
    }
    const QString executable =
        QFileInfo(player.enginePath).completeBaseName();
    return executable.isEmpty()
        ? QObject::tr("Unnamed participant")
        : executable;
}

bool editParticipant(QWidget *parent,
                     TournamentParticipant& participant)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(
        participant.id.isEmpty()
            ? QObject::tr("Add participant")
            : QObject::tr("Edit participant"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *typeCombo = new QComboBox(&dialog);
    typeCombo->addItem(QObject::tr("Human"),
                       static_cast<int>(PlayerType::Human));
    typeCombo->addItem(QObject::tr("Engine"),
                       static_cast<int>(PlayerType::Engine));
    typeCombo->setCurrentIndex(
        participant.player.type == PlayerType::Engine ? 1 : 0);

    auto *nameEdit = new QLineEdit(
        participant.player.name, &dialog);
    nameEdit->setPlaceholderText(
        QObject::tr("Required for humans; optional engine alias"));

    auto *engineContainer = new QWidget(&dialog);
    auto *engineLayout = new QHBoxLayout(engineContainer);
    engineLayout->setContentsMargins(0, 0, 0, 0);
    auto *enginePathEdit = new QLineEdit(
        participant.player.enginePath, engineContainer);
    enginePathEdit->setReadOnly(true);
    auto *browseButton =
        new QPushButton(QObject::tr("Browse..."), engineContainer);
    engineLayout->addWidget(enginePathEdit, 1);
    engineLayout->addWidget(browseButton);

    form->addRow(QObject::tr("Type"), typeCombo);
    form->addRow(QObject::tr("Display name"), nameEdit);
    form->addRow(QObject::tr("Engine executable"),
                 engineContainer);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted,
                     &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     &dialog, &QDialog::reject);

    const auto updateType = [typeCombo, engineContainer]() {
        engineContainer->setEnabled(
            typeCombo->currentData().toInt()
            == static_cast<int>(PlayerType::Engine));
    };
    QObject::connect(
        typeCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        &dialog,
        [updateType](int) { updateType(); });
    QObject::connect(browseButton, &QPushButton::clicked,
                     &dialog, [&dialog, enginePathEdit]() {
#if defined(Q_OS_WIN)
        const QString filter =
            QObject::tr("Executable files (*.exe);;All files (*)");
#else
        const QString filter = QObject::tr("All files (*)");
#endif
        const QString initialDirectory =
            enginePathEdit->text().isEmpty()
            ? QString()
            : QFileInfo(enginePathEdit->text()).absolutePath();
        const QString path = QFileDialog::getOpenFileName(
            &dialog,
            QObject::tr("Select chess engine"),
            initialDirectory,
            filter);
        if (!path.isEmpty()) {
            enginePathEdit->setText(path);
        }
    });
    updateType();

    for (;;) {
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        const PlayerType type =
            typeCombo->currentData().toInt()
                == static_cast<int>(PlayerType::Engine)
            ? PlayerType::Engine
            : PlayerType::Human;
        const QString name = nameEdit->text().trimmed();
        const QString enginePath = enginePathEdit->text().trimmed();
        if (type == PlayerType::Human && name.isEmpty()) {
            QMessageBox::warning(
                &dialog,
                QObject::tr("Missing participant name"),
                QObject::tr("Enter a name for the human participant."));
            continue;
        }
        if (type == PlayerType::Engine && enginePath.isEmpty()) {
            QMessageBox::warning(
                &dialog,
                QObject::tr("Missing engine"),
                QObject::tr("Select an engine executable."));
            continue;
        }

        participant.player.type = type;
        participant.player.name = name;
        participant.player.enginePath =
            type == PlayerType::Engine ? enginePath : QString();
        return true;
    }
}

} // namespace

TournamentDialog::TournamentDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TournamentDialog)
{
    ui->setupUi(this);

    ui->matchSettingsWidget->setPlayersVisible(false);
    ui->roundsLabel->setText(tr("Cycles"));
    ui->roundsLabel->setToolTip(
        tr("Number of complete passes through all pairings"));

    auto *participantsGroup =
        new QGroupBox(tr("Participants"), this);
    auto *participantsLayout =
        new QVBoxLayout(participantsGroup);
    m_participantsTable = new QTableWidget(participantsGroup);
    m_participantsTable->setObjectName(
        QStringLiteral("participantsTable"));
    m_participantsTable->setColumnCount(3);
    m_participantsTable->setHorizontalHeaderLabels(
        {tr("Participant"), tr("Type"), tr("Configuration")});
    m_participantsTable->setSelectionBehavior(
        QAbstractItemView::SelectRows);
    m_participantsTable->setSelectionMode(
        QAbstractItemView::SingleSelection);
    m_participantsTable->setEditTriggers(
        QAbstractItemView::NoEditTriggers);
    m_participantsTable->verticalHeader()->setVisible(false);
    m_participantsTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    m_participantsTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    m_participantsTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Stretch);
    m_participantsTable->setMinimumHeight(150);
    participantsLayout->addWidget(m_participantsTable);

    auto *participantButtons = new QHBoxLayout;
    auto *addButton =
        new QPushButton(tr("Add"), participantsGroup);
    m_editParticipantButton =
        new QPushButton(tr("Edit"), participantsGroup);
    m_removeParticipantButton =
        new QPushButton(tr("Remove"), participantsGroup);
    addButton->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    m_editParticipantButton->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_removeParticipantButton->setIcon(
        style()->standardIcon(QStyle::SP_TrashIcon));
    participantButtons->addWidget(addButton);
    participantButtons->addWidget(m_editParticipantButton);
    participantButtons->addWidget(m_removeParticipantButton);
    participantButtons->addStretch();
    participantsLayout->addLayout(participantButtons);
    ui->verticalLayout->insertWidget(0, participantsGroup);

    m_formatCombo = new QComboBox(this);
    m_formatCombo->setObjectName(QStringLiteral("formatCombo"));
    m_formatCombo->addItem(
        tr("Round-robin"),
        static_cast<int>(TournamentFormat::RoundRobin));
    m_formatCombo->addItem(
        tr("Gauntlet"),
        static_cast<int>(TournamentFormat::Gauntlet));
    m_gauntletParticipantCombo = new QComboBox(this);
    m_gauntletParticipantCombo->setObjectName(
        QStringLiteral("gauntletParticipantCombo"));
    if (auto *settingsForm = qobject_cast<QFormLayout*>(
            ui->groupBoxTournamentSettings->layout())) {
        settingsForm->insertRow(0, tr("Format"),
                                m_formatCombo);
        settingsForm->insertRow(1, tr("Main participant"),
                                m_gauntletParticipantCombo);
    }

    connect(addButton, &QPushButton::clicked,
            this, &TournamentDialog::addParticipant);
    connect(m_editParticipantButton, &QPushButton::clicked,
            this, &TournamentDialog::editSelectedParticipant);
    connect(m_removeParticipantButton, &QPushButton::clicked,
            this, &TournamentDialog::removeSelectedParticipant);
    connect(m_participantsTable,
            &QTableWidget::itemSelectionChanged,
            this, &TournamentDialog::updateParticipantActions);
    connect(m_participantsTable, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) {
        editSelectedParticipant();
    });
    connect(m_formatCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        updateFormatControls();
    });

    if (ui->roundsSpin) {
        ui->roundsSpin->setMinimum(1);
    }
    if (ui->gamesPerPairingSpin) {
        ui->gamesPerPairingSpin->setMinimum(1);
    }

    PlayerConfig firstPlayer;
    firstPlayer.type = PlayerType::Human;
    firstPlayer.name = tr("Player 1");
    PlayerConfig secondPlayer;
    secondPlayer.type = PlayerType::Human;
    secondPlayer.name = tr("Player 2");
    m_participants = {
        {QUuid::createUuid().toString(QUuid::WithoutBraces),
         firstPlayer},
        {QUuid::createUuid().toString(QUuid::WithoutBraces),
         secondPlayer}
    };
    refreshParticipantTable();
    updateFormatControls();
}

TournamentDialog::~TournamentDialog()
{
    delete ui;
}

TournamentConfig TournamentDialog::config() const
{
    return m_config;
}

void TournamentDialog::setConfig(const TournamentConfig& config)
{
    if (!ui) {
        return;
    }
    TournamentConfig normalized = config;
    normalizeTournamentConfig(normalized);
    if (normalized.participants.size() >= 2) {
        m_participants = normalized.participants;
    }
    if (ui->matchSettingsWidget) {
        ui->matchSettingsWidget->setConfig(normalized.match);
    }
    m_formatCombo->setCurrentIndex(
        m_formatCombo->findData(
            static_cast<int>(normalized.format)));
    ui->roundsSpin->setValue(normalized.rounds);
    ui->gamesPerPairingSpin->setValue(
        normalized.gamesPerPairing);
    ui->maxMovesSpin->setValue(normalized.maxMoves);
    refreshParticipantTable();
    const int gauntletIndex =
        m_gauntletParticipantCombo->findData(
            normalized.gauntletParticipantId);
    if (gauntletIndex >= 0) {
        m_gauntletParticipantCombo->setCurrentIndex(gauntletIndex);
    }
    updateFormatControls();
}

void TournamentDialog::accept()
{
    if (!ui || !ui->matchSettingsWidget) {
        QDialog::accept();
        return;
    }

    TournamentConfig config;
    config.participants = m_participants;
    config.format =
        static_cast<TournamentFormat>(
            m_formatCombo->currentData().toInt());
    config.gauntletParticipantId =
        m_gauntletParticipantCombo->currentData().toString();
    config.match.game = ui->matchSettingsWidget->gameConfig();
    if (config.participants.size() >= 2) {
        config.match.player1 = config.participants.at(0).player;
        config.match.player2 = config.participants.at(1).player;
    }
    config.rounds = ui->roundsSpin->value();
    config.gamesPerPairing = ui->gamesPerPairingSpin->value();
    config.maxMoves = ui->maxMovesSpin->value();

    normalizeTournamentConfig(config);
    const ValidationError error = validateTournamentConfig(config);
    if (error != ValidationError::None) {
        QMessageBox::warning(this, validationErrorTitle(error),
                             validationErrorMessage(error));
        return;
    }

    m_config = config;
    QDialog::accept();
}

void TournamentDialog::addParticipant()
{
    TournamentParticipant participant;
    participant.id =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    participant.player.type = PlayerType::Human;
    if (!editParticipant(this, participant)) {
        return;
    }

    m_participants.append(participant);
    refreshParticipantTable();
    m_participantsTable->selectRow(
        m_participants.size() - 1);
}

void TournamentDialog::editSelectedParticipant()
{
    const int row = m_participantsTable->currentRow();
    if (row < 0 || row >= m_participants.size()) {
        return;
    }

    TournamentParticipant participant = m_participants.at(row);
    if (!editParticipant(this, participant)) {
        return;
    }
    m_participants[row] = participant;
    refreshParticipantTable();
    m_participantsTable->selectRow(row);
}

void TournamentDialog::removeSelectedParticipant()
{
    const int row = m_participantsTable->currentRow();
    if (row < 0 || row >= m_participants.size()) {
        return;
    }

    m_participants.removeAt(row);
    refreshParticipantTable();
    if (!m_participants.isEmpty()) {
        m_participantsTable->selectRow(
            qMin(row, static_cast<int>(m_participants.size()) - 1));
    }
}

void TournamentDialog::refreshParticipantTable()
{
    if (!m_participantsTable) {
        return;
    }

    const QSignalBlocker blocker(m_participantsTable);
    m_participantsTable->setRowCount(m_participants.size());
    for (qsizetype row = 0; row < m_participants.size(); ++row) {
        const TournamentParticipant& participant =
            m_participants.at(row);
        auto *nameItem =
            new QTableWidgetItem(participantName(participant.player));
        nameItem->setData(Qt::UserRole, participant.id);
        auto *typeItem = new QTableWidgetItem(
            participant.player.type == PlayerType::Engine
                ? tr("Engine")
                : tr("Human"));
        const QString configuration =
            participant.player.type == PlayerType::Engine
            ? participant.player.enginePath
            : tr("Local player");
        auto *configurationItem =
            new QTableWidgetItem(configuration);
        configurationItem->setToolTip(configuration);
        m_participantsTable->setItem(row, 0, nameItem);
        m_participantsTable->setItem(row, 1, typeItem);
        m_participantsTable->setItem(row, 2, configurationItem);
    }

    refreshGauntletParticipants();
    updateParticipantActions();
}

void TournamentDialog::refreshGauntletParticipants()
{
    if (!m_gauntletParticipantCombo) {
        return;
    }

    const QString selectedId =
        m_gauntletParticipantCombo->currentData().toString();
    const QSignalBlocker blocker(m_gauntletParticipantCombo);
    m_gauntletParticipantCombo->clear();
    for (const TournamentParticipant& participant : m_participants) {
        m_gauntletParticipantCombo->addItem(
            participantName(participant.player),
            participant.id);
    }
    int index =
        m_gauntletParticipantCombo->findData(selectedId);
    if (index < 0 && m_gauntletParticipantCombo->count() > 0) {
        index = 0;
    }
    m_gauntletParticipantCombo->setCurrentIndex(index);
}

void TournamentDialog::updateParticipantActions()
{
    const bool selected =
        m_participantsTable
        && m_participantsTable->currentRow() >= 0;
    if (m_editParticipantButton) {
        m_editParticipantButton->setEnabled(selected);
    }
    if (m_removeParticipantButton) {
        m_removeParticipantButton->setEnabled(selected);
    }
}

void TournamentDialog::updateFormatControls()
{
    if (!m_formatCombo || !m_gauntletParticipantCombo) {
        return;
    }
    const bool gauntlet =
        static_cast<TournamentFormat>(
            m_formatCombo->currentData().toInt())
        == TournamentFormat::Gauntlet;
    m_gauntletParticipantCombo->setEnabled(gauntlet);
}
