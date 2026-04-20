#pragma once

#include "types.h"
#include <cstdint>
namespace ChessGame{

/*Move:
.... 0000 0000 0000 0000 0000 0000 0000 0011 1111 -> From
.... 0000 0000 0000 0000 0000 0000 1111 1100 0000 -> To
.... 0000 0000 0000 0000 0000 0011 0000 0000 0000 -> Special move flag
.... 0000 0000 0000 0000 0001 1100 0000 0000 0000 -> Promotion Piece Type
.... 0000 0000 0000 0001 1110 0000 0000 0000 0000 -> Captured piece
.... 0000 0000 0001 1110 0000 0000 0000 0000 0000 -> Attacker piece
.... 1111 1111 1110 0000 0000 0000 0000 0000 0000 -> Score
*/

/*Move Scores:
1000 0000 0000 0000 ... -> PVMove  0x4000
0111 1100 0000 0000 ... -> MVVLVA  0x3F80
0000 0010 0000 0000 ... -> Killer1 0x40
0000 0001 0000 0000 ... -> Killer2 0x20
.... 1111 1111 1111 111 -> History Heuristic
*/

using Move = uint64_t;
constexpr Move NOMOVE = 0;

using MoveScore = uint64_t;
constexpr MoveScore MS_ONE = 1;


constexpr int MAX_KILLERMOVES = 2;

constexpr MoveScore PV_SCORE = (MS_ONE << 38);
constexpr MoveScore CAP_SHIFT = 33;
constexpr MoveScore KILLERMOVE_SOCORE_0 = (MS_ONE << 32);
constexpr MoveScore KILLERMOVE_SOCORE_1 = (MS_ONE << 31);

//[attackerType][capturedType]
constexpr MoveScore MVVLVAScores[PIECETYPE_SIZE][PIECETYPE_SIZE] = {
    {0,  0,  0,  0,  0,  0,  0},
    {0, (MoveScore{6} << CAP_SHIFT), (MoveScore{12} << CAP_SHIFT), (MoveScore{18} << CAP_SHIFT), (MoveScore{24} << CAP_SHIFT), (MoveScore{30} << CAP_SHIFT), 0},
    {0, (MoveScore{5} << CAP_SHIFT), (MoveScore{11} << CAP_SHIFT), (MoveScore{17} << CAP_SHIFT), (MoveScore{23} << CAP_SHIFT), (MoveScore{29} << CAP_SHIFT), 0},
    {0, (MoveScore{4} << CAP_SHIFT), (MoveScore{10} << CAP_SHIFT), (MoveScore{16} << CAP_SHIFT), (MoveScore{22} << CAP_SHIFT), (MoveScore{28} << CAP_SHIFT), 0},
    {0, (MoveScore{3} << CAP_SHIFT), (MoveScore{9} <<  CAP_SHIFT), (MoveScore{15} << CAP_SHIFT), (MoveScore{21} << CAP_SHIFT), (MoveScore{27} << CAP_SHIFT), 0},
    {0, (MoveScore{2} << CAP_SHIFT), (MoveScore{8} <<  CAP_SHIFT), (MoveScore{14} << CAP_SHIFT), (MoveScore{20} << CAP_SHIFT), (MoveScore{26} << CAP_SHIFT), 0},
    {0, (MoveScore{1} << CAP_SHIFT), (MoveScore{7} <<  CAP_SHIFT), (MoveScore{13} << CAP_SHIFT), (MoveScore{19} << CAP_SHIFT), (MoveScore{25} << CAP_SHIFT), 0} 
};

enum MoveType{
    QUIET,
    CAPTURE
};

enum SpecialMove: int{
    NO_SPECIAL          = 0,
    ENPASSANT           = 1,
    PAWN_START          = 2,
    CASTLE              = 3,
    PROMOTION_KNIGHT    = KNIGHT    << 2, 
    PROMOTION_BISHOP    = BISHOP    << 2, 
    PROMOTION_ROOK      = ROOK      << 2, 
    PROMOTION_QUEEN     = QUEEN     << 2
};

inline Move make_quiet_move(Square64 from, Square64 to, SpecialMove SpecialMove) {
    return Move((SpecialMove << 12) | (to << 6) | from);
}

inline Move make_capture_move(Square64 from, Square64 to, SpecialMove SpecialMove, Piece attackerPiece , Piece capturedPiece) {
    return Move((attackerPiece << 21) | (capturedPiece << 17) | (SpecialMove << 12) | (to << 6) | from);
}

inline Move make_enpassant_move(Square64 from, Square64 to) {
    return Move((ENPASSANT << 12) | (to << 6) | from);
}

inline Square64 move_from(Move move) {
	return Square64(move & 0x3f);
}

inline Square64 move_to(Move move) {
	return Square64((move >> 6) & 0x3f);
}

inline SpecialMove move_special(Move move) {
    return SpecialMove((move >> 12) & 0x1F);
}

inline PieceType promoted_piece(Move move){
    return PieceType((move >> 14) & 0x07);
} 

inline Piece captured_piece(Move move){
    return Piece((move >> 17) & 0x0f);
}

inline Piece attacker_piece(Move move){
    return Piece((move >> 21) & 0x0f);
} 

inline MoveScore move_score(Move move){
    return move >> 25;
}

inline Move set_heuristic_score(Move move, MoveScore moveScore){
    return move | (moveScore << 25);
}

inline bool is_capture(Move m) {
    return captured_piece(m) != NO_PIECE || move_special(m) == ENPASSANT;
}

inline Move raw_move(Move move){
    return move & 0x1FFFFFFULL;
}

inline bool equal_move(Move move1, Move move2){
    return raw_move(move1) == raw_move(move2);
}

inline std::string algebraic_move(Move move) {
    std::string algebraic_move;
    Square64 from   = move_from(move);
    Square64 to     = move_to(move);
    algebraic_move = SQUARE_NAMES[from] + SQUARE_NAMES[to];

    PieceType promoted = promoted_piece(move);

    switch (promoted)
    {
        case KNIGHT:    algebraic_move += 'n'; break;
        case BISHOP:    algebraic_move += 'b'; break;
        case ROOK:      algebraic_move += 'r'; break;
        case QUEEN:     algebraic_move += 'q'; break;
        default:                               break;
    }

    return algebraic_move;
}

} // namespace ChessGame


