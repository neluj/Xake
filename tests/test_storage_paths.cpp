#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QtTest>

#include "storage_paths.h"

class TestStoragePaths : public QObject
{
    Q_OBJECT

private slots:
    void usesWritableApplicationDataLocation();
};

void TestStoragePaths::usesWritableApplicationDataLocation()
{
    const QString expectedRoot = QDir(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("sessions"));

    QCOMPARE(sessionsRootDir(), QDir::cleanPath(expectedRoot));
    QCOMPARE(defaultSessionDir(QStringLiteral("20260726_120000"),
                               QStringLiteral("match")),
             QDir(expectedRoot).filePath(
                 QStringLiteral("20260726_120000_match")));
    QVERIFY(QDir::isAbsolutePath(defaultSessionDir(QStringLiteral("tag"),
                                                   QStringLiteral("tournament"))));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Xake"));
    QCoreApplication::setApplicationName(QStringLiteral("StoragePathTests"));
    QStandardPaths::setTestModeEnabled(true);

    TestStoragePaths test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_storage_paths.moc"
