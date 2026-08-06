#include "tournament_runner.h"

#include "match_settings_validation.h"
#include "pgn_export.h"

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

QJsonObject standingToJson(const TournamentStanding& standing)
{
    QJsonObject object;
    object["participantId"] = standing.participantId;
    object["name"] = standing.name;
    object["games"] = standing.games();
    object["wins"] = standing.wins;
    object["losses"] = standing.losses;
    object["draws"] = standing.draws;
    object["points"] = standing.points();
    object["whiteGames"] = standing.whiteGames;
    object["blackGames"] = standing.blackGames;
    object["sequence"] = standing.sequence;
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
    QJsonArray standings;
    for (const TournamentStanding& standing : summary.standings) {
        standings.append(standingToJson(standing));
    }
    object["standings"] = standings;
    return object;
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

QJsonObject gameRecordToJson(const TournamentGameRecord& record)
{
    QJsonObject object;
    object["gameNumber"] = record.gameNumber;
    object["roundNumber"] = record.roundNumber;
    object["cycleNumber"] = record.cycleNumber;
    object["gameInPairing"] = record.gameInPairing;
    object["whiteParticipantId"] = record.whiteParticipantId;
    object["blackParticipantId"] = record.blackParticipantId;
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
    if (!record.moveRecords.isEmpty()) {
        object["moveRecords"] = moveRecordsToJson(record.moveRecords);
    }

    QJsonArray moves;
    for (const QString& move : record.moves) {
        moves.append(move);
    }
    object["moves"] = moves;

    if (record.termination != GameTermination::Unknown) {
        object["termination"] = gameTerminationKey(record.termination);
    }
    if (record.completed) {
        object["result"] = gameResultNotation(record.result.outcome);
        object["outcome"] = gameOutcomeKey(record.result.outcome);
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
    connect(m_gameController, &GameController::engineFailureOccurred,
            this, &TournamentRunner::handleEngineFailure);
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

    const TournamentScheduleResult schedule =
        buildTournamentSchedule(normalized);
    if (!schedule.succeeded()) {
        return false;
    }

    m_config = normalized;
    m_schedule = schedule.games;
    m_openings = openings;
    m_logDir = logDir;
    m_sessionTag = sessionTag;
    m_summary = TournamentSummary{};
    m_summary.totalGames = m_schedule.size();
    for (const TournamentParticipant& participant : m_config.participants) {
        m_summary.standings.append({
            participant.id,
            pgnPlayerName(participant.player)
        });
    }
    m_gameRecords.clear();
    m_reportFilePath = m_logDir.isEmpty()
        ? QString()
        : QDir(m_logDir).filePath(QStringLiteral("tournament_report.json"));
    m_pgnFilePath = m_logDir.isEmpty()
        ? QString()
        : QDir(m_logDir).filePath(QStringLiteral("tournament.pgn"));
    m_status = QStringLiteral("in_progress");
    m_startedAtIso = currentTimestamp();
    m_finishedAtIso.clear();
    m_abortTitle.clear();
    m_abortMessage.clear();
    m_nextScheduleIndex = 0;
    m_currentGameNumber = 0;
    m_currentScheduledGame = TournamentScheduledGame{};
    m_currentColorsSwapped = false;
    m_active = true;
    m_paused = false;
    m_waitingForNextGame = false;
    m_waitingForHumanGame = false;
    m_failedEngineSide = -1;
    m_reportErrorEmitted = false;
    m_pgnErrorEmitted = false;
    ++m_runGeneration;

    persistReport();
    persistPgn();
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
    } else if (m_waitingForHumanGame) {
        if (const TournamentGameRecord *current =
                currentGameRecord()) {
            emit humanGameReadyRequested(
                m_currentGameNumber, m_summary.totalGames,
                current->match);
        }
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
    m_waitingForHumanGame = false;
    ++m_runGeneration;
    m_status = QStringLiteral("aborted");
    m_finishedAtIso = currentTimestamp();
    m_abortTitle = tr("Tournament stopped");
    m_abortMessage = tr("Tournament stopped by user.");

    if (TournamentGameRecord *current = currentGameRecord();
        current && !current->completed) {
        if (m_gameController->isActive()) {
            current->moves = m_gameController->moveHistoryUci();
            current->moveRecords = m_gameController->moveRecords();
        }
        current->aborted = true;
        current->termination = GameTermination::Stopped;
        current->finishedAtIso = m_finishedAtIso;
        current->abortTitle = m_abortTitle;
        current->abortMessage = m_abortMessage;
    }

    m_gameController->stopMatch();
    persistReport();
    persistPgn();
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

void TournamentRunner::setHumanGameConfirmationEnabled(bool enabled)
{
    m_humanGameConfirmationEnabled = enabled;
}

bool TournamentRunner::startPendingHumanGame()
{
    if (!m_active || m_paused || !m_waitingForHumanGame) {
        return false;
    }

    m_waitingForHumanGame = false;
    launchCurrentGame();
    return true;
}

bool TournamentRunner::isWaitingForHumanGame() const
{
    return m_active && m_waitingForHumanGame;
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

QString TournamentRunner::pgnFilePath() const
{
    return m_pgnFilePath;
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
    if (m_nextScheduleIndex >= m_schedule.size()) {
        finishTournament();
        return;
    }

    m_currentScheduledGame = m_schedule.at(m_nextScheduleIndex++);
    m_currentGameNumber = m_currentScheduledGame.gameNumber;
    const MatchConfig match =
        matchForScheduledGame(m_currentScheduledGame);
    const OpeningEntry& opening =
        openingForScheduledGame(m_currentScheduledGame);
    m_currentColorsSwapped =
        m_config.participants.size() >= 2
        && m_currentScheduledGame.whiteParticipantId
            == m_config.participants.at(1).id
        && m_currentScheduledGame.blackParticipantId
            == m_config.participants.at(0).id;

    TournamentGameRecord record;
    record.gameNumber = m_currentGameNumber;
    record.roundNumber = m_currentScheduledGame.roundNumber;
    record.cycleNumber = m_currentScheduledGame.cycleNumber;
    record.gameInPairing = m_currentScheduledGame.gameInPairing;
    record.whiteParticipantId =
        m_currentScheduledGame.whiteParticipantId;
    record.blackParticipantId =
        m_currentScheduledGame.blackParticipantId;
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

    const bool hasHuman =
        match.player1.type == PlayerType::Human
        || match.player2.type == PlayerType::Human;
    if (m_humanGameConfirmationEnabled && hasHuman) {
        m_waitingForHumanGame = true;
        persistReport();
        emit humanGameReadyRequested(
            m_currentGameNumber, m_summary.totalGames, match);
        return;
    }

    launchCurrentGame();
}

void TournamentRunner::launchCurrentGame()
{
    if (!m_active || m_paused || m_currentGameNumber == 0) {
        return;
    }

    TournamentGameRecord *current = currentGameRecord();
    if (!current) {
        return;
    }
    const OpeningEntry& opening =
        openingForScheduledGame(m_currentScheduledGame);
    const QString gameTag = QStringLiteral("%1_game%2")
        .arg(m_sessionTag)
        .arg(m_currentGameNumber, 3, 10, QChar('0'));
    if (!m_gameController->startMatch(current->match,
                                      opening.startFen.toStdString(),
                                      m_logDir,
                                      gameTag,
                                      m_config.maxMoves,
                                      opening.movesUci)) {
        if (current->completed) {
            return;
        }

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
        persistPgn();
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
    current->moveRecords = m_gameController->moveRecords();
}

void TournamentRunner::handleGameFinished(const GameResult& result)
{
    if (!m_active || m_currentGameNumber == 0) {
        return;
    }

    completeCurrentGame(result);
}

void TournamentRunner::handleEngineFailure(EngineFailure,
                                           EngineSide side,
                                           const QString&)
{
    if (m_active && m_currentGameNumber != 0) {
        m_failedEngineSide =
            side == EngineSide::White ? 0 : 1;
    }
}

void TournamentRunner::completeCurrentGame(const GameResult& result)
{
    TournamentGameRecord *current = currentGameRecord();
    if (!m_active || !current || current->completed) {
        return;
    }

    current->completed = true;
    current->aborted = false;
    current->result = result;
    current->termination = result.termination;
    current->finishedAtIso = currentTimestamp();
    current->moves = m_gameController->moveHistoryUci();
    current->moveRecords = m_gameController->moveRecords();
    m_waitingForHumanGame = false;
    m_failedEngineSide = -1;

    ++m_summary.completedGames;
    TournamentStanding *white =
        standing(current->whiteParticipantId);
    TournamentStanding *black =
        standing(current->blackParticipantId);
    if (white) {
        ++white->whiteGames;
    }
    if (black) {
        ++black->blackGames;
    }

    QString winnerId;
    switch (result.outcome) {
    case GameOutcome::WhiteWin:
        ++m_summary.whiteWins;
        winnerId = current->whiteParticipantId;
        if (white) {
            ++white->wins;
            white->sequence += QLatin1Char('1');
        }
        if (black) {
            ++black->losses;
            black->sequence += QLatin1Char('0');
        }
        break;
    case GameOutcome::BlackWin:
        ++m_summary.blackWins;
        winnerId = current->blackParticipantId;
        if (black) {
            ++black->wins;
            black->sequence += QLatin1Char('1');
        }
        if (white) {
            ++white->losses;
            white->sequence += QLatin1Char('0');
        }
        break;
    case GameOutcome::Draw:
        ++m_summary.draws;
        if (white) {
            ++white->draws;
            white->sequence += QLatin1Char('=');
        }
        if (black) {
            ++black->draws;
            black->sequence += QLatin1Char('=');
        }
        break;
    }

    if (!winnerId.isEmpty() && m_config.participants.size() >= 2) {
        if (winnerId == m_config.participants.at(0).id) {
            ++m_summary.player1Wins;
        } else if (winnerId == m_config.participants.at(1).id) {
            ++m_summary.player2Wins;
        }
    }

    persistReport();
    persistPgn();
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

void TournamentRunner::handleGameAborted(GameTermination termination,
                                         const QString& title,
                                         const QString& message)
{
    if (!m_active) {
        return;
    }

    if (termination == GameTermination::EngineFailure
        && m_failedEngineSide >= 0) {
        const GameOutcome outcome =
            m_failedEngineSide == 0
            ? GameOutcome::BlackWin
            : GameOutcome::WhiteWin;
        completeCurrentGame({
            outcome,
            GameTermination::EngineFailure,
            tr("%1 The opponent wins by forfeit.").arg(message)
        });
        return;
    }

    const bool wasPaused = m_paused;
    m_active = false;
    m_paused = false;
    m_waitingForNextGame = false;
    m_waitingForHumanGame = false;
    ++m_runGeneration;
    m_status = QStringLiteral("aborted");
    m_finishedAtIso = currentTimestamp();
    m_abortTitle = title;
    m_abortMessage = message;
    if (TournamentGameRecord *current = currentGameRecord()) {
        current->aborted = true;
        current->termination = termination;
        current->finishedAtIso = m_finishedAtIso;
        current->abortTitle = title;
        current->abortMessage = message;
        current->moves = m_gameController->moveHistoryUci();
        current->moveRecords = m_gameController->moveRecords();
    }
    persistReport();
    persistPgn();
    if (wasPaused) {
        emit pauseChanged(false);
    }
    emit tournamentAborted(title, message);
}

MatchConfig TournamentRunner::matchForScheduledGame(
    const TournamentScheduledGame& game) const
{
    MatchConfig match = m_config.match;
    const TournamentParticipant *white =
        participant(game.whiteParticipantId);
    const TournamentParticipant *black =
        participant(game.blackParticipantId);
    if (white) {
        match.player1 = white->player;
    }
    if (black) {
        match.player2 = black->player;
    }
    const OpeningEntry& opening = openingForScheduledGame(game);
    match.game.useOpeningFile = false;
    match.game.openingFilePath.clear();
    match.game.useStartPos = false;
    match.game.startPosition = opening.startFen;
    return match;
}

const OpeningEntry& TournamentRunner::openingForScheduledGame(
    const TournamentScheduledGame& game) const
{
    const int openingIndex =
        qMax(0, game.openingGroup) % m_openings.size();
    return m_openings.at(openingIndex);
}

const TournamentParticipant* TournamentRunner::participant(
    const QString& participantId) const
{
    for (const TournamentParticipant& candidate : m_config.participants) {
        if (candidate.id == participantId) {
            return &candidate;
        }
    }
    return nullptr;
}

TournamentStanding* TournamentRunner::standing(
    const QString& participantId)
{
    for (TournamentStanding& candidate : m_summary.standings) {
        if (candidate.participantId == participantId) {
            return &candidate;
        }
    }
    return nullptr;
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
    persistPgn();
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

void TournamentRunner::persistPgn()
{
    QString error;
    if (writeTournamentPgn(&error) || m_pgnErrorEmitted) {
        return;
    }

    m_pgnErrorEmitted = true;
    emit tournamentReportError(
        tr("Could not write tournament PGN '%1': %2")
            .arg(m_pgnFilePath, error));
}

bool TournamentRunner::writeTournamentPgn(QString* errorOut) const
{
    if (m_pgnFilePath.isEmpty()) {
        return true;
    }

    QVector<PgnGameRecord> games;
    for (const TournamentGameRecord& record : m_gameRecords) {
        if (!record.completed && !record.aborted) {
            continue;
        }

        PgnGameRecord game;
        game.event = QStringLiteral("Xake tournament");
        game.date = pgnDate(record.startedAtIso);
        game.round = QString::number(
            record.roundNumber > 0
                ? record.roundNumber : record.gameNumber);
        game.white = pgnPlayerName(record.match.player1);
        game.black = pgnPlayerName(record.match.player2);
        game.result = record.completed
            ? gameResultNotation(record.result.outcome)
            : QStringLiteral("*");
        game.termination = gameTerminationPgn(record.termination);
        game.startFen = record.startFen;
        game.opening = record.openingName;
        game.timeControl = pgnTimeControl(record.match.game);
        game.movesUci = record.moves;
        game.openingMoveCount =
            static_cast<int>(record.openingMoves.size());
        games.append(game);
    }

    return writePgnFile(games, m_pgnFilePath, errorOut);
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
    tournament["format"] =
        m_config.format == TournamentFormat::Gauntlet
        ? QStringLiteral("gauntlet")
        : QStringLiteral("round_robin");
    tournament["gauntletParticipantId"] =
        m_config.gauntletParticipantId;
    tournament["player1"] = playerToJson(m_config.match.player1);
    tournament["player2"] = playerToJson(m_config.match.player2);
    QJsonArray participants;
    for (const TournamentParticipant& participant : m_config.participants) {
        QJsonObject participantObject;
        participantObject["id"] = participant.id;
        participantObject["player"] = playerToJson(participant.player);
        participants.append(participantObject);
    }
    tournament["participants"] = participants;

    QJsonObject gameSettings;
    gameSettings["timeControl"] = m_config.match.game.timeControl;
    gameSettings["baseTimeSeconds"] = m_config.match.game.baseTimeSeconds;
    gameSettings["incrementSeconds"] = m_config.match.game.incrementSeconds;
    gameSettings["useOpeningFile"] = m_config.match.game.useOpeningFile;
    gameSettings["openingFilePath"] = m_config.match.game.openingFilePath;
    gameSettings["openingCount"] = m_openings.size();
    tournament["game"] = gameSettings;

    QJsonArray games;
    for (const TournamentGameRecord& record : m_gameRecords) {
        games.append(gameRecordToJson(record));
    }

    QJsonObject root;
    root["formatVersion"] = 2;
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
