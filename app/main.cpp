#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Xake"));
    QApplication::setApplicationDisplayName(QStringLiteral("Xake"));
    MainWindow w;
    w.show();
    return a.exec();
}
