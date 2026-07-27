#include "pgn_export.h"

#include "bitboards.h"
#include "move.h"
#include "movegen.h"
#include "position.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSaveFile>

using namespace Xake;

namespace {

constexpr char kStartFen[] =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

QString escapedTag(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return value;
}

QString squareName(Square64 square)
{
    if (square < SQ64_A1 || square >= SQ64_SIZE) {
        return QString();
    }
    return QString::fromLatin1(SQUARE_NAMES[square].data(),
                               static_cast<int>(SQUARE_NAMES[square].size()));
}

QChar pieceLetter(PieceType type)
{
    switch (type) {
    case KNIGHT:
        return QLatin1Char('N');
    case BISHOP:
        return QLatin1Char('B');
    case ROOK:
        return QLatin1Char('R');
    case QUEEN:
        return QLatin1Char('Q');
    case KING:
        return QLatin1Char('K');
    default:
        return QChar();
    }
}

bool isLegalMove(const Position& position, Move move)
{
    Position candidate = position;
    return candidate.do_move(move);
}

Move resolveUciMove(const Position& position, const QString& notation)
{
    static const QRegularExpression expression(
        QStringLiteral("^[a-h][1-8][a-h][1-8][nbrq]?$"),
        QRegularExpression::CaseInsensitiveOption);
    if (!expression.match(notation).hasMatch()) {
        return NOMOVE;
    }

    const QByteArray bytes = notation.toLower().toLatin1();
    const Square64 from = Square64((bytes[1] - '1') * 8 + bytes[0] - 'a');
    const Square64 to = Square64((bytes[3] - '1') * 8 + bytes[2] - 'a');
    PieceType promotion = NO_PIECE_TYPE;
    if (bytes.size() == 5) {
        switch (bytes[4]) {
        case 'n':
            promotion = KNIGHT;
            break;
        case 'b':
            promotion = BISHOP;
            break;
        case 'r':
            promotion = ROOK;
            break;
        case 'q':
            promotion = QUEEN;
            break;
        default:
            return NOMOVE;
        }
    }

    MoveGen::MoveList moves;
    MoveGen::generate_pseudo_moves(position, moves);
    for (int index = 0; index < moves.size; ++index) {
        const Move move = moves.moves[index];
        if (move_from(move) == from
            && move_to(move) == to
            && promoted_piece(move) == promotion
            && isLegalMove(position, move)) {
            return move;
        }
    }
    return NOMOVE;
}

bool hasLegalMove(const Position& position)
{
    MoveGen::MoveList moves;
    MoveGen::generate_pseudo_moves(position, moves);
    for (int index = 0; index < moves.size; ++index) {
        if (isLegalMove(position, moves.moves[index])) {
            return true;
        }
    }
    return false;
}

bool sideToMoveIsInCheck(const Position& position)
{
    const Color side = position.get_side_to_move();
    const Bitboard king = position.get_pieceTypes_bitboard(side, KING);
    if (!king) {
        return false;
    }
    return position.square_is_attacked_bySide(
        Square64(Bitboards::ctz(king)), ~side);
}

QString sanForMove(const Position& position, Move move)
{
    const Square64 from = move_from(move);
    const Square64 to = move_to(move);
    const Piece movingPiece = position.get_mailbox_piece(from);
    if (movingPiece == NO_PIECE) {
        return QString();
    }

    QString san;
    if (move_special(move) == CASTLE) {
        san = to > from ? QStringLiteral("O-O") : QStringLiteral("O-O-O");
    } else {
        const PieceType movingType = piece_type(movingPiece);
        const bool capture = is_capture(move);
        if (movingType != PAWN) {
            san.append(pieceLetter(movingType));

            bool hasAlternative = false;
            bool sameFile = false;
            bool sameRank = false;
            MoveGen::MoveList moves;
            MoveGen::generate_pseudo_moves(position, moves);
            for (int index = 0; index < moves.size; ++index) {
                const Move candidate = moves.moves[index];
                const Square64 candidateFrom = move_from(candidate);
                if (candidateFrom == from
                    || move_to(candidate) != to
                    || position.get_mailbox_piece(candidateFrom) == NO_PIECE
                    || piece_type(position.get_mailbox_piece(candidateFrom))
                        != movingType
                    || !isLegalMove(position, candidate)) {
                    continue;
                }
                hasAlternative = true;
                sameFile = sameFile || square_file(candidateFrom) == square_file(from);
                sameRank = sameRank || square_rank(candidateFrom) == square_rank(from);
            }
            if (hasAlternative) {
                if (!sameFile) {
                    san.append(QLatin1Char('a' + square_file(from)));
                } else if (!sameRank) {
                    san.append(QLatin1Char('1' + square_rank(from)));
                } else {
                    san.append(squareName(from));
                }
            }
        } else if (capture) {
            san.append(QLatin1Char('a' + square_file(from)));
        }

        if (capture) {
            san.append(QLatin1Char('x'));
        }
        san.append(squareName(to));

        const PieceType promotion = promoted_piece(move);
        if (promotion != NO_PIECE_TYPE) {
            san.append(QLatin1Char('='));
            san.append(pieceLetter(promotion));
        }
    }

    Position next = position;
    if (!next.do_move(move)) {
        return QString();
    }
    if (sideToMoveIsInCheck(next)) {
        san.append(hasLegalMove(next) ? QLatin1Char('+') : QLatin1Char('#'));
    }
    return san;
}

QString wrappedMoveText(const QStringList& tokens)
{
    QString text;
    int lineLength = 0;
    for (const QString& token : tokens) {
        const int separator = lineLength == 0 ? 0 : 1;
        if (lineLength > 0 && lineLength + separator + token.size() > 80) {
            text.append(QLatin1Char('\n'));
            lineLength = 0;
        } else if (lineLength > 0) {
            text.append(QLatin1Char(' '));
            ++lineLength;
        }
        text.append(token);
        lineLength += token.size();
    }
    return text;
}

QString gameText(const PgnGameRecord& game, QString* errorOut)
{
    Position position;
    if (!position.set_FEN(game.startFen.toStdString())) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid PGN start FEN.");
        }
        return QString();
    }

    QStringList moveTokens;
    for (qsizetype ply = 0; ply < game.movesUci.size(); ++ply) {
        const QString& uci = game.movesUci.at(ply);
        const Move move = resolveUciMove(position, uci);
        if (move == NOMOVE) {
            if (errorOut) {
                *errorOut = QStringLiteral("Could not convert move %1 (%2) to SAN.")
                    .arg(ply + 1)
                    .arg(uci);
            }
            return QString();
        }

        if (position.get_side_to_move() == WHITE) {
            moveTokens.append(QStringLiteral("%1.").arg(position.get_moves_counter()));
        } else if (ply == 0) {
            moveTokens.append(QStringLiteral("%1...")
                                  .arg(position.get_moves_counter()));
        }
        const QString san = sanForMove(position, move);
        if (san.isEmpty() || !position.do_move(move)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Could not apply move %1 (%2) while exporting PGN.")
                    .arg(ply + 1)
                    .arg(uci);
            }
            return QString();
        }
        moveTokens.append(san);
    }
    moveTokens.append(game.result.isEmpty() ? QStringLiteral("*") : game.result);

    const auto tag = [](const QString& name, const QString& value) {
        return QStringLiteral("[%1 \"%2\"]\n").arg(name, escapedTag(value));
    };

    QString text;
    text += tag(QStringLiteral("Event"),
                game.event.isEmpty() ? QStringLiteral("Xake game") : game.event);
    text += tag(QStringLiteral("Site"),
                game.site.isEmpty() ? QStringLiteral("?") : game.site);
    text += tag(QStringLiteral("Date"),
                game.date.isEmpty() ? QStringLiteral("????.??.??") : game.date);
    text += tag(QStringLiteral("Round"),
                game.round.isEmpty() ? QStringLiteral("?") : game.round);
    text += tag(QStringLiteral("White"),
                game.white.isEmpty() ? QStringLiteral("?") : game.white);
    text += tag(QStringLiteral("Black"),
                game.black.isEmpty() ? QStringLiteral("?") : game.black);
    text += tag(QStringLiteral("Result"),
                game.result.isEmpty() ? QStringLiteral("*") : game.result);
    if (!game.termination.isEmpty()) {
        text += tag(QStringLiteral("Termination"), game.termination);
    }
    if (!game.opening.isEmpty()) {
        text += tag(QStringLiteral("Opening"), game.opening);
    }
    if (!game.timeControl.isEmpty()) {
        text += tag(QStringLiteral("TimeControl"), game.timeControl);
    }
    if (game.startFen != QString::fromLatin1(kStartFen)) {
        text += tag(QStringLiteral("SetUp"), QStringLiteral("1"));
        text += tag(QStringLiteral("FEN"), game.startFen);
    }
    text += QLatin1Char('\n');
    text += wrappedMoveText(moveTokens);
    text += QLatin1Char('\n');
    return text;
}

} // namespace

QString pgnText(const QVector<PgnGameRecord>& games, QString* errorOut)
{
    QString text;
    for (qsizetype index = 0; index < games.size(); ++index) {
        QString gameError;
        const QString currentGame = gameText(games.at(index), &gameError);
        if (currentGame.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Game %1: %2")
                    .arg(index + 1)
                    .arg(gameError);
            }
            return QString();
        }
        if (!text.isEmpty()) {
            text += QLatin1Char('\n');
        }
        text += currentGame;
    }
    return text;
}

bool writePgnFile(const QVector<PgnGameRecord>& games,
                  const QString& filePath,
                  QString* errorOut)
{
    const QString text = pgnText(games, errorOut);
    if (games.size() > 0 && text.isEmpty()) {
        return false;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return false;
    }
    const QByteArray encoded = text.toUtf8();
    if (file.write(encoded) != encoded.size() || !file.commit()) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return false;
    }
    return true;
}
