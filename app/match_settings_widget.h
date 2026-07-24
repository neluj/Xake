#pragma once

#include "match_settings_types.h"

#include <QString>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;
class QStackedWidget;

namespace Ui {
class MatchSettingsWidget;
}

class MatchSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MatchSettingsWidget(QWidget *parent = nullptr);
    ~MatchSettingsWidget();

    PlayerConfig player1Config() const;
    PlayerConfig player2Config() const;
    GameConfig gameConfig() const;

private:
    void applyTimeControlSelection();
    void rememberCustomTimeSettings();
    void applyPlayerType(QComboBox *typeCombo,
                         QStackedWidget *stack,
                         QLineEdit *nameEdit,
                         QLineEdit *enginePathEdit,
                         QPushButton *browseButton);
    void browseEngine(QLineEdit *targetEdit);
    PlayerConfig makePlayerConfig(QComboBox *typeCombo,
                                  QLineEdit *nameEdit,
                                  QLineEdit *enginePathEdit) const;

    Ui::MatchSettingsWidget *ui;
    QString m_lastEngineDir;
    int m_customBaseTimeSeconds = 300;
    int m_customIncrementSeconds = 0;
    int m_customMovesToGo = 0;
};
