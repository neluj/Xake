#include "opening_book.h"

#include "movegen.h"
#include "position.h"

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>

using namespace Xake;

namespace {

constexpr char kStartFen[] =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct RawPgnGame {
    QMap<QString, QString> tags;
    QStringList moveText;
};

PieceType pieceTypeFromSan(QChar piece)
{
    switch (piece.toLatin1()) {
    case 'N':
        return KNIGHT;
    case 'B':
        return BISHOP;
    case 'R':
        return ROOK;
    case 'Q':
        return QUEEN;
    case 'K':
        return KING;
    default:
        return PAWN;
    }
}

PieceType promotionTypeFromSan(QChar piece)
{
    const PieceType type = pieceTypeFromSan(piece.toUpper());
    return type == KNIGHT || type == BISHOP || type == ROOK || type == QUEEN
        ? type
        : NO_PIECE_TYPE;
}

Square64 squareFromName(const QString& name)
{
    if (name.size() != 2
        || name.at(0) < QLatin1Char('a') || name.at(0) > QLatin1Char('h')
        || name.at(1) < QLatin1Char('1') || name.at(1) > QLatin1Char('8')) {
        return SQ64_NO_SQUARE;
    }

    const int file = name.at(0).toLatin1() - 'a';
    const int rank = name.at(1).toLatin1() - '1';
    return Square64(rank * 8 + file);
}

bool moveIsLegal(const Position& position, Move move)
{
    Position candidate = position;
    return candidate.do_move(move);
}

Move resolveUciMove(const Position& position, const QString& notation)
{
    static const QRegularExpression uciExpression(
        QStringLiteral("^[a-h][1-8][a-h][1-8][nbrq]?$"),
        QRegularExpression::CaseInsensitiveOption);
    if (!uciExpression.match(notation).hasMatch()) {
        return NOMOVE;
    }

    const Square64 from = squareFromName(notation.left(2).toLower());
    const Square64 to = squareFromName(notation.mid(2, 2).toLower());
    const PieceType promotion = notation.size() == 5
        ? promotionTypeFromSan(notation.at(4))
        : NO_PIECE_TYPE;

    MoveGen::MoveList moveList;
    MoveGen::generate_pseudo_moves(position, moveList);
    for (int index = 0; index < moveList.size; ++index) {
        const Move move = moveList.moves[index];
        if (move_from(move) == from
            && move_to(move) == to
            && promoted_piece(move) == promotion
            && moveIsLegal(position, move)) {
            return move;
        }
    }
    return NOMOVE;
}

QString cleanSan(QString san)
{
    san = san.trimmed();
    while (!san.isEmpty()) {
        const QChar suffix = san.back();
        if (suffix == QLatin1Char('+')
            || suffix == QLatin1Char('#')
            || suffix == QLatin1Char('!')
            || suffix == QLatin1Char('?')) {
            san.chop(1);
            continue;
        }
        break;
    }
    if (san.endsWith(QStringLiteral("e.p."), Qt::CaseInsensitive)) {
        san.chop(4);
        san = san.trimmed();
    }
    return san;
}

Move resolveCastle(const Position& position, const QString& san)
{
    QString castle = san;
    castle.replace(QLatin1Char('0'), QLatin1Char('O'));
    if (castle != QStringLiteral("O-O") && castle != QStringLiteral("O-O-O")) {
        return NOMOVE;
    }

    const bool kingSide = castle == QStringLiteral("O-O");
    const Color side = position.get_side_to_move();
    const Square64 destination = side == WHITE
        ? (kingSide ? SQ64_G1 : SQ64_C1)
        : (kingSide ? SQ64_G8 : SQ64_C8);

    MoveGen::MoveList moveList;
    MoveGen::generate_pseudo_moves(position, moveList);
    for (int index = 0; index < moveList.size; ++index) {
        const Move move = moveList.moves[index];
        if (move_special(move) == CASTLE
            && move_to(move) == destination
            && moveIsLegal(position, move)) {
            return move;
        }
    }
    return NOMOVE;
}

Move resolveSanMove(const Position& position, const QString& notation)
{
    QString san = cleanSan(notation);
    if (san.isEmpty()) {
        return NOMOVE;
    }

    const Move uciMove = resolveUciMove(position, san);
    if (uciMove != NOMOVE) {
        return uciMove;
    }

    const Move castleMove = resolveCastle(position, san);
    if (castleMove != NOMOVE) {
        return castleMove;
    }

    PieceType promotion = NO_PIECE_TYPE;
    static const QRegularExpression promotionExpression(
        QStringLiteral("=([NBRQ])$"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch promotionMatch = promotionExpression.match(san);
    if (promotionMatch.hasMatch()) {
        promotion = promotionTypeFromSan(promotionMatch.captured(1).at(0));
        san.chop(promotionMatch.capturedLength());
    } else if (san.size() >= 3) {
        const PieceType compactPromotion = promotionTypeFromSan(san.back());
        const QString possibleSquare = san.mid(san.size() - 3, 2).toLower();
        if (compactPromotion != NO_PIECE_TYPE
            && squareFromName(possibleSquare) != SQ64_NO_SQUARE) {
            promotion = compactPromotion;
            san.chop(1);
        }
    }

    static const QRegularExpression destinationExpression(
        QStringLiteral("([a-h][1-8])$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch destinationMatch = destinationExpression.match(san);
    if (!destinationMatch.hasMatch()) {
        return NOMOVE;
    }

    const Square64 destination =
        squareFromName(destinationMatch.captured(1).toLower());
    QString prefix = san.left(destinationMatch.capturedStart());
    PieceType movingType = PAWN;
    if (!prefix.isEmpty()
        && QStringLiteral("NBRQK").contains(prefix.front())) {
        movingType = pieceTypeFromSan(prefix.front());
        prefix.remove(0, 1);
    }

    const bool isSanCapture = prefix.contains(QLatin1Char('x'), Qt::CaseInsensitive);
    if (prefix.count(QLatin1Char('x'), Qt::CaseInsensitive) > 1) {
        return NOMOVE;
    }
    prefix.remove(QLatin1Char('x'), Qt::CaseInsensitive);
    if (prefix.size() > 2) {
        return NOMOVE;
    }

    int fromFile = -1;
    int fromRank = -1;
    for (const QChar disambiguation : prefix) {
        const QChar lower = disambiguation.toLower();
        if (lower >= QLatin1Char('a') && lower <= QLatin1Char('h') && fromFile < 0) {
            fromFile = lower.toLatin1() - 'a';
        } else if (disambiguation >= QLatin1Char('1')
                   && disambiguation <= QLatin1Char('8')
                   && fromRank < 0) {
            fromRank = disambiguation.toLatin1() - '1';
        } else {
            return NOMOVE;
        }
    }
    if (movingType == PAWN
        && ((isSanCapture && fromFile < 0) || (!isSanCapture && !prefix.isEmpty()))) {
        return NOMOVE;
    }

    Move resolved = NOMOVE;
    MoveGen::MoveList moveList;
    MoveGen::generate_pseudo_moves(position, moveList);
    for (int index = 0; index < moveList.size; ++index) {
        const Move move = moveList.moves[index];
        const Square64 from = move_from(move);
        const Piece piece = position.get_mailbox_piece(from);
        if (piece == NO_PIECE
            || piece_type(piece) != movingType
            || move_to(move) != destination
            || promoted_piece(move) != promotion
            || is_capture(move) != isSanCapture
            || (fromFile >= 0 && square_file(from) != fromFile)
            || (fromRank >= 0 && square_rank(from) != fromRank)
            || !moveIsLegal(position, move)) {
            continue;
        }
        if (resolved != NOMOVE) {
            return NOMOVE;
        }
        resolved = move;
    }
    return resolved;
}

QString mainlineText(const QString& moveText, bool* complete)
{
    QString result;
    int braceDepth = 0;
    int variationDepth = 0;
    bool lineComment = false;

    for (const QChar character : moveText) {
        if (lineComment) {
            if (character == QLatin1Char('\n')) {
                lineComment = false;
                result += QLatin1Char(' ');
            }
            continue;
        }
        if (braceDepth > 0) {
            if (character == QLatin1Char('{')) {
                ++braceDepth;
            } else if (character == QLatin1Char('}')) {
                --braceDepth;
            }
            continue;
        }
        if (character == QLatin1Char('{')) {
            ++braceDepth;
            result += QLatin1Char(' ');
        } else if (character == QLatin1Char(';')) {
            lineComment = true;
            result += QLatin1Char(' ');
        } else if (character == QLatin1Char('(')) {
            ++variationDepth;
            result += QLatin1Char(' ');
        } else if (character == QLatin1Char(')')) {
            if (variationDepth > 0) {
                --variationDepth;
            }
        } else if (variationDepth == 0) {
            result += character;
        }
    }

    if (complete) {
        *complete = braceDepth == 0 && variationDepth == 0;
    }
    return result;
}

bool isResultToken(const QString& token)
{
    return token == QStringLiteral("1-0")
        || token == QStringLiteral("0-1")
        || token == QStringLiteral("1/2-1/2")
        || token == QStringLiteral("*");
}

QStringList pgnMoveTokens(const QString& moveText, QString* errorOut)
{
    bool commentsComplete = true;
    QString mainline = mainlineText(moveText, &commentsComplete);
    if (!commentsComplete) {
        if (errorOut) {
            *errorOut = QStringLiteral("Unterminated PGN comment or variation.");
        }
        return {};
    }

    static const QRegularExpression nagExpression(QStringLiteral("\\$\\d+"));
    mainline.remove(nagExpression);

    static const QRegularExpression moveNumberExpression(
        QStringLiteral("^\\d+\\.(?:\\.\\.)?"));
    QStringList moves;
    const QStringList rawTokens =
        mainline.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (QString token : rawTokens) {
        while (true) {
            const QRegularExpressionMatch numberMatch = moveNumberExpression.match(token);
            if (!numberMatch.hasMatch()) {
                break;
            }
            token.remove(0, numberMatch.capturedLength());
        }
        while (token.startsWith(QLatin1Char('.'))) {
            token.remove(0, 1);
        }
        if (token.isEmpty()
            || token.compare(QStringLiteral("e.p."), Qt::CaseInsensitive) == 0
            || isResultToken(token)) {
            continue;
        }
        moves.append(token);
    }
    return moves;
}

QString openingName(const QMap<QString, QString>& tags, int gameNumber)
{
    QString name = tags.value(QStringLiteral("OPENING")).trimmed();
    const QString variation = tags.value(QStringLiteral("VARIATION")).trimmed();
    if (!name.isEmpty() && !variation.isEmpty()) {
        name += QStringLiteral(" - %1").arg(variation);
    }
    if (name.isEmpty()) {
        name = tags.value(QStringLiteral("EVENT")).trimmed();
    }
    const QString eco = tags.value(QStringLiteral("ECO")).trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("PGN game %1").arg(gameNumber);
    }
    if (!eco.isEmpty()) {
        name += QStringLiteral(" [%1]").arg(eco);
    }
    return name;
}

bool appendPgnGame(const RawPgnGame& raw,
                   int gameNumber,
                   QVector<OpeningEntry>* openings,
                   QString* errorOut)
{
    const QString startFen = raw.tags.value(
        QStringLiteral("FEN"), QString::fromLatin1(kStartFen)).trimmed();
    Position position;
    if (!position.set_FEN(startFen.toStdString())) {
        if (errorOut) {
            *errorOut = QStringLiteral("PGN game %1 has an invalid FEN tag.")
                .arg(gameNumber);
        }
        return false;
    }

    QString tokenError;
    const QStringList sanMoves = pgnMoveTokens(raw.moveText.join('\n'), &tokenError);
    if (!tokenError.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("PGN game %1: %2").arg(gameNumber).arg(tokenError);
        }
        return false;
    }

    QStringList uciMoves;
    for (qsizetype ply = 0; ply < sanMoves.size(); ++ply) {
        const QString& san = sanMoves.at(ply);
        const Move move = resolveSanMove(position, san);
        if (move == NOMOVE || !position.do_move(move)) {
            if (errorOut) {
                *errorOut = QStringLiteral(
                    "PGN game %1 contains an invalid or ambiguous move at ply %2: %3")
                    .arg(gameNumber)
                    .arg(ply + 1)
                    .arg(san);
            }
            return false;
        }
        uciMoves.append(QString::fromStdString(algebraic_move(move)));
    }

    openings->append(OpeningEntry{
        gameNumber,
        openingName(raw.tags, gameNumber),
        startFen,
        QString::fromStdString(position.get_FEN()),
        uciMoves
    });
    return true;
}

} // namespace

bool loadOpeningFile(const QString& filePath,
                     QVector<OpeningEntry>* openings,
                     QString* errorOut)
{
    if (!openings) {
        if (errorOut) {
            *errorOut = QStringLiteral("Opening output container is not available.");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not open '%1': %2")
                .arg(filePath, file.errorString());
        }
        return false;
    }

    const QString contents = QString::fromUtf8(file.readAll());
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("pgn")) {
        return parsePgnOpenings(contents, openings, errorOut);
    }
    if (suffix == QStringLiteral("epd") || suffix == QStringLiteral("edp")) {
        return parseEpdOpenings(contents, openings, errorOut);
    }

    if (errorOut) {
        *errorOut = QStringLiteral("Unsupported opening file extension: .%1").arg(suffix);
    }
    return false;
}

bool parsePgnOpenings(const QString& contents,
                      QVector<OpeningEntry>* openings,
                      QString* errorOut)
{
    if (!openings) {
        return false;
    }
    openings->clear();

    static const QRegularExpression tagExpression(
        QStringLiteral("^\\[([A-Za-z0-9_]+)\\s+\"((?:\\\\.|[^\"])*)\"\\]\\s*$"));
    RawPgnGame raw;
    int gameNumber = 0;

    const auto finishGame = [&]() -> bool {
        if (raw.tags.isEmpty() && raw.moveText.join(QString()).trimmed().isEmpty()) {
            return true;
        }
        ++gameNumber;
        if (!appendPgnGame(raw, gameNumber, openings, errorOut)) {
            return false;
        }
        raw = RawPgnGame{};
        return true;
    };

    const QStringList lines = contents.split(QLatin1Char('\n'));
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString trimmed = lines.at(lineIndex).trimmed();
        if (trimmed.startsWith(QLatin1Char('['))) {
            if (!raw.moveText.join(QString()).trimmed().isEmpty() && !finishGame()) {
                return false;
            }
            const QRegularExpressionMatch tagMatch = tagExpression.match(trimmed);
            if (!tagMatch.hasMatch()) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Invalid PGN tag at line %1.")
                        .arg(lineIndex + 1);
                }
                return false;
            }
            QString value = tagMatch.captured(2);
            value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
            value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
            raw.tags.insert(tagMatch.captured(1).toUpper(), value);
        } else if (!trimmed.isEmpty()) {
            raw.moveText.append(lines.at(lineIndex));
        }
    }
    if (!finishGame()) {
        return false;
    }

    if (openings->isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("The PGN file does not contain any games.");
        }
        return false;
    }
    return true;
}

bool parseEpdOpenings(const QString& contents,
                      QVector<OpeningEntry>* openings,
                      QString* errorOut)
{
    if (!openings) {
        return false;
    }
    openings->clear();

    static const QRegularExpression idExpression(
        QStringLiteral("\\bid\\s+\"([^\"]*)\""),
        QRegularExpression::CaseInsensitiveOption);
    const QStringList lines = contents.split(QLatin1Char('\n'));
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString line = lines.at(lineIndex).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList fields =
            line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() < 4) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid EPD position at line %1.")
                    .arg(lineIndex + 1);
            }
            return false;
        }

        const QString fen = fields.mid(0, 4).join(QLatin1Char(' '))
            + QStringLiteral(" 0 1");
        Position position;
        if (!position.set_FEN(fen.toStdString())) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid EPD position at line %1.")
                    .arg(lineIndex + 1);
            }
            return false;
        }

        const QRegularExpressionMatch idMatch = idExpression.match(line);
        const QString name = idMatch.hasMatch() && !idMatch.captured(1).trimmed().isEmpty()
            ? idMatch.captured(1).trimmed()
            : QStringLiteral("EPD line %1").arg(lineIndex + 1);
        openings->append(OpeningEntry{
            static_cast<int>(openings->size()) + 1,
            name,
            QString::fromStdString(position.get_FEN()),
            QString::fromStdString(position.get_FEN()),
            {}
        });
    }

    if (openings->isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("The EPD file does not contain any positions.");
        }
        return false;
    }
    return true;
}
