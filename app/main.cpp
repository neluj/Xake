#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Xake"));
    QApplication::setApplicationName(QStringLiteral("Xake"));
    QApplication::setApplicationDisplayName(QStringLiteral("Xake"));
    QApplication::setApplicationVersion(QStringLiteral(XAKE_VERSION));
    QApplication::setWindowIcon(
        QIcon(QStringLiteral(":/assets/branding/xake-logo.png")));

    MainWindow w;
    if (a.arguments().contains(QStringLiteral("--smoke-test"))) {
        QTimer::singleShot(0, &a, &QCoreApplication::quit);
    } else {
        w.show();
    }

    return a.exec();
}
