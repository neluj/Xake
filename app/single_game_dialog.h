#pragma once

#include "match_settings_types.h"

#include <QDialog>

namespace Ui {
class SingleGameDialog;
}

class SingleGameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SingleGameDialog(QWidget *parent = nullptr);
    ~SingleGameDialog();

    MatchConfig config() const;

public slots:
    void accept() override;

private:
    Ui::SingleGameDialog *ui;
    MatchConfig m_config{};
};
