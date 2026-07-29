#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "application_data.h"
#include "application_data_dialog.h"

namespace {

void writeFile(const QString& path, const QByteArray& contents)
{
    const QFileInfo fileInfo(path);
    QVERIFY(QDir().mkpath(fileInfo.absolutePath()));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(contents), contents.size());
}

QString sessionFile(const QString& dataDirectory,
                    const QString& session,
                    const QString& fileName)
{
    return QDir(dataDirectory)
        .filePath(QStringLiteral("sessions/%1/%2").arg(session, fileName));
}

} // namespace

class TestApplicationData : public QObject
{
    Q_OBJECT

private slots:
    void inspectsStoredDataByCategory();
    void deletesOnlySelectedFileCategories();
    void clearsOnlySettingsWhenRequested();
    void deletesAllApplicationData();
    void removesOwnedEmptyOrganizationDirectory();
    void rejectsUnsafeDataDirectory();
    void dialogUpdatesSelectAllState();
};

void TestApplicationData::inspectsStoredDataByCategory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString dataDirectory =
        QDir(directory.path()).filePath(QStringLiteral("XakeData"));

    writeFile(sessionFile(dataDirectory,
                          QStringLiteral("match"),
                          QStringLiteral("session_match.json")),
              QByteArray("record"));
    writeFile(sessionFile(dataDirectory,
                          QStringLiteral("tournament"),
                          QStringLiteral("tournament_report.json")),
              QByteArray("report"));
    writeFile(sessionFile(dataDirectory,
                          QStringLiteral("match"),
                          QStringLiteral("game.pgn")),
              QByteArray("pgn"));
    writeFile(sessionFile(dataDirectory,
                          QStringLiteral("match"),
                          QStringLiteral("uci_communication.log")),
              QByteArray("log"));
    writeFile(QDir(dataDirectory).filePath(QStringLiteral("future.data")),
              QByteArray("other"));

    const ApplicationDataSummary summary =
        inspectApplicationData(dataDirectory);
    QCOMPARE(summary.records.fileCount, 2);
    QCOMPARE(summary.records.byteCount, qint64(12));
    QCOMPARE(summary.pgnFiles.fileCount, 1);
    QCOMPARE(summary.communicationLogs.fileCount, 1);
    QCOMPARE(summary.otherFiles.fileCount, 1);
    QCOMPARE(summary.totalFileCount(), 5);
    QCOMPARE(summary.totalByteCount(), qint64(23));
}

void TestApplicationData::deletesOnlySelectedFileCategories()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString dataDirectory =
        QDir(directory.path()).filePath(QStringLiteral("XakeData"));
    const QString recordPath = sessionFile(
        dataDirectory,
        QStringLiteral("match"),
        QStringLiteral("session_match.json"));
    const QString pgnPath = sessionFile(
        dataDirectory,
        QStringLiteral("match"),
        QStringLiteral("game.pgn"));
    const QString logPath = sessionFile(
        dataDirectory,
        QStringLiteral("logs"),
        QStringLiteral("uci_communication.log"));
    const QString otherPath =
        QDir(dataDirectory).filePath(QStringLiteral("future.data"));
    writeFile(recordPath, QByteArray("record"));
    writeFile(pgnPath, QByteArray("pgn"));
    writeFile(logPath, QByteArray("log"));
    writeFile(otherPath, QByteArray("other"));

    QSettings settings(
        directory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat);
    settings.setValue(QStringLiteral("lastMatch/available"), true);
    settings.sync();

    ApplicationDataSelection selection;
    selection.records = true;
    selection.communicationLogs = true;
    const ApplicationDataDeletionResult result =
        deleteApplicationData(dataDirectory, selection, settings);

    QVERIFY(result.succeeded());
    QCOMPARE(result.deletedFiles, 2);
    QVERIFY(!QFileInfo::exists(recordPath));
    QVERIFY(!QFileInfo::exists(logPath));
    QVERIFY(QFileInfo::exists(pgnPath));
    QVERIFY(QFileInfo::exists(otherPath));
    QVERIFY(settings.contains(QStringLiteral("lastMatch/available")));
    QVERIFY(!QDir(QFileInfo(logPath).absolutePath()).exists());
    QVERIFY(QDir(dataDirectory).exists());
}

void TestApplicationData::clearsOnlySettingsWhenRequested()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString dataDirectory =
        QDir(directory.path()).filePath(QStringLiteral("XakeData"));
    const QString recordPath = sessionFile(
        dataDirectory,
        QStringLiteral("match"),
        QStringLiteral("session_match.json"));
    writeFile(recordPath, QByteArray("record"));

    QSettings settings(
        directory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat);
    settings.setValue(QStringLiteral("lastMatch/available"), true);
    settings.sync();

    ApplicationDataSelection selection;
    selection.settings = true;
    const ApplicationDataDeletionResult result =
        deleteApplicationData(dataDirectory, selection, settings);

    QVERIFY(result.succeeded());
    QVERIFY(result.settingsCleared);
    QCOMPARE(result.deletedFiles, 0);
    QVERIFY(settings.allKeys().isEmpty());
    QVERIFY(QFileInfo::exists(recordPath));
}

void TestApplicationData::deletesAllApplicationData()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString dataDirectory =
        QDir(directory.path()).filePath(QStringLiteral("XakeData"));
    writeFile(sessionFile(dataDirectory,
                          QStringLiteral("match"),
                          QStringLiteral("session_match.json")),
              QByteArray("record"));
    writeFile(sessionFile(dataDirectory,
                          QStringLiteral("match"),
                          QStringLiteral("game.pgn")),
              QByteArray("pgn"));
    writeFile(QDir(dataDirectory).filePath(QStringLiteral("future.data")),
              QByteArray("other"));

    QSettings settings(
        directory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat);
    settings.setValue(QStringLiteral("lastTournament/available"), true);
    settings.sync();

    ApplicationDataSelection selection;
    selection.records = true;
    selection.pgnFiles = true;
    selection.communicationLogs = true;
    selection.settings = true;
    const ApplicationDataDeletionResult result =
        deleteApplicationData(dataDirectory, selection, settings);

    QVERIFY(result.succeeded());
    QVERIFY(result.settingsCleared);
    QCOMPARE(result.deletedFiles, 3);
    QVERIFY(result.deletedDirectories >= 2);
    QVERIFY(!QDir(dataDirectory).exists());
    QVERIFY(settings.allKeys().isEmpty());
}

void TestApplicationData::removesOwnedEmptyOrganizationDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString organizationName = QStringLiteral("TestOrganization");
    const QString applicationName = QStringLiteral("TestApplication");
    const QString organizationDirectory =
        QDir(directory.path()).filePath(organizationName);
    const QString dataDirectory =
        QDir(organizationDirectory).filePath(applicationName);
    writeFile(QDir(dataDirectory).filePath(QStringLiteral("future.data")),
              QByteArray("other"));

    QSettings settings(
        directory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat);
    const QString previousOrganization =
        QCoreApplication::organizationName();
    const QString previousApplication =
        QCoreApplication::applicationName();
    QCoreApplication::setOrganizationName(organizationName);
    QCoreApplication::setApplicationName(applicationName);

    ApplicationDataSelection selection;
    selection.records = true;
    selection.pgnFiles = true;
    selection.communicationLogs = true;
    selection.settings = true;
    const ApplicationDataDeletionResult result =
        deleteApplicationData(dataDirectory, selection, settings, true);

    QCoreApplication::setOrganizationName(previousOrganization);
    QCoreApplication::setApplicationName(previousApplication);
    QVERIFY(result.succeeded());
    QVERIFY(!QDir(dataDirectory).exists());
    QVERIFY(!QDir(organizationDirectory).exists());
}

void TestApplicationData::rejectsUnsafeDataDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(
        directory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat);
    settings.setValue(QStringLiteral("keep"), true);

    ApplicationDataSelection selection;
    selection.settings = true;
    const ApplicationDataDeletionResult result =
        deleteApplicationData(QString(), selection, settings);

    QVERIFY(!result.succeeded());
    QVERIFY(!result.errors.isEmpty());
    QVERIFY(settings.value(QStringLiteral("keep")).toBool());
}

void TestApplicationData::dialogUpdatesSelectAllState()
{
    ApplicationDataSummary summary;
    summary.records.fileCount = 1;
    ApplicationDataDialog dialog(
        QDir(QDir::tempPath()).filePath(QStringLiteral("XakeData")),
        summary);

    auto *selectAll =
        dialog.findChild<QCheckBox *>(QStringLiteral("selectAllCheckBox"));
    auto *records =
        dialog.findChild<QCheckBox *>(QStringLiteral("recordsCheckBox"));
    auto *pgn =
        dialog.findChild<QCheckBox *>(QStringLiteral("pgnCheckBox"));
    auto *logs =
        dialog.findChild<QCheckBox *>(QStringLiteral("logsCheckBox"));
    auto *settings =
        dialog.findChild<QCheckBox *>(QStringLiteral("settingsCheckBox"));
    auto *deleteButton =
        dialog.findChild<QPushButton *>(QStringLiteral("deleteButton"));

    QVERIFY(selectAll);
    QVERIFY(records);
    QVERIFY(pgn);
    QVERIFY(logs);
    QVERIFY(settings);
    QVERIFY(deleteButton);
    QCOMPARE(selectAll->checkState(), Qt::Unchecked);
    QVERIFY(!deleteButton->isEnabled());

    records->setChecked(true);
    QCOMPARE(selectAll->checkState(), Qt::PartiallyChecked);
    QVERIFY(deleteButton->isEnabled());

    selectAll->click();
    QCOMPARE(selectAll->checkState(), Qt::Checked);
    QVERIFY(records->isChecked());
    QVERIFY(pgn->isChecked());
    QVERIFY(logs->isChecked());
    QVERIFY(settings->isChecked());

    selectAll->click();
    QCOMPARE(selectAll->checkState(), Qt::Unchecked);
    QVERIFY(!records->isChecked());
    QVERIFY(!pgn->isChecked());
    QVERIFY(!logs->isChecked());
    QVERIFY(!settings->isChecked());
    QVERIFY(!deleteButton->isEnabled());
}

QTEST_MAIN(TestApplicationData)

#include "test_application_data.moc"
