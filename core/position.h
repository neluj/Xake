#pragma once

#include "types.h"

class Position
{
public:
    Bitboard bb[COLOR_NB][PIECE_NB];
    Bitboard occ[COLOR_NB];
    Bitboard occAll;
    Color stm;
    int castling;
    int epSquare;
    int halfmove;
    int fullmove;

    Position();

    void clear();
    void recomputeOcc();
    Piece pieceAt(int sq, Color& outColor) const;
    bool movePiece(int fromSq, int toSq);
};
