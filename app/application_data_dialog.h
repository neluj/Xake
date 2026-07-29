#pragma once

#include "application_data.h"

#include <QDialog>

class QCheckBox;
class QPushButton;

class ApplicationDataDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ApplicationDataDialog(
        const QString& dataDirectory,
        const ApplicationDataSummary& summary,
        QWidget *parent = nullptr);

    ApplicationDataSelection selection() const;

signals:
    void openDataFolderRequested();

private:
    void setAllCategoriesChecked(bool checked);
    void updateSelectionState();

    bool m_updatingChecks = false;
    QCheckBox *m_selectAllCheckBox = nullptr;
    QCheckBox *m_recordsCheckBox = nullptr;
    QCheckBox *m_pgnCheckBox = nullptr;
    QCheckBox *m_logsCheckBox = nullptr;
    QCheckBox *m_settingsCheckBox = nullptr;
    QPushButton *m_deleteButton = nullptr;
};
