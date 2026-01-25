#pragma once

#include <cstdint>

using Bitboard = std::uint64_t;
using Move = std::uint64_t;

enum Color {
    WHITE = 0,
    BLACK = 1,
    COLOR_NB = 2
};

enum Piece {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    PIECE_NB = 6,
    NO_PIECE = 6
};

enum CastlingRights {
    CASTLE_WK = 1,
    CASTLE_WQ = 2,
    CASTLE_BK = 4,
    CASTLE_BQ = 8
};

enum MoveFlag : std::uint8_t {
    MOVE_FLAG_NONE = 0,
    MOVE_FLAG_CAPTURE = 1 << 0,
    MOVE_FLAG_DOUBLE = 1 << 1,
    MOVE_FLAG_ENPASSANT = 1 << 2,
    MOVE_FLAG_CASTLE = 1 << 3,
    MOVE_FLAG_PROMOTION = 1 << 4
};

constexpr int MOVE_PIECE_NONE = 15;
constexpr Move MOVE_NONE = 0;

constexpr Bitboard bb_of(int sq)
{
    return Bitboard(1) << sq;
}

constexpr int move_piece_code(Piece piece)
{
    return (piece == NO_PIECE) ? MOVE_PIECE_NONE : static_cast<int>(piece);
}

constexpr Piece move_piece_from_code(int code)
{
    return (code == MOVE_PIECE_NONE) ? NO_PIECE : static_cast<Piece>(code);
}

constexpr Move make_move(int from, int to, int piece, int capture, int promo, int flags)
{
    return static_cast<Move>(from)
        | (static_cast<Move>(to) << 6)
        | (static_cast<Move>(piece) << 12)
        | (static_cast<Move>(capture) << 16)
        | (static_cast<Move>(promo) << 20)
        | (static_cast<Move>(flags) << 24);
}

constexpr int move_from(Move move)
{
    return static_cast<int>(move & 0x3F);
}

constexpr int move_to(Move move)
{
    return static_cast<int>((move >> 6) & 0x3F);
}

constexpr int move_piece(Move move)
{
    return static_cast<int>((move >> 12) & 0xF);
}

constexpr int move_capture(Move move)
{
    return static_cast<int>((move >> 16) & 0xF);
}

constexpr int move_promo(Move move)
{
    return static_cast<int>((move >> 20) & 0xF);
}

constexpr int move_flags(Move move)
{
    return static_cast<int>((move >> 24) & 0xFF);
}
