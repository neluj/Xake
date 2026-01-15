#include "fen.h"

#include <sstream>
#include <string>

namespace {

bool pieceFromChar(char c, Color& color, Piece& piece)
{
    switch (c) {
    case 'P':
        color = WHITE;
        piece = PAWN;
        return true;
    case 'N':
        color = WHITE;
        piece = KNIGHT;
        return true;
    case 'B':
        color = WHITE;
        piece = BISHOP;
        return true;
    case 'R':
        color = WHITE;
        piece = ROOK;
        return true;
    case 'Q':
        color = WHITE;
        piece = QUEEN;
        return true;
    case 'K':
        color = WHITE;
        piece = KING;
        return true;
    case 'p':
        color = BLACK;
        piece = PAWN;
        return true;
    case 'n':
        color = BLACK;
        piece = KNIGHT;
        return true;
    case 'b':
        color = BLACK;
        piece = BISHOP;
        return true;
    case 'r':
        color = BLACK;
        piece = ROOK;
        return true;
    case 'q':
        color = BLACK;
        piece = QUEEN;
        return true;
    case 'k':
        color = BLACK;
        piece = KING;
        return true;
    default:
        return false;
    }
}

char pieceToChar(Color color, Piece piece)
{
    static const char whitePieces[] = { 'P', 'N', 'B', 'R', 'Q', 'K' };
    static const char blackPieces[] = { 'p', 'n', 'b', 'r', 'q', 'k' };

    if (piece < PAWN || piece >= PIECE_NB) {
        return '?';
    }

    return color == WHITE ? whitePieces[piece] : blackPieces[piece];
}

bool parseIntStrict(const std::string& token, int& out)
{
    std::istringstream iss(token);
    int value = 0;
    char extra = '\0';
    if (!(iss >> value)) {
        return false;
    }
    if (iss >> extra) {
        return false;
    }
    out = value;
    return true;
}

bool parseCastling(const std::string& token, int& out)
{
    if (token == "-") {
        out = 0;
        return true;
    }

    int rights = 0;
    for (char c : token) {
        int bit = 0;
        switch (c) {
        case 'K':
            bit = CASTLE_WK;
            break;
        case 'Q':
            bit = CASTLE_WQ;
            break;
        case 'k':
            bit = CASTLE_BK;
            break;
        case 'q':
            bit = CASTLE_BQ;
            break;
        default:
            return false;
        }
        if (rights & bit) {
            return false;
        }
        rights |= bit;
    }
    out = rights;
    return true;
}

bool parseEpSquare(const std::string& token, int& out)
{
    if (token == "-") {
        out = -1;
        return true;
    }

    if (token.size() != 2) {
        return false;
    }

    const char file = token[0];
    const char rank = token[1];
    if (file < 'a' || file > 'h') {
        return false;
    }
    if (rank < '1' || rank > '8') {
        return false;
    }

    out = (rank - '1') * 8 + (file - 'a');
    return true;
}

} // namespace

bool setFromFen(Position& pos, const std::string& fen)
{
    std::istringstream iss(fen);
    std::string board;
    std::string stmToken;
    std::string castlingToken;
    std::string epToken;
    std::string halfmoveToken;
    std::string fullmoveToken;

    if (!(iss >> board >> stmToken >> castlingToken >> epToken >> halfmoveToken >> fullmoveToken)) {
        return false;
    }
    std::string extra;
    if (iss >> extra) {
        return false;
    }

    Position tmp;
    tmp.clear();

    int rank = 7;
    int file = 0;
    for (char c : board) {
        if (c == '/') {
            if (file != 8 || rank == 0) {
                return false;
            }
            --rank;
            file = 0;
            continue;
        }

        if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8) {
                return false;
            }
            continue;
        }

        Color color = WHITE;
        Piece piece = NO_PIECE;
        if (!pieceFromChar(c, color, piece)) {
            return false;
        }
        if (file >= 8) {
            return false;
        }

        const int sq = rank * 8 + file;
        tmp.bb[color][piece] |= bb_of(sq);
        ++file;
    }

    if (rank != 0 || file != 8) {
        return false;
    }

    tmp.recomputeOcc();

    if (stmToken == "w") {
        tmp.stm = WHITE;
    } else if (stmToken == "b") {
        tmp.stm = BLACK;
    } else {
        return false;
    }

    if (!parseCastling(castlingToken, tmp.castling)) {
        return false;
    }

    if (!parseEpSquare(epToken, tmp.epSquare)) {
        return false;
    }

    int halfmove = 0;
    int fullmove = 0;
    if (!parseIntStrict(halfmoveToken, halfmove) || halfmove < 0) {
        return false;
    }
    if (!parseIntStrict(fullmoveToken, fullmove) || fullmove < 1) {
        return false;
    }

    tmp.halfmove = halfmove;
    tmp.fullmove = fullmove;

    pos = tmp;
    return true;
}

std::string toFen(const Position& pos)
{
    std::string board;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const int sq = rank * 8 + file;
            Color color = WHITE;
            const Piece piece = pos.pieceAt(sq, color);
            if (piece == NO_PIECE) {
                ++empty;
                continue;
            }

            if (empty > 0) {
                board.push_back(static_cast<char>('0' + empty));
                empty = 0;
            }
            board.push_back(pieceToChar(color, piece));
        }
        if (empty > 0) {
            board.push_back(static_cast<char>('0' + empty));
        }
        if (rank > 0) {
            board.push_back('/');
        }
    }

    const char stmChar = pos.stm == WHITE ? 'w' : 'b';

    std::string castling;
    if (pos.castling == 0) {
        castling = "-";
    } else {
        if (pos.castling & CASTLE_WK) {
            castling.push_back('K');
        }
        if (pos.castling & CASTLE_WQ) {
            castling.push_back('Q');
        }
        if (pos.castling & CASTLE_BK) {
            castling.push_back('k');
        }
        if (pos.castling & CASTLE_BQ) {
            castling.push_back('q');
        }
    }

    std::string ep = "-";
    if (pos.epSquare != -1) {
        const int file = pos.epSquare % 8;
        const int rank = pos.epSquare / 8;
        ep.clear();
        ep.push_back(static_cast<char>('a' + file));
        ep.push_back(static_cast<char>('1' + rank));
    }

    return board + " " + stmChar + " " + castling + " " + ep + " "
        + std::to_string(pos.halfmove) + " " + std::to_string(pos.fullmove);
}
