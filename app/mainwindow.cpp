#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "application_data.h"
#include "application_data_dialog.h"
#include "app_settings.h"
#include "captured_pieces_widget.h"
#include "single_game_dialog.h"
#include "history_repository.h"
#include "opening_book.h"
#include "pgn_export.h"
#include "session_record.h"
#include "storage_paths.h"
#include "tournament_dialog.h"
#include "tournament_runner.h"
#include "uci_client.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QString>
#include <QStyle>
#include <QSvgRenderer>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
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
const char kWhiteEngineCommunicationColor[] = "#247C8F";
const char kBlackEngineCommunicationColor[] = "#B85C2B";
constexpr int kHistoryEntryRole = Qt::UserRole;
constexpr int kHistoryGameRole = Qt::UserRole + 1;
const char kHistoryHeaderStateKey[] = "ui/historyHeaderState";
constexpr int kHistoryMinimumColumnWidth = 70;

Qt::CaseSensitivity pathCaseSensitivity()
{
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

bool pathBelongsToDirectory(const QString& path, const QString& directory)
{
    if (path.isEmpty() || directory.isEmpty()) {
        return false;
    }

    const QFileInfo directoryInfo(directory);
    const QFileInfo pathInfo(path);
    QString directoryPath = directoryInfo.canonicalFilePath();
    QString candidatePath = pathInfo.canonicalFilePath();
    if (directoryPath.isEmpty()) {
        directoryPath = directoryInfo.absoluteFilePath();
    }
    if (candidatePath.isEmpty()) {
        candidatePath = pathInfo.absoluteFilePath();
    }

    directoryPath = QDir::fromNativeSeparators(QDir::cleanPath(directoryPath));
    candidatePath = QDir::fromNativeSeparators(QDir::cleanPath(candidatePath));
    const QString prefix = directoryPath + QLatin1Char('/');
    return QString::compare(candidatePath, directoryPath, pathCaseSensitivity()) == 0
        || candidatePath.startsWith(prefix, pathCaseSensitivity());
}

void configureHistoryColumns(QTreeWidget *tree, const QSettings& settings)
{
    if (!tree || !tree->header()) {
        return;
    }

    QHeaderView *header = tree->header();
    const QByteArray savedState =
        settings.value(QString::fromLatin1(kHistoryHeaderStateKey)).toByteArray();
    const bool restored =
        !savedState.isEmpty() && header->restoreState(savedState);

    header->setSectionsMovable(false);
    header->setMinimumSectionSize(kHistoryMinimumColumnWidth);
    for (int column = 0; column < tree->columnCount(); ++column) {
        header->setSectionResizeMode(column, QHeaderView::Interactive);
    }
    header->setStretchLastSection(true);
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    if (!restored) {
        tree->setColumnWidth(0, 150);
        tree->setColumnWidth(1, 95);
        tree->setColumnWidth(2, 260);
        tree->setColumnWidth(3, 180);
    }

    auto *saveTimer = new QTimer(tree);
    saveTimer->setSingleShot(true);
    saveTimer->setInterval(250);
    QObject::connect(header,
                     &QHeaderView::sectionResized,
                     saveTimer,
                     [saveTimer](int, int, int) {
        saveTimer->start();
    });
    QObject::connect(saveTimer, &QTimer::timeout, header, [header]() {
        QSettings currentSettings;
        currentSettings.setValue(
            QString::fromLatin1(kHistoryHeaderStateKey),
            header->saveState());
    });
}

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

void configureColumnMoveList(QPlainTextEdit *editor)
{
    configureMoveList(editor);
    if (!editor) {
        return;
    }

    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editor->document()->setDocumentMargin(2.0);
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

QTextCharFormat communicationTextFormat(const QString& line)
{
    QTextCharFormat format;
    if (line.contains(QStringLiteral("[White:"))) {
        format.setForeground(
            QColor(QString::fromLatin1(kWhiteEngineCommunicationColor)));
    } else if (line.contains(QStringLiteral("[Black:"))) {
        format.setForeground(
            QColor(QString::fromLatin1(kBlackEngineCommunicationColor)));
    }
    return format;
}

void scrollCommunicationToLatest(QPlainTextEdit *editor)
{
    if (!editor) {
        return;
    }
    editor->verticalScrollBar()->setValue(
        editor->verticalScrollBar()->maximum());
    editor->horizontalScrollBar()->setValue(
        editor->horizontalScrollBar()->minimum());
}

void appendCommunicationLine(QPlainTextEdit *editor,
                             const QString& line)
{
    if (!editor) {
        return;
    }

    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::End);
    if (!editor->document()->isEmpty()) {
        cursor.insertBlock();
    }
    cursor.insertText(line, communicationTextFormat(line));
    editor->setTextCursor(cursor);
    scrollCommunicationToLatest(editor);
}

void renderCommunicationHistory(QPlainTextEdit *editor,
                                const QStringList& history)
{
    if (!editor) {
        return;
    }

    editor->clear();
    QTextCursor cursor(editor->document());
    for (qsizetype index = 0; index < history.size(); ++index) {
        if (index > 0) {
            cursor.insertBlock();
        }
        const QString& line = history.at(index);
        cursor.insertText(line, communicationTextFormat(line));
    }
    editor->setTextCursor(cursor);
    scrollCommunicationToLatest(editor);
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

void renderGameMovesInColumns(QPlainTextEdit *editor,
                              const QStringList& moves,
                              int openingMoveCount)
{
    if (!editor) {
        return;
    }

    editor->clear();
    if (moves.isEmpty()) {
        return;
    }

    constexpr int kMoveColumnCharacters = 23;
    const int lineHeight = qMax(1, editor->fontMetrics().lineSpacing());
    const int availableHeight = qMax(1, editor->viewport()->height() - 4);
    const int rowsPerColumn = qMax(1, availableHeight / lineHeight);
    const int turnCount = (static_cast<int>(moves.size()) + 1) / 2;
    const int columnCount =
        (turnCount + rowsPerColumn - 1) / rowsPerColumn;
    const int displayedRows = qMin(rowsPerColumn, turnCount);
    const int openingCount =
        qBound(0, openingMoveCount, static_cast<int>(moves.size()));

    QTextCursor cursor(editor->document());
    const QTextCharFormat neutralFormat;
    for (int row = 0; row < displayedRows; ++row) {
        for (int column = 0; column < columnCount; ++column) {
            const int turn = column * rowsPerColumn + row;
            if (turn >= turnCount) {
                break;
            }

            const int whitePly = turn * 2;
            const int blackPly = whitePly + 1;
            const QString number = QStringLiteral("%1. ").arg(turn + 1);
            const QString whiteMove = moves.at(whitePly);
            const QString blackMove =
                blackPly < moves.size() ? moves.at(blackPly) : QString();

            cursor.insertText(number, neutralFormat);
            cursor.insertText(whiteMove,
                              moveTextFormat(whitePly < openingCount));
            int usedCharacters = number.size() + whiteMove.size();
            if (!blackMove.isEmpty()) {
                cursor.insertText(QStringLiteral("   "), neutralFormat);
                cursor.insertText(blackMove,
                                  moveTextFormat(blackPly < openingCount));
                usedCharacters += 3 + blackMove.size();
            }

            const bool hasNextColumn =
                (column + 1) * rowsPerColumn + row < turnCount;
            if (hasNextColumn) {
                const int padding =
                    qMax(4, kMoveColumnCharacters - usedCharacters);
                cursor.insertText(QString(padding, QLatin1Char(' ')),
                                  neutralFormat);
            }
        }
        if (row + 1 < displayedRows) {
            cursor.insertBlock();
        }
    }

    cursor.movePosition(QTextCursor::Start);
    editor->setTextCursor(cursor);
    editor->verticalScrollBar()->setValue(
        editor->verticalScrollBar()->minimum());
    editor->horizontalScrollBar()->setValue(
        editor->horizontalScrollBar()->maximum());
}

QString readableHistoryValue(QString value)
{
    value.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!value.isEmpty()) {
        value[0] = value.at(0).toUpper();
    }
    return value;
}

QString historyDate(const QDateTime& dateTime)
{
    return dateTime.isValid()
        ? QLocale().toString(dateTime.toLocalTime(), QLocale::ShortFormat)
        : QStringLiteral("-");
}

QString historyTimeControl(const HistoryEntry& entry)
{
    if (entry.baseTimeSeconds <= 0) {
        return entry.timeControl.isEmpty()
            ? QObject::tr("Untimed")
            : entry.timeControl;
    }

    const QString clock = QStringLiteral("%1+%2")
        .arg(entry.baseTimeSeconds)
        .arg(entry.incrementSeconds);
    return entry.timeControl.isEmpty()
        ? clock
        : QStringLiteral("%1  %2").arg(entry.timeControl, clock);
}

QString historyEntrySearchText(const HistoryEntry& entry)
{
    QStringList values = {
        entry.sessionTag,
        entry.status,
        entry.player1,
        entry.player2,
        entry.result,
        entry.termination,
        entry.openingName
    };
    return values.join(QLatin1Char(' ')).toLower();
}

QString historyGameSearchText(const HistoryGame& game)
{
    const QStringList values = {
        game.status,
        game.white,
        game.black,
        game.result,
        game.termination,
        game.openingName
    };
    return values.join(QLatin1Char(' ')).toLower();
}

void renderHistoryDetails(QPlainTextEdit *editor,
                          const HistoryEntry& entry,
                          const HistoryGame *game)
{
    if (!editor) {
        return;
    }

    editor->clear();
    QTextCursor cursor(editor->document());
    QTextCharFormat titleFormat;
    titleFormat.setFontWeight(QFont::Bold);
    titleFormat.setFontPointSize(12);
    QTextCharFormat sectionFormat;
    sectionFormat.setFontWeight(QFont::Bold);
    QTextCharFormat openingFormat = moveTextFormat(true);

    const auto section = [&cursor, &sectionFormat](const QString& title) {
        cursor.insertText(QStringLiteral("\n\n---------- %1 ----------\n")
                              .arg(title),
                          sectionFormat);
    };
    const auto field = [&cursor](const QString& name, const QString& value) {
        if (!value.isEmpty()) {
            cursor.insertText(QStringLiteral("%1: %2\n").arg(name, value));
        }
    };

    if (game) {
        cursor.insertText(
            QObject::tr("TOURNAMENT GAME %1").arg(game->gameNumber),
            titleFormat);
        const QString result = game->result.isEmpty()
            ? readableHistoryValue(game->status)
            : game->result;
        cursor.insertText(QStringLiteral("\n%1 - %2 : %3")
                              .arg(game->white,
                                   game->black,
                                   gameResultSummary(result,
                                                     game->termination)));
        section(QObject::tr("Information"));
        field(QObject::tr("Date"), historyDate(game->startedAt));
        field(QObject::tr("Status"), readableHistoryValue(game->status));
        field(QObject::tr("Time"), historyTimeControl(entry));
        field(QObject::tr("Opening"), game->openingName);
        field(QObject::tr("Termination"),
              gameTerminationDisplayName(game->termination));
        field(QObject::tr("Message"), game->message);
        if (!game->moves.isEmpty()) {
            section(QObject::tr("Moves"));
            appendFormattedMoves(cursor,
                                 game->moves,
                                 game->openingMoveCount,
                                 true);
        }
    } else if (entry.type == HistorySessionType::Tournament) {
        cursor.insertText(QObject::tr("TOURNAMENT"), titleFormat);
        cursor.insertText(QStringLiteral("\n%1 - %2")
                              .arg(entry.player1, entry.player2));
        section(QObject::tr("Summary"));
        field(QObject::tr("Date"), historyDate(entry.startedAt));
        field(QObject::tr("Status"), readableHistoryValue(entry.status));
        field(QObject::tr("Time"), historyTimeControl(entry));
        field(QObject::tr("Games"),
              QStringLiteral("%1/%2")
                  .arg(entry.completedGames)
                  .arg(entry.totalGames));
        field(QObject::tr("W-L-D"),
              QStringLiteral("%1-%2-%3")
                  .arg(entry.player1Wins)
                  .arg(entry.player2Wins)
                  .arg(entry.draws));
        if (!entry.games.isEmpty()) {
            section(QObject::tr("Games"));
            for (const HistoryGame& item : entry.games) {
                const QString result = item.result.isEmpty()
                    ? readableHistoryValue(item.status)
                    : item.result;
                cursor.insertText(
                    QStringLiteral("%1. %2 - %3 : %4\n")
                        .arg(item.gameNumber)
                        .arg(item.white)
                        .arg(item.black)
                        .arg(gameResultSummary(result,
                                               item.termination)));
            }
        }
    } else {
        const QString result = entry.result.isEmpty()
            ? readableHistoryValue(entry.status)
            : entry.result;
        cursor.insertText(QObject::tr("GAME"), titleFormat);
        cursor.insertText(QStringLiteral("\n%1 - %2 : %3")
                              .arg(entry.player1,
                                   entry.player2,
                                   gameResultSummary(result,
                                                     entry.termination)));
        section(QObject::tr("Information"));
        field(QObject::tr("Date"), historyDate(entry.startedAt));
        field(QObject::tr("Status"), readableHistoryValue(entry.status));
        field(QObject::tr("Time"), historyTimeControl(entry));
        if (!entry.openingName.isEmpty()) {
            cursor.insertText(
                QStringLiteral("%1: %2\n")
                    .arg(QObject::tr("Opening"), entry.openingName),
                openingFormat);
        }
        field(QObject::tr("Termination"),
              gameTerminationDisplayName(entry.termination));
        field(QObject::tr("Message"), entry.message);
        if (!entry.moves.isEmpty()) {
            section(QObject::tr("Moves"));
            appendFormattedMoves(cursor,
                                 entry.moves,
                                 entry.openingMoveCount,
                                 true);
        }
    }

    editor->setTextCursor(cursor);
    editor->verticalScrollBar()->setValue(
        editor->verticalScrollBar()->minimum());
    editor->horizontalScrollBar()->setValue(
        editor->horizontalScrollBar()->minimum());
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
            const QString termination =
                game.result.termination == GameTermination::Unknown
                ? QString()
                : gameTerminationKey(game.result.termination);
            status = gameResultSummary(
                gameResultNotation(game.result.outcome),
                termination);
        } else if (game.aborted) {
            const QString termination =
                game.termination == GameTermination::Unknown
                ? QString()
                : gameTerminationKey(game.termination);
            status = gameResultSummary(QObject::tr("ABORTED"),
                                       termination);
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

QString pgnDate(const QString& isoTimestamp)
{
    const QDateTime timestamp = QDateTime::fromString(isoTimestamp, Qt::ISODate);
    return timestamp.isValid()
        ? timestamp.date().toString(QStringLiteral("yyyy.MM.dd"))
        : QStringLiteral("????.??.??");
}

QString pgnPlayerName(const PlayerConfig& player)
{
    if (!player.name.trimmed().isEmpty()) {
        return player.name.trimmed();
    }
    const QString executable = QFileInfo(player.enginePath).completeBaseName();
    return executable.isEmpty() ? QStringLiteral("?") : executable;
}

QString pgnTimeControl(const GameConfig& game)
{
    if (game.baseTimeSeconds <= 0) {
        return QStringLiteral("-");
    }
    return QStringLiteral("%1+%2")
        .arg(game.baseTimeSeconds)
        .arg(game.incrementSeconds);
}

void warnSessionRecordFailure(QWidget *parent, const QString& detail)
{
    if (!detail.isEmpty()) {
        QMessageBox::warning(parent,
                             QObject::tr("Session log"),
                             QObject::tr("Failed to write session file: %1").arg(detail));
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
    , m_gameMovesLayoutTimer(new QTimer(this))
{
    // Build the widget tree from the .ui description.
    ui->setupUi(this);
    QSettings settings;
    m_state = loadAppState(settings);
    setColorIndicator(ui->labelWhiteTime, WHITE);
    setColorIndicator(ui->labelBlackTime, BLACK);
    configureColumnMoveList(ui->gameMovesText);
    configureMoveList(ui->tournamentHistoryText);
    configureMoveList(ui->historyDetailsText);
    configureMoveLegend(ui->labelGameMoveLegend);
    configureMoveLegend(ui->labelTournamentMoveLegend);
    configureMoveLegend(ui->labelHistoryMoveLegend);
    configureEngineOutput(ui->whiteEngineOutputText);
    configureEngineOutput(ui->blackEngineOutputText);
    configureHistoryColumns(ui->historyTree, settings);
    ui->historySplitter->setStretchFactor(0, 3);
    ui->historySplitter->setStretchFactor(1, 4);
    ui->pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    ui->stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    ui->restartButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    ui->replayFirstButton->setIcon(
        style()->standardIcon(QStyle::SP_MediaSkipBackward));
    ui->replayPreviousButton->setIcon(
        style()->standardIcon(QStyle::SP_MediaSeekBackward));
    ui->replayNextButton->setIcon(
        style()->standardIcon(QStyle::SP_MediaSeekForward));
    ui->replayLastButton->setIcon(
        style()->standardIcon(QStyle::SP_MediaSkipForward));
    ui->replayCloseButton->setIcon(
        style()->standardIcon(QStyle::SP_DialogCloseButton));
    ui->historyRefreshButton->setIcon(
        style()->standardIcon(QStyle::SP_BrowserReload));
    ui->historyReplayButton->setIcon(
        style()->standardIcon(QStyle::SP_MediaPlay));
    ui->historyLoadReplayButton->setIcon(
        style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui->historyOpenPgnButton->setIcon(
        style()->standardIcon(QStyle::SP_FileIcon));
    ui->historyOpenFolderButton->setIcon(
        style()->standardIcon(QStyle::SP_DirOpenIcon));
    ui->historyDeleteButton->setIcon(
        style()->standardIcon(QStyle::SP_TrashIcon));

    connect(ui->debugButton, &QPushButton::clicked, this, [this]() {
        openDebugWindow();
    });
    connect(ui->pauseButton, &QPushButton::clicked,
            this, &MainWindow::togglePause);
    connect(ui->stopButton, &QPushButton::clicked,
            this, &MainWindow::stopCurrentSession);
    connect(ui->restartButton, &QPushButton::clicked,
            this, &MainWindow::restartLastSession);
    connect(ui->historyRefreshButton, &QPushButton::clicked,
            this, &MainWindow::refreshHistory);
    connect(ui->historyReplayButton, &QPushButton::clicked,
            this, &MainWindow::replaySelectedHistory);
    connect(ui->historyLoadReplayButton, &QPushButton::clicked,
            this, &MainWindow::openReplayFile);
    connect(ui->replayGameCombo, &QComboBox::currentIndexChanged,
            this, [this](int index) {
        if (m_replayActive) {
            loadReplayGame(index);
        }
    });
    connect(ui->replayFirstButton, &QPushButton::clicked,
            this, [this]() { navigateReplayTo(0); });
    connect(ui->replayPreviousButton, &QPushButton::clicked,
            this, [this]() {
        navigateReplayTo(m_replay.currentPly() - 1);
    });
    connect(ui->replayNextButton, &QPushButton::clicked,
            this, [this]() {
        navigateReplayTo(m_replay.currentPly() + 1);
    });
    connect(ui->replayLastButton, &QPushButton::clicked,
            this, [this]() { navigateReplayTo(m_replay.totalPly()); });
    connect(ui->replayCloseButton, &QPushButton::clicked,
            this, [this]() { leaveReplay(true); });
    connect(ui->historyFilterEdit, &QLineEdit::textChanged,
            this, [this]() {
        populateHistoryTree();
    });
    connect(ui->historyTypeCombo, &QComboBox::currentIndexChanged,
            this, [this]() {
        populateHistoryTree();
    });
    connect(ui->historyTree, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::updateHistoryDetails);
    connect(ui->historyOpenPgnButton, &QPushButton::clicked,
            this, &MainWindow::openSelectedHistoryPgn);
    connect(ui->historyOpenFolderButton, &QPushButton::clicked,
            this, &MainWindow::openSelectedHistoryDirectory);
    connect(ui->historyDeleteButton, &QPushButton::clicked,
            this, &MainWindow::deleteSelectedHistorySession);
    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, [this](int index) {
        if (ui->tabWidget->widget(index) == ui->tabHistory) {
            refreshHistory();
        }
    });

    if (m_clockUiTimer) {
        m_clockUiTimer->setInterval(100);
        connect(m_clockUiTimer, &QTimer::timeout, this, [this]() {
            updateClockUi();
        });
    }
    if (m_gameMovesLayoutTimer) {
        m_gameMovesLayoutTimer->setSingleShot(true);
        m_gameMovesLayoutTimer->setInterval(0);
        connect(m_gameMovesLayoutTimer, &QTimer::timeout,
                this, &MainWindow::updateGameMoveList);
        ui->gameMovesText->viewport()->installEventFilter(this);
    }

    if (ui->actionSingleGame) {
        connect(ui->actionSingleGame, &QAction::triggered, this, [this]() {
            SingleGameDialog dialog(this);
            if (m_state.hasLastMatch) {
                dialog.setConfig(m_state.lastMatch);
            }
            if (dialog.exec() == QDialog::Accepted) {
                const MatchConfig config = dialog.config();
                m_state.lastMatch = config;
                m_state.hasLastMatch = true;
                QSettings settings;
                saveLastMatch(settings, config);
                startMatch(config, nullptr);
            }
        });
    }

    if (ui->actionTournament) {
        connect(ui->actionTournament, &QAction::triggered, this, [this]() {
            TournamentDialog dialog(this);
            if (m_state.hasLastTournament) {
                dialog.setConfig(m_state.lastTournament);
            }
            if (dialog.exec() == QDialog::Accepted) {
                const TournamentConfig config = dialog.config();
                m_state.lastTournament = config;
                m_state.hasLastTournament = true;
                QSettings settings;
                saveLastTournament(settings, config);
                startMatch(config.match, &config);
            }
        });
    }
    if (ui->actionAbout) {
        connect(ui->actionAbout, &QAction::triggered,
                this, &MainWindow::showAboutDialog);
    }
    if (ui->actionManageApplicationData) {
        connect(ui->actionManageApplicationData, &QAction::triggered,
                this, &MainWindow::manageApplicationData);
    }
    if (ui->actionOpenReplay) {
        connect(ui->actionOpenReplay, &QAction::triggered,
                this, &MainWindow::openReplayFile);
    }

    connect(m_gameController, &GameController::positionChanged, this,
            [this](const Xake::Position& position, Xake::Move lastMove) {
        if (m_replayActive) {
            return;
        }
        if (ui && ui->board) {
            ui->board->setPosition(position, lastMove);
        }
        updateSideToMoveLabel(position);
        updateClockUi();
        updateSessionControls();
    });

    connect(m_gameController, &GameController::movePlayed, this,
            [this](int, const QString&) {
        updateGameMoveList();
        updateCapturedPieces();
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
            appendCommunicationLine(m_debugText, line);
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
        updateCapturedPieces();
        updateClockUi();
        if (m_clockUiTimer) {
            m_clockUiTimer->start();
        }
        updateSessionControls();
    });

    connect(m_gameController, &GameController::matchStopped, this, [this]() {
        if (m_hasActiveMatchRecord) {
            finalizeMatchRecord(QStringLiteral("stopped"),
                                nullptr,
                                tr("Game stopped"),
                                tr("The game was stopped before completion."),
                                GameTermination::Stopped);
        }
        if (m_clockUiTimer) {
            m_clockUiTimer->stop();
        }
        updateClockUi();
        updateSessionControls();
    });

    connect(m_gameController, &GameController::pauseChanged, this,
            [this](bool paused) {
        if (m_clockUiTimer) {
            if (paused || !m_gameController->isActive()) {
                m_clockUiTimer->stop();
            } else {
                m_clockUiTimer->start();
            }
        }
        updateClockUi();
        updateSessionControls();
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

    connect(m_gameController, &GameController::gameFinished, this,
            [this](const GameResult& result) {
        finalizeMatchRecord(QStringLiteral("completed"), &result);
    });
    connect(m_gameController, &GameController::gameAborted, this,
            [this](GameTermination termination,
                   const QString& title,
                   const QString& message) {
        finalizeMatchRecord(QStringLiteral("aborted"),
                            nullptr,
                            title,
                            message,
                            termination);
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentStarted, this,
            [this](int totalGames) {
        resetTournamentPanel(totalGames);
        updateSessionControls();
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
        refreshHistory();
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
        refreshHistory();
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(message);
        }
        updateSessionControls();
        QMessageBox::information(this, tr("Tournament finished"), message);
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentAborted, this,
            [this](const QString& title, const QString& message) {
        if (ui && ui->labelTournamentStatus) {
            ui->labelTournamentStatus->setText(title);
        }
        updateTournamentHistory();
        updateTournamentStandings();
        refreshHistory();
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(tr("%1: %2").arg(title, message));
        }
        updateSessionControls();
    });

    connect(m_tournamentRunner, &TournamentRunner::pauseChanged, this,
            [this](bool paused) {
        if (ui && ui->labelTournamentStatus) {
            if (paused) {
                ui->labelTournamentStatus->setText(tr("Tournament paused"));
            } else if (m_tournamentRunner->isActive()) {
                const QVector<TournamentGameRecord> records =
                    m_tournamentRunner->gameRecords();
                if (!records.isEmpty() && !records.constLast().completed) {
                    ui->labelTournamentStatus->setText(
                        tr("Game %1 of %2")
                            .arg(records.constLast().gameNumber)
                            .arg(m_tournamentRunner->summary().totalGames));
                }
            }
        }
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(
                paused ? tr("Tournament paused.") : tr("Tournament resumed."));
        }
        updateSessionControls();
    });

    connect(m_tournamentRunner, &TournamentRunner::tournamentReportError, this,
            [this](const QString& message) {
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(message, 10000);
        }
    });

    if (ui && ui->board) {
        connect(ui->board, &BoardWidget::moveRequested, this, [this](Xake::Move move) {
            if (m_replayActive) {
                return;
            }
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
    updateCapturedPieces();
    updateSessionControls();
    refreshHistory();
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (ui && ui->gameMovesText
        && watched == ui->gameMovesText->viewport()
        && event->type() == QEvent::Resize
        && m_gameMovesLayoutTimer) {
        m_gameMovesLayoutTimer->start();
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::startMatch(const MatchConfig& config, const TournamentConfig* tournament)
{
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

    const bool tournamentActive =
        m_tournamentRunner && m_tournamentRunner->isActive();
    const bool gameActive = m_gameController && m_gameController->isActive();
    if (tournamentActive || gameActive) {
        const QString currentSession =
            tournamentActive ? tr("tournament") : tr("game");
        const QString newSession = tournament ? tr("tournament") : tr("game");

        QMessageBox confirmation(
            QMessageBox::Warning,
            tr("Session in progress"),
            tr("A %1 is currently in progress.\n\n"
               "Do you want to stop it and start the new %2, or keep the "
               "current %1 running?")
                .arg(currentSession, newSession),
            QMessageBox::NoButton,
            this);
        QPushButton *stopAndStartButton = confirmation.addButton(
            tr("Stop and start new"),
            QMessageBox::AcceptRole);
        QPushButton *keepPlayingButton = confirmation.addButton(
            tr("Keep current session"),
            QMessageBox::RejectRole);
        confirmation.setDefaultButton(keepPlayingButton);
        confirmation.setEscapeButton(keepPlayingButton);
        confirmation.exec();

        if (confirmation.clickedButton() != stopAndStartButton) {
            return false;
        }
        const bool stillActive =
            (m_tournamentRunner && m_tournamentRunner->isActive())
            || (m_gameController && m_gameController->isActive());
        if (stillActive && !stopActiveSession()) {
            return false;
        }
    }
    if (m_replayActive) {
        leaveReplay(false);
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
        record.updatedAtIso = record.startTimeIso;
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
        if (!tournament) {
            m_matchRecord = record;
            m_matchRecordPath = recordPath;
            m_hasActiveMatchRecord = true;
        }
    }

    if (tournament) {
        m_hasActiveMatchRecord = false;
        m_matchRecordPath.clear();
        const bool started =
            m_tournamentRunner->start(*tournament, openings, sessionDir, sessionTag);
        if (started) {
            m_lastSessionKind = SessionKind::Tournament;
            updateSessionControls();
        }
        return started;
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
        m_lastSessionKind = SessionKind::Match;
        clearTournamentPanel();
        updateSessionControls();
    } else if (m_hasActiveMatchRecord) {
        finalizeMatchRecord(QStringLiteral("aborted"),
                            nullptr,
                            tr("Game start failed"),
                            tr("The game could not be started."),
                            GameTermination::StartFailure);
    }
    return started;
}

void MainWindow::finalizeMatchRecord(const QString& status,
                                     const GameResult* result,
                                     const QString& abortTitle,
                                     const QString& abortMessage,
                                     GameTermination termination)
{
    if (!m_hasActiveMatchRecord || m_matchRecordPath.isEmpty()) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    m_matchRecord.status = status;
    m_matchRecord.updatedAtIso = timestamp;
    m_matchRecord.finishedAtIso = timestamp;
    m_matchRecord.moves = m_gameController->moveHistoryUci();
    m_matchRecord.moveRecords = m_gameController->moveRecords();
    m_matchRecord.finalFen =
        QString::fromStdString(m_gameController->currentPosition().get_FEN());
    m_matchRecord.whiteTimeMs = m_gameController->remainingTimeMs(WHITE);
    m_matchRecord.blackTimeMs = m_gameController->remainingTimeMs(BLACK);
    m_matchRecord.hasResult = result != nullptr;
    if (result) {
        m_matchRecord.result = *result;
        m_matchRecord.termination = result->termination;
    } else {
        m_matchRecord.termination = termination;
    }
    m_matchRecord.abortTitle = abortTitle;
    m_matchRecord.abortMessage = abortMessage;

    QString errorDetail;
    if (!writeSessionRecord(m_matchRecord, m_matchRecordPath, &errorDetail)) {
        warnSessionRecordFailure(this, errorDetail);
    }

    PgnGameRecord pgn;
    pgn.event = QStringLiteral("Xake game");
    pgn.date = pgnDate(m_matchRecord.startTimeIso);
    pgn.white = pgnPlayerName(m_matchRecord.match.player1);
    pgn.black = pgnPlayerName(m_matchRecord.match.player2);
    pgn.result = result ? gameResultNotation(result->outcome)
                        : QStringLiteral("*");
    pgn.termination = gameTerminationPgn(m_matchRecord.termination);
    pgn.startFen = m_matchRecord.startFen;
    pgn.opening = m_matchRecord.openingName;
    pgn.timeControl = pgnTimeControl(m_matchRecord.match.game);
    pgn.movesUci = m_matchRecord.moves;
    pgn.openingMoveCount =
        static_cast<int>(m_matchRecord.openingMoves.size());
    const QString pgnPath = QFileInfo(m_matchRecordPath)
        .dir()
        .filePath(QStringLiteral("game.pgn"));
    errorDetail.clear();
    if (!writePgnFile({pgn}, pgnPath, &errorDetail)) {
        warnSessionRecordFailure(this, errorDetail);
    }
    m_hasActiveMatchRecord = false;
    refreshHistory();
}

bool MainWindow::stopActiveSession()
{
    bool stopped = false;
    if (m_tournamentRunner && m_tournamentRunner->isActive()) {
        stopped = m_tournamentRunner->stop();
    } else if (m_gameController && m_gameController->isActive()) {
        m_gameController->stopMatch();
        stopped = true;
    }
    updateSessionControls();
    return stopped;
}

void MainWindow::togglePause()
{
    bool changed = false;
    bool paused = false;
    if (m_tournamentRunner && m_tournamentRunner->isActive()) {
        paused = !m_tournamentRunner->isPaused();
        changed = paused ? m_tournamentRunner->pause()
                         : m_tournamentRunner->resume();
    } else if (m_gameController && m_gameController->isActive()) {
        paused = !m_gameController->isPaused();
        changed = paused ? m_gameController->pauseMatch()
                         : m_gameController->resumeMatch();
        if (changed && ui && ui->statusbar) {
            ui->statusbar->showMessage(
                paused ? tr("Game paused.") : tr("Game resumed."));
        }
    }

    if (changed) {
        updateClockUi();
    }
    updateSessionControls();
}

void MainWindow::stopCurrentSession()
{
    const bool tournamentActive =
        m_tournamentRunner && m_tournamentRunner->isActive();
    const bool gameActive = m_gameController && m_gameController->isActive();
    if (!tournamentActive && !gameActive) {
        return;
    }

    const QString sessionName = tournamentActive ? tr("tournament") : tr("game");
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Stop %1").arg(sessionName),
        tr("Are you sure you want to stop the current %1?").arg(sessionName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    if (!stopActiveSession()) {
        return;
    }
    if (!tournamentActive && ui && ui->statusbar) {
        ui->statusbar->showMessage(tr("Game stopped."));
    }
}

void MainWindow::restartLastSession()
{
    const SessionKind sessionKind = m_lastSessionKind;
    if (sessionKind == SessionKind::None) {
        return;
    }

    const QString sessionName = sessionKind == SessionKind::Tournament
        ? tr("tournament")
        : tr("game");
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Restart %1").arg(sessionName),
        tr("Are you sure you want to restart the %1 from the beginning?")
            .arg(sessionName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const bool active =
        (m_tournamentRunner && m_tournamentRunner->isActive())
        || (m_gameController && m_gameController->isActive());
    if (active && !stopActiveSession()) {
        return;
    }

    if (sessionKind == SessionKind::Tournament && m_state.hasLastTournament) {
        startMatch(m_state.lastTournament.match, &m_state.lastTournament);
    } else if (sessionKind == SessionKind::Match && m_state.hasLastMatch) {
        startMatch(m_state.lastMatch, nullptr);
    }
}

void MainWindow::updateSessionControls()
{
    if (!ui || !ui->pauseButton || !ui->stopButton || !ui->restartButton) {
        return;
    }

    const bool tournamentActive =
        m_tournamentRunner && m_tournamentRunner->isActive();
    const bool gameActive = m_gameController && m_gameController->isActive();
    const bool active = tournamentActive || gameActive;
    const bool paused = tournamentActive
        ? m_tournamentRunner->isPaused()
        : gameActive && m_gameController->isPaused();

    ui->pauseButton->setEnabled(active);
    ui->pauseButton->setText(paused ? tr("Resume") : tr("Pause"));
    ui->pauseButton->setIcon(
        style()->standardIcon(paused ? QStyle::SP_MediaPlay
                                    : QStyle::SP_MediaPause));
    ui->stopButton->setEnabled(active);
    ui->restartButton->setEnabled(m_lastSessionKind != SessionKind::None);
    if (ui->actionManageApplicationData) {
        ui->actionManageApplicationData->setEnabled(!active);
    }

    if (ui->board) {
        bool humanTurn = !m_replayActive && gameActive && !paused;
        if (humanTurn) {
            const MatchConfig match = m_gameController->matchConfig();
            const bool whiteToMove =
                m_gameController->currentPosition().get_side_to_move() == WHITE;
            const PlayerConfig& player = whiteToMove ? match.player1 : match.player2;
            humanTurn = player.type == PlayerType::Human;
        }
        ui->board->setMoveInputEnabled(humanTurn);
    }
    updateHistoryDeleteButton();
}

void MainWindow::clearTournamentPanel()
{
    if (!ui) {
        return;
    }

    if (ui->labelTournamentStatus) {
        ui->labelTournamentStatus->setText(tr("No tournament loaded"));
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

void MainWindow::clearSessionPanels()
{
    if (!ui) {
        return;
    }

    m_replayActive = false;
    m_replay.clear();
    m_replayGames.clear();
    ui->replayPanel->hide();
    ui->replayGameCombo->clear();
    m_gameController->clearFinishedSessionData();
    m_currentOpeningName.clear();
    m_currentOpeningIndex = 0;
    m_currentOpeningCount = 0;
    m_matchRecord = SessionRecord{};
    m_matchRecordPath.clear();
    m_hasActiveMatchRecord = false;

    ui->board->setMoveInputEnabled(false);
    ui->board->setFromFenString(kStartFen);
    ui->labelWhitePlayer->clear();
    ui->labelBlackPlayer->clear();
    ui->labelSideToMove->clear();
    ui->labelGameOpening->setText(tr("Opening: -"));
    ui->gameMovesText->clear();
    ui->capturedPiecesWidget->setCapturedPieces({});
    ui->labelWhiteEngineOutput->setText(tr("White"));
    ui->labelBlackEngineOutput->setText(tr("Black"));
    ui->whiteEngineOutputText->clear();
    ui->blackEngineOutputText->clear();

    clearTournamentPanel();
    updateClockUi();
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

    if (m_replayActive) {
        renderGameMovesInColumns(ui->gameMovesText,
                                 m_replay.visibleMoves(),
                                 m_replay.game().openingMoveCount);
        return;
    }
    renderGameMovesInColumns(ui->gameMovesText,
                             m_gameController->moveHistoryUci(),
                             m_gameController->initialMoveCount());
}

void MainWindow::updateCapturedPieces()
{
    if (!ui || !ui->capturedPiecesWidget || !m_gameController) {
        return;
    }

    if (m_replayActive) {
        ui->capturedPiecesWidget->setCapturedPieces(
            m_replay.capturedPieces());
        return;
    }
    ui->capturedPiecesWidget->setCapturedPieces(
        m_gameController->capturedPieces());
}

void MainWindow::refreshHistory()
{
    if (!ui || !ui->historyTree) {
        return;
    }

    QString selectedSessionTag;
    int selectedGame = -1;
    if (QTreeWidgetItem *selected = ui->historyTree->currentItem()) {
        const int entryIndex =
            selected->data(0, kHistoryEntryRole).toInt();
        if (entryIndex >= 0 && entryIndex < m_historyEntries.size()) {
            selectedSessionTag = m_historyEntries.at(entryIndex).sessionTag;
            selectedGame = selected->data(0, kHistoryGameRole).toInt();
        }
    }

    const HistoryLoadResult result = loadSessionHistory(sessionsRootDir());
    m_historyEntries = result.entries;
    populateHistoryTree();

    if (!selectedSessionTag.isEmpty()) {
        for (int topIndex = 0;
             topIndex < ui->historyTree->topLevelItemCount();
             ++topIndex) {
            QTreeWidgetItem *top = ui->historyTree->topLevelItem(topIndex);
            const int entryIndex =
                top->data(0, kHistoryEntryRole).toInt();
            if (entryIndex < 0 || entryIndex >= m_historyEntries.size()
                || m_historyEntries.at(entryIndex).sessionTag
                    != selectedSessionTag) {
                continue;
            }

            QTreeWidgetItem *selection = top;
            if (selectedGame >= 0) {
                for (int childIndex = 0;
                     childIndex < top->childCount();
                     ++childIndex) {
                    QTreeWidgetItem *child = top->child(childIndex);
                    if (child->data(0, kHistoryGameRole).toInt()
                        == selectedGame) {
                        selection = child;
                        top->setExpanded(true);
                        break;
                    }
                }
            }
            ui->historyTree->setCurrentItem(selection);
            break;
        }
    }

    if (!result.warnings.isEmpty() && ui->statusbar) {
        ui->statusbar->showMessage(
            tr("History loaded with %n unreadable record(s).",
               nullptr,
               result.warnings.size()),
            8000);
    }
}

void MainWindow::populateHistoryTree()
{
    if (!ui || !ui->historyTree) {
        return;
    }

    const QString filter = ui->historyFilterEdit->text().trimmed().toLower();
    const int typeFilter = ui->historyTypeCombo->currentIndex();
    ui->historyTree->setUpdatesEnabled(false);
    ui->historyTree->clear();

    for (int entryIndex = 0;
         entryIndex < m_historyEntries.size();
         ++entryIndex) {
        const HistoryEntry& entry = m_historyEntries.at(entryIndex);
        if ((typeFilter == 1 && entry.type != HistorySessionType::Match)
            || (typeFilter == 2
                && entry.type != HistorySessionType::Tournament)) {
            continue;
        }

        const bool parentMatches = filter.isEmpty()
            || historyEntrySearchText(entry).contains(filter);
        bool gameMatches = false;
        for (const HistoryGame& game : entry.games) {
            gameMatches = gameMatches
                || historyGameSearchText(game).contains(filter);
        }
        if (!parentMatches && !gameMatches) {
            continue;
        }

        const QString type = entry.type == HistorySessionType::Tournament
            ? tr("Tournament")
            : tr("Game");
        const QString result =
            entry.type == HistorySessionType::Tournament
            ? tr("W-L-D %1-%2-%3")
                  .arg(entry.player1Wins)
                  .arg(entry.player2Wins)
                  .arg(entry.draws)
            : gameResultSummary(
                  entry.result.isEmpty()
                      ? readableHistoryValue(entry.status)
                      : entry.result,
                  entry.termination);
        auto *top = new QTreeWidgetItem(ui->historyTree);
        top->setText(0, historyDate(entry.startedAt));
        top->setText(1, type);
        top->setText(2, QStringLiteral("%1 - %2")
                            .arg(entry.player1, entry.player2));
        top->setText(3, result);
        top->setData(0, kHistoryEntryRole, entryIndex);
        top->setData(0, kHistoryGameRole, -1);

        if (entry.type == HistorySessionType::Tournament) {
            QFont font = top->font(0);
            font.setBold(true);
            for (int column = 0; column < 4; ++column) {
                top->setFont(column, font);
            }
        }

        for (int gameIndex = 0;
             gameIndex < entry.games.size();
             ++gameIndex) {
            const HistoryGame& game = entry.games.at(gameIndex);
            if (!parentMatches
                && !historyGameSearchText(game).contains(filter)) {
                continue;
            }

            auto *child = new QTreeWidgetItem(top);
            child->setText(0, historyDate(game.startedAt));
            child->setText(1, tr("Game %1").arg(game.gameNumber));
            child->setText(2, QStringLiteral("%1 - %2")
                                  .arg(game.white, game.black));
            child->setText(
                3,
                gameResultSummary(
                    game.result.isEmpty()
                        ? readableHistoryValue(game.status)
                        : game.result,
                    game.termination));
            child->setData(0, kHistoryEntryRole, entryIndex);
            child->setData(0, kHistoryGameRole, gameIndex);
        }
        top->setExpanded(!filter.isEmpty());
    }

    ui->historyTree->setUpdatesEnabled(true);
    if (ui->historyTree->topLevelItemCount() > 0) {
        ui->historyTree->setCurrentItem(
            ui->historyTree->topLevelItem(0));
    } else {
        updateHistoryDetails();
    }
}

void MainWindow::updateHistoryDetails()
{
    if (!ui || !ui->historyTree || !ui->historyDetailsText) {
        return;
    }

    QTreeWidgetItem *selected = ui->historyTree->currentItem();
    if (!selected) {
        ui->historyDetailsText->setPlainText(
            m_historyEntries.isEmpty()
                ? tr("No saved games or tournaments were found.")
                : tr("No sessions match the current filter."));
        ui->historyOpenPgnButton->setEnabled(false);
        ui->historyOpenFolderButton->setEnabled(false);
        ui->historyReplayButton->setEnabled(false);
        updateHistoryDeleteButton();
        return;
    }

    const int entryIndex =
        selected->data(0, kHistoryEntryRole).toInt();
    if (entryIndex < 0 || entryIndex >= m_historyEntries.size()) {
        ui->historyReplayButton->setEnabled(false);
        ui->historyOpenPgnButton->setEnabled(false);
        ui->historyOpenFolderButton->setEnabled(false);
        updateHistoryDeleteButton();
        return;
    }

    const HistoryEntry& entry = m_historyEntries.at(entryIndex);
    const int gameIndex = selected->data(0, kHistoryGameRole).toInt();
    const HistoryGame *game =
        gameIndex >= 0 && gameIndex < entry.games.size()
        ? &entry.games.at(gameIndex)
        : nullptr;
    renderHistoryDetails(ui->historyDetailsText, entry, game);
    ui->historyOpenPgnButton->setEnabled(
        !entry.pgnFilePath.isEmpty()
        && QFileInfo::exists(entry.pgnFilePath));
    ui->historyOpenFolderButton->setEnabled(
        QDir(entry.directoryPath).exists());
    const bool hasRecord = !entry.recordFilePath.isEmpty()
        && QFileInfo::exists(entry.recordFilePath);
    const bool hasPgn = !entry.pgnFilePath.isEmpty()
        && QFileInfo::exists(entry.pgnFilePath);
    ui->historyReplayButton->setEnabled(
        (hasRecord || hasPgn)
        && (entry.type != HistorySessionType::Tournament
            || !entry.games.isEmpty() || hasPgn));
    updateHistoryDeleteButton();
}

void MainWindow::updateHistoryDeleteButton()
{
    if (!ui || !ui->historyTree || !ui->historyDeleteButton) {
        return;
    }

    QPushButton *button = ui->historyDeleteButton;
    button->setEnabled(false);
    button->setText(tr("Delete"));
    button->setToolTip(QString());

    QTreeWidgetItem *selected = ui->historyTree->currentItem();
    if (!selected) {
        return;
    }

    const int entryIndex = selected->data(0, kHistoryEntryRole).toInt();
    if (entryIndex < 0 || entryIndex >= m_historyEntries.size()) {
        return;
    }

    const HistoryEntry& entry = m_historyEntries.at(entryIndex);
    const int gameIndex = selected->data(0, kHistoryGameRole).toInt();
    if (gameIndex >= 0) {
        button->setToolTip(
            tr("Individual tournament games cannot be deleted. "
               "Select the tournament instead."));
        return;
    }

    const bool active =
        (m_tournamentRunner && m_tournamentRunner->isActive())
        || (m_gameController && m_gameController->isActive());
    if (active) {
        button->setToolTip(
            tr("Stop the current game or tournament before deleting history."));
        return;
    }

    if (!isManagedHistorySessionDirectory(sessionsRootDir(),
                                          entry.directoryPath)) {
        button->setToolTip(
            tr("This session is not stored in the managed history directory."));
        return;
    }

    button->setEnabled(true);
}

void MainWindow::openSelectedHistoryPgn()
{
    if (!ui || !ui->historyTree) {
        return;
    }

    QTreeWidgetItem *selected = ui->historyTree->currentItem();
    if (!selected) {
        return;
    }
    const int entryIndex =
        selected->data(0, kHistoryEntryRole).toInt();
    if (entryIndex < 0 || entryIndex >= m_historyEntries.size()) {
        return;
    }

    const QString pgnPath = m_historyEntries.at(entryIndex).pgnFilePath;
    if (pgnPath.isEmpty() || !QFileInfo::exists(pgnPath)
        || !QDesktopServices::openUrl(QUrl::fromLocalFile(pgnPath))) {
        QMessageBox::warning(
            this,
            tr("Open PGN"),
            tr("The PGN file is not available or could not be opened."));
    }
}

void MainWindow::openSelectedHistoryDirectory()
{
    if (!ui || !ui->historyTree) {
        return;
    }

    QTreeWidgetItem *selected = ui->historyTree->currentItem();
    if (!selected) {
        return;
    }
    const int entryIndex =
        selected->data(0, kHistoryEntryRole).toInt();
    if (entryIndex < 0 || entryIndex >= m_historyEntries.size()) {
        return;
    }

    const QString directory =
        m_historyEntries.at(entryIndex).directoryPath;
    if (!QDir(directory).exists()
        || !QDesktopServices::openUrl(QUrl::fromLocalFile(directory))) {
        QMessageBox::warning(
            this,
            tr("Open folder"),
            tr("The session folder is not available or could not be opened."));
    }
}

void MainWindow::deleteSelectedHistorySession()
{
    if (!ui || !ui->historyTree) {
        return;
    }

    QTreeWidgetItem *selected = ui->historyTree->currentItem();
    if (!selected || selected->data(0, kHistoryGameRole).toInt() >= 0) {
        return;
    }

    const int entryIndex = selected->data(0, kHistoryEntryRole).toInt();
    if (entryIndex < 0 || entryIndex >= m_historyEntries.size()) {
        return;
    }

    const bool active =
        (m_tournamentRunner && m_tournamentRunner->isActive())
        || (m_gameController && m_gameController->isActive());
    if (active) {
        QMessageBox::warning(
            this,
            tr("Delete history"),
            tr("Stop the current game or tournament before deleting history."));
        return;
    }

    const HistoryEntry entry = m_historyEntries.at(entryIndex);
    const QString sessionsDirectory = sessionsRootDir();
    if (!isManagedHistorySessionDirectory(sessionsDirectory,
                                          entry.directoryPath)) {
        QMessageBox::warning(
            this,
            tr("Delete history"),
            tr("The selected session is not stored in the managed history "
               "directory and will not be deleted."));
        return;
    }

    const bool tournament =
        entry.type == HistorySessionType::Tournament;
    QMessageBox confirmation(
        QMessageBox::Warning,
        tournament ? tr("Delete tournament") : tr("Delete game"),
        tournament
            ? tr("Delete the selected tournament permanently?")
            : tr("Delete the selected game permanently?"),
        QMessageBox::NoButton,
        this);
    confirmation.setInformativeText(
        tr("%1 - %2\n\nAll associated records, PGN files and communication "
           "logs will be removed. This action cannot be undone.")
            .arg(entry.player1, entry.player2));
    QPushButton *deleteButton = confirmation.addButton(
        tournament ? tr("Delete tournament") : tr("Delete game"),
        QMessageBox::DestructiveRole);
    QPushButton *cancelButton =
        confirmation.addButton(QMessageBox::Cancel);
    confirmation.setDefaultButton(cancelButton);
    confirmation.setEscapeButton(cancelButton);
    confirmation.exec();
    if (confirmation.clickedButton() != deleteButton) {
        return;
    }

    const QString sessionDirectory = entry.directoryPath;
    const bool replayUsesSession =
        std::any_of(m_replayGames.cbegin(),
                    m_replayGames.cend(),
                    [&sessionDirectory](const ReplayGame& game) {
        return pathBelongsToDirectory(game.sourcePath, sessionDirectory);
    });
    const QString communicationLogPath =
        m_gameController ? m_gameController->communicationLogFilePath()
                         : QString();
    const bool logUsesSession =
        pathBelongsToDirectory(communicationLogPath, sessionDirectory);
    const bool displayedSession =
        replayUsesSession
        || pathBelongsToDirectory(m_matchRecordPath, sessionDirectory)
        || (m_tournamentRunner
            && pathBelongsToDirectory(m_tournamentRunner->reportFilePath(),
                                      sessionDirectory))
        || logUsesSession;

    if (logUsesSession && m_gameController) {
        if (m_debugDialog) {
            m_debugDialog->close();
        }
        m_gameController->closeCommunicationLog();
        updateDebugLogPath();
    }

    const HistorySessionDeletionResult result =
        deleteHistorySession(sessionsDirectory, sessionDirectory);
    if (!result.succeeded()) {
        QMessageBox::warning(
            this,
            tr("Delete history"),
            tr("The selected session could not be deleted:\n\n%1")
                .arg(result.error));
        refreshHistory();
        return;
    }

    if (displayedSession) {
        clearSessionPanels();
    }
    refreshHistory();
    if (ui->statusbar) {
        ui->statusbar->showMessage(
            tournament ? tr("Tournament deleted.")
                       : tr("Game deleted."),
            4000);
    }
}

void MainWindow::openReplayFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open game replay"),
        QString(),
        tr("Replay files (*.json *.pgn *.epd *.edp);;"
           "Xake records (*.json);;PGN games (*.pgn);;"
           "EPD positions (*.epd *.edp);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    const ReplayLoadResult loaded = loadReplayFile(path);
    if (!loaded.success()) {
        QMessageBox::warning(this, tr("Open replay"), loaded.error);
        return;
    }
    beginReplay(loaded.games);
}

void MainWindow::replaySelectedHistory()
{
    if (!ui || !ui->historyTree) {
        return;
    }

    QTreeWidgetItem *selected = ui->historyTree->currentItem();
    if (!selected) {
        return;
    }
    const int entryIndex =
        selected->data(0, kHistoryEntryRole).toInt();
    if (entryIndex < 0 || entryIndex >= m_historyEntries.size()) {
        return;
    }

    const HistoryEntry& entry = m_historyEntries.at(entryIndex);
    QString path = entry.recordFilePath;
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        path = entry.pgnFilePath;
    }
    ReplayLoadResult loaded = loadReplayFile(path);
    if (!loaded.success()
        && !entry.pgnFilePath.isEmpty()
        && QFileInfo::exists(entry.pgnFilePath)
        && QFileInfo(entry.pgnFilePath).absoluteFilePath()
            != QFileInfo(path).absoluteFilePath()) {
        loaded = loadReplayFile(entry.pgnFilePath);
    }
    if (!loaded.success()) {
        QMessageBox::warning(this, tr("Replay history"), loaded.error);
        return;
    }

    int gameIndex = selected->data(0, kHistoryGameRole).toInt();
    if (gameIndex < 0) {
        gameIndex = 0;
    }
    beginReplay(loaded.games, gameIndex);
}

bool MainWindow::beginReplay(const QVector<ReplayGame>& games,
                             int initialGameIndex)
{
    if (games.isEmpty()) {
        return false;
    }

    for (int index = 0; index < games.size(); ++index) {
        GameReplay validator;
        QString error;
        if (!validator.load(games.at(index), &error)) {
            QMessageBox::warning(
                this,
                tr("Invalid replay"),
                tr("Game %1 could not be loaded:\n%2")
                    .arg(games.at(index).gameNumber)
                    .arg(error));
            return false;
        }
    }

    const bool tournamentActive =
        m_tournamentRunner && m_tournamentRunner->isActive();
    const bool gameActive =
        m_gameController && m_gameController->isActive();
    if (!m_replayActive && (tournamentActive || gameActive)) {
        const QString sessionName =
            tournamentActive ? tr("tournament") : tr("game");
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            tr("Session in progress"),
            tr("A %1 is currently in progress.\n\n"
               "Do you want to stop it and open the replay?")
                .arg(sessionName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes || !stopActiveSession()) {
            return false;
        }
    }

    clearSessionPanels();
    m_replayGames = games;
    m_replayActive = true;
    ui->board->setMoveInputEnabled(false);
    ui->replayPanel->show();

    {
        const QSignalBlocker blocker(ui->replayGameCombo);
        ui->replayGameCombo->clear();
        for (const ReplayGame& game : games) {
            QString label = tr("Game %1").arg(game.gameNumber);
            if (!game.white.isEmpty() || !game.black.isEmpty()) {
                label += QStringLiteral(" | %1 - %2")
                    .arg(game.white.isEmpty() ? tr("White") : game.white,
                         game.black.isEmpty() ? tr("Black") : game.black);
            } else if (!game.title.isEmpty()) {
                label += QStringLiteral(" | %1").arg(game.title);
            }
            const QString summary =
                gameResultSummary(game.result, game.termination);
            if (!summary.isEmpty()) {
                label += QStringLiteral(" | %1").arg(summary);
            }
            ui->replayGameCombo->addItem(label);
        }
        initialGameIndex = qBound(
            0, initialGameIndex, static_cast<int>(games.size()) - 1);
        ui->replayGameCombo->setCurrentIndex(initialGameIndex);
    }
    ui->replayGameCombo->setEnabled(games.size() > 1);

    if (!loadReplayGame(initialGameIndex)) {
        leaveReplay(false);
        return false;
    }
    ui->tabWidget->setCurrentWidget(ui->tabGame);
    updateSessionControls();
    return true;
}

bool MainWindow::loadReplayGame(int index)
{
    if (!m_replayActive
        || index < 0 || index >= m_replayGames.size()) {
        return false;
    }

    QString error;
    if (!m_replay.load(m_replayGames.at(index), &error)) {
        QMessageBox::warning(this, tr("Invalid replay"), error);
        return false;
    }

    {
        const QSignalBlocker blocker(ui->replayGameCombo);
        ui->replayGameCombo->setCurrentIndex(index);
    }
    const ReplayGame& game = m_replay.game();
    const QString white =
        game.white.isEmpty() ? tr("White") : game.white;
    const QString black =
        game.black.isEmpty() ? tr("Black") : game.black;
    ui->labelWhitePlayer->setText(white);
    ui->labelBlackPlayer->setText(black);
    const QString summary =
        gameResultSummary(game.result, game.termination);
    ui->labelReplayTitle->setText(
        summary.isEmpty()
            ? tr("Replay | %1 - %2").arg(white, black)
            : tr("Replay | %1 - %2 | %3")
                  .arg(white, black, summary));
    m_currentOpeningName = game.openingName;
    m_currentOpeningIndex = game.gameNumber;
    m_currentOpeningCount = 1;
    updateGameOpeningLabel();
    navigateReplayTo(0);
    if (ui->statusbar) {
        ui->statusbar->showMessage(
            tr("Replay loaded: game %1, %n move(s).",
               nullptr,
               m_replay.totalPly())
                .arg(game.gameNumber),
            5000);
    }
    return true;
}

void MainWindow::navigateReplayTo(int ply)
{
    if (!m_replayActive
        || ply < 0 || ply > m_replay.totalPly()) {
        return;
    }

    const int previousPly = m_replay.currentPly();
    if (!m_replay.goToPly(ply)) {
        QMessageBox::warning(
            this,
            tr("Replay error"),
            tr("The replay position could not be reconstructed."));
        return;
    }
    const Move animatedMove =
        ply == previousPly + 1 ? m_replay.lastMove() : NOMOVE;
    updateReplayUi(animatedMove);
}

void MainWindow::updateReplayUi(Move animatedMove)
{
    if (!m_replayActive || !ui) {
        return;
    }
    ui->board->setPosition(m_replay.position(), animatedMove);
    updateSideToMoveLabel(m_replay.position());
    updateGameMoveList();
    updateCapturedPieces();
    updateClockUi();
    updateReplayControls();
}

void MainWindow::updateReplayControls()
{
    if (!m_replayActive || !ui) {
        return;
    }
    const int current = m_replay.currentPly();
    const int total = m_replay.totalPly();
    ui->labelReplayProgress->setText(
        tr("Ply %1 / %2").arg(current).arg(total));
    ui->replayFirstButton->setEnabled(current > 0);
    ui->replayPreviousButton->setEnabled(current > 0);
    ui->replayNextButton->setEnabled(current < total);
    ui->replayLastButton->setEnabled(current < total);
}

void MainWindow::leaveReplay(bool resetGamePanel)
{
    if (!m_replayActive) {
        return;
    }

    if (resetGamePanel) {
        clearSessionPanels();
    } else {
        m_replayActive = false;
        m_replay.clear();
        m_replayGames.clear();
        ui->replayPanel->hide();
        ui->replayGameCombo->clear();
    }

    updateSessionControls();

    if (resetGamePanel && ui->statusbar) {
        ui->statusbar->showMessage(tr("Replay closed."), 3000);
    }
}

void MainWindow::updateGameOpeningLabel()
{
    if (!ui || !ui->labelGameOpening) {
        return;
    }

    if (m_replayActive) {
        ui->labelGameOpening->setText(
            m_currentOpeningName.isEmpty()
                ? tr("Replay | Opening: -")
                : tr("Replay | Opening: %1").arg(m_currentOpeningName));
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
        scrollCommunicationToLatest(m_debugText);
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
    auto *openLogButton = new QPushButton(tr("Open log file"), dialog);
    auto *pathLayout = new QHBoxLayout;
    pathLayout->addWidget(pathLabel, 1);
    pathLayout->addWidget(openLogButton);
    layout->addLayout(pathLayout);

    auto *debugText = new QPlainTextEdit(dialog);
    configureEngineOutput(debugText);
    debugText->setMaximumBlockCount(
        GameController::kCommunicationHistoryLimit);
    renderCommunicationHistory(debugText,
                               m_gameController->communicationHistory());
    layout->addWidget(debugText);

    m_debugDialog = dialog;
    m_debugPathLabel = pathLabel;
    m_debugText = debugText;
    m_debugOpenLogButton = openLogButton;
    connect(openLogButton, &QPushButton::clicked, this, [this]() {
        const QString logPath = m_gameController->communicationLogFilePath();
        if (logPath.isEmpty() || !QFileInfo::exists(logPath)) {
            QMessageBox::warning(
                m_debugDialog,
                tr("Open log file"),
                tr("The communication log file is not available."));
            return;
        }

        const QString absolutePath = QFileInfo(logPath).absoluteFilePath();
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath))) {
            QMessageBox::warning(
                m_debugDialog,
                tr("Open log file"),
                tr("The system could not open the communication log file."));
        }
    });
    updateDebugLogPath();
    dialog->show();
}

void MainWindow::manageApplicationData()
{
    const bool active =
        (m_tournamentRunner && m_tournamentRunner->isActive())
        || (m_gameController && m_gameController->isActive());
    if (active) {
        QMessageBox::warning(
            this,
            tr("Manage application data"),
            tr("Stop the current game or tournament before deleting "
               "application data."));
        return;
    }

    const QString dataDirectory = applicationDataDir();
    const ApplicationDataSummary summary =
        inspectApplicationData(dataDirectory);
    ApplicationDataDialog dialog(dataDirectory, summary, this);
    connect(&dialog,
            &ApplicationDataDialog::openDataFolderRequested,
            this,
            [this, dataDirectory]() {
        if (!QDesktopServices::openUrl(
                QUrl::fromLocalFile(dataDirectory))) {
            QMessageBox::warning(
                this,
                tr("Open session files folder"),
                tr("The system could not open the session files folder."));
        }
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const ApplicationDataSelection selection = dialog.selection();
    if (!selection.anySelected()) {
        return;
    }

    QStringList selectedCategories;
    if (selection.records) {
        selectedCategories.append(tr("- Game and tournament records"));
    }
    if (selection.pgnFiles) {
        selectedCategories.append(tr("- Exported PGN files"));
    }
    if (selection.communicationLogs) {
        selectedCategories.append(tr("- Engine communication logs"));
    }
    if (selection.settings) {
        selectedCategories.append(tr("- Saved settings and engine paths"));
    }

    QString confirmationText =
        tr("The following data will be permanently deleted:\n\n%1\n\n"
           "This action cannot be undone.")
            .arg(selectedCategories.join(QLatin1Char('\n')));
    if (selection.allSelected()) {
        confirmationText +=
            tr("\n\nThe complete Xake data directory will be removed, "
               "including any other files stored there.");
    }

    QMessageBox confirmation(
        QMessageBox::Warning,
        tr("Delete application data"),
        confirmationText,
        QMessageBox::NoButton,
        this);
    QPushButton *deleteButton = confirmation.addButton(
        tr("Delete selected data"),
        QMessageBox::DestructiveRole);
    QPushButton *cancelButton = confirmation.addButton(
        QMessageBox::Cancel);
    confirmation.setDefaultButton(cancelButton);
    confirmation.setEscapeButton(cancelButton);
    confirmation.exec();
    if (confirmation.clickedButton() != deleteButton) {
        return;
    }

    if (selection.communicationLogs && m_gameController) {
        if (m_debugDialog) {
            m_debugDialog->close();
        }
        m_gameController->closeCommunicationLog();
    }

    QSettings settings;
    const ApplicationDataDeletionResult result =
        deleteApplicationData(dataDirectory,
                              selection,
                              settings,
                              true);

    if (selection.settings && result.settingsCleared) {
        m_state = AppState{};
        m_lastSessionKind = SessionKind::None;
    }
    if (selection.records || selection.pgnFiles) {
        refreshHistory();
    }
    updateSessionControls();

    if (!result.succeeded()) {
        QMessageBox::warning(
            this,
            tr("Application data"),
            tr("Some application data could not be deleted:\n\n%1")
                .arg(result.errors.join(QLatin1Char('\n'))));
        return;
    }

    QString resultText;
    if (result.deletedFiles > 0) {
        resultText =
            tr("Deleted %n stored file(s).",
               nullptr,
               result.deletedFiles);
    } else if (!result.settingsCleared) {
        resultText = tr("No matching stored files were found.");
    }
    if (result.settingsCleared) {
        if (!resultText.isEmpty()) {
            resultText += QLatin1Char('\n');
        }
        resultText += tr("Saved settings were cleared.");
    }

    if (selection.allSelected()) {
        clearSessionPanels();
    }

    QMessageBox::information(
        this,
        tr("Application data deleted"),
        resultText);
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(
        this,
        tr("About Xake"),
        tr("<h2>Xake %1</h2>"
           "<p>Chess GUI and tournament manager for human players and "
           "UCI-compatible engines.</p>"
           "<p>Developed by <b>Julen Aristondo</b>.<br>"
           "Source code: <a href=\"https://github.com/neluj/Xake\">"
           "github.com/neluj/Xake</a></p>"
           "<p>Copyright &copy; 2026 Julen Aristondo.<br>"
           "Licensed under GPL-3.0-only.</p>"
           "<p>Built with Qt %2.</p>")
            .arg(QApplication::applicationVersion(),
                 QString::fromLatin1(qVersion())));
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
    if (m_debugOpenLogButton) {
        m_debugOpenLogButton->setEnabled(
            !logPath.isEmpty() && QFileInfo::exists(logPath));
    }
}

void MainWindow::updateClockUi()
{
    if (!ui || !m_gameController) {
        return;
    }
    if (!ui->whiteTimeLcd || !ui->blackTimeLcd) {
        return;
    }

    if (m_replayActive) {
        const qint64 whiteMs = m_replay.whiteTimeMs();
        const qint64 blackMs = m_replay.blackTimeMs();
        ui->whiteTimeLcd->display(
            whiteMs >= 0 ? formatClockMs(whiteMs)
                         : QStringLiteral("--:--"));
        ui->blackTimeLcd->display(
            blackMs >= 0 ? formatClockMs(blackMs)
                         : QStringLiteral("--:--"));
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
