#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Xake"));
    QApplication::setApplicationName(QStringLiteral("Xake"));
    QApplication::setApplicationDisplayName(QStringLiteral("Xake"));
    QApplication::setWindowIcon(
        QIcon(QStringLiteral(":/assets/branding/xake-logo.png")));
    MainWindow w;
    w.show();
    return a.exec();
}
