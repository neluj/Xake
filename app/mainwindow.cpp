#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "single_game_dialog.h"
#include "opening_book.h"
#include "session_record.h"
#include "tournament_dialog.h"
#include "tournament_runner.h"
#include "uci_client.h"

#include <QAction>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QString>
#include <QSvgRenderer>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include <cmath>
#include <string>

using namespace Xake;

namespace {

const char kStartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const char kWhiteColorResource[] = ":/assets/colors/color_w.svg";
const char kBlackColorResource[] = ":/assets/colors/color_b.svg";
constexpr int kColorIndicatorSize = 28;
const char kOpeningMoveColor[] = "#247C8F";
const char kPlayedMoveColor[] = "#B85C2B";

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

void configureEngineOutput(QPlainTextEdit *editor)
{
    configureMoveList(editor);
    if (!editor) {
        return;
    }

    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setMaximumBlockCount(3000);
}

QTextCharFormat moveTextFormat(bool openingMove)
{
    QTextCharFormat format;
    format.setForeground(QColor(
        openingMove ? QString::fromLatin1(kOpeningMoveColor)
                    : QString::fromLatin1(kPlayedMoveColor)));
    format.setFontWeight(QFont::DemiBold);
    return format;
}

void configureMoveLegend(QLabel *label)
{
    if (!label) {
        return;
    }

    label->setTextFormat(Qt::RichText);
    label->setText(
        QStringLiteral("<span style=\"color:%1; font-weight:600\">%2</span>"
                       "&nbsp;&nbsp;|&nbsp;&nbsp;"
                       "<span style=\"color:%3; font-weight:600\">%4</span>")
            .arg(QString::fromLatin1(kOpeningMoveColor),
                 QObject::tr("Opening moves"),
                 QString::fromLatin1(kPlayedMoveColor),
                 QObject::tr("Played moves")));
}

void appendFormattedMoves(QTextCursor& cursor,
                          const QStringList& moves,
                          int openingMoveCount,
                          bool oneTurnPerRow)
{
    const int moveCount = static_cast<int>(moves.size());
    const int openingCount = qBound(0, openingMoveCount, moveCount);
    const QTextCharFormat neutralFormat;
    for (qsizetype index = 0; index < moves.size(); ++index) {
        if ((index % 2) == 0) {
            if (index > 0) {
                cursor.insertText(oneTurnPerRow ? QStringLiteral("\n")
                                                : QStringLiteral("  "),
                                  neutralFormat);
            }
            cursor.insertText(QStringLiteral("%1. ").arg(index / 2 + 1),
                              neutralFormat);
        } else {
            cursor.insertText(oneTurnPerRow ? QStringLiteral("     ")
                                            : QStringLiteral(" "),
                              neutralFormat);
        }
        cursor.insertText(moves.at(index),
                          moveTextFormat(index < openingCount));
    }
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

QString gameResultNotation(GameOutcome outcome)
{
    switch (outcome) {
    case GameOutcome::WhiteWin:
        return QStringLiteral("1-0");
    case GameOutcome::BlackWin:
        return QStringLiteral("0-1");
    case GameOutcome::Draw:
        return QStringLiteral("1/2-1/2");
    }

    return QString();
}

struct PlayerStanding {
    QString name;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    QString sequence;

    int games() const
    {
        return wins + losses + draws;
    }

    double points() const
    {
        return wins + draws * 0.5;
    }
};

QPair<PlayerStanding, PlayerStanding> tournamentStandings(
    const TournamentConfig& config,
    const QVector<TournamentGameRecord>& games)
{
    PlayerStanding player1;
    player1.name = playerDisplayName(config.match.player1, QObject::tr("Player 1"));
    PlayerStanding player2;
    player2.name = playerDisplayName(config.match.player2, QObject::tr("Player 2"));

    for (const TournamentGameRecord& game : games) {
        if (!game.completed) {
            continue;
        }
        if (game.result.outcome == GameOutcome::Draw) {
            ++player1.draws;
            ++player2.draws;
            player1.sequence += '=';
            player2.sequence += '=';
            continue;
        }

        const bool player1Won =
            (game.result.outcome == GameOutcome::WhiteWin && !game.colorsSwapped)
            || (game.result.outcome == GameOutcome::BlackWin && game.colorsSwapped);
        if (player1Won) {
            ++player1.wins;
            ++player2.losses;
            player1.sequence += '1';
            player2.sequence += '0';
        } else {
            ++player1.losses;
            ++player2.wins;
            player1.sequence += '0';
            player2.sequence += '1';
        }
    }

    return qMakePair(player1, player2);
}

int performanceEloDifference(const PlayerStanding& standing)
{
    if (standing.games() == 0) {
        return 0;
    }

    const double score = standing.points() / standing.games();
    if (score <= 0.0) {
        return -1200;
    }
    if (score >= 1.0) {
        return 1200;
    }

    const int difference = qRound(-400.0 * std::log10(1.0 / score - 1.0));
    return qBound(-1200, difference, 1200);
}

QString formattedPoints(double points)
{
    return QLocale::system().toString(points, 'f', 1);
}

QString signedNumber(int value)
{
    return value > 0 ? QStringLiteral("+%1").arg(value)
                     : QString::number(value);
}

QString standingBlock(const PlayerStanding& standing, const PlayerStanding& opponent)
{
    const int percentage = standing.games() == 0
        ? 0
        : qRound(standing.points() * 100.0 / standing.games());
    const QString sequence = standing.sequence.isEmpty()
        ? QStringLiteral("-")
        : standing.sequence;

    return QStringLiteral("----------------- %1 -----------------\n"
                          "%1 - %2 : %3/%4  W-L-D %5-%6-%7  (%8)  %9%  %10")
        .arg(standing.name)
        .arg(opponent.name)
        .arg(formattedPoints(standing.points()))
        .arg(standing.games())
        .arg(standing.wins)
        .arg(standing.losses)
        .arg(standing.draws)
        .arg(sequence)
        .arg(percentage)
        .arg(signedNumber(performanceEloDifference(standing)));
}

QString tournamentStandingsText(const TournamentConfig& config,
                                const QVector<TournamentGameRecord>& games)
{
    const auto standings = tournamentStandings(config, games);
    return standingBlock(standings.first, standings.second)
        + QStringLiteral("\n\n")
        + standingBlock(standings.second, standings.first);
}

QString compactTournamentResult(const TournamentConfig& config,
                                const QVector<TournamentGameRecord>& games)
{
    const auto standings = tournamentStandings(config, games);
    return QStringLiteral("%1: %2/%3 (W-L-D %4-%5-%6)\n"
                          "%7: %8/%9 (W-L-D %10-%11-%12)")
        .arg(standings.first.name)
        .arg(formattedPoints(standings.first.points()))
        .arg(standings.first.games())
        .arg(standings.first.wins)
        .arg(standings.first.losses)
        .arg(standings.first.draws)
        .arg(standings.second.name)
        .arg(formattedPoints(standings.second.points()))
        .arg(standings.second.games())
        .arg(standings.second.wins)
        .arg(standings.second.losses)
        .arg(standings.second.draws);
}

void renderTournamentHistory(QPlainTextEdit *editor,
                             const QVector<TournamentGameRecord>& games)
{
    if (!editor) {
        return;
    }

    editor->clear();
    QTextCursor cursor(editor->document());
    QTextCharFormat headerFormat;
    headerFormat.setFontWeight(QFont::Bold);
    QTextCharFormat openingFormat = moveTextFormat(true);
    QTextCharFormat errorFormat;
    errorFormat.setForeground(QColor(QStringLiteral("#A33A32")));

    for (const TournamentGameRecord& game : games) {
        QString status = QObject::tr("IN PROGRESS");
        if (game.completed) {
            status = gameResultNotation(game.result.outcome);
        } else if (game.aborted) {
            status = QObject::tr("ABORTED");
        }

        const QString whiteName =
            playerDisplayName(game.match.player1, QObject::tr("Player"));
        const QString blackName =
            playerDisplayName(game.match.player2, QObject::tr("Player"));
        cursor.insertText(
            QStringLiteral("GAME %1  |  %2 - %3 : %4")
                .arg(game.gameNumber, 3)
                .arg(whiteName)
                .arg(blackName)
                .arg(status),
            headerFormat);
        cursor.insertText(QStringLiteral("\n"));
        cursor.insertText(
            QObject::tr("Opening %1: %2")
                .arg(game.openingIndex)
                .arg(game.openingName),
            openingFormat);
        if (!game.moves.isEmpty()) {
            cursor.insertText(QStringLiteral("\n"));
            appendFormattedMoves(cursor,
                                 game.moves,
                                 static_cast<int>(game.openingMoves.size()),
                                 false);
        }
        if (game.aborted && !game.abortMessage.isEmpty()) {
            cursor.insertText(QStringLiteral("\n%1").arg(game.abortMessage),
                              errorFormat);
        }
        if (&game != &games.constLast()) {
            cursor.insertText(QStringLiteral("\n\n"));
        }
    }

    editor->setTextCursor(cursor);
    editor->ensureCursorVisible();
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
    configureMoveList(ui->gameMovesText);
    configureMoveList(ui->tournamentHistoryText);
    configureMoveLegend(ui->labelGameMoveLegend);
    configureMoveLegend(ui->labelTournamentMoveLegend);
    configureEngineOutput(ui->whiteEngineOutputText);
    configureEngineOutput(ui->blackEngineOutputText);
    setTournamentTabActive(false);

    connect(ui->debugButton, &QPushButton::clicked, this, [this]() {
        openDebugWindow();
    });

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

    connect(m_gameController, &GameController::engineOutputReceived, this,
            [this](EngineSide side, const QString& line) {
        QPlainTextEdit *output = side == EngineSide::White
            ? ui->whiteEngineOutputText
            : ui->blackEngineOutputText;
        if (!output) {
            return;
        }
        output->appendPlainText(line);
        output->verticalScrollBar()->setValue(output->verticalScrollBar()->maximum());
        output->horizontalScrollBar()->setValue(output->horizontalScrollBar()->minimum());
    });

    connect(m_gameController, &GameController::engineSearchStarted, this,
            [this](EngineSide side) {
        QPlainTextEdit *output = side == EngineSide::White
            ? ui->whiteEngineOutputText
            : ui->blackEngineOutputText;
        if (output) {
            output->clear();
        }
    });

    connect(m_gameController, &GameController::communicationHistoryReset, this,
            [this]() {
        updateDebugLogPath();
        if (m_debugText) {
            m_debugText->clear();
        }
    });

    connect(m_gameController, &GameController::communicationLogged, this,
            [this](const QString& line) {
        if (m_debugText) {
            m_debugText->appendPlainText(line);
            m_debugText->moveCursor(QTextCursor::End);
        }
    });

    connect(m_gameController, &GameController::communicationLogError, this,
            [this](const QString& message) {
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(message);
        }
    });

    connect(m_gameController, &GameController::matchStarted, this, [this](const MatchConfig& match) {
        updatePlayerNames(match);
        updateGameOpeningLabel();
        updateEngineOutputPanels(match);
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
                tr("Game %1 of %2").arg(gameNumber).arg(totalGames));
        }
        if (ui && ui->labelTournamentOpening) {
            const QVector<TournamentGameRecord> records =
                m_tournamentRunner->gameRecords();
            if (!records.isEmpty()) {
                const TournamentGameRecord& game = records.constLast();
                m_currentOpeningName = game.openingName;
                m_currentOpeningIndex = game.openingIndex;
                m_currentOpeningCount = m_tournamentRunner->openingCount();
                updateGameOpeningLabel();
                ui->labelTournamentOpening->setText(
                    tr("Opening %1/%2: %3")
                        .arg(game.openingIndex)
                        .arg(m_tournamentRunner->openingCount())
                        .arg(game.openingName));
                ui->labelTournamentOpening->setToolTip(
                    tr("Start FEN: %1").arg(game.startFen));
            }
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
        updateTournamentStandings();
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentFinished, this,
            [this](const TournamentSummary& summary) {
        const QString message = tr("Tournament finished after %1 games.\nResult: %2")
            .arg(summary.completedGames)
            .arg(compactTournamentResult(m_state.lastTournament,
                                         m_tournamentRunner->gameRecords()));
        if (ui && ui->labelTournamentStatus) {
            ui->labelTournamentStatus->setText(tr("Tournament finished"));
        }
        updateTournamentHistory();
        updateTournamentStandings();
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(message);
        }
        QMessageBox::information(this, tr("Tournament finished"), message);
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentAborted, this,
            [this](const QString& title, const QString& message) {
        if (ui && ui->labelTournamentStatus) {
            ui->labelTournamentStatus->setText(tr("Tournament aborted"));
        }
        updateTournamentHistory();
        updateTournamentStandings();
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(tr("%1: %2").arg(title, message));
        }
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

    QVector<OpeningEntry> openings;
    if (config.game.useOpeningFile) {
        QString openingError;
        if (!loadOpeningFile(config.game.openingFilePath, &openings, &openingError)) {
            QMessageBox::warning(this, tr("Opening file"), openingError);
            return false;
        }
    } else {
        const QString fen = QString::fromStdString(resolveStartFen(config.game));
        const QString name = config.game.useStartPos
            ? tr("Start position")
            : tr("Custom position");
        openings.append(OpeningEntry{1, name, fen, fen, {}});
    }

    const OpeningEntry& firstOpening = openings.first();
    m_currentOpeningName = firstOpening.name;
    m_currentOpeningIndex = firstOpening.sourceIndex;
    m_currentOpeningCount = static_cast<int>(openings.size());
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
        record.startFen = firstOpening.startFen;
        record.openingCount = static_cast<int>(openings.size());
        record.openingName = tournament ? QString() : firstOpening.name;
        record.finalOpeningFen = tournament ? QString() : firstOpening.finalFen;
        record.openingMoves = tournament ? QStringList() : firstOpening.movesUci;
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
        return m_tournamentRunner->start(*tournament, openings, sessionDir, sessionTag);
    }

    MatchConfig runtimeConfig = config;
    runtimeConfig.game.useOpeningFile = false;
    runtimeConfig.game.openingFilePath.clear();
    runtimeConfig.game.useStartPos = false;
    runtimeConfig.game.startPosition = firstOpening.startFen;
    const bool started = m_gameController->startMatch(runtimeConfig,
                                                      firstOpening.startFen.toStdString(),
                                                      sessionDir,
                                                      sessionTag,
                                                      0,
                                                      firstOpening.movesUci);
    if (started) {
        clearTournamentPanel();
        setTournamentTabActive(false);
    }
    return started;
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

void MainWindow::clearTournamentPanel()
{
    if (!ui) {
        return;
    }

    if (ui->labelTournamentStatus) {
        ui->labelTournamentStatus->setText(tr("No tournament in progress"));
    }
    if (ui->labelTournamentOpening) {
        ui->labelTournamentOpening->setText(tr("Opening: -"));
        ui->labelTournamentOpening->setToolTip(QString());
    }
    if (ui->labelTournamentInfo) {
        ui->labelTournamentInfo->clear();
    }
    if (ui->tournamentStandingsText) {
        ui->tournamentStandingsText->clear();
    }
    if (ui->tournamentHistoryText) {
        ui->tournamentHistoryText->clear();
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
    if (ui->labelTournamentOpening) {
        ui->labelTournamentOpening->setText(tr("Opening: -"));
        ui->labelTournamentOpening->setToolTip(QString());
    }
    if (ui->labelTournamentInfo) {
        const QString reportName = QFileInfo(m_tournamentRunner->reportFilePath()).fileName();
        const QString openingInfo = config.match.game.useOpeningFile
            ? tr("%1 (%2 positions)")
                  .arg(QFileInfo(config.match.game.openingFilePath).fileName())
                  .arg(m_tournamentRunner->openingCount())
            : tr("Configured start position");
        ui->labelTournamentInfo->setText(
            tr("Type: %1\nFormat: %2 rounds x %3 games (%4 total)\n"
               "Time: %5 %6+%7\nOpenings: %8\nReport: %9")
                .arg(config.tournamentType)
                .arg(config.rounds)
                .arg(config.gamesPerPairing)
                .arg(totalGames)
                .arg(config.match.game.timeControl)
                .arg(config.match.game.baseTimeSeconds)
                .arg(config.match.game.incrementSeconds)
                .arg(openingInfo)
                .arg(reportName));
    }
    if (ui->tournamentHistoryText) {
        ui->tournamentHistoryText->clear();
    }
    updateTournamentStandings();
}

void MainWindow::updateTournamentStandings()
{
    if (!ui || !ui->tournamentStandingsText || !m_tournamentRunner) {
        return;
    }

    ui->tournamentStandingsText->setText(
        tournamentStandingsText(m_state.lastTournament,
                                m_tournamentRunner->gameRecords()));
}

void MainWindow::updateTournamentHistory()
{
    if (!ui || !ui->tournamentHistoryText || !m_tournamentRunner) {
        return;
    }

    renderTournamentHistory(ui->tournamentHistoryText,
                            m_tournamentRunner->gameRecords());
}

void MainWindow::updateGameMoveList()
{
    if (!ui || !ui->gameMovesText || !m_gameController) {
        return;
    }

    ui->gameMovesText->clear();
    QTextCursor cursor(ui->gameMovesText->document());
    appendFormattedMoves(cursor,
                         m_gameController->moveHistoryUci(),
                         m_gameController->initialMoveCount(),
                         true);
    ui->gameMovesText->setTextCursor(cursor);
    ui->gameMovesText->ensureCursorVisible();
}

void MainWindow::updateGameOpeningLabel()
{
    if (!ui || !ui->labelGameOpening) {
        return;
    }

    if (m_currentOpeningName.isEmpty()) {
        ui->labelGameOpening->setText(tr("Opening: -"));
        return;
    }
    if (m_currentOpeningCount > 1) {
        ui->labelGameOpening->setText(
            tr("Opening %1/%2: %3")
                .arg(m_currentOpeningIndex)
                .arg(m_currentOpeningCount)
                .arg(m_currentOpeningName));
        return;
    }
    ui->labelGameOpening->setText(
        tr("Opening: %1").arg(m_currentOpeningName));
}

void MainWindow::updatePlayerNames(const MatchConfig& match)
{
    if (!ui || !ui->labelWhitePlayer || !ui->labelBlackPlayer) {
        return;
    }

    ui->labelWhitePlayer->setText(playerDisplayName(match.player1, tr("Player")));
    ui->labelBlackPlayer->setText(playerDisplayName(match.player2, tr("Player")));
}

void MainWindow::updateEngineOutputPanels(const MatchConfig& match)
{
    if (!ui) {
        return;
    }

    const auto outputTitle = [this](const QString& color, const PlayerConfig& player) {
        if (player.type != PlayerType::Engine) {
            return tr("%1 - No engine").arg(color);
        }
        return tr("%1 - %2")
            .arg(color, playerDisplayName(player, tr("Unnamed engine")));
    };

    if (ui->labelWhiteEngineOutput) {
        ui->labelWhiteEngineOutput->setText(outputTitle(tr("White"), match.player1));
    }
    if (ui->labelBlackEngineOutput) {
        ui->labelBlackEngineOutput->setText(outputTitle(tr("Black"), match.player2));
    }
    if (ui->whiteEngineOutputText) {
        ui->whiteEngineOutputText->clear();
    }
    if (ui->blackEngineOutputText) {
        ui->blackEngineOutputText->clear();
    }
}

void MainWindow::openDebugWindow()
{
    if (m_debugDialog) {
        m_debugDialog->show();
        m_debugDialog->raise();
        m_debugDialog->activateWindow();
        return;
    }

    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Engine communication debug"));
    dialog->resize(920, 620);

    auto *layout = new QVBoxLayout(dialog);
    auto *pathLabel = new QLabel(dialog);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel->setWordWrap(true);
    layout->addWidget(pathLabel);

    auto *debugText = new QPlainTextEdit(dialog);
    configureEngineOutput(debugText);
    debugText->setMaximumBlockCount(0);
    debugText->setPlainText(m_gameController->communicationHistory().join('\n'));
    debugText->moveCursor(QTextCursor::End);
    layout->addWidget(debugText);

    m_debugDialog = dialog;
    m_debugPathLabel = pathLabel;
    m_debugText = debugText;
    updateDebugLogPath();
    dialog->show();
}

void MainWindow::updateDebugLogPath()
{
    if (!m_debugPathLabel) {
        return;
    }

    const QString logPath = m_gameController->communicationLogFilePath();
    m_debugPathLabel->setText(logPath.isEmpty()
                                  ? tr("No communication log is active.")
                                  : tr("Log: %1").arg(QDir::toNativeSeparators(logPath)));
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
