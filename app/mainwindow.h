#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "app_state.h"

#include <QMainWindow>
#include <QString>

class UciClient;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void configureEngine();

    Ui::MainWindow *ui;
    UciClient *m_uciClient;
    QString m_enginePath;
    AppState m_state;
};
#endif // MAINWINDOW_H
