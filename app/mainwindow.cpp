#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "uci_client.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_uciClient(new UciClient(this))
{
    // Build the widget tree from the .ui description.
    ui->setupUi(this);

    if (ui->actionConfigureEngine) {
        connect(ui->actionConfigureEngine, &QAction::triggered,
                this, &MainWindow::configureEngine);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::configureEngine()
{
    const QString filter =
        tr("Executable files (*.exe);;All files (*)");
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Chess Engine"), QString(), filter);
    if (path.isEmpty()) {
        return;
    }

    m_enginePath = path;
    statusBar()->showMessage(tr("Engine: %1").arg(QFileInfo(path).fileName()), 3000);

    if (m_uciClient->isRunning()) {
        m_uciClient->sendQuit();
        m_uciClient->stopProcess();
    }

    if (!m_uciClient->start(path)) {
        statusBar()->showMessage(tr("Failed to start engine"), 5000);
        return;
    }

    m_uciClient->sendUci();
    m_uciClient->sendIsReady();
}
