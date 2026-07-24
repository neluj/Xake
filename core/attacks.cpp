#include "attacks.h"

#include <mutex>

namespace Xake{

using namespace Bitboards;
namespace Attacks {

Bitboard pawnAttacks[COLOR_SIZE][SQ64_SIZE];
Bitboard knightAttacks[SQ64_SIZE];
Bitboard kingAttacks[SQ64_SIZE];
Bitboard rookAttacks[SQ64_SIZE][1 << 12];
Bitboard bishopAttacks[SQ64_SIZE][1 << 9];

Bitboard sideRays[SIDE_ATTACK_DIR_SIZE][SQ64_SIZE];
Bitboard diagonalRays[SIDE_ATTACK_DIR_SIZE][SQ64_SIZE];

Bitboard rookRays[SQ64_SIZE];
Bitboard bishopRays[SQ64_SIZE];
std::once_flag initOnce;

constexpr int ROOK_BITS[SQ64_SIZE] = {
  12, 11, 11, 11, 11, 11, 11, 12,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  12, 11, 11, 11, 11, 11, 11, 12
};
constexpr int BISHOP_BITS[SQ64_SIZE] = {
  6, 5, 5, 5, 5, 5, 5, 6,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 7, 7, 7, 7, 5, 5,
  5, 5, 7, 9, 9, 7, 5, 5,
  5, 5, 7, 9, 9, 7, 5, 5,
  5, 5, 7, 7, 7, 7, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
  6, 5, 5, 5, 5, 5, 5, 6
};

constexpr uint64_t ROOK_MAGICS[SQ64_SIZE] = {
  0xa8002c000108020ULL,
  0x6c00049b0002001ULL,
  0x100200010090040ULL,
  0x2480041000800801ULL,
  0x280028004000800ULL,
  0x900410008040022ULL,
  0x280020001001080ULL,
  0x2880002041000080ULL,
  0xa000800080400034ULL,
  0x4808020004000ULL,
  0x2290802004801000ULL,
  0x411000d00100020ULL,
  0x402800800040080ULL,
  0xb000401004208ULL,
  0x2409000100040200ULL,
  0x1002100004082ULL,
  0x22878001e24000ULL,
  0x1090810021004010ULL,
  0x801030040200012ULL,
  0x500808008001000ULL,
  0xa08018014000880ULL,
  0x8000808004000200ULL,
  0x201008080010200ULL,
  0x801020000441091ULL,
  0x800080204005ULL,
  0x1040200040100048ULL,
  0x120200402082ULL,
  0xd14880480100080ULL,
  0x12040280080080ULL,
  0x100040080020080ULL,
  0x9020010080800200ULL,
  0x813241200148449ULL,
  0x491604001800080ULL,
  0x100401000402001ULL,
  0x4820010021001040ULL,
  0x400402202000812ULL,
  0x209009005000802ULL,
  0x810800601800400ULL,
  0x4301083214000150ULL,
  0x204026458e001401ULL,
  0x40204000808000ULL,
  0x8001008040010020ULL,
  0x8410820820420010ULL,
  0x1003001000090020ULL,
  0x804040008008080ULL,
  0x12000810020004ULL,
  0x1000100200040208ULL,
  0x430000a044020001ULL,
  0x280009023410300ULL,
  0xe0100040002240ULL,
  0x200100401700ULL,
  0x2244100408008080ULL,
  0x8000400801980ULL,
  0x2000810040200ULL,
  0x8010100228810400ULL,
  0x2000009044210200ULL,
  0x4080008040102101ULL,
  0x40002080411d01ULL,
  0x2005524060000901ULL,
  0x502001008400422ULL,
  0x489a000810200402ULL,
  0x1004400080a13ULL,
  0x4000011008020084ULL,
  0x26002114058042ULL,
};
constexpr uint64_t BISHOP_MAGICS[SQ64_SIZE] = {
  0x89a1121896040240ULL,
  0x2004844802002010ULL,
  0x2068080051921000ULL,
  0x62880a0220200808ULL,
  0x4042004000000ULL,
  0x100822020200011ULL,
  0xc00444222012000aULL,
  0x28808801216001ULL,
  0x400492088408100ULL,
  0x201c401040c0084ULL,
  0x840800910a0010ULL,
  0x82080240060ULL,
  0x2000840504006000ULL,
  0x30010c4108405004ULL,
  0x1008005410080802ULL,
  0x8144042209100900ULL,
  0x208081020014400ULL,
  0x4800201208ca00ULL,
  0xf18140408012008ULL,
  0x1004002802102001ULL,
  0x841000820080811ULL,
  0x40200200a42008ULL,
  0x800054042000ULL,
  0x88010400410c9000ULL,
  0x520040470104290ULL,
  0x1004040051500081ULL,
  0x2002081833080021ULL,
  0x400c00c010142ULL,
  0x941408200c002000ULL,
  0x658810000806011ULL,
  0x188071040440a00ULL,
  0x4800404002011c00ULL,
  0x104442040404200ULL,
  0x511080202091021ULL,
  0x4022401120400ULL,
  0x80c0040400080120ULL,
  0x8040010040820802ULL,
  0x480810700020090ULL,
  0x102008e00040242ULL,
  0x809005202050100ULL,
  0x8002024220104080ULL,
  0x431008804142000ULL,
  0x19001802081400ULL,
  0x200014208040080ULL,
  0x3308082008200100ULL,
  0x41010500040c020ULL,
  0x4012020c04210308ULL,
  0x208220a202004080ULL,
  0x111040120082000ULL,
  0x6803040141280a00ULL,
  0x2101004202410000ULL,
  0x8200000041108022ULL,
  0x21082088000ULL,
  0x2410204010040ULL,
  0x40100400809000ULL,
  0x822088220820214ULL,
  0x40808090012004ULL,
  0x910224040218c9ULL,
  0x402814422015008ULL,
  0x90014004842410ULL,
  0x1000042304105ULL,
  0x10008830412a00ULL,
  0x2520081090008908ULL,
  0x40102000a0a60140ULL,

};

void init_pawn_attacks();
void init_knight_attacks();
void init_king_attacks();
void init_side_rays();
void init_diagonal_rays();
void init_rook_rays();
void init_bishop_rays();
void init_rook_attacks();
void init_bishop_attacks();
Bitboard calc_side_attacks(Square64 sq64, Bitboard occupied);
Bitboard calc_diagonal_attacks(Square64 sq64, Bitboard occupied);

void init(){
    std::call_once(initOnce, []() {
        init_pawn_attacks();
        init_knight_attacks();
        init_king_attacks();
        init_side_rays();
        init_diagonal_rays();
        init_rook_rays();
        init_bishop_rays();
        init_rook_attacks();
        init_bishop_attacks();
    });
}

Bitboard sliding_side_attacks(Square64 sq64, Bitboard occupied){
    return rookAttacks[sq64][((occupied & rookRays[sq64]) * ROOK_MAGICS[sq64]) >> (64 - ROOK_BITS[sq64])];
}

Bitboard sliding_diagonal_attacks(Square64 sq64, Bitboard occupied){
    return bishopAttacks[sq64][((occupied & bishopRays[sq64]) * BISHOP_MAGICS[sq64]) >> (64 - BISHOP_BITS[sq64])];
}

void init_pawn_attacks(){

    for(Square64 sq64 = SQ64_A1; sq64 < SQ64_SIZE; ++sq64)
    {

        pawnAttacks[WHITE][sq64] = make_direction<NORTH_WEST>(set_pieces(sq64)) 
                                 | make_direction<NORTH_EAST>(set_pieces(sq64)); 

        pawnAttacks[BLACK][sq64] = make_direction<SOUTH_WEST>(set_pieces(sq64)) 
                                 | make_direction<SOUTH_EAST>(set_pieces(sq64)); 
    }
    
}

void init_knight_attacks(){

    for(Square64 sq64 = SQ64_A1; sq64 < SQ64_SIZE; ++sq64)
    {

        knightAttacks[sq64] = make_direction<NORTH_NORTH_WEST>(set_pieces(sq64))
                            | make_direction<NORTH_NORTH_EAST>(set_pieces(sq64))
                            | make_direction<NORTH_EAST_EAST> (set_pieces(sq64))
                            | make_direction<SOUTH_EAST_EAST> (set_pieces(sq64))
                            | make_direction<SOUTH_SOUTH_EAST>(set_pieces(sq64))
                            | make_direction<SOUTH_SOUTH_WEST>(set_pieces(sq64))
                            | make_direction<SOUTH_WEST_WEST> (set_pieces(sq64))
                            | make_direction<NORTH_WEST_WEST> (set_pieces(sq64));
    }
}

void init_king_attacks(){

    for(Square64 sq64 = SQ64_A1; sq64 < SQ64_SIZE; ++sq64)
    {

        kingAttacks[sq64] = make_direction<NORTH>      (set_pieces(sq64))
                          | make_direction<SOUTH>      (set_pieces(sq64))
                          | make_direction<EAST>       (set_pieces(sq64))
                          | make_direction<WEST>       (set_pieces(sq64))
                          | make_direction<NORTH_EAST> (set_pieces(sq64))
                          | make_direction<NORTH_WEST> (set_pieces(sq64))
                          | make_direction<SOUTH_EAST> (set_pieces(sq64))
                          | make_direction<SOUTH_WEST> (set_pieces(sq64));
    }
}

void init_side_rays(){

    for(Square64 sq64 = SQ64_A1; sq64 < SQ64_SIZE; ++sq64)
    {
        sideRays[NORTH_ATTACK][sq64] = 0x0101010101010100ULL << sq64;          
        sideRays[SOUTH_ATTACK][sq64] = 0x0080808080808080ULL >> (63 - sq64);
        sideRays[EAST_ATTACK][sq64] = 2 * ((ONE << (sq64 | 7)) - (ONE << sq64));
        sideRays[WEST_ATTACK][sq64] = ((ONE << sq64) - (ONE << (sq64 & 56)));
    }                         
}

void init_diagonal_rays(){

    auto westN = [](Bitboard board, int n) {
        for (int i = 0; i < n; ++i)
            board = (board >> 1) & ~FILE_H_MASK;
        return board;
    };

    auto eastN = [](Bitboard board, int n) {
        for (int i = 0; i < n; ++i)
            board = (board << 1) & ~FILE_A_MASK;
        return board;
    };

    for (Rank rank = RANK_8; rank >= RANK_1; --rank)
    {
        for (File file = FILE_A; file <= FILE_H; ++file)
        {
            Square64 sq64 = make_square64(rank, file);

            diagonalRays[NORTH_EAST_ATTACK][sq64] = eastN(0x8040201008040200ULL, file)    << (rank * 8);
            diagonalRays[NORTH_WEST_ATTACK][sq64] = westN(0x102040810204000ULL, 7 - file) << (rank * 8); 
            diagonalRays[SOUTH_EAST_ATTACK][sq64] = eastN(0x2040810204080ULL, file)       >> ((7 - rank) * 8);
            diagonalRays[SOUTH_WEST_ATTACK][sq64] = westN(0x40201008040201ULL, 7 - file)  >> ((7 - rank) * 8);

        }
    }
                       
}

void init_rook_rays(){
    
    for(Square64 sq64 = SQ64_A1; sq64 < SQ64_SIZE; ++sq64)
    {
        rookRays[sq64] = (sideRays[NORTH_ATTACK][sq64] & ~RANK_8_MASK)
                         | (sideRays[SOUTH_ATTACK][sq64] & ~RANK_1_MASK)
                         | (sideRays[EAST_ATTACK][sq64] & ~FILE_H_MASK)
                         | (sideRays[WEST_ATTACK][sq64] & ~FILE_A_MASK);
    }   
}

void init_bishop_rays(){

    for(Square64 sq64 = SQ64_A1; sq64 < SQ64_SIZE; ++sq64)
    {
        bishopRays[sq64] = (diagonalRays[NORTH_EAST_ATTACK][sq64] & ~(Bitboards::FILE_H_MASK | Bitboards::RANK_8_MASK))
                           | (diagonalRays[NORTH_WEST_ATTACK][sq64] & ~(Bitboards::FILE_A_MASK | Bitboards::RANK_8_MASK))
                           | (diagonalRays[SOUTH_EAST_ATTACK][sq64] & ~(Bitboards::FILE_H_MASK | Bitboards::RANK_1_MASK))
                           | (diagonalRays[SOUTH_WEST_ATTACK][sq64] & ~(Bitboards::FILE_A_MASK | Bitboards::RANK_1_MASK));
    }   
}

void init_rook_attacks(){

    for(Square64 sq64 = SQ64_A1; sq64 < SQ64_SIZE; ++sq64)
    {
        Bitboard subset = 0ULL;                           
        do {
            const int idx = ((subset & rookRays[sq64]) * ROOK_MAGICS[sq64]) >> (64 - ROOK_BITS[sq64]);
            rookAttacks[sq64][idx] = calc_side_attacks(sq64, subset);
            subset = (subset - rookRays[sq64]) & rookRays[sq64];             
        } while (subset);
    }   
}

void init_bishop_attacks(){

    for(Square64 sq64 = SQ64_A1; sq64 < SQ64_SIZE; ++sq64)
    {
        Bitboard subset = 0ULL;                           
        do {
            const int idx = ((subset & bishopRays[sq64]) * BISHOP_MAGICS[sq64]) >> (64 - BISHOP_BITS[sq64]);
            bishopAttacks[sq64][idx] = calc_diagonal_attacks(sq64, subset);
            subset = (subset - bishopRays[sq64]) & bishopRays[sq64];             
        } while (subset);
    }   

}


Bitboard calc_side_attacks( Square64 sq64, Bitboard occupied){

    //North
    Bitboard bloquersN = occupied & sideRays[NORTH_ATTACK][sq64];
    Bitboard nAttacks = sideRays[NORTH_ATTACK][sq64];
    if(bloquersN != 0){
        Square64 sq64BloquerN{ctz(bloquersN)};
        nAttacks &= ~sideRays[NORTH_ATTACK][sq64BloquerN];
    }


    //South
    Bitboard bloquersS = occupied & sideRays[SOUTH_ATTACK][sq64];
    Bitboard sAttacks = sideRays[SOUTH_ATTACK][sq64];
    if(bloquersS != 0){
        Square64 sq64BloquerS{SQ64_H8 - clz(bloquersS)};
        sAttacks &= ~sideRays[SOUTH_ATTACK][sq64BloquerS];
    }

    //East
    Bitboard bloquersE = occupied & sideRays[EAST_ATTACK][sq64];
    Bitboard eAttacks = sideRays[EAST_ATTACK][sq64];
    if(bloquersE != 0){
        Square64 sq64BloquerE{ctz(bloquersE)};
        eAttacks &= ~sideRays[EAST_ATTACK][sq64BloquerE];
    }


    //West
    Bitboard bloquersW = occupied & sideRays[WEST_ATTACK][sq64];
    Bitboard wAttacks = sideRays[WEST_ATTACK][sq64];
    if(bloquersW != 0){
        Square64 sq64BloquerW{SQ64_H8 - clz(bloquersW)};
        wAttacks &= ~sideRays[WEST_ATTACK][sq64BloquerW];
    }
 
    return nAttacks | sAttacks | eAttacks | wAttacks;

}


Bitboard calc_diagonal_attacks(Square64 sq64, Bitboard occupied){

    //North East
    Bitboard bloquersNE = occupied & diagonalRays[NORTH_EAST_ATTACK][sq64];
    Bitboard neAttacks = diagonalRays[NORTH_EAST_ATTACK][sq64];
    if(bloquersNE != 0){
        Square64 sq64BloquerNE{ctz(bloquersNE)};
        neAttacks &= ~diagonalRays[NORTH_EAST_ATTACK][sq64BloquerNE];
    }

    //South East
    Bitboard bloquersSE = occupied & diagonalRays[SOUTH_EAST_ATTACK][sq64];
    Bitboard seAttacks = diagonalRays[SOUTH_EAST_ATTACK][sq64];
    if(bloquersSE != 0){
        Square64 sq64BloquerSE{SQ64_H8 - clz(bloquersSE)};
        seAttacks &= ~diagonalRays[SOUTH_EAST_ATTACK][sq64BloquerSE];
    }

    //North West
    Bitboard bloquersNW = occupied & diagonalRays[NORTH_WEST_ATTACK][sq64];
    Bitboard nwAttacks = diagonalRays[NORTH_WEST_ATTACK][sq64];
    if(bloquersNW != 0){
        Square64 sq64BloquerNW{ctz(bloquersNW)};
        nwAttacks &= ~diagonalRays[NORTH_WEST_ATTACK][sq64BloquerNW];
    }

    //South West
    Bitboard bloquersSW = occupied & diagonalRays[SOUTH_WEST_ATTACK][sq64];
    Bitboard swAttacks = diagonalRays[SOUTH_WEST_ATTACK][sq64];
    if(bloquersSW != 0){
        Square64 sq64BloquerSW{SQ64_H8 - clz(bloquersSW)};
        swAttacks &= ~diagonalRays[SOUTH_WEST_ATTACK][sq64BloquerSW];
    }

    return neAttacks | seAttacks | nwAttacks | swAttacks;

}
}
}
