#pragma once

#include "bitboards.h"

namespace ChessGame{


namespace Attacks {

    enum SIDE_ATTACK_DIRECTION{
        NORTH_ATTACK,
        EAST_ATTACK,
        SOUTH_ATTACK,
        WEST_ATTACK,
        SIDE_ATTACK_DIR_SIZE
    };

    enum DIAGONAL_ATTACK_DIRECTION{
        NORTH_EAST_ATTACK,
        NORTH_WEST_ATTACK,
        SOUTH_EAST_ATTACK,
        SOUTH_WEST_ATTACK,
        DIAGONAL_ATTACK_DIR_SIZE
    };

    extern Bitboard pawnAttacks[COLOR_SIZE][SQ64_SIZE];
    extern Bitboard knightAttacks[SQ64_SIZE];
    extern Bitboard kingAttacks[SQ64_SIZE];
    
    void init();
    Bitboard sliding_side_attacks(Square64 sq64, Bitboard occupied);
    Bitboard sliding_diagonal_attacks(Square64 sq64, Bitboard occupied);
};  

} // namespace ChessGame