#include "application_data_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFont>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString formattedSize(qint64 bytes)
{
    constexpr qint64 kKilobyte = 1024;
    constexpr qint64 kMegabyte = 1024 * kKilobyte;
    const QLocale locale;
    if (bytes >= kMegabyte) {
        return locale.toString(
                   static_cast<double>(bytes) / kMegabyte,
                   'f',
                   1)
            + QStringLiteral(" MB");
    }
    if (bytes >= kKilobyte) {
        return locale.toString(
                   static_cast<double>(bytes) / kKilobyte,
                   'f',
                   1)
            + QStringLiteral(" KB");
    }
    return QObject::tr("%n byte(s)", nullptr, static_cast<int>(bytes));
}

QString categoryDetails(const ApplicationDataCategorySummary& category)
{
    return QObject::tr("%n file(s), %1",
                       nullptr,
                       category.fileCount)
        .arg(formattedSize(category.byteCount));
}

} // namespace

ApplicationDataDialog::ApplicationDataDialog(
    const QString& dataDirectory,
    const ApplicationDataSummary& summary,
    QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage application data"));
    setModal(true);
    resize(620, 410);

    auto *layout = new QVBoxLayout(this);
    auto *heading = new QLabel(tr("<h2>Manage application data</h2>"), this);
    layout->addWidget(heading);

    auto *description = new QLabel(
        tr("Choose which data created by Xake should be permanently deleted. "
           "External engines, opening files and the application itself are "
           "never removed."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *pathLabel = new QLabel(
        tr("Data directory: %1")
            .arg(QDir::toNativeSeparators(dataDirectory)),
        this);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel->setWordWrap(true);
    layout->addWidget(pathLabel);

    auto *summaryLabel = new QLabel(
        tr("%n stored file(s), %1 in total.",
           nullptr,
           summary.totalFileCount())
            .arg(formattedSize(summary.totalByteCount())),
        this);
    layout->addWidget(summaryLabel);

    m_selectAllCheckBox = new QCheckBox(tr("Select all application data"), this);
    m_selectAllCheckBox->setObjectName(QStringLiteral("selectAllCheckBox"));
    m_selectAllCheckBox->setTristate(true);
    QFont selectAllFont = m_selectAllCheckBox->font();
    selectAllFont.setBold(true);
    m_selectAllCheckBox->setFont(selectAllFont);
    layout->addWidget(m_selectAllCheckBox);

    m_recordsCheckBox = new QCheckBox(
        tr("Game and tournament records (%1)")
            .arg(categoryDetails(summary.records)),
        this);
    m_recordsCheckBox->setObjectName(QStringLiteral("recordsCheckBox"));
    layout->addWidget(m_recordsCheckBox);

    m_pgnCheckBox = new QCheckBox(
        tr("Exported PGN files (%1)")
            .arg(categoryDetails(summary.pgnFiles)),
        this);
    m_pgnCheckBox->setObjectName(QStringLiteral("pgnCheckBox"));
    layout->addWidget(m_pgnCheckBox);

    m_logsCheckBox = new QCheckBox(
        tr("Engine communication logs (%1)")
            .arg(categoryDetails(summary.communicationLogs)),
        this);
    m_logsCheckBox->setObjectName(QStringLiteral("logsCheckBox"));
    layout->addWidget(m_logsCheckBox);

    m_settingsCheckBox = new QCheckBox(
        tr("Saved settings and engine paths"),
        this);
    m_settingsCheckBox->setObjectName(QStringLiteral("settingsCheckBox"));
    layout->addWidget(m_settingsCheckBox);

    if (summary.otherFiles.fileCount > 0) {
        auto *otherFilesLabel = new QLabel(
            tr("Selecting all also removes %n other file(s) stored in Xake's "
               "data directory.",
               nullptr,
               summary.otherFiles.fileCount),
            this);
        otherFilesLabel->setWordWrap(true);
        layout->addWidget(otherFilesLabel);
    }

    layout->addStretch();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *openButton = buttonBox->addButton(
        tr("Open data folder"),
        QDialogButtonBox::ActionRole);
    openButton->setEnabled(QDir(dataDirectory).exists());
    m_deleteButton = buttonBox->addButton(
        tr("Delete selected data"),
        QDialogButtonBox::DestructiveRole);
    m_deleteButton->setObjectName(QStringLiteral("deleteButton"));
    m_deleteButton->setEnabled(false);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    connect(openButton, &QPushButton::clicked,
            this, &ApplicationDataDialog::openDataFolderRequested);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(m_selectAllCheckBox, &QCheckBox::clicked,
            this, [this](bool checked) {
        if (!m_updatingChecks) {
            setAllCategoriesChecked(checked);
        }
    });

    const auto updateFromChild = [this]() {
        updateSelectionState();
    };
    connect(m_recordsCheckBox, &QCheckBox::toggled,
            this, updateFromChild);
    connect(m_pgnCheckBox, &QCheckBox::toggled,
            this, updateFromChild);
    connect(m_logsCheckBox, &QCheckBox::toggled,
            this, updateFromChild);
    connect(m_settingsCheckBox, &QCheckBox::toggled,
            this, updateFromChild);
}

ApplicationDataSelection ApplicationDataDialog::selection() const
{
    ApplicationDataSelection selected;
    selected.records = m_recordsCheckBox->isChecked();
    selected.pgnFiles = m_pgnCheckBox->isChecked();
    selected.communicationLogs = m_logsCheckBox->isChecked();
    selected.settings = m_settingsCheckBox->isChecked();
    return selected;
}

void ApplicationDataDialog::setAllCategoriesChecked(bool checked)
{
    m_updatingChecks = true;
    m_recordsCheckBox->setChecked(checked);
    m_pgnCheckBox->setChecked(checked);
    m_logsCheckBox->setChecked(checked);
    m_settingsCheckBox->setChecked(checked);
    m_updatingChecks = false;
    updateSelectionState();
}

void ApplicationDataDialog::updateSelectionState()
{
    const ApplicationDataSelection selected = selection();
    int selectedCount = 0;
    selectedCount += selected.records ? 1 : 0;
    selectedCount += selected.pgnFiles ? 1 : 0;
    selectedCount += selected.communicationLogs ? 1 : 0;
    selectedCount += selected.settings ? 1 : 0;

    m_updatingChecks = true;
    if (selectedCount == 0) {
        m_selectAllCheckBox->setCheckState(Qt::Unchecked);
    } else if (selectedCount == 4) {
        m_selectAllCheckBox->setCheckState(Qt::Checked);
    } else {
        m_selectAllCheckBox->setCheckState(Qt::PartiallyChecked);
    }
    m_updatingChecks = false;
    m_deleteButton->setEnabled(selected.anySelected());
}
