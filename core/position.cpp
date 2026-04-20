#include "position.h"
#include "attacks.h"

#include <limits>
#include <random>

namespace ChessGame{

namespace {

bool piece_from_fen_char(char token, Piece& piece)
{
    switch (token) {
    case 'P': piece = W_PAWN; return true;
    case 'N': piece = W_KNIGHT; return true;
    case 'B': piece = W_BISHOP; return true;
    case 'R': piece = W_ROOK; return true;
    case 'Q': piece = W_QUEEN; return true;
    case 'K': piece = W_KING; return true;
    case 'p': piece = B_PAWN; return true;
    case 'n': piece = B_KNIGHT; return true;
    case 'b': piece = B_BISHOP; return true;
    case 'r': piece = B_ROOK; return true;
    case 'q': piece = B_QUEEN; return true;
    case 'k': piece = B_KING; return true;
    default:
        return false;
    }
}

bool parse_counter_field(const std::string& token,
                         unsigned short int minimumValue,
                         unsigned short int& outValue)
{
    std::istringstream iss(token);
    int value = 0;
    char extra = '\0';
    if (!(iss >> value) || (iss >> extra)) {
        return false;
    }
    if (value < minimumValue || value > std::numeric_limits<unsigned short int>::max()) {
        return false;
    }
    outValue = static_cast<unsigned short int>(value);
    return true;
}

bool parse_castling_field(const std::string& token, CastlingRight& castlingRight)
{
    if (token == "-") {
        castlingRight = NO_RIGHT;
        return true;
    }

    int rights = NO_RIGHT;
    for (char c : token) {
        int bit = NO_RIGHT;
        switch (c) {
        case 'K': bit = WKCA; break;
        case 'Q': bit = WQCA; break;
        case 'k': bit = BKCA; break;
        case 'q': bit = BQCA; break;
        default:
            return false;
        }

        if (rights & bit) {
            return false;
        }
        rights |= bit;
    }

    castlingRight = CastlingRight(rights);
    return true;
}

bool parse_enpassant_field(const std::string& token, Square64& square)
{
    if (token == "-") {
        square = SQ64_NO_SQUARE;
        return true;
    }

    if (token.size() != 2) {
        return false;
    }

    const char file = token[0];
    const char rank = token[1];
    if (file < 'a' || file > 'h') {
        return false;
    }
    if (rank != '3' && rank != '6') {
        return false;
    }

    square = make_square64(Rank(rank - '1'), File(file - 'a'));
    return true;
}

bool castling_rights_match_position(const Position& position)
{
    const CastlingRight rights = position.get_castling_right();

    if ((rights & WKCA)
        && (position.get_mailbox_piece(SQ64_E1) != W_KING
            || position.get_mailbox_piece(SQ64_H1) != W_ROOK)) {
        return false;
    }

    if ((rights & WQCA)
        && (position.get_mailbox_piece(SQ64_E1) != W_KING
            || position.get_mailbox_piece(SQ64_A1) != W_ROOK)) {
        return false;
    }

    if ((rights & BKCA)
        && (position.get_mailbox_piece(SQ64_E8) != B_KING
            || position.get_mailbox_piece(SQ64_H8) != B_ROOK)) {
        return false;
    }

    if ((rights & BQCA)
        && (position.get_mailbox_piece(SQ64_E8) != B_KING
            || position.get_mailbox_piece(SQ64_A8) != B_ROOK)) {
        return false;
    }

    return true;
}

bool enpassant_square_matches_position(const Position& position)
{
    const Square64 enpassantSquare = position.get_enpassant_square();
    if (enpassantSquare == SQ64_NO_SQUARE) {
        return true;
    }

    if (position.get_mailbox_piece(enpassantSquare) != NO_PIECE) {
        return false;
    }

    if (position.get_side_to_move() == WHITE) {
        if (square_rank(enpassantSquare) != RANK_6) {
            return false;
        }
        return position.get_mailbox_piece(Square64(int(enpassantSquare) + SOUTH)) == B_PAWN;
    }

    if (position.get_side_to_move() == BLACK) {
        if (square_rank(enpassantSquare) != RANK_3) {
            return false;
        }
        return position.get_mailbox_piece(Square64(int(enpassantSquare) + NORTH)) == W_PAWN;
    }

    return false;
}

} // namespace

const int CASTLE_PERSMISION_UPDATES[SQ64_SIZE] = {
    13, 15, 15, 15, 12, 15, 15, 14, 
    15, 15, 15, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 
     7, 15, 15, 15,  3, 15, 15, 11,
};

void Position::clear_position_info(){

    sideToMove = COLOR_NC;

    ply = 1;

    moveHistory[ply-1].nextMove = 0;
    moveHistory[ply-1].castlingRight = NO_RIGHT;
    moveHistory[ply-1].fiftyHalfMoves = 0;
    moveHistory[ply-1].fullMoves = 1;
    moveHistory[ply-1].enpassantSquare = SQ64_NO_SQUARE;

}

void Position::clear_pieceTypes_bitboards(){
    //clear pieceTypes bitboards
    for(int c = 0; c < COLOR_SIZE; ++c){
        for(int p = 0; p < PIECETYPE_SIZE; ++p){
            pieceTypesBitboards[c][p] = ZERO;
        }
    }
}

void Position::clear_occupied_bitboards(){
    //clear occupied bitboards
    for(int c = 0; c < COLOR_SIZE; ++c){
        occupiedBitboards[c] = ZERO;
        
    }
}

void Position::clear_mailbox(){
    //clear mailbox
    for(int i = 0; i < SQ64_SIZE; ++i){
        mailbox[i] = NO_PIECE;
    }
}

bool Position::set_FEN(std::string fenNotation){

    std::istringstream iss(fenNotation);
    std::string boardField;
    std::string sideField;
    std::string castlingField;
    std::string enpassantField;
    std::string halfmoveField;
    std::string fullmoveField;

    if (!(iss >> boardField >> sideField >> castlingField >> enpassantField >> halfmoveField >> fullmoveField)) {
        return false;
    }

    std::string extraField;
    if (iss >> extraField) {
        return false;
    }

    Position parsed;
    parsed.clear_pieceTypes_bitboards();
    parsed.clear_occupied_bitboards();
    parsed.clear_mailbox();
    parsed.clear_position_info();

    int rank = int(RANK_8);
    int file = int(FILE_A);
    int whiteKings = 0;
    int blackKings = 0;

    for (char token : boardField) {
        if (token == '/') {
            if (file != int(FILE_SIZE) || rank == int(RANK_1)) {
                return false;
            }
            --rank;
            file = int(FILE_A);
            continue;
        }

        if (token >= '1' && token <= '8') {
            file += token - '0';
            if (file > int(FILE_SIZE)) {
                return false;
            }
            continue;
        }

        Piece piece = NO_PIECE;
        if (!piece_from_fen_char(token, piece)) {
            return false;
        }
        if (file >= int(FILE_SIZE)) {
            return false;
        }

        const PieceType pieceType = piece_type(piece);
        if (pieceType == PAWN && (rank == int(RANK_1) || rank == int(RANK_8))) {
            return false;
        }
        if (piece == W_KING) {
            ++whiteKings;
        } else if (piece == B_KING) {
            ++blackKings;
        }

        parsed.add_piece(make_square64(Rank(rank), File(file)), piece);
        ++file;
    }

    if (rank != int(RANK_1) || file != int(FILE_SIZE)) {
        return false;
    }
    if (whiteKings != 1 || blackKings != 1) {
        return false;
    }

    if (sideField == "w") {
        parsed.sideToMove = WHITE;
    } else if (sideField == "b") {
        parsed.sideToMove = BLACK;
    } else {
        return false;
    }

    if (!parse_castling_field(castlingField, parsed.moveHistory[parsed.ply - 1].castlingRight)) {
        return false;
    }
    if (!castling_rights_match_position(parsed)) {
        return false;
    }

    if (!parse_enpassant_field(enpassantField, parsed.moveHistory[parsed.ply - 1].enpassantSquare)) {
        return false;
    }
    if (!enpassant_square_matches_position(parsed)) {
        return false;
    }

    if (!parse_counter_field(halfmoveField, 0, parsed.moveHistory[parsed.ply - 1].fiftyHalfMoves)) {
        return false;
    }
    if (!parse_counter_field(fullmoveField, 1, parsed.moveHistory[parsed.ply - 1].fullMoves)) {
        return false;
    }

    *this = parsed;
    return true;
}

std::string Position::get_FEN() const{

    std::ostringstream oss;
    
    for (Rank rank = RANK_8; rank >= RANK_1; --rank) {
        int emptySquaresCounter = 0;
        for (File file = FILE_A; file <= FILE_H; ++file) {
            Square64 square = make_square64(rank, file);
            if (mailbox[square] == NO_PIECE) {
                ++emptySquaresCounter;
            } else {
                if (emptySquaresCounter) {
                    oss << emptySquaresCounter;
                    emptySquaresCounter = 0;
                }
                auto pn = PIECE_NAMES[mailbox[square]];
                if (pn != ' ')
                    oss << pn;
            }
        }
        if (emptySquaresCounter) {
            oss << emptySquaresCounter;
        }
        if (rank > RANK_1)
            oss << "/";
    }

    oss << (sideToMove == WHITE ? " w " : " b ");

    CastlingRight castlingRights = get_castling_right();

    if(castlingRights & CastlingRight::WKCA)
        oss << 'K';

    if(castlingRights & CastlingRight::WQCA)
        oss << 'Q';

    if(castlingRights & CastlingRight::BKCA)
        oss << 'k';
        
    if(castlingRights & CastlingRight::BQCA)
        oss << 'q';
    
    if(castlingRights == CastlingRight::NO_RIGHT)
        oss << '-';

    if(get_enpassant_square() == SQ64_NO_SQUARE)
        oss << " -";
    else
         oss << " " << SQUARE_NAMES[get_enpassant_square()];

    oss << " " << get_fifty_moves_counter();
    
    oss << " " << get_moves_counter();

    return oss.str();
}


/*returns true if the side is attacking the square*/
bool Position::square_is_attacked_bySide(Square64 sq64, Color side) const{
    Attacks::init();

    return   (Attacks::pawnAttacks[~side][sq64] & pieceTypesBitboards[side][PAWN])
           | (Attacks::knightAttacks[sq64] & pieceTypesBitboards[side][KNIGHT])
           | (Attacks::kingAttacks[sq64] &  pieceTypesBitboards[side][KING])
           | (Attacks::sliding_diagonal_attacks( sq64, occupiedBitboards[COLOR_NC]) & (pieceTypesBitboards[side][BISHOP] | pieceTypesBitboards[side][QUEEN]))
           | (Attacks::sliding_side_attacks(sq64, occupiedBitboards[COLOR_NC]) & (pieceTypesBitboards[side][ROOK] | pieceTypesBitboards[side][QUEEN]));
}


bool Position::do_move(Move move){

    Square64 from = move_from(move);
    Square64 to = move_to(move);
    SpecialMove specialMove = move_special(move);

    //Set move to the move history
    moveHistory[ply-1].nextMove = move;

    ++ply;

    if(specialMove != SpecialMove::NO_SPECIAL){
        if(specialMove == SpecialMove::ENPASSANT ){
            if(sideToMove==Color::WHITE){
                remove_piece(Square64(to+Direction::SOUTH));
            }else{
                remove_piece(Square64(to+Direction::NORTH));
            }
        }
        else if(specialMove == SpecialMove::CASTLE){
            switch (to)
            {
            case Square64::SQ64_C1:
                move_piece(Square64::SQ64_A1, Square64::SQ64_D1);
                break;
            case Square64::SQ64_G1:
                move_piece(Square64::SQ64_H1, Square64::SQ64_F1);
                break;
            case Square64::SQ64_C8:
                move_piece(Square64::SQ64_A8, Square64::SQ64_D8);
                break;
            case Square64::SQ64_G8:
                move_piece(Square64::SQ64_H8, Square64::SQ64_F8);
                break;
            default:
                break;
            }
        }
    }
    
    //Set in the new history

    //Castling rights
    moveHistory[ply-1].castlingRight = moveHistory[ply-2].castlingRight & CASTLE_PERSMISION_UPDATES[from];
    moveHistory[ply-1].castlingRight &= CASTLE_PERSMISION_UPDATES[to];
    
    //Set next move to empty
    moveHistory[ply-1].nextMove = 0;

    //Set fifty moves counter
    Piece capturedPiece = captured_piece(move);
    moveHistory[ply-1].fiftyHalfMoves = moveHistory[ply-2].fiftyHalfMoves+1;

    if(capturedPiece != Piece::NO_PIECE){
        remove_piece(to);
        moveHistory[ply-1].fiftyHalfMoves = 0;
    }

    //If black move, add 1 to moves counter
    moveHistory[ply-1].fullMoves = moveHistory[ply-2].fullMoves;
    if(sideToMove==Color::BLACK)
        moveHistory[ply-1].fullMoves = moveHistory[ply-2].fullMoves+1;

    moveHistory[ply-1].enpassantSquare = Square64::SQ64_NO_SQUARE;
    if(piece_type(mailbox[from]) == PieceType::PAWN){
        moveHistory[ply-1].fiftyHalfMoves = 0;
        
        if(sideToMove==Color::WHITE && specialMove == SpecialMove::PAWN_START){
            moveHistory[ply-1].enpassantSquare = Square64(from + Direction::NORTH);
        } else if(sideToMove==Color::BLACK && specialMove == SpecialMove::PAWN_START){
            moveHistory[ply-1].enpassantSquare = Square64(from + Direction::SOUTH);
        }
    }

    move_piece(from, to);
    

    PieceType promPieceType = promoted_piece(move);

    if(promPieceType != PieceType::NO_PIECE_TYPE){
        Piece promPiece = make_piece(sideToMove, promPieceType);
        remove_piece(to);
        add_piece(to, promPiece);
    }

    Bitboard kingBitboard = pieceTypesBitboards[sideToMove][KING];
    Square64 kingsq64{Bitboards::ctz(kingBitboard)};

    if(square_is_attacked_bySide(kingsq64, ~sideToMove)){
        sideToMove =~ sideToMove;
        undo_move();
        return false;
    }

    sideToMove =~ sideToMove;

    return true;
}

void Position::undo_move(){
    
    Move move = moveHistory[ply-2].nextMove;
    Square64 from = move_from(move);
    Square64 to = move_to(move);
    SpecialMove specialMove = move_special(move);

    sideToMove =~ sideToMove;
    

    if(specialMove != SpecialMove::NO_SPECIAL){
        if(specialMove == SpecialMove::ENPASSANT){
            if(sideToMove==Color::WHITE){
                add_piece(Square64(to + Direction::SOUTH), Piece::B_PAWN);
            }else{
                add_piece(Square64(to + Direction::NORTH), Piece::W_PAWN);
            }
        }
        if(specialMove == SpecialMove::CASTLE){
            switch (to)
            {
            case Square64::SQ64_C1:
                move_piece(Square64::SQ64_D1, Square64::SQ64_A1);
                break;
            case Square64::SQ64_G1:
                move_piece(Square64::SQ64_F1, Square64::SQ64_H1);
                break;
            case Square64::SQ64_C8:
                move_piece(Square64::SQ64_D8, Square64::SQ64_A8);
                break;
            case Square64::SQ64_G8:
                move_piece(Square64::SQ64_F8, Square64::SQ64_H8);
                break;
            default:
                break;
            }
        }
    }

    move_piece(to, from);

    Piece capturedPiece = captured_piece(move);
    if(capturedPiece != Piece::NO_PIECE){
        add_piece(to, capturedPiece);
    }

    PieceType promPieceType = promoted_piece(move);

    if(promPieceType != PieceType::NO_PIECE_TYPE){
        remove_piece(from);
        add_piece(from, sideToMove == Color::WHITE ? Piece::W_PAWN : Piece::B_PAWN);
    }

    moveHistory[ply-2].nextMove = 0;
    moveHistory[ply-1].castlingRight = NO_RIGHT;
    moveHistory[ply-1].fiftyHalfMoves = 0;
    moveHistory[ply-1].fullMoves = 0;
    moveHistory[ply-1].enpassantSquare = Square64::SQ64_NO_SQUARE;

    --ply;

}

void Position::move_piece(Square64 from, Square64 to){

    Piece piece = mailbox[from];
    mailbox[from] =  NO_PIECE;
    mailbox[to] =  piece;
    PieceType pieceType = piece_type(piece);
    Color pieceColor = piece_color(piece);

    pieceTypesBitboards[pieceColor][pieceType] = Bitboards::clear_pieces(pieceTypesBitboards[pieceColor][pieceType], from);
    occupiedBitboards[pieceColor] = Bitboards::clear_pieces(occupiedBitboards[pieceColor], from);
    occupiedBitboards[Color::COLOR_NC] = Bitboards::clear_pieces(occupiedBitboards[Color::COLOR_NC],from);

    pieceTypesBitboards[pieceColor][pieceType] = Bitboards::set_pieces(pieceTypesBitboards[pieceColor][pieceType], to);
    occupiedBitboards[pieceColor] = Bitboards::set_pieces(occupiedBitboards[pieceColor], to);
    occupiedBitboards[Color::COLOR_NC] = Bitboards::set_pieces(occupiedBitboards[Color::COLOR_NC],to);

}

void Position::remove_piece(Square64 square){

    Piece piece = mailbox[square];
    mailbox[square] = NO_PIECE;
    Color pieceColor = piece_color(piece);
    PieceType pieceType = piece_type(piece);

    pieceTypesBitboards[pieceColor][pieceType] = Bitboards::clear_pieces(pieceTypesBitboards[pieceColor][pieceType], square);
    occupiedBitboards[pieceColor] = Bitboards::clear_pieces(occupiedBitboards[pieceColor], square);
    occupiedBitboards[Color::COLOR_NC] = Bitboards::clear_pieces(occupiedBitboards[Color::COLOR_NC], square);

}

void Position::add_piece(Square64 square, Piece piece){

    Color pieceColor = piece_color(piece);
    PieceType pieceType = piece_type(piece);

    mailbox[square] = piece;
    pieceTypesBitboards[pieceColor][pieceType] = Bitboards::set_pieces(pieceTypesBitboards[pieceColor][pieceType], square);
    occupiedBitboards[pieceColor] = Bitboards::set_pieces(occupiedBitboards[pieceColor], square);
    occupiedBitboards[Color::COLOR_NC] = Bitboards::set_pieces(occupiedBitboards[Color::COLOR_NC], square);

}

} // namespace ChessGame
