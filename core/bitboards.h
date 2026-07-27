#pragma once

#include "types.h"
#include <iostream>

namespace Xake{


using Bitboard = uint64_t;

inline constexpr Bitboard ZERO = Bitboard{0};
inline constexpr Bitboard ONE  = Bitboard{1};

namespace Bitboards {

    constexpr Bitboard FILE_A_MASK = 0x0101010101010101ULL;
    constexpr Bitboard FILE_B_MASK = 0x0202020202020202ULL;
    constexpr Bitboard FILE_G_MASK = 0x4040404040404040ULL;
    constexpr Bitboard FILE_H_MASK = 0x8080808080808080ULL;
    constexpr Bitboard RANK_1_MASK = 0x00000000000000FFULL;
    constexpr Bitboard RANK_2_MASK = 0x000000000000FF00ULL;
    constexpr Bitboard RANK_3_MASK = 0x0000000000FF0000ULL;
    constexpr Bitboard RANK_6_MASK = 0x0000FF0000000000ULL;
    constexpr Bitboard RANK_7_MASK = 0x00FF000000000000ULL;
    constexpr Bitboard RANK_8_MASK = 0xFF00000000000000ULL;

    //GCC/Clang
    #if defined(__clang__) || defined(__GNUC__)

    inline int ctz (Bitboard bitboard){ return __builtin_ctzll(bitboard); }      
    inline int clz (Bitboard bitboard){ return __builtin_clzll(bitboard); }      
    inline int cpop(Bitboard bitboard){ return __builtin_popcountll(bitboard); }

    //For other compilers, use C++ 20 std
    #else

    #include <bit>
    inline int ctz (Bitboard bitboard){ return (int)std::countr_zero(bitboard); } 
    inline int clz (Bitboard bitboard){ return (int)std::countl_zero(bitboard); }
    inline int cpop(Bitboard bitboard){ return (int)std::popcount(bitboard); }

    #endif

    template<typename... Squares>
    inline Bitboard set_pieces(Squares... squares) {
        return ZERO | ((ONE << squares) | ...);
    }

    template<typename... Squares>
    inline Bitboard set_pieces(Bitboard bitboard, Squares... squares) {
        return bitboard | ((ONE << squares) | ...);
    }

    template<typename... Squares>
    inline Bitboard clear_pieces(Bitboard bitboard, Squares... squares) {
        return (bitboard & ... & ~(ONE << squares));
    }

    template<typename... Masks>
    inline Bitboard clear_masks(Bitboard bitboard, Masks... masks) {
        return bitboard & ~(... | masks);
    }

    template<Direction D>
    constexpr Bitboard make_direction(Bitboard bitboard){

        return D == NORTH            ? clear_masks(bitboard << 8 , RANK_1_MASK)
             : D == NORTH_NORTH      ? clear_masks(bitboard << 16, RANK_1_MASK, RANK_2_MASK)    
             : D == SOUTH            ? clear_masks(bitboard >> 8,  RANK_8_MASK)    
             : D == SOUTH_SOUTH      ? clear_masks(bitboard >> 16, RANK_8_MASK, RANK_7_MASK)    
             : D == EAST             ? clear_masks(bitboard << 1,  FILE_A_MASK)    
             : D == WEST             ? clear_masks(bitboard >> 1,  FILE_H_MASK)    
             : D == NORTH_EAST       ? clear_masks(bitboard << 9,  FILE_A_MASK, RANK_1_MASK)    
             : D == NORTH_WEST       ? clear_masks(bitboard << 7,  FILE_H_MASK, RANK_1_MASK)    
             : D == SOUTH_EAST       ? clear_masks(bitboard >> 7,  FILE_A_MASK, RANK_8_MASK)    
             : D == SOUTH_WEST       ? clear_masks(bitboard >> 9,  FILE_H_MASK, RANK_8_MASK)    
             : D == NORTH_NORTH_WEST ? clear_masks(bitboard << 15, RANK_1_MASK, RANK_2_MASK, FILE_H_MASK)    
             : D == NORTH_NORTH_EAST ? clear_masks(bitboard << 17, RANK_1_MASK, RANK_2_MASK, FILE_A_MASK)    
             : D == NORTH_WEST_WEST  ? clear_masks(bitboard << 6,  RANK_1_MASK, FILE_G_MASK, FILE_H_MASK)    
             : D == NORTH_EAST_EAST  ? clear_masks(bitboard << 10, RANK_1_MASK, FILE_A_MASK, FILE_B_MASK)    
             : D == SOUTH_WEST_WEST  ? clear_masks(bitboard >> 10, RANK_8_MASK, FILE_G_MASK, FILE_H_MASK)    
             : D == SOUTH_EAST_EAST  ? clear_masks(bitboard >> 6,  RANK_8_MASK, FILE_A_MASK, FILE_B_MASK)    
             : D == SOUTH_SOUTH_WEST ? clear_masks(bitboard >> 17, RANK_8_MASK, RANK_7_MASK, FILE_H_MASK)    
             : D == SOUTH_SOUTH_EAST ? clear_masks(bitboard >> 15, RANK_8_MASK, RANK_7_MASK, FILE_A_MASK)
                                     : ZERO;

    }

};
} // namespace Xake


