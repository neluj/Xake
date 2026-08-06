#pragma once

#include "match_settings_types.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QPushButton;
class QTableWidget;

namespace Ui {
class TournamentDialog;
}

class TournamentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TournamentDialog(QWidget *parent = nullptr);
    ~TournamentDialog();

    TournamentConfig config() const;
    void setConfig(const TournamentConfig& config);

public slots:
    void accept() override;

private:
    void addParticipant();
    void editSelectedParticipant();
    void removeSelectedParticipant();
    void refreshParticipantTable();
    void refreshGauntletParticipants();
    void updateParticipantActions();
    void updateFormatControls();

    Ui::TournamentDialog *ui;
    TournamentConfig m_config{};
    QVector<TournamentParticipant> m_participants;
    QTableWidget *m_participantsTable = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QComboBox *m_gauntletParticipantCombo = nullptr;
    QPushButton *m_editParticipantButton = nullptr;
    QPushButton *m_removeParticipantButton = nullptr;
};
