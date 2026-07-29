#include "game_controller.h"

#include "match_settings_validation.h"
#include "movegen.h"
#include "storage_paths.h"
#include "uci_client.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <cstdlib>

using namespace Xake;

namespace {

constexpr char kStartFen[] =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

SpecialMove promotionMove(PieceType pieceType)
{
    switch (pieceType) {
    case KNIGHT:
        return PROMOTION_KNIGHT;
    case BISHOP:
        return PROMOTION_BISHOP;
    case ROOK:
        return PROMOTION_ROOK;
    case QUEEN:
        return PROMOTION_QUEEN;
    default:
        return NO_SPECIAL;
    }
}

PieceType promotionPieceFromUciChar(char c)
{
    switch (c) {
    case 'q':
    case 'Q':
        return QUEEN;
    case 'r':
    case 'R':
        return ROOK;
    case 'b':
    case 'B':
        return BISHOP;
    case 'n':
    case 'N':
        return KNIGHT;
    default:
        return NO_PIECE_TYPE;
    }
}

Move makeMoveCandidate(int fromSq, int toSq, PieceType promotion)
{
    return make_quiet_move(Square64(fromSq), Square64(toSq), promotionMove(promotion));
}

QString engineSideName(EngineSide side)
{
    return (side == EngineSide::White) ? QStringLiteral("White") : QStringLiteral("Black");
}

} // namespace

GameController::GameController(QObject *parent)
    : QObject(parent)
    , m_whiteSession{new UciClient(this)}
    , m_blackSession{new UciClient(this)}
    , m_flagTimer(new QTimer(this))
{
    if (m_flagTimer) {
        m_flagTimer->setSingleShot(true);
        connect(m_flagTimer, &QTimer::timeout, this, [this]() {
            handleTurnTimeout();
        });
    }

    connect(m_whiteSession.client, &UciClient::uciOk, this, [this]() {
        handleUciOk(EngineSide::White);
    });
    connect(m_whiteSession.client, &UciClient::readyOk, this, [this]() {
        handleReadyOk(EngineSide::White);
    });
    connect(m_whiteSession.client, &UciClient::bestMove, this,
            [this](const QString& move, const QString& ponder) {
        Q_UNUSED(ponder);
        handleBestMove(EngineSide::White, move);
    });
    connect(m_whiteSession.client, &UciClient::standardErrorOutput, this,
            [this](const QString& line) {
        handleEngineStandardError(EngineSide::White, line);
    });
    connect(m_whiteSession.client, &UciClient::processError, this,
            [this](QProcess::ProcessError error, const QString& detail) {
        handleEngineProcessError(EngineSide::White, error, detail);
    });
    connect(m_whiteSession.client, &UciClient::engineExited, this,
            [this](int exitCode, QProcess::ExitStatus status) {
        handleEngineExited(EngineSide::White, exitCode, status);
    });
    connect(m_whiteSession.client, &UciClient::engineOutput, this,
            [this](const QString& line) {
        emit engineOutputReceived(EngineSide::White, line);
    });
    connect(m_whiteSession.client, &UciClient::communication, this,
            [this](const QString& prefix, const QString& line) {
        handleEngineCommunication(EngineSide::White, prefix, line);
    });

    connect(m_blackSession.client, &UciClient::uciOk, this, [this]() {
        handleUciOk(EngineSide::Black);
    });
    connect(m_blackSession.client, &UciClient::readyOk, this, [this]() {
        handleReadyOk(EngineSide::Black);
    });
    connect(m_blackSession.client, &UciClient::bestMove, this,
            [this](const QString& move, const QString& ponder) {
        Q_UNUSED(ponder);
        handleBestMove(EngineSide::Black, move);
    });
    connect(m_blackSession.client, &UciClient::standardErrorOutput, this,
            [this](const QString& line) {
        handleEngineStandardError(EngineSide::Black, line);
    });
    connect(m_blackSession.client, &UciClient::processError, this,
            [this](QProcess::ProcessError error, const QString& detail) {
        handleEngineProcessError(EngineSide::Black, error, detail);
    });
    connect(m_blackSession.client, &UciClient::engineExited, this,
            [this](int exitCode, QProcess::ExitStatus status) {
        handleEngineExited(EngineSide::Black, exitCode, status);
    });
    connect(m_blackSession.client, &UciClient::engineOutput, this,
            [this](const QString& line) {
        emit engineOutputReceived(EngineSide::Black, line);
    });
    connect(m_blackSession.client, &UciClient::communication, this,
            [this](const QString& prefix, const QString& line) {
        handleEngineCommunication(EngineSide::Black, prefix, line);
    });
}

GameController::~GameController()
{
    stopEngines();
    if (m_communicationLogFile.isOpen()) {
        m_communicationLogFile.flush();
        m_communicationLogFile.close();
    }
}

bool GameController::startMatch(const MatchConfig& config,
                                const std::string& fen,
                                const QString& logDir,
                                const QString& logTag,
                                int maxFullMoves,
                                const QStringList& initialMoves)
{
    stopEngines();
    m_uciMoves.clear();
    m_moveHistory.clear();
    m_initialMoveCount = 0;
    m_paused = false;
    if (m_flagTimer) {
        m_flagTimer->stop();
    }

    MatchConfig normalized = config;
    normalizeMatchConfig(normalized);
    const ValidationError error = validateMatchConfig(normalized);
    if (error != ValidationError::None) {
        emit errorOccurred(validationErrorTitle(error),
                           validationErrorMessage(error));
        return false;
    }

    Position position;
    if (!position.set_FEN(fen)) {
        emit errorOccurred(tr("Invalid start position"),
                           tr("Start position is not a valid FEN."));
        return false;
    }

    m_config = normalized;
    m_position = position;
    m_active = true;
    m_baseFen = position.get_FEN();
    m_baseIsStartpos = m_baseFen == kStartFen;

    for (qsizetype ply = 0; ply < initialMoves.size(); ++ply) {
        const Move parsed = moveFromUci(initialMoves.at(ply));
        Move appliedMove = NOMOVE;
        if (!applyMove(parsed, &appliedMove)) {
            m_active = false;
            emit errorOccurred(
                tr("Invalid opening"),
                tr("Opening move %1 could not be applied: %2")
                    .arg(ply + 1)
                    .arg(initialMoves.at(ply)));
            return false;
        }
        m_uciMoves.append(uciFromMove(appliedMove));
    }
    m_initialMoveCount = static_cast<int>(m_uciMoves.size());

    m_timeControlEnabled = (m_config.game.baseTimeSeconds > 0);
    m_whiteTimeMs = static_cast<qint64>(m_config.game.baseTimeSeconds) * 1000;
    m_blackTimeMs = static_cast<qint64>(m_config.game.baseTimeSeconds) * 1000;
    m_incrementMs = static_cast<qint64>(m_config.game.incrementSeconds) * 1000;
    m_timerRunning = false;
    m_logDir = logDir;
    m_logTag = logTag;
    m_maxFullMoves = qMax(0, maxFullMoves);
    prepareCommunicationLog();

    if (m_config.player1.type == PlayerType::Engine) {
        if (!startEngineForPlayer(EngineSide::White,
                                  m_whiteSession,
                                  m_config.player1)) {
            stopEngines();
            m_active = false;
            return false;
        }
    }
    if (m_config.player2.type == PlayerType::Engine) {
        if (!startEngineForPlayer(EngineSide::Black,
                                  m_blackSession,
                                  m_config.player2)) {
            stopEngines();
            m_active = false;
            return false;
        }
    }

    emit matchStarted(m_config);
    emit positionChanged(m_position, NOMOVE);
    if (finishGameIfNoLegalMoves() || finishGameIfDraw()) {
        return true;
    }
    startTurnIfReady();
    return true;
}

void GameController::stopMatch()
{
    if (!m_active) {
        return;
    }
    if (m_flagTimer) {
        m_flagTimer->stop();
    }
    if (m_timerRunning) {
        stopSideTimer(m_timedSide);
    }
    m_active = false;
    stopEngines();
    const bool wasPaused = m_paused;
    m_paused = false;
    if (wasPaused) {
        emit pauseChanged(false);
    }
    emit matchStopped();
}

bool GameController::pauseMatch()
{
    if (!m_active || m_paused) {
        return false;
    }

    m_paused = true;
    stopSideTimer(m_position.get_side_to_move());

    const auto stopSearch = [](EngineSession& session) {
        if (!session.active || !session.searching || !session.client) {
            return;
        }
        session.discardBestMove = true;
        session.client->sendStop();
    };
    stopSearch(m_whiteSession);
    stopSearch(m_blackSession);

    emit pauseChanged(true);
    return true;
}

bool GameController::resumeMatch()
{
    if (!m_active || !m_paused) {
        return false;
    }

    m_paused = false;
    emit pauseChanged(false);
    startTurnIfReady();
    return true;
}

bool GameController::applyHumanMove(Move move)
{
    if (!m_active || m_paused) {
        return false;
    }
    if (finishGameIfTimeExpired()) {
        return false;
    }

    const Color stm = m_position.get_side_to_move();
    const PlayerConfig& player = (stm == WHITE) ? m_config.player1 : m_config.player2;
    if (player.type != PlayerType::Human) {
        return false;
    }

    Move appliedMove = NOMOVE;
    if (!applyMove(move, &appliedMove)) {
        return false;
    }

    afterMoveApplied(appliedMove);
    return true;
}

bool GameController::isActive() const
{
    return m_active;
}

bool GameController::isPaused() const
{
    return m_paused;
}

MatchConfig GameController::matchConfig() const
{
    return m_config;
}

Xake::Position GameController::currentPosition() const
{
    return m_position;
}

QStringList GameController::moveHistoryUci() const
{
    return m_uciMoves;
}

int GameController::initialMoveCount() const
{
    return m_initialMoveCount;
}

QStringList GameController::communicationHistory() const
{
    return m_communicationHistory;
}

QString GameController::communicationLogFilePath() const
{
    return m_communicationLogFile.fileName();
}

void GameController::closeCommunicationLog()
{
    if (m_communicationLogFile.isOpen()) {
        m_communicationLogFile.flush();
        m_communicationLogFile.close();
    }
    m_communicationLogFile.setFileName(QString());
    m_communicationHistory.clear();
    m_communicationLogErrorReported = false;
    emit communicationHistoryReset();
}

bool GameController::clearFinishedSessionData()
{
    if (m_active || !m_position.set_FEN(kStartFen)) {
        return false;
    }

    m_config = MatchConfig{};
    m_uciMoves.clear();
    m_moveHistory.clear();
    m_initialMoveCount = 0;
    m_baseIsStartpos = true;
    m_baseFen = kStartFen;
    m_timeControlEnabled = false;
    m_whiteTimeMs = 0;
    m_blackTimeMs = 0;
    m_incrementMs = 0;
    m_timerRunning = false;
    m_paused = false;
    m_logDir.clear();
    m_logTag.clear();
    m_maxFullMoves = 0;
    return true;
}

bool GameController::timeControlEnabled() const
{
    return m_timeControlEnabled;
}

qint64 GameController::remainingTimeMs(Xake::Color side) const
{
    qint64 remaining = (side == WHITE) ? m_whiteTimeMs : m_blackTimeMs;
    if (m_timeControlEnabled && m_timerRunning && m_timedSide == side) {
        const qint64 elapsed = m_turnTimer.elapsed();
        remaining = qMax<qint64>(0, remaining - elapsed);
    }
    return remaining;
}

bool GameController::startEngineForPlayer(EngineSide side,
                                          EngineSession& session,
                                          const PlayerConfig& player)
{
    if (!session.client) {
        reportEngineFailure(side,
                            EngineFailure::ClientUnavailable,
                            QString(),
                            QString(),
                            0,
                            false);
        return false;
    }

    session.active = false;
    session.uciOk = false;
    session.newGameSent = false;
    session.readyOk = false;
    session.searching = false;
    session.discardBestMove = false;
    session.failureReported = false;
    ++session.handshakeGeneration;
    session.lastErrorLine.clear();

    session.client->disableLogging();

    if (session.client->isRunning()) {
        session.client->sendQuit();
        session.client->stopProcess();
    }

    if (!session.client->start(player.enginePath)) {
        QString detail = player.enginePath;
        const QString processError = session.client->errorString().trimmed();
        if (!processError.isEmpty()) {
            detail += tr(" (%1)").arg(processError);
        }
        reportEngineFailure(side,
                            EngineFailure::StartFailed,
                            detail,
                            QString(),
                            0,
                            false);
        return false;
    }

    session.active = true;
    session.client->sendUci();
    armEngineResponseTimeout(side, EngineFailure::UciHandshakeTimeout);

    return true;
}

void GameController::stopEngines()
{
    if (m_whiteSession.client) {
        m_whiteSession.active = false;
        m_whiteSession.uciOk = false;
        m_whiteSession.newGameSent = false;
        m_whiteSession.readyOk = false;
        m_whiteSession.searching = false;
        m_whiteSession.discardBestMove = false;
        m_whiteSession.failureReported = false;
        ++m_whiteSession.handshakeGeneration;
        m_whiteSession.lastErrorLine.clear();
        m_whiteSession.client->sendStop();
        m_whiteSession.client->sendQuit();
        m_whiteSession.client->stopProcess();
    }
    if (m_blackSession.client) {
        m_blackSession.active = false;
        m_blackSession.uciOk = false;
        m_blackSession.newGameSent = false;
        m_blackSession.readyOk = false;
        m_blackSession.searching = false;
        m_blackSession.discardBestMove = false;
        m_blackSession.failureReported = false;
        ++m_blackSession.handshakeGeneration;
        m_blackSession.lastErrorLine.clear();
        m_blackSession.client->sendStop();
        m_blackSession.client->sendQuit();
        m_blackSession.client->stopProcess();
    }
}

Move GameController::moveFromUci(const QString& move) const
{
    if (move.size() != 4 && move.size() != 5) {
        return NOMOVE;
    }

    const char fromFile = move[0].toLatin1();
    const char fromRank = move[1].toLatin1();
    const char toFile = move[2].toLatin1();
    const char toRank = move[3].toLatin1();
    if (fromFile < 'a' || fromFile > 'h' || toFile < 'a' || toFile > 'h') {
        return NOMOVE;
    }
    if (fromRank < '1' || fromRank > '8' || toRank < '1' || toRank > '8') {
        return NOMOVE;
    }

    const int fromSq = (fromRank - '1') * 8 + (fromFile - 'a');
    const int toSq = (toRank - '1') * 8 + (toFile - 'a');
    const PieceType promotion = (move.size() == 5)
        ? promotionPieceFromUciChar(move[4].toLatin1())
        : NO_PIECE_TYPE;
    if (move.size() == 5 && promotion == NO_PIECE_TYPE) {
        return NOMOVE;
    }

    return makeMoveCandidate(fromSq, toSq, promotion);
}

Move GameController::resolveMoveCandidate(Move move) const
{
    if (move == NOMOVE) {
        return NOMOVE;
    }

    MoveGen::MoveList moveList;
    MoveGen::generate_pseudo_moves(m_position, moveList);

    for (int i = 0; i < moveList.size; ++i) {
        const Move generatedMove = moveList.moves[i];
        if (move_from(generatedMove) != move_from(move)
            || move_to(generatedMove) != move_to(move)
            || promoted_piece(generatedMove) != promoted_piece(move)) {
            continue;
        }

        Position testPosition = m_position;
        if (testPosition.do_move(generatedMove)) {
            return generatedMove;
        }
    }

    return NOMOVE;
}

QString GameController::uciFromMove(Move move) const
{
    if (move == NOMOVE) {
        return QString();
    }

    const int fromSq = move_from(move);
    const int toSq = move_to(move);
    const char fromFile = static_cast<char>('a' + (fromSq % 8));
    const char fromRank = static_cast<char>('1' + (fromSq / 8));
    const char toFile = static_cast<char>('a' + (toSq % 8));
    const char toRank = static_cast<char>('1' + (toSq / 8));

    QString uci;
    uci.reserve(5);
    uci.append(QChar(fromFile));
    uci.append(QChar(fromRank));
    uci.append(QChar(toFile));
    uci.append(QChar(toRank));

    const PieceType promotion = promoted_piece(move);
    if (promotion != NO_PIECE_TYPE) {
        switch (promotion) {
        case QUEEN:
            uci.append('q');
            break;
        case ROOK:
            uci.append('r');
            break;
        case BISHOP:
            uci.append('b');
            break;
        case KNIGHT:
            uci.append('n');
            break;
        default:
            break;
        }
    }

    return uci;
}

bool GameController::applyMove(Move move, Move* appliedMove)
{
    const Move legalMove = resolveMoveCandidate(move);
    if (legalMove == NOMOVE) {
        return false;
    }

    if (!m_position.do_move(legalMove)) {
        return false;
    }

    if (appliedMove) {
        *appliedMove = legalMove;
    }

    return true;
}

void GameController::prepareCommunicationLog()
{
    const bool hasEngine = m_config.player1.type == PlayerType::Engine
        || m_config.player2.type == PlayerType::Engine;
    const bool shouldLog = hasEngine || !m_logDir.trimmed().isEmpty();

    if (!shouldLog) {
        if (m_communicationLogFile.isOpen()) {
            m_communicationLogFile.close();
        }
        if (!m_communicationLogFile.fileName().isEmpty()) {
            m_communicationLogFile.setFileName(QString());
            m_communicationHistory.clear();
            emit communicationHistoryReset();
        }
        return;
    }

    const QString tag = m_logTag.isEmpty()
        ? QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))
        : m_logTag;
    const QString effectiveLogDir = m_logDir.isEmpty()
        ? defaultSessionDir(tag, QStringLiteral("match"))
        : m_logDir;
    const QString logPath = QDir(effectiveLogDir)
        .filePath(QStringLiteral("uci_communication.log"));
    if (m_communicationLogFile.fileName() != logPath) {
        if (m_communicationLogFile.isOpen()) {
            m_communicationLogFile.close();
        }
        m_communicationLogFile.setFileName(logPath);
        m_communicationHistory.clear();
        m_communicationLogErrorReported = false;
        emit communicationHistoryReset();
    }

    if (!QDir().mkpath(effectiveLogDir)) {
        if (!m_communicationLogErrorReported) {
            m_communicationLogErrorReported = true;
            emit communicationLogError(
                tr("Could not create the engine communication log directory: %1")
                    .arg(effectiveLogDir));
        }
        return;
    }

    if (!m_communicationLogFile.isOpen()
        && !m_communicationLogFile.open(QIODevice::WriteOnly
                                        | QIODevice::Append
                                        | QIODevice::Text)) {
        if (!m_communicationLogErrorReported) {
            m_communicationLogErrorReported = true;
            emit communicationLogError(
                tr("Could not open the engine communication log: %1")
                    .arg(m_communicationLogFile.errorString()));
        }
        return;
    }

    writeCommunicationLog(QStringLiteral("Session"),
                          QStringLiteral("##"),
                          QStringLiteral("match %1 started").arg(tag));
}

void GameController::handleEngineCommunication(EngineSide side,
                                               const QString& prefix,
                                               const QString& line)
{
    const QString source = QStringLiteral("%1: %2")
        .arg(engineSideName(side), engineDisplayName(side));
    writeCommunicationLog(source, prefix, line);
}

void GameController::writeCommunicationLog(const QString& source,
                                           const QString& prefix,
                                           const QString& line)
{
    if (!m_communicationLogFile.isOpen()) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString formatted = QStringLiteral("%1 [%2] %3 %4")
        .arg(timestamp, source, prefix, line.trimmed());
    const QByteArray encoded = formatted.toUtf8() + '\n';
    const qint64 written = m_communicationLogFile.write(encoded);
    const bool flushed = m_communicationLogFile.flush();
    if (written != encoded.size() || !flushed) {
        if (!m_communicationLogErrorReported) {
            m_communicationLogErrorReported = true;
            emit communicationLogError(
                tr("Could not write the engine communication log: %1")
                    .arg(m_communicationLogFile.errorString()));
        }
        return;
    }

    m_communicationHistory.append(formatted);
    if (m_communicationHistory.size() > kCommunicationHistoryLimit) {
        m_communicationHistory.removeFirst();
    }
    emit communicationLogged(formatted);
}

QString GameController::engineDisplayName(EngineSide side) const
{
    const PlayerConfig& player = side == EngineSide::White
        ? m_config.player1
        : m_config.player2;
    const QString configuredName = player.name.trimmed();
    if (!configuredName.isEmpty()) {
        return configuredName;
    }

    const QString fileName = QFileInfo(player.enginePath).fileName();
    return fileName.isEmpty() ? tr("Unnamed engine") : fileName;
}

void GameController::handleUciOk(EngineSide side)
{
    EngineSession& session = sessionForSide(side);
    if (!session.active || !session.client) {
        return;
    }
    if (session.uciOk) {
        return;
    }
    session.uciOk = true;
    ++session.handshakeGeneration;
    session.client->sendIsReady();
    armEngineResponseTimeout(side, EngineFailure::ReadyHandshakeTimeout);
}

void GameController::handleReadyOk(EngineSide side)
{
    EngineSession& session = sessionForSide(side);
    if (!session.active || !session.client || !session.uciOk) {
        return;
    }
    if (!session.newGameSent) {
        session.newGameSent = true;
        ++session.handshakeGeneration;
        session.client->sendNewGame();
        session.client->sendIsReady();
        armEngineResponseTimeout(side, EngineFailure::ReadyHandshakeTimeout);
        return;
    }
    if (session.readyOk) {
        return;
    }
    session.readyOk = true;
    ++session.handshakeGeneration;

    const Color engineColor = side == EngineSide::White ? WHITE : BLACK;
    if (m_position.get_side_to_move() == engineColor) {
        startTurnIfReady();
    }
}

void GameController::handleEngineStandardError(EngineSide side,
                                               const QString& line)
{
    EngineSession& session = sessionForSide(side);
    if (!session.active) {
        return;
    }

    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
        session.lastErrorLine = trimmed;
        emit engineOutputReceived(
            side, tr("[stderr] %1").arg(trimmed));
    }
}

void GameController::handleEngineProcessError(EngineSide side,
                                              QProcess::ProcessError error,
                                              const QString& detail)
{
    EngineSession& session = sessionForSide(side);
    if (!session.active) {
        return;
    }

    EngineFailure failure = EngineFailure::UnknownProcessError;
    switch (error) {
    case QProcess::FailedToStart:
        failure = EngineFailure::StartFailed;
        break;
    case QProcess::Crashed:
        session.lastErrorLine = detail.trimmed();
        return;
    case QProcess::Timedout:
        failure = EngineFailure::ProcessTimeout;
        break;
    case QProcess::WriteError:
        failure = EngineFailure::WriteError;
        break;
    case QProcess::ReadError:
        failure = EngineFailure::ReadError;
        break;
    case QProcess::UnknownError:
        failure = EngineFailure::UnknownProcessError;
        break;
    }

    reportEngineFailure(side, failure, detail);
}

void GameController::handleEngineExited(EngineSide side, int exitCode, QProcess::ExitStatus status)
{
    EngineSession& session = sessionForSide(side);
    if (!session.active) {
        return;
    }

    if (!m_active) {
        session.active = false;
        session.lastErrorLine.clear();
        return;
    }

    const EngineFailure failure = status == QProcess::CrashExit
        ? EngineFailure::ProcessCrashed
        : EngineFailure::UnexpectedExit;
    reportEngineFailure(side,
                        failure,
                        session.lastErrorLine,
                        QString(),
                        exitCode);
}

void GameController::handleBestMove(EngineSide side, const QString& move)
{
    if (!m_active) {
        return;
    }

    EngineSession& session = sessionForSide(side);
    if (!session.active || !session.searching) {
        return;
    }
    session.searching = false;

    if (session.discardBestMove) {
        session.discardBestMove = false;
        if (!m_paused) {
            startTurnIfReady();
        }
        return;
    }
    if (m_paused) {
        return;
    }

    const Color engineColor = side == EngineSide::White ? WHITE : BLACK;
    if (m_position.get_side_to_move() != engineColor) {
        reportEngineFailure(side,
                            EngineFailure::UnexpectedBestMove,
                            QString(),
                            move);
        return;
    }
    if (finishGameIfTimeExpired()) {
        return;
    }
    const QString normalizedMove = move.trimmed();
    if (normalizedMove.isEmpty()) {
        reportEngineFailure(side, EngineFailure::EmptyBestMove);
        return;
    }
    if (normalizedMove == QStringLiteral("0000")) {
        reportEngineFailure(side,
                            EngineFailure::NoMoveBestMove,
                            QString(),
                            normalizedMove);
        return;
    }

    const Move parsed = moveFromUci(normalizedMove);
    if (parsed == NOMOVE) {
        reportEngineFailure(side,
                            EngineFailure::MalformedBestMove,
                            QString(),
                            normalizedMove);
        return;
    }

    Move appliedMove = NOMOVE;
    if (!applyMove(parsed, &appliedMove)) {
        reportEngineFailure(side,
                            EngineFailure::IllegalBestMove,
                            QString(),
                            normalizedMove);
        return;
    }
    afterMoveApplied(appliedMove);
}

void GameController::armEngineResponseTimeout(EngineSide side,
                                              EngineFailure failure)
{
    EngineSession& session = sessionForSide(side);
    const quint64 generation = ++session.handshakeGeneration;
    const int timeoutMs = qMax(1, m_engineResponseTimeoutMs);
    QTimer::singleShot(timeoutMs, this, [this, side, failure, generation]() {
        EngineSession& pendingSession = sessionForSide(side);
        if (!m_active
            || !pendingSession.active
            || pendingSession.handshakeGeneration != generation) {
            return;
        }
        if (failure == EngineFailure::UciHandshakeTimeout
            && pendingSession.uciOk) {
            return;
        }
        if (failure == EngineFailure::ReadyHandshakeTimeout
            && pendingSession.readyOk) {
            return;
        }
        reportEngineFailure(side, failure);
    });
}

void GameController::reportEngineFailure(EngineSide side,
                                         EngineFailure failure,
                                         const QString& detail,
                                         const QString& move,
                                         int exitCode,
                                         bool abortGame)
{
    EngineSession& session = sessionForSide(side);
    if (session.failureReported) {
        return;
    }
    session.failureReported = true;
    ++session.handshakeGeneration;

    const QString message =
        engineFailureMessage(side, failure, detail, move, exitCode);
    emit engineFailureOccurred(failure, side, message);

    if (abortGame && m_active) {
        emit gameAborted(tr("Engine error"), message);
        stopMatch();
    }
    emit errorOccurred(tr("Engine error"), message);
}

QString GameController::engineFailureMessage(EngineSide side,
                                             EngineFailure failure,
                                             const QString& detail,
                                             const QString& move,
                                             int exitCode) const
{
    const QString engine = tr("%1 engine (%2)")
        .arg(engineSideName(side), engineDisplayName(side));
    const QString cleanDetail = detail.trimmed();
    const QString detailSuffix = cleanDetail.isEmpty()
        ? QString()
        : tr(" Details: %1").arg(cleanDetail);

    switch (failure) {
    case EngineFailure::ClientUnavailable:
        return tr("%1 client is not available.").arg(engine);
    case EngineFailure::StartFailed:
        return tr("%1 could not be started: %2.")
            .arg(engine, cleanDetail.isEmpty() ? tr("unknown reason") : cleanDetail);
    case EngineFailure::UciHandshakeTimeout:
        return tr("%1 did not answer \"uci\" with \"uciok\" within the timeout.")
            .arg(engine);
    case EngineFailure::ReadyHandshakeTimeout:
        return tr("%1 did not answer \"isready\" with \"readyok\" within the timeout.")
            .arg(engine);
    case EngineFailure::ProcessCrashed:
        return tr("%1 process crashed (exit code %2).%3")
            .arg(engine)
            .arg(exitCode)
            .arg(detailSuffix);
    case EngineFailure::UnexpectedExit:
        return tr("%1 exited unexpectedly (exit code %2).%3")
            .arg(engine)
            .arg(exitCode)
            .arg(detailSuffix);
    case EngineFailure::ReadError:
        return tr("Could not read data from %1.%2").arg(engine, detailSuffix);
    case EngineFailure::WriteError:
        return tr("Could not send data to %1.%2").arg(engine, detailSuffix);
    case EngineFailure::ProcessTimeout:
        return tr("Communication with %1 timed out.%2").arg(engine, detailSuffix);
    case EngineFailure::UnknownProcessError:
        return tr("An unknown process error occurred for %1.%2")
            .arg(engine, detailSuffix);
    case EngineFailure::EmptyBestMove:
        return tr("%1 returned an empty bestmove response.").arg(engine);
    case EngineFailure::NoMoveBestMove:
        return tr("%1 returned \"bestmove 0000\" although legal moves are available.")
            .arg(engine);
    case EngineFailure::MalformedBestMove:
        return tr("%1 returned malformed bestmove \"%2\". Expected UCI coordinate notation.")
            .arg(engine, move);
    case EngineFailure::IllegalBestMove:
        return tr("%1 returned illegal bestmove \"%2\" for the current position.")
            .arg(engine, move);
    case EngineFailure::UnexpectedBestMove:
        return tr("%1 returned bestmove \"%2\" when it was not that engine's turn.")
            .arg(engine, move);
    }

    return tr("An unknown engine error occurred for %1.").arg(engine);
}

void GameController::sendPositionToEngine(EngineSession& session)
{
    if (!session.client || !session.readyOk || session.searching) {
        return;
    }
    if (m_baseIsStartpos) {
        session.client->sendPositionStartpos(m_uciMoves);
    } else {
        session.client->sendPositionFen(QString::fromStdString(m_baseFen), m_uciMoves);
    }
}

void GameController::sendGoForSide(EngineSide side)
{
    EngineSession& session = sessionForSide(side);
    if (m_paused
        || !session.active
        || !session.readyOk
        || session.searching
        || !session.client) {
        return;
    }
    if ((side == EngineSide::White && m_position.get_side_to_move() != WHITE)
        || (side == EngineSide::Black && m_position.get_side_to_move() != BLACK)) {
        return;
    }

    session.searching = true;
    emit engineSearchStarted(side);
    if (m_timeControlEnabled) {
        const int wtime = static_cast<int>(qMax<qint64>(0, m_whiteTimeMs));
        const int btime = static_cast<int>(qMax<qint64>(0, m_blackTimeMs));
        const int incMs = static_cast<int>(qMax<qint64>(0, m_incrementMs));
        session.client->sendGoWtimeBtime(wtime, btime, incMs, incMs);
    } else {
        session.client->sendGoInfinite();
    }

    startSideTimer(m_position.get_side_to_move());
}

EngineSession& GameController::sessionForSide(EngineSide side)
{
    return (side == EngineSide::White) ? m_whiteSession : m_blackSession;
}

void GameController::afterMoveApplied(Move move)
{
    const QString uci = uciFromMove(move);
    if (uci.isEmpty()) {
        return;
    }
    const Color movedSide = (m_position.get_side_to_move() == WHITE) ? BLACK : WHITE;
    stopSideTimer(movedSide);
    if (m_timeControlEnabled) {
        if (movedSide == WHITE) {
            m_whiteTimeMs += m_incrementMs;
        } else {
            m_blackTimeMs += m_incrementMs;
        }
    }

    m_uciMoves.append(uci.toLower());
    m_moveHistory.append(move);
    emit movePlayed(m_uciMoves.size(), m_uciMoves.constLast());
    emit positionChanged(m_position, move);

    if (finishGameIfNoLegalMoves()) {
        return;
    }
    if (finishGameIfDraw()) {
        return;
    }
    if (finishGameIfMoveLimit()) {
        return;
    }

    startTurnIfReady();
}

bool GameController::finishGameIfTimeExpired()
{
    if (!m_active || !m_timeControlEnabled || !m_timerRunning) {
        return false;
    }
    if (remainingTimeMs(m_timedSide) > 0) {
        return false;
    }

    return finishGameOnTime(m_timedSide);
}

bool GameController::finishGameOnTime(Color flaggedSide)
{
    stopSideTimer(flaggedSide);
    if (flaggedSide == WHITE) {
        m_whiteTimeMs = 0;
    } else {
        m_blackTimeMs = 0;
    }

    const Color winner = ~flaggedSide;
    if (!m_position.has_mating_material(winner)) {
        return finishGameAsDraw(
            GameTermination::TimeForfeit,
            tr("Draw on time: %1 has no mating material.")
                .arg(winner == WHITE ? tr("White") : tr("Black")));
    }

    return finishGame(winner == WHITE ? GameOutcome::WhiteWin : GameOutcome::BlackWin,
                      GameTermination::TimeForfeit,
                      tr("Time"),
                      tr("%1 wins on time.")
                          .arg(winner == WHITE ? tr("White") : tr("Black")));
}

bool GameController::finishGameIfDraw()
{
    if (m_position.get_fifty_moves_counter() >= 100) {
        return finishGameAsDraw(GameTermination::FiftyMoveRule,
                                tr("Draw by fifty-move rule."));
    }

    if (m_position.has_threefold_repetition()) {
        return finishGameAsDraw(GameTermination::ThreefoldRepetition,
                                tr("Draw by repetition."));
    }

    if (m_position.has_insufficient_material()) {
        return finishGameAsDraw(GameTermination::InsufficientMaterial,
                                tr("Draw by insufficient material."));
    }

    return false;
}

bool GameController::finishGameIfMoveLimit()
{
    if (m_maxFullMoves <= 0
        || m_moveHistory.size() < static_cast<qsizetype>(m_maxFullMoves) * 2) {
        return false;
    }

    return finishGameAsDraw(GameTermination::MoveLimit,
                            tr("Draw by tournament move limit."));
}

bool GameController::finishGameAsDraw(GameTermination termination, const QString& message)
{
    return finishGame(GameOutcome::Draw, termination, tr("Draw"), message);
}

bool GameController::finishGame(GameOutcome outcome,
                                GameTermination termination,
                                const QString& title,
                                const QString& message)
{
    emit gameFinished(GameResult{outcome, termination, message});
    stopMatch();
    emit errorOccurred(title, message);
    return true;
}

bool GameController::finishGameIfNoLegalMoves()
{
    const Color sideToMove = m_position.get_side_to_move();

    MoveGen::MoveList moveList;
    MoveGen::generate_pseudo_moves(m_position, moveList);
    for (int i = 0; i < moveList.size; ++i) {
        Position testPosition = m_position;
        if (testPosition.do_move(moveList.moves[i])) {
            return false;
        }
    }

    const Bitboard kingBitboard = m_position.get_pieceTypes_bitboard(sideToMove, KING);
    if (!kingBitboard) {
        const QString message = tr("Position has no king for the side to move.");
        emit gameAborted(tr("Invalid position"), message);
        stopMatch();
        emit errorOccurred(tr("Invalid position"), message);
        return true;
    }

    const Square64 kingSquare{Bitboards::ctz(kingBitboard)};
    const bool inCheck = m_position.square_is_attacked_bySide(kingSquare, ~sideToMove);
    if (inCheck) {
        const Color winner = ~sideToMove;
        return finishGame(winner == WHITE ? GameOutcome::WhiteWin : GameOutcome::BlackWin,
                          GameTermination::Checkmate,
                          tr("Checkmate"),
                          tr("%1 wins by checkmate.")
                              .arg(winner == WHITE ? tr("White") : tr("Black")));
    }

    return finishGameAsDraw(GameTermination::Stalemate, tr("Stalemate."));
}

void GameController::startSideTimer(Color side)
{
    if (!m_timeControlEnabled || m_paused) {
        return;
    }

    if (m_flagTimer) {
        m_flagTimer->stop();
    }

    const qint64 remaining = (side == WHITE) ? m_whiteTimeMs : m_blackTimeMs;
    m_timedSide = side;
    m_turnTimer.restart();
    m_timerRunning = true;
    if (remaining <= 0) {
        finishGameOnTime(side);
        return;
    }
    if (m_flagTimer) {
        m_flagTimer->start(static_cast<int>(remaining));
    }
}

void GameController::stopSideTimer(Color side)
{
    if (!m_timeControlEnabled || !m_timerRunning) {
        return;
    }
    if (m_timedSide != side) {
        return;
    }
    if (m_flagTimer) {
        m_flagTimer->stop();
    }

    const qint64 elapsed = m_turnTimer.elapsed();
    if (side == WHITE) {
        m_whiteTimeMs = qMax<qint64>(0, m_whiteTimeMs - elapsed);
    } else {
        m_blackTimeMs = qMax<qint64>(0, m_blackTimeMs - elapsed);
    }
    m_timerRunning = false;
}

void GameController::handleTurnTimeout()
{
    finishGameIfTimeExpired();
}

void GameController::startTurnIfReady()
{
    if (!m_active || m_paused) {
        return;
    }

    const Color sideToMove = m_position.get_side_to_move();
    if (sideToMove == WHITE) {
        if (m_config.player1.type == PlayerType::Engine) {
            if (m_whiteSession.readyOk) {
                sendPositionToEngine(m_whiteSession);
                sendGoForSide(EngineSide::White);
            }
        } else {
            startSideTimer(WHITE);
        }
    } else {
        if (m_config.player2.type == PlayerType::Engine) {
            if (m_blackSession.readyOk) {
                sendPositionToEngine(m_blackSession);
                sendGoForSide(EngineSide::Black);
            }
        } else {
            startSideTimer(BLACK);
        }
    }
}
