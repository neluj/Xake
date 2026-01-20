#pragma once

#include "match_settings_types.h"

#include <QDialog>

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

public slots:
    void accept() override;

private:
    Ui::TournamentDialog *ui;
    TournamentConfig m_config{};
};
