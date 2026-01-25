#include "game_controller.h"

#include "fen.h"
#include "match_settings_validation.h"
#include "uci_client.h"

#include <QDateTime>
#include <QDir>
#include <QString>
#include <QStringList>
#include <cstdlib>

GameController::GameController(QObject *parent)
    : QObject(parent)
    , m_whiteSession{new UciClient(this)}
    , m_blackSession{new UciClient(this)}
{
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
}

bool GameController::startMatch(const MatchConfig& config,
                                const std::string& fen,
                                const QString& logDir,
                                const QString& logTag)
{
    stopEngines();
    m_uciMoves.clear();
    m_moveHistory.clear();

    MatchConfig normalized = config;
    normalizeMatchConfig(normalized);
    const ValidationError error = validateMatchConfig(normalized);
    if (error != ValidationError::None) {
        emit errorOccurred(validationErrorTitle(error),
                           validationErrorMessage(error));
        return false;
    }

    Position position;
    if (!setFromFen(position, fen)) {
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
    startTurnIfReady();
    return true;
}

void GameController::stopMatch()
{
    if (!m_active) {
        return;
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

    const Color stm = m_position.stm;
    const PlayerConfig& player = (stm == WHITE) ? m_config.player1 : m_config.player2;
    if (player.type != PlayerType::Human) {
        return false;
    }

    const int fromSq = move_from(move);
    const int toSq = move_to(move);
    Color fromColor = WHITE;
    const Piece fromPiece = m_position.pieceAt(fromSq, fromColor);
    if (fromPiece == NO_PIECE || fromColor != stm) {
        return false;
    }

    if (!applyMove(move)) {
        emit errorOccurred(tr("Invalid move"), tr("Move could not be applied."));
        return false;
    }

    afterMoveApplied(move);
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

Position GameController::currentPosition() const
{
    return m_position;
}

bool GameController::timeControlEnabled() const
{
    return m_timeControlEnabled;
}

qint64 GameController::remainingTimeMs(Color side) const
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

    session.active = true;
    session.uciOk = false;
    session.readyOk = false;

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

    session.client->sendUci();

    return true;
}

void GameController::stopEngines()
{
    if (m_whiteSession.client) {
        m_whiteSession.client->sendStop();
        m_whiteSession.client->sendQuit();
        m_whiteSession.client->stopProcess();
        m_whiteSession.active = false;
        m_whiteSession.uciOk = false;
        m_whiteSession.readyOk = false;
    }
    if (m_blackSession.client) {
        m_blackSession.client->sendStop();
        m_blackSession.client->sendQuit();
        m_blackSession.client->stopProcess();
        m_blackSession.active = false;
        m_blackSession.uciOk = false;
        m_blackSession.readyOk = false;
    }
}

Move GameController::moveFromUci(const QString& move) const
{
    if (move.size() < 4) {
        return MOVE_NONE;
    }

    const char fromFile = move[0].toLatin1();
    const char fromRank = move[1].toLatin1();
    const char toFile = move[2].toLatin1();
    const char toRank = move[3].toLatin1();
    if (fromFile < 'a' || fromFile > 'h' || toFile < 'a' || toFile > 'h') {
        return MOVE_NONE;
    }
    if (fromRank < '1' || fromRank > '8' || toRank < '1' || toRank > '8') {
        return MOVE_NONE;
    }

    const int fromSq = (fromRank - '1') * 8 + (fromFile - 'a');
    const int toSq = (toRank - '1') * 8 + (toFile - 'a');

    Color fromColor = WHITE;
    const Piece fromPiece = m_position.pieceAt(fromSq, fromColor);
    if (fromPiece == NO_PIECE) {
        return MOVE_NONE;
    }

    Color toColor = WHITE;
    const Piece toPiece = m_position.pieceAt(toSq, toColor);
    if (toPiece != NO_PIECE && toColor == fromColor) {
        return MOVE_NONE;
    }

    Piece promoPiece = NO_PIECE;
    if (move.size() >= 5) {
        switch (move[4].toLatin1()) {
        case 'q':
        case 'Q':
            promoPiece = QUEEN;
            break;
        case 'r':
        case 'R':
            promoPiece = ROOK;
            break;
        case 'b':
        case 'B':
            promoPiece = BISHOP;
            break;
        case 'n':
        case 'N':
            promoPiece = KNIGHT;
            break;
        default:
            promoPiece = NO_PIECE;
            break;
        }
    }

    int flags = MOVE_FLAG_NONE;
    Piece capturePiece = toPiece;
    if (toPiece != NO_PIECE) {
        flags |= MOVE_FLAG_CAPTURE;
    }
    if (promoPiece != NO_PIECE) {
        flags |= MOVE_FLAG_PROMOTION;
    }
    if (fromPiece == KING && std::abs(toSq - fromSq) == 2) {
        flags |= MOVE_FLAG_CASTLE;
    }
    if (fromPiece == PAWN && std::abs(toSq - fromSq) == 16) {
        flags |= MOVE_FLAG_DOUBLE;
    }
    if (fromPiece == PAWN && toPiece == NO_PIECE
        && (fromSq % 8) != (toSq % 8) && m_position.epSquare == toSq) {
        flags |= MOVE_FLAG_ENPASSANT;
        capturePiece = PAWN;
        flags |= MOVE_FLAG_CAPTURE;
    }

    return make_move(fromSq,
                     toSq,
                     move_piece_code(fromPiece),
                     move_piece_code(capturePiece),
                     move_piece_code(promoPiece),
                     flags);
}

QString GameController::uciFromMove(Move move) const
{
    if (move == MOVE_NONE) {
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

    const Piece promoPiece = move_piece_from_code(move_promo(move));
    if (promoPiece != NO_PIECE) {
        switch (promoPiece) {
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

bool GameController::applyMove(Move move)
{
    if (move == MOVE_NONE) {
        return false;
    }

    const int fromSq = move_from(move);
    const int toSq = move_to(move);

    Color fromColor = WHITE;
    const Piece fromPiece = m_position.pieceAt(fromSq, fromColor);
    if (fromPiece == NO_PIECE) {
        return false;
    }

    Color toColor = WHITE;
    const Piece toPiece = m_position.pieceAt(toSq, toColor);
    if (toPiece != NO_PIECE && toColor == fromColor) {
        return false;
    }

    const int flags = move_flags(move);
    bool isCapture = (toPiece != NO_PIECE);
    bool isCastle = (flags & MOVE_FLAG_CASTLE) != 0;
    bool isEnPassant = (flags & MOVE_FLAG_ENPASSANT) != 0;
    if (!isCastle && fromPiece == KING && std::abs(toSq - fromSq) == 2) {
        isCastle = true;
    }
    if (!isEnPassant && fromPiece == PAWN && toPiece == NO_PIECE
        && (fromSq % 8) != (toSq % 8) && m_position.epSquare == toSq) {
        isEnPassant = true;
    }

    int epCaptureSq = -1;
    if (isEnPassant) {
        if (m_position.epSquare != toSq) {
            return false;
        }
        epCaptureSq = (fromColor == WHITE) ? (toSq - 8) : (toSq + 8);
        Color capColor = WHITE;
        const Piece capPiece = m_position.pieceAt(epCaptureSq, capColor);
        if (capPiece != PAWN || capColor == fromColor) {
            return false;
        }
        isCapture = true;
    }

    if (!m_position.movePiece(fromSq, toSq)) {
        return false;
    }

    if (isCastle) {
        int rookFrom = -1;
        int rookTo = -1;
        if (fromColor == WHITE && fromSq == 4) {
            if (toSq == 6) {
                rookFrom = 7;
                rookTo = 5;
            } else if (toSq == 2) {
                rookFrom = 0;
                rookTo = 3;
            }
        } else if (fromColor == BLACK && fromSq == 60) {
            if (toSq == 62) {
                rookFrom = 63;
                rookTo = 61;
            } else if (toSq == 58) {
                rookFrom = 56;
                rookTo = 59;
            }
        }

        if (rookFrom == -1 || rookTo == -1) {
            return false;
        }

        Color rookColor = WHITE;
        const Piece rookPiece = m_position.pieceAt(rookFrom, rookColor);
        if (rookPiece != ROOK || rookColor != fromColor) {
            return false;
        }

        if (!m_position.movePiece(rookFrom, rookTo)) {
            return false;
        }
    }

    if (isEnPassant && epCaptureSq >= 0) {
        const Bitboard capMask = bb_of(epCaptureSq);
        const Color capColor = (fromColor == WHITE) ? BLACK : WHITE;
        m_position.bb[capColor][PAWN] &= ~capMask;
        m_position.recomputeOcc();
    }

    const Piece promoPiece = move_piece_from_code(move_promo(move));
    if (promoPiece != NO_PIECE && fromPiece == PAWN) {
        const Bitboard toMask = bb_of(toSq);
        m_position.bb[fromColor][PAWN] &= ~toMask;
        m_position.bb[fromColor][promoPiece] |= toMask;
        m_position.recomputeOcc();
    }

    if (fromPiece == KING) {
        if (fromColor == WHITE) {
            m_position.castling &= ~(CASTLE_WK | CASTLE_WQ);
        } else {
            m_position.castling &= ~(CASTLE_BK | CASTLE_BQ);
        }
    }
    if (fromPiece == ROOK) {
        if (fromSq == 0) {
            m_position.castling &= ~CASTLE_WQ;
        } else if (fromSq == 7) {
            m_position.castling &= ~CASTLE_WK;
        } else if (fromSq == 56) {
            m_position.castling &= ~CASTLE_BQ;
        } else if (fromSq == 63) {
            m_position.castling &= ~CASTLE_BK;
        }
    }
    if (toPiece == ROOK) {
        if (toSq == 0) {
            m_position.castling &= ~CASTLE_WQ;
        } else if (toSq == 7) {
            m_position.castling &= ~CASTLE_WK;
        } else if (toSq == 56) {
            m_position.castling &= ~CASTLE_BQ;
        } else if (toSq == 63) {
            m_position.castling &= ~CASTLE_BK;
        }
    }

    m_position.epSquare = -1;
    if (fromPiece == PAWN && std::abs(toSq - fromSq) == 16) {
        m_position.epSquare = (fromColor == WHITE) ? (fromSq + 8) : (fromSq - 8);
    }

    if (fromPiece == PAWN || isCapture) {
        m_position.halfmove = 0;
    } else {
        ++m_position.halfmove;
    }
    m_position.stm = (m_position.stm == WHITE) ? BLACK : WHITE;
    if (m_position.stm == WHITE) {
        ++m_position.fullmove;
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

void GameController::handleBestMove(EngineSide side, const QString& move)
{
    if (!m_active) {
        return;
    }
    if (move.isEmpty() || move == "0000") {
        return;
    }

    const Move parsed = moveFromUci(move);
    if (!applyMove(parsed)) {
        emit errorOccurred(tr("Engine error"), tr("Invalid bestmove: %1").arg(move));
        return;
    }
    afterMoveApplied(parsed);
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
    if ((side == EngineSide::White && m_position.stm != WHITE)
        || (side == EngineSide::Black && m_position.stm != BLACK)) {
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

    startSideTimer(m_position.stm);
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
    const Color movedSide = (m_position.stm == WHITE) ? BLACK : WHITE;
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

    startTurnIfReady();
}

void GameController::startSideTimer(Color side)
{
    if (!m_timeControlEnabled) {
        return;
    }
    m_timedSide = side;
    m_turnTimer.restart();
    m_timerRunning = true;
}

void GameController::stopSideTimer(Color side)
{
    if (!m_timeControlEnabled || !m_timerRunning) {
        return;
    }
    if (m_timedSide != side) {
        return;
    }

    const qint64 elapsed = m_turnTimer.elapsed();
    if (side == WHITE) {
        m_whiteTimeMs = qMax<qint64>(0, m_whiteTimeMs - elapsed);
    } else {
        m_blackTimeMs = qMax<qint64>(0, m_blackTimeMs - elapsed);
    }
    m_timerRunning = false;
}

void GameController::startTurnIfReady()
{
    const Color sideToMove = m_position.stm;
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
