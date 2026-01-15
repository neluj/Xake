#include "position.h"

Position::Position()
{
    clear();
}

void Position::clear()
{
    for (int c = 0; c < COLOR_NB; ++c) {
        for (int p = 0; p < PIECE_NB; ++p) {
            bb[c][p] = 0;
        }
        occ[c] = 0;
    }
    occAll = 0;
    stm = WHITE;
    castling = 0;
    epSquare = -1;
    halfmove = 0;
    fullmove = 1;
}

void Position::recomputeOcc()
{
    for (int c = 0; c < COLOR_NB; ++c) {
        occ[c] = 0;
        for (int p = 0; p < PIECE_NB; ++p) {
            occ[c] |= bb[c][p];
        }
    }
    occAll = occ[WHITE] | occ[BLACK];
}

Piece Position::pieceAt(int sq, Color& outColor) const
{
    if (sq < 0 || sq >= 64) {
        outColor = WHITE;
        return NO_PIECE;
    }

    const Bitboard mask = bb_of(sq);
    for (int c = 0; c < COLOR_NB; ++c) {
        for (int p = 0; p < PIECE_NB; ++p) {
            if (bb[c][p] & mask) {
                outColor = static_cast<Color>(c);
                return static_cast<Piece>(p);
            }
        }
    }

    outColor = WHITE;
    return NO_PIECE;
}

bool Position::movePiece(int fromSq, int toSq)
{
    if (fromSq < 0 || fromSq >= 64 || toSq < 0 || toSq >= 64 || fromSq == toSq) {
        return false;
    }

    Color fromColor = WHITE;
    const Piece fromPiece = pieceAt(fromSq, fromColor);
    if (fromPiece == NO_PIECE) {
        return false;
    }

    Color toColor = WHITE;
    const Piece toPiece = pieceAt(toSq, toColor);
    if (toPiece != NO_PIECE && toColor == fromColor) {
        return false;
    }

    const Bitboard fromMask = bb_of(fromSq);
    const Bitboard toMask = bb_of(toSq);

    bb[fromColor][fromPiece] &= ~fromMask;
    bb[fromColor][fromPiece] |= toMask;
    occ[fromColor] = (occ[fromColor] & ~fromMask) | toMask;

    if (toPiece != NO_PIECE) {
        bb[toColor][toPiece] &= ~toMask;
        occ[toColor] &= ~toMask;
    }

    occAll = occ[WHITE] | occ[BLACK];
    return true;
}
