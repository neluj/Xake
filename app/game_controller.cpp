#include "game_controller.h"

#include "match_settings_validation.h"
#include "movegen.h"
#include "uci_client.h"

#include <QDateTime>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <cstdlib>

using namespace ChessGame;

namespace {

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
    connect(m_whiteSession.client, &UciClient::engineError, this,
            [this](const QString& line) {
        handleEngineError(EngineSide::White, line);
    });
    connect(m_whiteSession.client, &UciClient::engineExited, this,
            [this](int exitCode, QProcess::ExitStatus status) {
        handleEngineExited(EngineSide::White, exitCode, status);
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
    connect(m_blackSession.client, &UciClient::engineError, this,
            [this](const QString& line) {
        handleEngineError(EngineSide::Black, line);
    });
    connect(m_blackSession.client, &UciClient::engineExited, this,
            [this](int exitCode, QProcess::ExitStatus status) {
        handleEngineExited(EngineSide::Black, exitCode, status);
    });
}

bool GameController::startMatch(const MatchConfig& config,
                                const std::string& fen,
                                const QString& logDir,
                                const QString& logTag)
{
    stopEngines();
    m_uciMoves.clear();
    m_moveHistory.clear();
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
    m_baseFen = fen;
    const QString startToken = m_config.game.startPosition.trimmed();
    m_baseIsStartpos = m_config.game.useStartPos
        || startToken.compare(QStringLiteral("startpos"), Qt::CaseInsensitive) == 0;
    m_timeControlEnabled = (m_config.game.baseTimeSeconds > 0);
    m_whiteTimeMs = static_cast<qint64>(m_config.game.baseTimeSeconds) * 1000;
    m_blackTimeMs = static_cast<qint64>(m_config.game.baseTimeSeconds) * 1000;
    m_incrementMs = static_cast<qint64>(m_config.game.incrementSeconds) * 1000;
    m_timerRunning = false;
    m_logDir = logDir;
    m_logTag = logTag;

    if (m_config.player1.type == PlayerType::Engine) {
        if (!startEngineForPlayer(m_whiteSession, m_config.player1, EngineSide::White)) {
            stopEngines();
            m_active = false;
            return false;
        }
    }
    if (m_config.player2.type == PlayerType::Engine) {
        if (!startEngineForPlayer(m_blackSession, m_config.player2, EngineSide::Black)) {
            stopEngines();
            m_active = false;
            return false;
        }
    }

    emit matchStarted(m_config);
    emit positionChanged(m_position);
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
    m_active = false;
    stopEngines();
    m_timerRunning = false;
    emit matchStopped();
}

bool GameController::applyHumanMove(Move move)
{
    if (!m_active) {
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
        emit errorOccurred(tr("Invalid move"), tr("Move could not be applied."));
        return false;
    }

    afterMoveApplied(appliedMove);
    return true;
}

bool GameController::isActive() const
{
    return m_active;
}

MatchConfig GameController::matchConfig() const
{
    return m_config;
}

ChessGame::Position GameController::currentPosition() const
{
    return m_position;
}

bool GameController::timeControlEnabled() const
{
    return m_timeControlEnabled;
}

qint64 GameController::remainingTimeMs(ChessGame::Color side) const
{
    qint64 remaining = (side == WHITE) ? m_whiteTimeMs : m_blackTimeMs;
    if (m_timeControlEnabled && m_timerRunning && m_timedSide == side) {
        const qint64 elapsed = m_turnTimer.elapsed();
        remaining = qMax<qint64>(0, remaining - elapsed);
    }
    return remaining;
}

bool GameController::startEngineForPlayer(EngineSession& session,
                                          const PlayerConfig& player,
                                          EngineSide side)
{
    if (!session.client) {
        emit errorOccurred(tr("Engine error"), tr("Engine client not available."));
        return false;
    }

    session.active = false;
    session.uciOk = false;
    session.readyOk = false;
    session.lastErrorLine.clear();

    const QString effectiveLogDir = m_logDir.isEmpty()
        ? QDir::current().filePath("logs")
        : m_logDir;
    QDir().mkpath(effectiveLogDir);
    const QString effectiveTag = m_logTag.isEmpty()
        ? QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")
        : m_logTag;
    const QString sideName = (side == EngineSide::White) ? QStringLiteral("white")
                                                         : QStringLiteral("black");
    const QString logFile = QDir(effectiveLogDir).filePath(
        QString("uci_%1_%2.log").arg(sideName, effectiveTag));
    session.client->setLogFilePath(logFile);

    if (session.client->isRunning()) {
        session.client->sendQuit();
        session.client->stopProcess();
    }

    if (!session.client->start(player.enginePath)) {
        emit errorOccurred(tr("Engine error"),
                           tr("Failed to start engine: %1").arg(player.enginePath));
        return false;
    }

    session.active = true;
    session.client->sendUci();

    return true;
}

void GameController::stopEngines()
{
    if (m_whiteSession.client) {
        m_whiteSession.active = false;
        m_whiteSession.uciOk = false;
        m_whiteSession.readyOk = false;
        m_whiteSession.lastErrorLine.clear();
        m_whiteSession.client->sendStop();
        m_whiteSession.client->sendQuit();
        m_whiteSession.client->stopProcess();
    }
    if (m_blackSession.client) {
        m_blackSession.active = false;
        m_blackSession.uciOk = false;
        m_blackSession.readyOk = false;
        m_blackSession.lastErrorLine.clear();
        m_blackSession.client->sendStop();
        m_blackSession.client->sendQuit();
        m_blackSession.client->stopProcess();
    }
}

Move GameController::moveFromUci(const QString& move) const
{
    if (move.size() < 4) {
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
    const PieceType promotion = (move.size() >= 5)
        ? promotionPieceFromUciChar(move[4].toLatin1())
        : NO_PIECE_TYPE;
    if (move.size() >= 5 && promotion == NO_PIECE_TYPE) {
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
    session.client->sendIsReady();
}

void GameController::handleReadyOk(EngineSide side)
{
    EngineSession& session = sessionForSide(side);
    if (!session.active || !session.client) {
        return;
    }
    if (session.readyOk) {
        return;
    }
    session.readyOk = true;
    session.client->sendNewGame();
    startTurnIfReady();
}

void GameController::handleEngineError(EngineSide side, const QString& line)
{
    EngineSession& session = sessionForSide(side);
    if (!session.active) {
        return;
    }

    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
        session.lastErrorLine = trimmed;
    }
}

void GameController::handleEngineExited(EngineSide side, int exitCode, QProcess::ExitStatus status)
{
    EngineSession& session = sessionForSide(side);
    if (!session.active) {
        return;
    }

    session.active = false;
    session.uciOk = false;
    session.readyOk = false;

    if (!m_active) {
        session.lastErrorLine.clear();
        return;
    }

    QString detail;
    if (!session.lastErrorLine.isEmpty()) {
        detail = tr(" Last error: %1").arg(session.lastErrorLine);
    }

    const QString sideName = engineSideName(side);
    const QString statusText = (status == QProcess::CrashExit)
        ? tr("crashed")
        : tr("exited");
    emit errorOccurred(
        tr("Engine error"),
        tr("%1 engine %2 (code %3).%4")
            .arg(sideName)
            .arg(statusText)
            .arg(exitCode)
            .arg(detail));
    session.lastErrorLine.clear();
    stopMatch();
}

void GameController::handleBestMove(EngineSide side, const QString& move)
{
    Q_UNUSED(side);

    if (!m_active) {
        return;
    }
    if (finishGameIfTimeExpired()) {
        return;
    }
    if (move.isEmpty() || move == "0000") {
        return;
    }

    const Move parsed = moveFromUci(move);
    Move appliedMove = NOMOVE;
    if (!applyMove(parsed, &appliedMove)) {
        emit errorOccurred(tr("Engine error"), tr("Invalid bestmove: %1").arg(move));
        return;
    }
    afterMoveApplied(appliedMove);
}

void GameController::sendPositionToEngine(EngineSession& session)
{
    if (!session.client || !session.readyOk) {
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
    if (!session.active || !session.readyOk || !session.client) {
        return;
    }
    if ((side == EngineSide::White && m_position.get_side_to_move() != WHITE)
        || (side == EngineSide::Black && m_position.get_side_to_move() != BLACK)) {
        return;
    }

    if (m_timeControlEnabled) {
        const int wtime = static_cast<int>(qMax<qint64>(0, m_whiteTimeMs));
        const int btime = static_cast<int>(qMax<qint64>(0, m_blackTimeMs));
        const int incMs = static_cast<int>(qMax<qint64>(0, m_incrementMs));
        session.client->sendGoWtimeBtime(wtime, btime, incMs, incMs, m_config.game.movesToGo);
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
    emit positionChanged(m_position);

    if (finishGameIfNoLegalMoves()) {
        return;
    }
    if (finishGameIfDraw()) {
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
    emit errorOccurred(tr("Time"),
                       tr("%1 wins on time.")
                           .arg(winner == WHITE ? tr("White") : tr("Black")));
    stopMatch();
    return true;
}

bool GameController::finishGameIfDraw()
{
    if (m_position.get_fifty_moves_counter() >= 100) {
        return finishGameAsDraw(tr("Draw by fifty-move rule."));
    }

    if (m_position.has_threefold_repetition()) {
        return finishGameAsDraw(tr("Draw by repetition."));
    }

    if (m_position.has_insufficient_material()) {
        return finishGameAsDraw(tr("Draw by insufficient material."));
    }

    return false;
}

bool GameController::finishGameAsDraw(const QString& message)
{
    emit errorOccurred(tr("Draw"), message);
    stopMatch();
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
        stopMatch();
        return true;
    }

    const Square64 kingSquare{Bitboards::ctz(kingBitboard)};
    const bool inCheck = m_position.square_is_attacked_bySide(kingSquare, ~sideToMove);
    if (inCheck) {
        const Color winner = ~sideToMove;
        emit errorOccurred(tr("Checkmate"),
                           tr("%1 wins by checkmate.")
                               .arg(winner == WHITE ? tr("White") : tr("Black")));
    } else {
        emit errorOccurred(tr("Draw"), tr("Stalemate."));
    }

    stopMatch();
    return true;
}

void GameController::startSideTimer(Color side)
{
    if (!m_timeControlEnabled) {
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
