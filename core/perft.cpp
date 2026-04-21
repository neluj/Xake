#include "perft.h"

#include "movegen.h"

namespace ChessGame {

NodesSize perft(Position& position, DepthSize depth)
{
    if (depth == 0) {
        return 1;
    }

    MoveGen::MoveList moveList;
    MoveGen::generate_pseudo_moves(position, moveList);

    NodesSize nodes = 0;
    for (int i = 0; i < moveList.size; ++i) {
        const Move move = moveList.moves[i];
        if (!position.do_move(move)) {
            continue;
        }

        nodes += perft(position, DepthSize(depth - 1));
        position.undo_move();
    }

    return nodes;
}

NodesSize perft_root(Position& position,
                     DepthSize depth,
                     std::vector<PerftDivideEntry>* divide)
{
    if (divide) {
        divide->clear();
    }

    if (depth == 0) {
        return 1;
    }

    MoveGen::MoveList moveList;
    MoveGen::generate_pseudo_moves(position, moveList);

    NodesSize totalNodes = 0;
    for (int i = 0; i < moveList.size; ++i) {
        const Move move = moveList.moves[i];
        if (!position.do_move(move)) {
            continue;
        }

        const NodesSize childNodes = perft(position, DepthSize(depth - 1));
        position.undo_move();

        totalNodes += childNodes;
        if (divide) {
            divide->push_back(PerftDivideEntry{move, childNodes});
        }
    }

    return totalNodes;
}

} // namespace ChessGame
