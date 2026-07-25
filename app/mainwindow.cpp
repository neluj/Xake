#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "single_game_dialog.h"
#include "session_record.h"
#include "tournament_dialog.h"
#include "tournament_runner.h"
#include "uci_client.h"

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QString>
#include <QSvgRenderer>
#include <QTextCursor>

#include <string>

using namespace Xake;

namespace {

const char kStartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const char kWhiteColorResource[] = ":/assets/colors/color_w.svg";
const char kBlackColorResource[] = ":/assets/colors/color_b.svg";
constexpr int kColorIndicatorSize = 28;

QPixmap colorIndicatorPixmap(Color color, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // The supplied white SVG needs a subtle outline on light widget backgrounds.
    if (color == WHITE) {
        const qreal radius = size * 18.0 / 45.0 + 1.0;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(96, 96, 96));
        painter.drawEllipse(QPointF(size / 2.0, size / 2.0), radius, radius);
    }

    QSvgRenderer renderer(color == WHITE ? QString::fromLatin1(kWhiteColorResource)
                                         : QString::fromLatin1(kBlackColorResource));
    renderer.render(&painter, QRectF(0, 0, size, size));
    return pixmap;
}

void setColorIndicator(QLabel *label, Color color)
{
    if (!label) {
        return;
    }

    label->setMinimumSize(kColorIndicatorSize, kColorIndicatorSize);
    label->setPixmap(colorIndicatorPixmap(color, kColorIndicatorSize));
    label->setAlignment(Qt::AlignCenter);
}

void configureMoveList(QPlainTextEdit *editor)
{
    if (!editor) {
        return;
    }

    QPalette palette = editor->palette();
    palette.setBrush(QPalette::Base, Qt::transparent);
    editor->setPalette(palette);
    editor->setAutoFillBackground(false);
    editor->viewport()->setAutoFillBackground(false);
    editor->setFrameShape(QFrame::NoFrame);
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    editor->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background: transparent; border: none; padding: 0px; }"));
}

QString formattedMoveList(const QStringList& moves)
{
    QStringList turns;
    for (qsizetype index = 0; index < moves.size(); index += 2) {
        QString turn = QStringLiteral("%1. %2")
            .arg(index / 2 + 1)
            .arg(moves.at(index));
        if (index + 1 < moves.size()) {
            turn += QStringLiteral(" %1").arg(moves.at(index + 1));
        }
        turns.append(turn);
    }
    return turns.join(QStringLiteral("  "));
}

QString formattedMoveRows(const QStringList& moves)
{
    QStringList rows;
    for (qsizetype index = 0; index < moves.size(); index += 2) {
        QString row = QStringLiteral("%1. %2")
            .arg(index / 2 + 1)
            .arg(moves.at(index));
        if (index + 1 < moves.size()) {
            row += QStringLiteral("     %1").arg(moves.at(index + 1));
        }
        rows.append(row);
    }
    return rows.join('\n');
}

void setMoveListText(QPlainTextEdit *editor, const QString& text)
{
    if (!editor) {
        return;
    }

    editor->setPlainText(text);
    editor->moveCursor(QTextCursor::End);
    editor->ensureCursorVisible();
}

QString formatClockMs(qint64 ms)
{
    ms = qMax<qint64>(0, ms);

    const qint64 totalSeconds = ms / 1000;
    if (ms < 10000) {
        const qint64 tenths = (ms % 1000) / 100;
        return QString("%1.%2")
            .arg(totalSeconds, 2, 10, QChar('0'))
            .arg(tenths);
    }

    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString playerDisplayName(const PlayerConfig& player, const QString& fallback)
{
    const QString name = player.name.trimmed();
    if (!name.isEmpty()) {
        return name;
    }

    const QString engineName = QFileInfo(player.enginePath).completeBaseName();
    return engineName.isEmpty() ? fallback : engineName;
}

QString gameResultText(GameOutcome outcome)
{
    switch (outcome) {
    case GameOutcome::WhiteWin:
        return QStringLiteral("W: 1-0");
    case GameOutcome::BlackWin:
        return QStringLiteral("L: 0-1");
    case GameOutcome::Draw:
        return QStringLiteral("D: 1/2-1/2");
    }

    return QString();
}

QString tournamentResultText(const TournamentSummary& summary)
{
    return QStringLiteral("W: %1 - D: %2 - L: %3")
        .arg(summary.whiteWins)
        .arg(summary.draws)
        .arg(summary.blackWins);
}

QString tournamentHistoryText(const QVector<TournamentGameRecord>& games)
{
    QStringList lines;
    for (const TournamentGameRecord& game : games) {
        QString status = QObject::tr("IN PROGRESS");
        if (game.completed) {
            status = gameResultText(game.result.outcome);
        } else if (game.aborted) {
            status = QObject::tr("ABORTED");
        }

        lines.append(
            QStringLiteral("GAME %1  |  %2")
                .arg(game.gameNumber, 3)
                .arg(status));
        if (!game.moves.isEmpty()) {
            lines.append(formattedMoveList(game.moves));
        }
        if (game.aborted && !game.abortMessage.isEmpty()) {
            lines.append(QStringLiteral("     %1").arg(game.abortMessage));
        }
        lines.append(QString());
    }

    if (!lines.isEmpty()) {
        lines.removeLast();
    }
    return lines.join('\n');
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
    , m_tournamentRunner(new TournamentRunner(m_gameController, this))
    , m_clockUiTimer(new QTimer(this))
{
    // Build the widget tree from the .ui description.
    ui->setupUi(this);
    setColorIndicator(ui->labelWhiteTime, WHITE);
    setColorIndicator(ui->labelBlackTime, BLACK);
    setColorIndicator(ui->labelTournamentWhiteColor, WHITE);
    setColorIndicator(ui->labelTournamentBlackColor, BLACK);
    configureMoveList(ui->gameMovesText);
    configureMoveList(ui->tournamentHistoryText);
    setTournamentTabActive(false);

    if (m_clockUiTimer) {
        m_clockUiTimer->setInterval(100);
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

    connect(m_gameController, &GameController::positionChanged, this, [this](const Xake::Position& position) {
        if (ui && ui->board) {
            ui->board->setPosition(position);
        }
        updateSideToMoveLabel(position);
        updateClockUi();
    });

    connect(m_gameController, &GameController::movePlayed, this,
            [this](int, const QString&) {
        updateGameMoveList();
        if (m_tournamentRunner && m_tournamentRunner->isActive()) {
            updateTournamentHistory();
        }
    });

    connect(m_gameController, &GameController::matchStarted, this, [this](const MatchConfig& match) {
        updatePlayerNames(match);
        updateSideToMoveLabel(m_gameController->currentPosition());
        updateGameMoveList();
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
        if (ui && ui->labelWhitePlayer && ui->labelBlackPlayer) {
            ui->labelWhitePlayer->clear();
            ui->labelBlackPlayer->clear();
        }
        if (ui && ui->gameMovesText) {
            ui->gameMovesText->clear();
        }
    });

    connect(m_gameController, &GameController::errorOccurred, this,
            [this](const QString& title, const QString& message) {
        const bool isNormalGameResult = title == tr("Checkmate")
            || title == tr("Draw")
            || title == tr("Time");
        if (m_tournamentRunner && m_tournamentRunner->isActive() && isNormalGameResult) {
            if (ui && ui->statusbar) {
                ui->statusbar->showMessage(message);
            }
            return;
        }
        QMessageBox::warning(this, title, message);
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentStarted, this,
            [this](int totalGames) {
        setTournamentTabActive(true);
        resetTournamentPanel(totalGames);
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentGameStarted, this,
            [this](int gameNumber, int totalGames, const MatchConfig&) {
        if (ui && ui->labelTournamentStatus) {
            ui->labelTournamentStatus->setText(
                tr("Game %1 of %2")
                    .arg(gameNumber)
                    .arg(totalGames));
        }
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(
                tr("Tournament game %1 of %2.").arg(gameNumber).arg(totalGames));
        }
        updateTournamentHistory();
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentGameFinished, this,
            [this](int, const GameResult&) {
        updateTournamentHistory();
        updateTournamentScore(m_tournamentRunner->summary());
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentFinished, this,
            [this](const TournamentSummary& summary) {
        const QString message = tr("Tournament finished after %1 games.\nResult: %2")
            .arg(summary.completedGames)
            .arg(tournamentResultText(summary));
        if (ui && ui->labelTournamentStatus) {
            ui->labelTournamentStatus->setText(tr("Tournament finished"));
        }
        updateTournamentHistory();
        updateTournamentScore(summary);
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(message);
        }
        setTournamentTabActive(false);
        QMessageBox::information(this, tr("Tournament finished"), message);
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentAborted, this,
            [this](const QString& title, const QString& message) {
        if (ui && ui->labelTournamentStatus) {
            ui->labelTournamentStatus->setText(tr("Tournament aborted"));
        }
        updateTournamentHistory();
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(tr("%1: %2").arg(title, message));
        }
        setTournamentTabActive(false);
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentReportError, this,
            [this](const QString& message) {
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(message);
        }
        QMessageBox::warning(this, tr("Tournament report"), message);
    });

    if (ui && ui->board) {
        connect(ui->board, &BoardWidget::moveRequested, this, [this](Xake::Move move) {
            m_gameController->applyHumanMove(move);
        });
    }

    updateClockUi();
    if (ui && ui->labelSideToMove) {
        ui->labelSideToMove->clear();
    }
    if (ui && ui->labelWhitePlayer && ui->labelBlackPlayer) {
        ui->labelWhitePlayer->clear();
        ui->labelBlackPlayer->clear();
    }
    updateGameMoveList();
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::startMatch(const MatchConfig& config, const TournamentConfig* tournament)
{
    if (m_tournamentRunner && m_tournamentRunner->isActive()) {
        QMessageBox::warning(this,
                             tr("Tournament in progress"),
                             tr("Finish the current tournament before starting another game."));
        return false;
    }
    if (m_gameController && m_gameController->isActive()) {
        QMessageBox::warning(this,
                             tr("Game in progress"),
                             tr("Finish the current game before starting another one."));
        return false;
    }

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

    if (tournament) {
        return m_tournamentRunner->start(*tournament, fen, sessionDir, sessionTag);
    }

    return m_gameController->startMatch(config, fen, sessionDir, sessionTag);
}

void MainWindow::setTournamentTabActive(bool active)
{
    if (!ui || !ui->tabWidget || !ui->tabTournament) {
        return;
    }

    const int index = ui->tabWidget->indexOf(ui->tabTournament);
    if (index < 0) {
        return;
    }

    if (!active && ui->tabWidget->currentIndex() == index) {
        ui->tabWidget->setCurrentIndex(0);
    }
    ui->tabWidget->setTabEnabled(index, active);
    if (active) {
        ui->tabWidget->setCurrentIndex(index);
    }
}

void MainWindow::resetTournamentPanel(int totalGames)
{
    if (!ui) {
        return;
    }

    const TournamentConfig& config = m_state.lastTournament;
    if (ui->labelTournamentStatus) {
        ui->labelTournamentStatus->setText(tr("Preparing tournament"));
    }
    if (ui->labelTournamentInfo) {
        const QString reportName = QFileInfo(m_tournamentRunner->reportFilePath()).fileName();
        ui->labelTournamentInfo->setText(
            tr("Type: %1\nFormat: %2 rounds x %3 games (%4 total)\n"
               "Time: %5 %6+%7\nReport: %8")
                .arg(config.tournamentType)
                .arg(config.rounds)
                .arg(config.gamesPerPairing)
                .arg(totalGames)
                .arg(config.match.game.timeControl)
                .arg(config.match.game.baseTimeSeconds)
                .arg(config.match.game.incrementSeconds)
                .arg(reportName));
    }
    if (ui->tournamentHistoryText) {
        ui->tournamentHistoryText->clear();
    }
    updateTournamentScore(m_tournamentRunner->summary());
}

void MainWindow::updateTournamentScore(const TournamentSummary& summary)
{
    if (!ui || !ui->labelTournamentScore) {
        return;
    }

    ui->labelTournamentScore->setText(QStringLiteral(": ") + tournamentResultText(summary));
}

void MainWindow::updateTournamentHistory()
{
    if (!ui || !ui->tournamentHistoryText || !m_tournamentRunner) {
        return;
    }

    setMoveListText(ui->tournamentHistoryText,
                    tournamentHistoryText(m_tournamentRunner->gameRecords()));
}

void MainWindow::updateGameMoveList()
{
    if (!ui || !ui->gameMovesText || !m_gameController) {
        return;
    }

    setMoveListText(ui->gameMovesText,
                    formattedMoveRows(m_gameController->moveHistoryUci()));
}

void MainWindow::updatePlayerNames(const MatchConfig& match)
{
    if (!ui || !ui->labelWhitePlayer || !ui->labelBlackPlayer) {
        return;
    }

    ui->labelWhitePlayer->setText(playerDisplayName(match.player1, tr("Player")));
    ui->labelBlackPlayer->setText(playerDisplayName(match.player2, tr("Player")));
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

void MainWindow::updateSideToMoveLabel(const Xake::Position& position)
{
    if (!ui || !ui->labelSideToMove) {
        return;
    }
    setColorIndicator(ui->labelSideToMove, position.get_side_to_move());
}
