#pragma once

#include <cstdint>

using Bitboard = std::uint64_t;

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

constexpr Bitboard bb_of(int sq)
{
    return Bitboard(1) << sq;
}
