/*General type definitios for Xake*/
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <chrono>

namespace Xake{

using DepthSize = uint16_t;
using NodesSize = uint64_t;
using MovesSize = int;

constexpr int MAX_POSITION_MOVES_SIZE = 255;
constexpr int MAX_GAME_MOVES = 2048;
constexpr int MAX_SAME_PIECE = 10;
constexpr DepthSize MAX_DEPTH = 64;

constexpr int CASTLING_POSIBILITIES = 4 * 4;

enum PieceType{
  NO_PIECE_TYPE,
  PAWN, 
  KNIGHT, 
  BISHOP, 
  ROOK, 
  QUEEN, 
  KING,
  PIECETYPE_SIZE
};

enum Piece : int{
  NO_PIECE,
  W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
  B_PAWN = 8 + W_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
  PIECE_SIZE = 16
};

const std::string_view PIECE_NAMES{" PNBRQK  pnbrqk "};

enum Color : int{
  WHITE,
  BLACK,
  COLOR_NC,
  COLOR_SIZE
};

constexpr Piece make_piece(Color c, PieceType pt) { 
  return Piece(((c << 3) + pt) & ~(1 << 4)); 
}

constexpr Color piece_color(Piece piece) { 
  return Color(piece >> 3);
}

constexpr PieceType piece_type(Piece piece) {
  return PieceType(0x7 & piece);
}

constexpr Color operator~(Color color) {
  return Color(color ^ BLACK); 
}

enum File : int{
  FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_SIZE
};

enum Rank : int{
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_SIZE
};

enum Square64 : int{
	SQ64_A1, SQ64_B1, SQ64_C1, SQ64_D1, SQ64_E1, SQ64_F1, SQ64_G1, SQ64_H1,
	SQ64_A2, SQ64_B2, SQ64_C2, SQ64_D2, SQ64_E2, SQ64_F2, SQ64_G2, SQ64_H2,
	SQ64_A3, SQ64_B3, SQ64_C3, SQ64_D3, SQ64_E3, SQ64_F3, SQ64_G3, SQ64_H3,
	SQ64_A4, SQ64_B4, SQ64_C4, SQ64_D4, SQ64_E4, SQ64_F4, SQ64_G4, SQ64_H4,
	SQ64_A5, SQ64_B5, SQ64_C5, SQ64_D5, SQ64_E5, SQ64_F5, SQ64_G5, SQ64_H5,
	SQ64_A6, SQ64_B6, SQ64_C6, SQ64_D6, SQ64_E6, SQ64_F6, SQ64_G6, SQ64_H6,
	SQ64_A7, SQ64_B7, SQ64_C7, SQ64_D7, SQ64_E7, SQ64_F7, SQ64_G7, SQ64_H7,
	SQ64_A8, SQ64_B8, SQ64_C8, SQ64_D8, SQ64_E8, SQ64_F8, SQ64_G8, SQ64_H8,
  SQ64_SIZE, SQ64_NO_SQUARE, SQ64_OFFBOARD
};

const std::string SQUARE_NAMES[SQ64_SIZE] = {
   "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", 
   "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", 
   "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", 
   "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", 
   "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", 
   "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", 
   "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", 
   "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"
};


constexpr Square64 make_square64(Rank rank, File file){
  return Square64((rank * 8) + file);
} 

constexpr File square_file(Square64 square){ 
  return File(square % 8); 
}

constexpr Rank square_rank(Square64 square){
  return Rank(square/8);
}

enum CastlingRight: int{ 
  NO_RIGHT = 0,
  WKCA     = 1, 
  WQCA     = 2, 
  BKCA     = 4, 
  BQCA     = 8,
  CASTLE_WK = WKCA,
  CASTLE_WQ = WQCA,
  CASTLE_BK = BKCA,
  CASTLE_BQ = BQCA
};

using CastlingRights = CastlingRight;

static_assert(int(WKCA) == 1);
static_assert(int(WQCA) == 2);
static_assert(int(BKCA) == 4);
static_assert(int(BQCA) == 8);
static_assert(int(CASTLE_WK) == int(WKCA));
static_assert(int(CASTLE_WQ) == int(WQCA));
static_assert(int(CASTLE_BK) == int(BKCA));
static_assert(int(CASTLE_BQ) == int(BQCA));

enum Direction: int{
    NORTH = 8,
    NORTH_NORTH = NORTH + NORTH,
    SOUTH = -NORTH,
    SOUTH_SOUTH = SOUTH + SOUTH,
    EAST  = 1,
    WEST  = -EAST,

    NORTH_EAST = NORTH + EAST,
    NORTH_WEST = NORTH + WEST,
    SOUTH_EAST = SOUTH + EAST,
    SOUTH_WEST = SOUTH + WEST,

    NORTH_NORTH_WEST = NORTH + NORTH_WEST,
    NORTH_NORTH_EAST = NORTH + NORTH_EAST,
    NORTH_WEST_WEST  = NORTH_WEST + WEST,
    NORTH_EAST_EAST  = NORTH_EAST + EAST,
    SOUTH_WEST_WEST  = SOUTH_WEST + WEST,
    SOUTH_EAST_EAST  = SOUTH_EAST + EAST,
    SOUTH_SOUTH_WEST = SOUTH + SOUTH_WEST,
    SOUTH_SOUTH_EAST = SOUTH + SOUTH_EAST
};

constexpr Square64 operator+(Square64& sq1, int sq2) { return Square64(int(sq1) + sq2); }
inline Square64& operator++(Square64& sq) { return sq = Square64(int(sq) + 1); }

constexpr Square64 operator-(Square64& sq1, int sq2) { return Square64(int(sq1) - sq2); }
inline Square64& operator--(Square64& sq) { return sq = Square64(int(sq) - 1); }

inline File& operator++(File& f) { return f = File(int(f) + 1); }
inline File& operator--(File& f) { return f = File(int(f) - 1); }
constexpr File operator+(File f1, int f2) { return File (int(f1) + f2); }
inline File&   operator+=(File& f1, int f2) { return f1 = f1 + f2; }

inline Rank& operator--(Rank& r) { return r = Rank(int(r) - 1); }
inline Rank& operator++(Rank& r) { return r = Rank(int(r) + 1); }
constexpr Rank operator+(Rank r1, int r2) { return Rank(int(r1) + r2); }
inline Rank& operator+=(Rank& r1, int r2) { return r1 = r1 + r2; }

constexpr CastlingRight operator|(CastlingRight cr1, CastlingRight cr2){
  return CastlingRight(int(cr1) | int(cr2));
}

constexpr CastlingRight operator|(CastlingRight cr1, int cr2){
  return CastlingRight(int(cr1) | cr2);
}

inline CastlingRight&   operator|=(CastlingRight& cr1, int cr2){ return cr1 = CastlingRight(cr1 | cr2); }
constexpr CastlingRight   operator&(CastlingRight cr1, CastlingRight cr2){ return CastlingRight(int(cr1) & int(cr2)); }
constexpr CastlingRight   operator&(CastlingRight cr1, int cr2){ return CastlingRight(int(cr1) & cr2); }
inline CastlingRight&   operator&=(CastlingRight& cr1, int cr2){ return cr1 = CastlingRight(cr1 & cr2); }

} // namespace Xake
