#pragma once

#include <vector>
#include "move.h"

namespace ChessGame{

class Position;

namespace MoveGen{

struct MoveList{
    
    void set_move(Move move){
        moves[size] = move;
        ++size;
    }
    Move moves[MAX_POSITION_MOVES_SIZE];
    int size{0};
};


void generate_pseudo_moves(const Position &pos, MoveList &moveList);
void generate_pseudo_captures(Position &pos, MoveList &moveList);


} // namespace ChessGame
}