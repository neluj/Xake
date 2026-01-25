#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "single_game_dialog.h"
#include "session_record.h"
#include "tournament_dialog.h"
#include "uci_client.h"

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QString>

#include <string>

namespace {

const char kStartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

QString formatClockMs(qint64 ms)
{
    if (ms < 0) {
        ms = 0;
    }
    const qint64 totalSeconds = ms / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

std::string resolveStartFen(const GameConfig& gameConfig)
{
    const QString start = gameConfig.startPosition.trimmed();
    if (start.compare(QStringLiteral("startpos"), Qt::CaseInsensitive) == 0) {
        return std::string(kStartFen);
    }
    return start.toStdString();
}

bool ensureSessionDir(const QString& dir, QString* errorOut)
{
    if (QDir().mkpath(dir)) {
        return true;
    }
    if (errorOut) {
        *errorOut = QStringLiteral("Failed to create directory: %1").arg(dir);
    }
    return false;
}

void warnSessionRecordFailure(QWidget *parent, const QString& detail)
{
    if (!detail.isEmpty()) {
        QMessageBox::warning(parent,
                             QObject::tr("Session log"),
                             QObject::tr("Failed to write session record: %1").arg(detail));
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_uciClient(new UciClient(this))
    , m_gameController(new GameController(this))
    , m_clockUiTimer(new QTimer(this))
{
    // Build the widget tree from the .ui description.
    ui->setupUi(this);

    if (m_clockUiTimer) {
        m_clockUiTimer->setInterval(200);
        connect(m_clockUiTimer, &QTimer::timeout, this, [this]() {
            updateClockUi();
        });
    }

    if (ui->actionSingleGame) {
        connect(ui->actionSingleGame, &QAction::triggered, this, [this]() {
            SingleGameDialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                const MatchConfig config = dialog.config();
                m_state.lastMatch = config;
                m_state.hasLastMatch = true;
                startMatch(config, nullptr);
            }
        });
    }

    if (ui->actionTournament) {
        connect(ui->actionTournament, &QAction::triggered, this, [this]() {
            TournamentDialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                const TournamentConfig config = dialog.config();
                m_state.lastTournament = config;
                m_state.hasLastTournament = true;
                startMatch(config.match, &config);
            }
        });
    }

    connect(m_gameController, &GameController::positionChanged, this, [this](const Position& position) {
        if (ui && ui->board) {
            ui->board->setPosition(position);
        }
        updateSideToMoveLabel(position);
        updateClockUi();
    });

    connect(m_gameController, &GameController::matchStarted, this, [this](const MatchConfig&) {
        updateSideToMoveLabel(m_gameController->currentPosition());
        updateClockUi();
        if (m_clockUiTimer) {
            m_clockUiTimer->start();
        }
    });

    connect(m_gameController, &GameController::matchStopped, this, [this]() {
        if (m_clockUiTimer) {
            m_clockUiTimer->stop();
        }
        updateClockUi();
        if (ui && ui->labelSideToMove) {
            ui->labelSideToMove->clear();
        }
    });

    connect(m_gameController, &GameController::errorOccurred, this,
            [this](const QString& title, const QString& message) {
        QMessageBox::warning(this, title, message);
    });

    if (ui && ui->board) {
        connect(ui->board, &BoardWidget::moveRequested, this, [this](Move move) {
            m_gameController->applyHumanMove(move);
        });
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::startMatch(const MatchConfig& config, const TournamentConfig* tournament)
{
    const std::string fen = resolveStartFen(config.game);
    const QString sessionTag = sessionTagNow();
    const QString sessionType = tournament ? QStringLiteral("tournament")
                                           : QStringLiteral("match");
    const QString sessionDir = defaultSessionDir(sessionTag, sessionType);
    QString errorDetail;
    if (!ensureSessionDir(sessionDir, &errorDetail)) {
        warnSessionRecordFailure(this, errorDetail);
    } else {
        SessionRecord record;
        record.sessionType = sessionType;
        record.sessionTag = sessionTag;
        record.startTimeIso = QDateTime::currentDateTime().toString(Qt::ISODate);
        record.logDir = sessionDir;
        record.match = config;
        record.startFen = QString::fromStdString(fen);
        if (tournament) {
            record.hasTournament = true;
            record.tournament = *tournament;
        }

        const QString recordPath = QDir(sessionDir)
            .filePath(QString("session_%1.json").arg(sessionTag));
        if (!writeSessionRecord(record, recordPath, &errorDetail)) {
            warnSessionRecordFailure(this, errorDetail);
        }
    }

    return m_gameController->startMatch(config, fen, sessionDir, sessionTag);
}

void MainWindow::updateClockUi()
{
    if (!ui || !m_gameController) {
        return;
    }
    if (!ui->whiteTimeLcd || !ui->blackTimeLcd) {
        return;
    }

    if (!m_gameController->isActive() || !m_gameController->timeControlEnabled()) {
        ui->whiteTimeLcd->display("--:--");
        ui->blackTimeLcd->display("--:--");
        return;
    }

    const qint64 whiteMs = m_gameController->remainingTimeMs(WHITE);
    const qint64 blackMs = m_gameController->remainingTimeMs(BLACK);
    ui->whiteTimeLcd->display(formatClockMs(whiteMs));
    ui->blackTimeLcd->display(formatClockMs(blackMs));
}

void MainWindow::updateSideToMoveLabel(const Position& position)
{
    if (!ui || !ui->labelSideToMove) {
        return;
    }
    ui->labelSideToMove->setText(position.stm == WHITE ? tr("White") : tr("Black"));
}
