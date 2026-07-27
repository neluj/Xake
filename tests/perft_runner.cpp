#include "move.h"
#include "movegen.h"
#include "perft.h"
#include "position.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr char kStartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

using namespace Xake;

struct Options {
    bool interactive = true;
    bool divide = true;
    bool useStartpos = false;
    std::string fen;
    int depth = -1;
};

std::vector<std::string> splitWhitespace(const std::string& text)
{
    std::istringstream input(text);
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool parseDepth(const std::string& text, int& depth)
{
    try {
        size_t parsed = 0;
        depth = std::stoi(text, &parsed);
        if (parsed != text.size()) {
            return false;
        }
    } catch (...) {
        return false;
    }

    return depth >= 0;
}

PieceType promotionPieceFromUciChar(char c)
{
    switch (c) {
    case 'q':
    case 'Q':
        return QUEEN;
    case 'r':
    case 'R':
        return ROOK;
    case 'b':
    case 'B':
        return BISHOP;
    case 'n':
    case 'N':
        return KNIGHT;
    default:
        return NO_PIECE_TYPE;
    }
}

SpecialMove promotionMove(PieceType pieceType)
{
    switch (pieceType) {
    case KNIGHT:
        return PROMOTION_KNIGHT;
    case BISHOP:
        return PROMOTION_BISHOP;
    case ROOK:
        return PROMOTION_ROOK;
    case QUEEN:
        return PROMOTION_QUEEN;
    default:
        return NO_SPECIAL;
    }
}

Move makeMoveCandidate(std::string_view text)
{
    if (text.size() < 4) {
        return NOMOVE;
    }

    const char fromFile = text[0];
    const char fromRank = text[1];
    const char toFile = text[2];
    const char toRank = text[3];
    if (fromFile < 'a' || fromFile > 'h' || toFile < 'a' || toFile > 'h') {
        return NOMOVE;
    }
    if (fromRank < '1' || fromRank > '8' || toRank < '1' || toRank > '8') {
        return NOMOVE;
    }

    const int fromSq = (fromRank - '1') * 8 + (fromFile - 'a');
    const int toSq = (toRank - '1') * 8 + (toFile - 'a');
    const PieceType promotion = (text.size() >= 5)
        ? promotionPieceFromUciChar(text[4])
        : NO_PIECE_TYPE;
    if (text.size() >= 5 && promotion == NO_PIECE_TYPE) {
        return NOMOVE;
    }

    return make_quiet_move(Square64(fromSq), Square64(toSq), promotionMove(promotion));
}

Move resolveMoveCandidate(const Position& position, Move candidate)
{
    if (candidate == NOMOVE) {
        return NOMOVE;
    }

    MoveGen::MoveList moveList;
    MoveGen::generate_pseudo_moves(position, moveList);

    for (int i = 0; i < moveList.size; ++i) {
        const Move move = moveList.moves[i];
        if (move_from(move) != move_from(candidate)
            || move_to(move) != move_to(candidate)
            || promoted_piece(move) != promoted_piece(candidate)) {
            continue;
        }

        Position testPosition = position;
        if (testPosition.do_move(move)) {
            return move;
        }
    }

    return NOMOVE;
}

bool applyUciMove(Position& position, std::string_view text)
{
    const Move move = resolveMoveCandidate(position, makeMoveCandidate(text));
    if (move == NOMOVE) {
        return false;
    }

    return position.do_move(move);
}

bool applyPositionCommand(Position& position,
                          const std::string& command,
                          std::string& error)
{
    const std::vector<std::string> tokens = splitWhitespace(command);
    if (tokens.empty()) {
        error = "missing position payload";
        return false;
    }

    Position parsed;
    size_t index = 0;
    if (tokens[index] == "startpos") {
        if (!parsed.set_FEN(kStartFen)) {
            error = "failed to load start position";
            return false;
        }
        ++index;
    } else if (tokens[index] == "fen") {
        if (tokens.size() < index + 7) {
            error = "incomplete FEN";
            return false;
        }

        std::ostringstream fen;
        for (size_t i = index + 1; i < index + 7; ++i) {
            if (i != index + 1) {
                fen << ' ';
            }
            fen << tokens[i];
        }

        if (!parsed.set_FEN(fen.str())) {
            error = "invalid FEN";
            return false;
        }
        index += 7;
    } else {
        error = "expected 'startpos' or 'fen'";
        return false;
    }

    if (index < tokens.size()) {
        if (tokens[index] != "moves") {
            error = "unexpected tokens after position";
            return false;
        }
        ++index;

        for (; index < tokens.size(); ++index) {
            if (!applyUciMove(parsed, tokens[index])) {
                error = "illegal move in position command: " + tokens[index];
                return false;
            }
        }
    }

    position = parsed;
    return true;
}

void printPerftReport(Position position, DepthSize depth, bool divide, std::ostream& out)
{
    const auto start = std::chrono::steady_clock::now();

    std::vector<PerftDivideEntry> entries;
    NodesSize totalNodes = 0;
    if (divide) {
        totalNodes = perft_root(position, depth, &entries);
    } else {
        totalNodes = perft_root(position, depth, nullptr);
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    out << "\n";
    if (divide) {
        for (const PerftDivideEntry& entry : entries) {
            out << algebraic_move(entry.move) << ": " << entry.nodes << "\n";
        }
        out << "\n";
    }

    out << "total nodes size: " << totalNodes
        << " time ms: " << elapsed.count()
        << "\n\n";
    out.flush();
}

void printUsage(std::ostream& out)
{
    out << "Usage:\n"
        << "  perft_runner --fen \"<fen>\" --depth <n> [--no-divide]\n"
        << "  perft_runner --startpos --depth <n> [--no-divide]\n"
        << "  perft_runner\n";
}

bool parseOptions(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(std::cout);
            return false;
        }
        if (arg == "--fen") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --fen\n";
                return false;
            }
            options.fen = argv[++i];
            options.interactive = false;
            continue;
        }
        if (arg == "--startpos") {
            options.useStartpos = true;
            options.interactive = false;
            continue;
        }
        if (arg == "--depth") {
            if (i + 1 >= argc || !parseDepth(argv[i + 1], options.depth)) {
                std::cerr << "Invalid value for --depth\n";
                return false;
            }
            ++i;
            options.interactive = false;
            continue;
        }
        if (arg == "--no-divide") {
            options.divide = false;
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        printUsage(std::cerr);
        return false;
    }

    if (!options.interactive) {
        if (options.useStartpos == !options.fen.empty()) {
            std::cerr << "Choose exactly one of --startpos or --fen\n";
            return false;
        }
        if (options.depth < 0) {
            std::cerr << "--depth is required in one-shot mode\n";
            return false;
        }
    }

    return true;
}

int runOneShot(const Options& options)
{
    Position position;
    const bool ok = options.useStartpos
        ? position.set_FEN(kStartFen)
        : position.set_FEN(options.fen);
    if (!ok) {
        std::cerr << "Invalid position\n";
        return 1;
    }

    printPerftReport(position, DepthSize(options.depth), options.divide, std::cout);
    return 0;
}

int runInteractive()
{
    Position currentPosition;
    if (!currentPosition.set_FEN(kStartFen)) {
        std::cerr << "Failed to initialize start position\n";
        return 1;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            return 0;
        }
        if (line == "uci") {
            std::cout << "id name Xake Perft Runner\n";
            std::cout << "id author Julen\n";
            std::cout << "uciok\n";
            std::cout.flush();
            continue;
        }
        if (line == "isready") {
            std::cout << "readyok\n";
            std::cout.flush();
            continue;
        }
        if (line == "ucinewgame") {
            currentPosition.set_FEN(kStartFen);
            continue;
        }
        if (line == "d") {
            std::cout << currentPosition.get_FEN() << "\n";
            std::cout.flush();
            continue;
        }
        if (line.rfind("position ", 0) == 0) {
            std::string error;
            if (!applyPositionCommand(currentPosition, line.substr(9), error)) {
                std::cout << "info string invalid position command: " << error << "\n";
                std::cout.flush();
            }
            continue;
        }
        if (line.rfind("go perft ", 0) == 0) {
            int depth = -1;
            if (!parseDepth(line.substr(9), depth)) {
                std::cout << "info string invalid perft depth\n";
                std::cout.flush();
                continue;
            }

            printPerftReport(currentPosition, DepthSize(depth), true, std::cout);
            continue;
        }

        std::cout << "info string unsupported command: " << line << "\n";
        std::cout.flush();
    }

    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseOptions(argc, argv, options)) {
        return 1;
    }

    if (options.interactive) {
        return runInteractive();
    }

    return runOneShot(options);
}
