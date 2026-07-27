#include "movegen.h"
#include "position.h"
#include "attacks.h"

namespace Xake{

namespace MoveGen{

//DECLARATIONS
//PAWNS
void white_pawn_moves(const Position &pos, MoveList &moveList);
void white_pawn_capture_moves(const Position &pos, MoveList &moveList);
void white_pawn_quiet_moves(const Position &pos, MoveList &moveList);

void black_pawn_moves(const Position &pos, MoveList &moveList);
void black_pawn_capture_moves(const Position &pos, MoveList &moveList);
void black_pawn_quiet_moves(const Position &pos, MoveList &moveList);

//CASTLING
template<Color C>
void castling_moves(const Position &pos, MoveList &moveList);

template<Color C, MoveType MT>
void bishop_moves(const Position &pos, MoveList &moveList);

template<Color C, MoveType MT>
void rook_moves(const Position &pos, MoveList &moveList);

template<Color C, MoveType MT>
void queen_moves(const Position &pos, MoveList &moveList);

//HELPERS
template<Color C, PieceType PT, MoveType MT>
void no_special_moves(const Position &pos, MoveList &moveList);

template<Direction D, SpecialMove SM>
void extract_quiet_moves(Bitboard toBitboard, MoveList &moveList);

template<SpecialMove SM>
void extract_quiet_moves(Square64 from, Bitboard toBitboard, MoveList &moveList);

template<Direction D, SpecialMove SM>
void extract_capture_moves(const Position &pos, Bitboard toBitboard, MoveList &moveList);

template<SpecialMove SM>
void extract_capture_moves(const Position &pos, Square64 from, Bitboard toBitboard, MoveList &moveList);

template<PieceType PT>
constexpr const Bitboard* non_sliding_attack_table();

//DEFINITIONS
void generate_pseudo_moves(const Position &pos, MoveList &moveList){
    Attacks::init();
    if(pos.get_side_to_move() == WHITE){
        white_pawn_moves(pos, moveList);
        no_special_moves<WHITE, KNIGHT, CAPTURE>(pos, moveList);
        no_special_moves<WHITE, KNIGHT, QUIET>(pos, moveList);
        no_special_moves<WHITE, KING, CAPTURE>(pos, moveList);
        no_special_moves<WHITE, KING, QUIET>(pos, moveList);
        castling_moves<WHITE>(pos, moveList);
        bishop_moves<WHITE, CAPTURE>(pos, moveList);
        bishop_moves<WHITE, QUIET>(pos, moveList); 
        rook_moves<WHITE, CAPTURE>(pos, moveList);
        rook_moves<WHITE, QUIET>(pos, moveList);  
        queen_moves<WHITE, CAPTURE>(pos, moveList);
        queen_moves<WHITE, QUIET>(pos, moveList);  
    }
    else{
        black_pawn_moves(pos, moveList);
        no_special_moves<BLACK, KNIGHT, CAPTURE>(pos, moveList);
        no_special_moves<BLACK, KNIGHT, QUIET>(pos, moveList);
        no_special_moves<BLACK, KING, CAPTURE>(pos, moveList);
        no_special_moves<BLACK, KING, QUIET>(pos, moveList);
        castling_moves<BLACK>(pos, moveList);
        bishop_moves<BLACK, CAPTURE>(pos, moveList);
        bishop_moves<BLACK, QUIET>(pos, moveList);
        rook_moves<BLACK, CAPTURE>(pos, moveList);
        rook_moves<BLACK, QUIET>(pos, moveList);   
        queen_moves<BLACK, CAPTURE>(pos, moveList);
        queen_moves<BLACK, QUIET>(pos, moveList);    
    }  
}

void white_pawn_moves(const Position &pos, MoveList &moveList){
    white_pawn_capture_moves(pos, moveList);
    white_pawn_quiet_moves(pos, moveList);
}

void black_pawn_moves(const Position &pos, MoveList &moveList){
    black_pawn_capture_moves(pos, moveList);
    black_pawn_quiet_moves(pos, moveList);
}

void generate_pseudo_captures(Position &pos, MoveList &moveList){
    Attacks::init();
    if(pos.get_side_to_move() == WHITE){
        white_pawn_capture_moves(pos, moveList);
        no_special_moves<WHITE, KNIGHT, CAPTURE>(pos, moveList);
        no_special_moves<WHITE, KING, CAPTURE>(pos, moveList);
        bishop_moves<WHITE, CAPTURE>(pos, moveList);
        rook_moves<WHITE, CAPTURE>(pos, moveList);
        queen_moves<WHITE, CAPTURE>(pos, moveList);
    }
    else{
        black_pawn_capture_moves(pos, moveList);
        no_special_moves<BLACK, KNIGHT, CAPTURE>(pos, moveList);
        no_special_moves<BLACK, KING, CAPTURE>(pos, moveList);
        bishop_moves<BLACK, CAPTURE>(pos, moveList);
        rook_moves<BLACK, CAPTURE>(pos, moveList);
        queen_moves<BLACK, CAPTURE>(pos, moveList);
    }   
}

void white_pawn_capture_moves(const Position &pos, MoveList &moveList){
    
    Bitboard northEastMoves = Bitboards::make_direction<NORTH_EAST>(pos.get_pieceTypes_bitboard(WHITE, PAWN));
    Bitboard occupiedBlackBitboard = pos.get_occupied_bitboard(BLACK);
    Bitboard northEastCaptures = northEastMoves & occupiedBlackBitboard & ~Bitboards::RANK_8_MASK;
    extract_capture_moves<NORTH_EAST, NO_SPECIAL>(pos, northEastCaptures, moveList);
    Bitboard northWestMoves = Bitboards::make_direction<NORTH_WEST>(pos.get_pieceTypes_bitboard(WHITE, PAWN));
    Bitboard northWestCaptures = northWestMoves & occupiedBlackBitboard & ~Bitboards::RANK_8_MASK;
    extract_capture_moves<NORTH_WEST, NO_SPECIAL>(pos, northWestCaptures, moveList);
    Bitboard northEastCapturesPromotions = northEastMoves & occupiedBlackBitboard & Bitboards::RANK_8_MASK;
    extract_capture_moves<NORTH_EAST, PROMOTION_QUEEN>(pos, northEastCapturesPromotions, moveList);
    extract_capture_moves<NORTH_EAST, PROMOTION_KNIGHT>(pos, northEastCapturesPromotions, moveList);
    extract_capture_moves<NORTH_EAST, PROMOTION_ROOK> (pos, northEastCapturesPromotions, moveList);
    extract_capture_moves<NORTH_EAST, PROMOTION_BISHOP>  (pos, northEastCapturesPromotions, moveList);
    Bitboard northWestCapturesPromotions = northWestMoves & occupiedBlackBitboard & Bitboards::RANK_8_MASK;
    extract_capture_moves<NORTH_WEST, PROMOTION_QUEEN>(pos, northWestCapturesPromotions, moveList);
    extract_capture_moves<NORTH_WEST, PROMOTION_KNIGHT>(pos, northWestCapturesPromotions, moveList);
    extract_capture_moves<NORTH_WEST, PROMOTION_ROOK> (pos, northWestCapturesPromotions, moveList);
    extract_capture_moves<NORTH_WEST, PROMOTION_BISHOP>  (pos, northWestCapturesPromotions, moveList);

    //Enpassant
    if(pos.get_enpassant_square() != SQ64_NO_SQUARE){
        Square64 enpassantSquare = pos.get_enpassant_square();
        Bitboard enpassantMask = Bitboards::set_pieces(enpassantSquare);
        if(northEastMoves & enpassantMask){
            moveList.set_move(make_enpassant_move(enpassantSquare-NORTH_EAST, enpassantSquare));
        }
        if(northWestMoves & enpassantMask){
            moveList.set_move(make_enpassant_move(enpassantSquare-NORTH_WEST, enpassantSquare));
        }
    }
}

void white_pawn_quiet_moves(const Position &pos, MoveList &moveList){

    Bitboard quietSimpleMoves = Bitboards::make_direction<NORTH>(pos.get_pieceTypes_bitboard(WHITE, PAWN)) & ~pos.get_occupied_bitboard(COLOR_NC);
    Bitboard notPromotionQuietMoves = quietSimpleMoves & ~Bitboards::RANK_8_MASK;
    extract_quiet_moves<NORTH, SpecialMove::NO_SPECIAL>(notPromotionQuietMoves, moveList);
    Bitboard startMoves = Bitboards::make_direction<NORTH>(quietSimpleMoves & Bitboards::RANK_3_MASK) & ~pos.get_occupied_bitboard(COLOR_NC);
    extract_quiet_moves<NORTH_NORTH, SpecialMove::PAWN_START>(startMoves, moveList);

    Bitboard promotionQuietMoves = quietSimpleMoves & Bitboards::RANK_8_MASK;
    extract_quiet_moves<NORTH, PROMOTION_QUEEN>(promotionQuietMoves, moveList);
    extract_quiet_moves<NORTH, PROMOTION_KNIGHT>(promotionQuietMoves, moveList);
    extract_quiet_moves<NORTH, PROMOTION_ROOK> (promotionQuietMoves, moveList);
    extract_quiet_moves<NORTH, PROMOTION_BISHOP>  (promotionQuietMoves, moveList);

}


void black_pawn_capture_moves(const Position &pos, MoveList &moveList){

    Bitboard southEastMoves = Bitboards::make_direction<SOUTH_EAST>(pos.get_pieceTypes_bitboard(BLACK, PAWN));
    Bitboard occupiedWhiteBitboard = pos.get_occupied_bitboard(WHITE);
    Bitboard southEastCaptures = southEastMoves & occupiedWhiteBitboard & ~Bitboards::RANK_1_MASK;
    extract_capture_moves<SOUTH_EAST, NO_SPECIAL>(pos, southEastCaptures, moveList);
    Bitboard southWestMoves = Bitboards::make_direction<SOUTH_WEST>(pos.get_pieceTypes_bitboard(BLACK, PAWN));
    Bitboard southWestCaptures = southWestMoves & occupiedWhiteBitboard & ~Bitboards::RANK_1_MASK;
    extract_capture_moves<SOUTH_WEST, NO_SPECIAL>(pos, southWestCaptures, moveList);
    Bitboard southEastCapturesPromotions = southEastMoves & occupiedWhiteBitboard & Bitboards::RANK_1_MASK;
    extract_capture_moves<SOUTH_EAST, PROMOTION_QUEEN>(pos, southEastCapturesPromotions, moveList);
    extract_capture_moves<SOUTH_EAST, PROMOTION_KNIGHT>(pos, southEastCapturesPromotions, moveList);
    extract_capture_moves<SOUTH_EAST, PROMOTION_ROOK> (pos, southEastCapturesPromotions, moveList);
    extract_capture_moves<SOUTH_EAST, PROMOTION_BISHOP>  (pos, southEastCapturesPromotions, moveList);
    Bitboard southWestCapturesPromotions = southWestMoves & occupiedWhiteBitboard & Bitboards::RANK_1_MASK;
    extract_capture_moves<SOUTH_WEST, PROMOTION_QUEEN>(pos, southWestCapturesPromotions, moveList);
    extract_capture_moves<SOUTH_WEST, PROMOTION_KNIGHT>(pos, southWestCapturesPromotions, moveList);
    extract_capture_moves<SOUTH_WEST, PROMOTION_ROOK> (pos, southWestCapturesPromotions, moveList);
    extract_capture_moves<SOUTH_WEST, PROMOTION_BISHOP>  (pos, southWestCapturesPromotions, moveList);

    //Enpassant
    if(pos.get_enpassant_square() != SQ64_NO_SQUARE){
        Square64 enpassantSquare = pos.get_enpassant_square();
        Bitboard enpassantMask = Bitboards::set_pieces(enpassantSquare);
        if(southEastMoves & enpassantMask){
            moveList.set_move(make_enpassant_move(enpassantSquare-SOUTH_EAST, enpassantSquare));
        }
        if(southWestMoves & enpassantMask){
            moveList.set_move(make_enpassant_move(enpassantSquare-SOUTH_WEST, enpassantSquare));
        }
    }
}

void black_pawn_quiet_moves(const Position &pos, MoveList &moveList){

    Bitboard quietSimpleMoves = Bitboards::make_direction<SOUTH>(pos.get_pieceTypes_bitboard(BLACK, PAWN)) & ~pos.get_occupied_bitboard(COLOR_NC);
    Bitboard notPromotionQuietMoves = quietSimpleMoves & ~Bitboards::RANK_1_MASK;
    extract_quiet_moves<SOUTH, SpecialMove::NO_SPECIAL>(notPromotionQuietMoves, moveList);
    Bitboard startMoves = Bitboards::make_direction<SOUTH>(quietSimpleMoves & Bitboards::RANK_6_MASK) & ~pos.get_occupied_bitboard(COLOR_NC);
    extract_quiet_moves<SOUTH_SOUTH, SpecialMove::PAWN_START>(startMoves, moveList);

    Bitboard promotionQuietMoves = quietSimpleMoves & Bitboards::RANK_1_MASK;
    extract_quiet_moves<SOUTH, PROMOTION_QUEEN>(promotionQuietMoves, moveList);
    extract_quiet_moves<SOUTH, PROMOTION_KNIGHT>(promotionQuietMoves, moveList);
    extract_quiet_moves<SOUTH, PROMOTION_ROOK> (promotionQuietMoves, moveList);
    extract_quiet_moves<SOUTH, PROMOTION_BISHOP>  (promotionQuietMoves, moveList);

}

template<Color C, PieceType PT, MoveType MT>
void no_special_moves(const Position &pos, MoveList &moveList){

    Bitboard pieceBitboards = pos.get_pieceTypes_bitboard(C, PT);
    Bitboard attackMoves{ZERO};
    while (pieceBitboards) {
        Square64 fromSquare{Bitboards::ctz(pieceBitboards)}; 

        if constexpr(MT == QUIET){
            attackMoves = non_sliding_attack_table<PT>()[fromSquare] & ~pos.get_occupied_bitboard(COLOR_NC);
            extract_quiet_moves<NO_SPECIAL>(fromSquare, attackMoves, moveList);

        }
        if constexpr(MT == CAPTURE){
            attackMoves = non_sliding_attack_table<PT>()[fromSquare] & pos.get_occupied_bitboard(~C);
            extract_capture_moves<NO_SPECIAL>(pos, fromSquare, attackMoves, moveList);
        }
        pieceBitboards &= pieceBitboards - 1;
    }
}

template<Color C>
void castling_moves(const Position &pos, MoveList &moveList){
    CastlingRight castlingRights = pos.get_castling_right();
    if constexpr(C == WHITE){
        if(castlingRights & CastlingRight::WKCA){
            if((pos.get_occupied_bitboard(COLOR_NC) & 0x0000000000000060) == 0){
                if(!pos.square_is_attacked_bySide(SQ64_F1, BLACK) && !pos.square_is_attacked_bySide(SQ64_E1, BLACK)){
                    moveList.set_move(make_quiet_move(SQ64_E1, SQ64_G1, SpecialMove::CASTLE));
                }
            }
        }
        if(castlingRights & CastlingRight::WQCA){
            if((pos.get_occupied_bitboard(COLOR_NC) & 0x000000000000000E) == 0){
                if(!pos.square_is_attacked_bySide(SQ64_E1, BLACK) && !pos.square_is_attacked_bySide(SQ64_D1, BLACK)){
                    moveList.set_move(make_quiet_move(SQ64_E1, SQ64_C1, SpecialMove::CASTLE));
                }
            }
        }
    }
    else if constexpr(C == BLACK){
        if(castlingRights & CastlingRight::BKCA){
            if((pos.get_occupied_bitboard(COLOR_NC) & 0x6000000000000000) == 0){
                if(!pos.square_is_attacked_bySide(SQ64_E8, WHITE) && !pos.square_is_attacked_bySide(SQ64_F8, WHITE)){
                    moveList.set_move(make_quiet_move(SQ64_E8, SQ64_G8, SpecialMove::CASTLE));
                }
            }
        }
        if(castlingRights & CastlingRight::BQCA){
            if((pos.get_occupied_bitboard(COLOR_NC) & 0x0E00000000000000) == 0){
                if(!pos.square_is_attacked_bySide(SQ64_E8, WHITE) && !pos.square_is_attacked_bySide(SQ64_D8, WHITE)){
                    moveList.set_move(make_quiet_move(SQ64_E8, SQ64_C8, SpecialMove::CASTLE));
                }
            }
        }
    }  
}

template<Color C, MoveType MT>
void bishop_moves(const Position &pos, MoveList &moveList){

    Bitboard fromBitboard = pos.get_pieceTypes_bitboard(C, BISHOP);
    while (fromBitboard) {
        Square64 from{Bitboards::ctz(fromBitboard)};
        Bitboard attacks = Attacks::sliding_diagonal_attacks(from, pos.get_occupied_bitboard(COLOR_NC));
        Bitboard captureAttacks = pos.get_occupied_bitboard(~C) & attacks;
        
        if constexpr(MT == QUIET){
            Bitboard quietAttacks = attacks & ~captureAttacks &  ~pos.get_occupied_bitboard(C);
            extract_quiet_moves<NO_SPECIAL>(from, quietAttacks, moveList);
        }
        if constexpr(MT == CAPTURE){
            extract_capture_moves<NO_SPECIAL>(pos, from, captureAttacks, moveList);
        }
        fromBitboard &= fromBitboard - 1;
    }
}

template<Color C, MoveType MT>
void rook_moves(const Position &pos, MoveList &moveList){

    Bitboard fromBitboard = pos.get_pieceTypes_bitboard(C, ROOK);
    while (fromBitboard) {
        Square64 from{Bitboards::ctz(fromBitboard)};
        Bitboard attacks = Attacks::sliding_side_attacks(from, pos.get_occupied_bitboard(COLOR_NC));
        Bitboard captureAttacks = pos.get_occupied_bitboard(~C) & attacks;
        
        if constexpr(MT == QUIET){
            Bitboard quietAttacks = attacks & ~captureAttacks &  ~pos.get_occupied_bitboard(C);
            extract_quiet_moves<NO_SPECIAL>(from, quietAttacks, moveList);
        }
        if constexpr(MT == CAPTURE){
            extract_capture_moves<NO_SPECIAL>(pos, from, captureAttacks, moveList);
        }
        fromBitboard &= fromBitboard - 1;
    }
}

template<Color C, MoveType MT>
void queen_moves(const Position &pos, MoveList &moveList){

    Bitboard fromBitboard = pos.get_pieceTypes_bitboard(C, QUEEN);
    while (fromBitboard) {
        Square64 from{Bitboards::ctz(fromBitboard)};
        Bitboard attacks =   Attacks::sliding_diagonal_attacks(from, pos.get_occupied_bitboard(COLOR_NC))
                           | Attacks::sliding_side_attacks(from, pos.get_occupied_bitboard(COLOR_NC));
        Bitboard captureAttacks = pos.get_occupied_bitboard(~C) & attacks;
        
        if constexpr(MT == QUIET){
            Bitboard quietAttacks = attacks & ~captureAttacks & ~pos.get_occupied_bitboard(C);
            extract_quiet_moves<NO_SPECIAL>(from, quietAttacks, moveList);
        }
        if constexpr(MT == CAPTURE){
            extract_capture_moves<NO_SPECIAL>(pos, from, captureAttacks, moveList);
        }
        fromBitboard &= fromBitboard - 1;
    }
}

template<PieceType PT>
constexpr const Bitboard* non_sliding_attack_table();
 
template<>
constexpr const Bitboard* non_sliding_attack_table<KNIGHT>() {
    return Attacks::knightAttacks;
}

template<>
constexpr const Bitboard* non_sliding_attack_table<KING>() {
    return Attacks::kingAttacks;
}

template<Direction D, SpecialMove SM>
void extract_quiet_moves(Bitboard toBitboard, MoveList &moveList){

    while (toBitboard) {
        Square64 to{Bitboards::ctz(toBitboard)};
        Square64 moveFrom{to - D};
        moveList.set_move(make_quiet_move(moveFrom, to, SM));
        toBitboard &= toBitboard - 1;
    }

}

template<SpecialMove SM>
void extract_quiet_moves(Square64 from, Bitboard toBitboard, MoveList &moveList){

    while (toBitboard) {
        Square64 to{Bitboards::ctz(toBitboard)};
        moveList.set_move(make_quiet_move(from, to, SM));
        toBitboard &= toBitboard - 1;
    }
}

template<Direction D, SpecialMove SM>
void extract_capture_moves(const Position &pos, Bitboard toBitboard, MoveList &moveList){

    while (toBitboard) {
        Square64 to{Bitboards::ctz(toBitboard)};
        Square64 from{to - D};
        moveList.set_move(make_capture_move(from, to, SM, pos.get_mailbox_piece(from), pos.get_mailbox_piece(to)));
        toBitboard &= toBitboard - 1;
    }

}

template<SpecialMove SM>
void extract_capture_moves(const Position &pos, Square64 from, Bitboard toBitboard, MoveList &moveList){

    while (toBitboard) {
        Square64 to{Bitboards::ctz(toBitboard)};
        moveList.set_move(make_capture_move(from, to, SM, pos.get_mailbox_piece(from), pos.get_mailbox_piece(to)));
        toBitboard &= toBitboard - 1;
    }

}

}
} 
