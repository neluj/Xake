#pragma once

#include "bitboards.h"
#include "move.h"
#include <sstream>


namespace ChessGame{

    struct HistoryInfo{
        Move nextMove;
        CastlingRight castlingRight;
        unsigned short int fiftyHalfMoves;
        unsigned short int fullMoves;
        Square64 enpassantSquare;
    };

class Position{

public:
    
    bool set_FEN(std::string fenNotation);
    std::string get_FEN() const;
    Color get_side_to_move() const;
    int get_ply() const;
    CastlingRight get_castling_right() const;
    Square64 get_enpassant_square() const;
    unsigned short get_fifty_moves_counter() const;
    unsigned short get_moves_counter() const;
    Piece get_mailbox_piece(Square64 square) const;
    Bitboard get_pieceTypes_bitboard(Color color, PieceType pieceType) const;
    Bitboard get_occupied_bitboard(Color color) const;
    bool square_is_attacked_bySide(Square64 square, Color side) const;
    bool has_threefold_repetition() const;
    bool has_insufficient_material() const;

    //Move related functions
    bool do_move(Move move);
    void undo_move();
    void move_piece(Square64 from, Square64 to);
    void remove_piece(Square64 square);
    void add_piece(Square64 square, Piece piece);   

private:

    void clear_position_info();
    void clear_pieceTypes_bitboards();
    void clear_occupied_bitboards();
    void clear_mailbox();
    bool same_repetition_state(const Position& other) const;
    Square64 repetition_enpassant_square() const;
    
    Bitboard pieceTypesBitboards[COLOR_SIZE][PIECETYPE_SIZE];
    Bitboard occupiedBitboards[COLOR_SIZE];
    Piece mailbox[SQ64_SIZE];
    Color sideToMove{COLOR_NC};
    int ply;
    HistoryInfo moveHistory[MAX_GAME_MOVES];
};

inline Color Position::get_side_to_move() const{
    return sideToMove;
}
inline int Position::get_ply() const{
    return ply;
}
inline CastlingRight Position::get_castling_right() const{
    return moveHistory[ply-1].castlingRight;
}
inline Square64 Position::get_enpassant_square() const{
    return moveHistory[ply-1].enpassantSquare;
}
inline unsigned short Position::get_fifty_moves_counter() const{
    return moveHistory[ply-1].fiftyHalfMoves;
}
inline unsigned short Position::get_moves_counter() const{
    return moveHistory[ply-1].fullMoves;
}
inline Piece Position::get_mailbox_piece(Square64 square) const{
    return mailbox[square];
}
inline Bitboard Position::get_pieceTypes_bitboard(Color color, PieceType pieceType) const{
    return pieceTypesBitboards[color][pieceType];
}
inline Bitboard Position::get_occupied_bitboard(Color color) const{
    return occupiedBitboards[color];
}

} // namespace ChessGame
