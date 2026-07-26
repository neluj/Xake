#include "tournament_runner.h"

#include "match_settings_validation.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>

#include <limits>
#include <utility>

namespace {

QString currentTimestamp()
{
    return QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
}

QJsonObject playerToJson(const PlayerConfig& player)
{
    QJsonObject object;
    object["type"] = player.type == PlayerType::Engine ? QStringLiteral("engine")
                                                       : QStringLiteral("human");
    object["name"] = player.name;
    object["enginePath"] = player.enginePath;
    return object;
}

QJsonObject summaryToJson(const TournamentSummary& summary)
{
    QJsonObject object;
    object["totalGames"] = summary.totalGames;
    object["completedGames"] = summary.completedGames;
    object["player1Wins"] = summary.player1Wins;
    object["player2Wins"] = summary.player2Wins;
    object["draws"] = summary.draws;
    object["whiteWins"] = summary.whiteWins;
    object["blackWins"] = summary.blackWins;
    return object;
}

QString resultNotation(GameOutcome outcome)
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

QString outcomeName(GameOutcome outcome)
{
    switch (outcome) {
    case GameOutcome::WhiteWin:
        return QStringLiteral("white_win");
    case GameOutcome::BlackWin:
        return QStringLiteral("black_win");
    case GameOutcome::Draw:
        return QStringLiteral("draw");
    }

    return QString();
}

QString terminationName(GameTermination termination)
{
    switch (termination) {
    case GameTermination::Checkmate:
        return QStringLiteral("checkmate");
    case GameTermination::Stalemate:
        return QStringLiteral("stalemate");
    case GameTermination::FiftyMoveRule:
        return QStringLiteral("fifty_move_rule");
    case GameTermination::ThreefoldRepetition:
        return QStringLiteral("threefold_repetition");
    case GameTermination::InsufficientMaterial:
        return QStringLiteral("insufficient_material");
    case GameTermination::TimeForfeit:
        return QStringLiteral("time_forfeit");
    case GameTermination::MoveLimit:
        return QStringLiteral("move_limit");
    }

    return QString();
}

QJsonObject gameRecordToJson(const TournamentGameRecord& record)
{
    QJsonObject object;
    object["gameNumber"] = record.gameNumber;
    object["status"] = record.completed ? QStringLiteral("completed")
                                        : record.aborted ? QStringLiteral("aborted")
                                                         : QStringLiteral("in_progress");
    object["startedAt"] = record.startedAtIso;
    object["colorsSwapped"] = record.colorsSwapped;
    if (!record.finishedAtIso.isEmpty()) {
        object["finishedAt"] = record.finishedAtIso;
    }
    object["white"] = playerToJson(record.match.player1);
    object["black"] = playerToJson(record.match.player2);

    QJsonObject opening;
    opening["index"] = record.openingIndex;
    opening["name"] = record.openingName;
    opening["startFen"] = record.startFen;
    QJsonArray openingMoves;
    for (const QString& move : record.openingMoves) {
        openingMoves.append(move);
    }
    opening["moves"] = openingMoves;
    object["opening"] = opening;

    QJsonArray moves;
    for (const QString& move : record.moves) {
        moves.append(move);
    }
    object["moves"] = moves;

    if (record.completed) {
        object["result"] = resultNotation(record.result.outcome);
        object["outcome"] = outcomeName(record.result.outcome);
        object["termination"] = terminationName(record.result.termination);
        object["message"] = record.result.message;
    }
    if (record.aborted) {
        object["abortTitle"] = record.abortTitle;
        object["abortMessage"] = record.abortMessage;
    }
    return object;
}

} // namespace

TournamentRunner::TournamentRunner(GameController *gameController, QObject *parent)
    : QObject(parent)
    , m_gameController(gameController)
{
    Q_ASSERT(m_gameController);

    connect(m_gameController, &GameController::gameFinished,
            this, &TournamentRunner::handleGameFinished);
    connect(m_gameController, &GameController::gameAborted,
            this, &TournamentRunner::handleGameAborted);
    connect(m_gameController, &GameController::movePlayed,
            this, &TournamentRunner::handleMovePlayed);
}

bool TournamentRunner::start(const TournamentConfig& config,
                             const QVector<OpeningEntry>& openings,
                             const QString& logDir,
                             const QString& sessionTag)
{
    if (m_active || !m_gameController || openings.isEmpty()) {
        return false;
    }

    TournamentConfig normalized = config;
    normalizeTournamentConfig(normalized);
    if (validateTournamentConfig(normalized) != ValidationError::None) {
        return false;
    }

    const qint64 totalGames = static_cast<qint64>(normalized.rounds)
        * normalized.gamesPerPairing;
    if (totalGames < 1 || totalGames > std::numeric_limits<int>::max()) {
        return false;
    }

    m_config = normalized;
    m_openings = openings;
    m_logDir = logDir;
    m_sessionTag = sessionTag;
    m_summary = TournamentSummary{static_cast<int>(totalGames)};
    m_gameRecords.clear();
    m_reportFilePath = m_logDir.isEmpty()
        ? QString()
        : QDir(m_logDir).filePath(QStringLiteral("tournament_report.json"));
    m_status = QStringLiteral("in_progress");
    m_startedAtIso = currentTimestamp();
    m_finishedAtIso.clear();
    m_abortTitle.clear();
    m_abortMessage.clear();
    m_nextGameNumber = 1;
    m_currentGameNumber = 0;
    m_currentColorsSwapped = false;
    m_active = true;
    m_paused = false;
    m_waitingForNextGame = false;
    m_reportErrorEmitted = false;
    ++m_runGeneration;

    persistReport();
    emit tournamentStarted(m_summary.totalGames);
    startNextGame();
    return true;
}

bool TournamentRunner::pause()
{
    if (!m_active || m_paused) {
        return false;
    }

    m_paused = true;
    m_status = QStringLiteral("paused");
    if (m_gameController->isActive()) {
        m_gameController->pauseMatch();
    }
    persistReport();
    emit pauseChanged(true);
    return true;
}

bool TournamentRunner::resume()
{
    if (!m_active || !m_paused) {
        return false;
    }

    m_paused = false;
    m_status = QStringLiteral("in_progress");
    if (m_gameController->isActive()) {
        m_gameController->resumeMatch();
    } else if (m_waitingForNextGame) {
        m_waitingForNextGame = false;
        startNextGame();
    }
    persistReport();
    emit pauseChanged(false);
    return true;
}

bool TournamentRunner::stop()
{
    if (!m_active) {
        return false;
    }

    const bool wasPaused = m_paused;
    m_active = false;
    m_paused = false;
    m_waitingForNextGame = false;
    ++m_runGeneration;
    m_status = QStringLiteral("aborted");
    m_finishedAtIso = currentTimestamp();
    m_abortTitle = tr("Tournament stopped");
    m_abortMessage = tr("Tournament stopped by user.");

    if (TournamentGameRecord *current = currentGameRecord();
        current && !current->completed) {
        current->moves = m_gameController->moveHistoryUci();
        current->aborted = true;
        current->finishedAtIso = m_finishedAtIso;
        current->abortTitle = m_abortTitle;
        current->abortMessage = m_abortMessage;
    }

    m_gameController->stopMatch();
    persistReport();
    if (wasPaused) {
        emit pauseChanged(false);
    }
    emit tournamentAborted(m_abortTitle, m_abortMessage);
    return true;
}

bool TournamentRunner::isActive() const
{
    return m_active;
}

bool TournamentRunner::isPaused() const
{
    return m_paused;
}

TournamentSummary TournamentRunner::summary() const
{
    return m_summary;
}

QVector<TournamentGameRecord> TournamentRunner::gameRecords() const
{
    return m_gameRecords;
}

QString TournamentRunner::reportFilePath() const
{
    return m_reportFilePath;
}

int TournamentRunner::openingCount() const
{
    return static_cast<int>(m_openings.size());
}

void TournamentRunner::startNextGame()
{
    if (!m_active || m_paused) {
        return;
    }
    if (m_nextGameNumber > m_summary.totalGames) {
        finishTournament();
        return;
    }

    m_currentGameNumber = m_nextGameNumber++;
    m_currentColorsSwapped = colorsAreSwappedForCurrentGame();
    const MatchConfig match = matchForCurrentGame();
    const OpeningEntry& opening = openingForCurrentGame();

    TournamentGameRecord record;
    record.gameNumber = m_currentGameNumber;
    record.match = match;
    record.startedAtIso = currentTimestamp();
    record.colorsSwapped = m_currentColorsSwapped;
    record.openingIndex = opening.sourceIndex;
    record.openingName = opening.name;
    record.startFen = opening.startFen;
    record.openingMoves = opening.movesUci;
    record.moves = opening.movesUci;
    m_gameRecords.append(record);
    persistReport();

    emit tournamentGameStarted(m_currentGameNumber, m_summary.totalGames, match);

    const QString gameTag = QStringLiteral("%1_game%2")
        .arg(m_sessionTag)
        .arg(m_currentGameNumber, 3, 10, QChar('0'));
    if (!m_gameController->startMatch(match,
                                      opening.startFen.toStdString(),
                                      m_logDir,
                                      gameTag,
                                      m_config.maxMoves,
                                      opening.movesUci)) {
        m_active = false;
        m_status = QStringLiteral("aborted");
        m_finishedAtIso = currentTimestamp();
        m_abortTitle = tr("Tournament error");
        m_abortMessage = tr("Could not start game %1.").arg(m_currentGameNumber);
        if (TournamentGameRecord *current = currentGameRecord()) {
            current->aborted = true;
            current->finishedAtIso = m_finishedAtIso;
            current->abortTitle = m_abortTitle;
            current->abortMessage = m_abortMessage;
        }
        persistReport();
        emit tournamentAborted(m_abortTitle, m_abortMessage);
    }
}

void TournamentRunner::handleMovePlayed(int ply, const QString& uciMove)
{
    if (!m_active || ply <= 0 || uciMove.isEmpty()) {
        return;
    }

    TournamentGameRecord *current = currentGameRecord();
    if (!current) {
        return;
    }

    if (ply == current->moves.size() + 1) {
        current->moves.append(uciMove);
    } else {
        current->moves = m_gameController->moveHistoryUci();
    }
    persistReport();
}

void TournamentRunner::handleGameFinished(const GameResult& result)
{
    if (!m_active || m_currentGameNumber == 0) {
        return;
    }

    if (TournamentGameRecord *current = currentGameRecord()) {
        current->completed = true;
        current->result = result;
        current->finishedAtIso = currentTimestamp();
    }

    ++m_summary.completedGames;
    switch (result.outcome) {
    case GameOutcome::WhiteWin:
        ++m_summary.whiteWins;
        if (m_currentColorsSwapped) {
            ++m_summary.player2Wins;
        } else {
            ++m_summary.player1Wins;
        }
        break;
    case GameOutcome::BlackWin:
        ++m_summary.blackWins;
        if (m_currentColorsSwapped) {
            ++m_summary.player1Wins;
        } else {
            ++m_summary.player2Wins;
        }
        break;
    case GameOutcome::Draw:
        ++m_summary.draws;
        break;
    }

    persistReport();
    emit tournamentGameFinished(m_currentGameNumber, result);
    m_waitingForNextGame = true;
    const quint64 runGeneration = m_runGeneration;
    QTimer::singleShot(0, this, [this, runGeneration]() {
        if (!m_active || m_paused || runGeneration != m_runGeneration) {
            return;
        }
        m_waitingForNextGame = false;
        startNextGame();
    });
}

void TournamentRunner::handleGameAborted(const QString& title, const QString& message)
{
    if (!m_active) {
        return;
    }

    const bool wasPaused = m_paused;
    m_active = false;
    m_paused = false;
    m_waitingForNextGame = false;
    ++m_runGeneration;
    m_status = QStringLiteral("aborted");
    m_finishedAtIso = currentTimestamp();
    m_abortTitle = title;
    m_abortMessage = message;
    if (TournamentGameRecord *current = currentGameRecord()) {
        current->aborted = true;
        current->finishedAtIso = m_finishedAtIso;
        current->abortTitle = title;
        current->abortMessage = message;
    }
    persistReport();
    if (wasPaused) {
        emit pauseChanged(false);
    }
    emit tournamentAborted(title, message);
}

MatchConfig TournamentRunner::matchForCurrentGame() const
{
    MatchConfig match = m_config.match;
    if (m_currentColorsSwapped) {
        std::swap(match.player1, match.player2);
    }
    const OpeningEntry& opening = openingForCurrentGame();
    match.game.useOpeningFile = false;
    match.game.openingFilePath.clear();
    match.game.useStartPos = false;
    match.game.startPosition = opening.startFen;
    return match;
}

const OpeningEntry& TournamentRunner::openingForCurrentGame() const
{
    const int pairIndex = qMax(0, m_currentGameNumber - 1) / 2;
    return m_openings.at(pairIndex % m_openings.size());
}

bool TournamentRunner::colorsAreSwappedForCurrentGame() const
{
    return (m_currentGameNumber % 2) == 0;
}

void TournamentRunner::finishTournament()
{
    if (!m_active) {
        return;
    }

    const bool wasPaused = m_paused;
    m_active = false;
    m_paused = false;
    m_waitingForNextGame = false;
    ++m_runGeneration;
    m_status = QStringLiteral("completed");
    m_finishedAtIso = currentTimestamp();
    persistReport();
    if (wasPaused) {
        emit pauseChanged(false);
    }
    emit tournamentFinished(m_summary);
}

TournamentGameRecord* TournamentRunner::currentGameRecord()
{
    if (m_gameRecords.isEmpty()
        || m_gameRecords.constLast().gameNumber != m_currentGameNumber) {
        return nullptr;
    }
    return &m_gameRecords.last();
}

void TournamentRunner::persistReport()
{
    QString error;
    if (writeReport(&error) || m_reportErrorEmitted) {
        return;
    }

    m_reportErrorEmitted = true;
    emit tournamentReportError(
        tr("Could not write tournament report '%1': %2")
            .arg(m_reportFilePath, error));
}

bool TournamentRunner::writeReport(QString* errorOut) const
{
    if (m_reportFilePath.isEmpty()) {
        return true;
    }

    const QFileInfo reportInfo(m_reportFilePath);
    if (!QDir().mkpath(reportInfo.absolutePath())) {
        if (errorOut) {
            *errorOut = tr("Could not create the report directory.");
        }
        return false;
    }

    QJsonObject tournament;
    tournament["type"] = m_config.tournamentType;
    tournament["rounds"] = m_config.rounds;
    tournament["gamesPerPairing"] = m_config.gamesPerPairing;
    tournament["maxMoves"] = m_config.maxMoves;
    tournament["player1"] = playerToJson(m_config.match.player1);
    tournament["player2"] = playerToJson(m_config.match.player2);

    QJsonObject gameSettings;
    gameSettings["timeControl"] = m_config.match.game.timeControl;
    gameSettings["baseTimeSeconds"] = m_config.match.game.baseTimeSeconds;
    gameSettings["incrementSeconds"] = m_config.match.game.incrementSeconds;
    gameSettings["movesToGo"] = m_config.match.game.movesToGo;
    gameSettings["useOpeningFile"] = m_config.match.game.useOpeningFile;
    gameSettings["openingFilePath"] = m_config.match.game.openingFilePath;
    gameSettings["openingCount"] = m_openings.size();
    tournament["game"] = gameSettings;

    QJsonArray games;
    for (const TournamentGameRecord& record : m_gameRecords) {
        games.append(gameRecordToJson(record));
    }

    QJsonObject root;
    root["sessionTag"] = m_sessionTag;
    root["status"] = m_status;
    root["startedAt"] = m_startedAtIso;
    root["updatedAt"] = currentTimestamp();
    if (!m_finishedAtIso.isEmpty()) {
        root["finishedAt"] = m_finishedAtIso;
    }
    root["startFen"] = m_openings.isEmpty() ? QString() : m_openings.first().startFen;
    root["moveFormat"] = QStringLiteral("uci");
    root["tournament"] = tournament;
    root["summary"] = summaryToJson(m_summary);
    root["games"] = games;
    if (!m_abortMessage.isEmpty()) {
        QJsonObject abort;
        abort["title"] = m_abortTitle;
        abort["message"] = m_abortMessage;
        root["abort"] = abort;
    }

    QSaveFile file(m_reportFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return false;
    }

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return false;
    }
    return true;
}
