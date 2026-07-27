#pragma once

#include "move.h"
#include "position.h"
#include "types.h"

#include <vector>

namespace Xake {

struct PerftDivideEntry {
    Move move = NOMOVE;
    NodesSize nodes = 0;
};

NodesSize perft(Position& position, DepthSize depth);
NodesSize perft_root(Position& position,
                     DepthSize depth,
                     std::vector<PerftDivideEntry>* divide = nullptr);

} // namespace Xake
