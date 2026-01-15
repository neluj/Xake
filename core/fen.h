#pragma once

#include <string>

#include "position.h"

bool setFromFen(Position& pos, const std::string& fen);
std::string toFen(const Position& pos);
