#include <cstring>
#include <assert.h>
#include "transpositionTables.hpp"
#include "bitboard.hpp"

TT::TT()
{
    for (uint8_t c = 0; c < 2; c++) {
        for (uint8_t pos = 0; pos < 64; pos++) {
            for (uint8_t piece = 0; piece < 8; piece++) {
                uint64_t random =
                    (((uint64_t) rand() <<  0) & 0x000000000000FFFFull) | 
                    (((uint64_t) rand() << 16) & 0x00000000FFFF0000ull) | 
                    (((uint64_t) rand() << 32) & 0x0000FFFF00000000ull) |
                    (((uint64_t) rand() << 48) & 0xFFFF000000000000ull);

                BOARDPOS_HASH[c][piece][pos] = random;
            }
        }
    }
    clear();
    clearHistory();
}

void TT::clearHistory() {
    std::memset(&moveHistoryScore, 0, 64*64*2*sizeof(int32_t));
}

void TT::updateHistoryScore(BitBoardState::Color turn, BitBoard::Move const& move, int32_t score) {
    // History gravity formula
    score = std::min(HISTORY_HEURISTIC_MAX_VALUE, std::max(HISTORY_HEURISTIC_MIN_VALUE, score));
    score -= moveHistoryScore[turn][move.from][move.to] * std::abs(score) / HISTORY_HEURISTIC_MAX_VALUE;
    moveHistoryScore[turn][move.from][move.to] += score;
}

int32_t TT::getHistoryScore(BitBoardState::Color turn, BitBoard::Move const& move) {
    return moveHistoryScore[turn][move.from][move.to];
}

TT::TTEntry& TT::lookupHash(uint64_t const hash) 
{
    size_t idx = getIdx(hash);
    return table[idx];
}

void TT::clear() {
    std::memset(&table, 0, sizeof(TTEntry)*TT_SIZE);
}

void TT::printEstimatedOccupancy() const
{
    uint32_t numTests = 10000;
    uint32_t stride = TT_SIZE / numTests;
    uint32_t numEmpty = 0;
    for (uint32_t idx = 0; idx<numTests; idx++) {
        numEmpty += !(table[idx*stride].hash);
    }
    std::cout << "Occupancy: " << std::to_string((float)(numTests-numEmpty)*100/numTests) 
              << "%" << std::endl;
}

void TT::updateEntry(BitBoard const& board, BitBoard::Move const& move,
                     int32_t const eval, uint8_t const depth, NodeType const node)
{
    TTEntry& entry = lookupHash(board.hash);
    entry.eval = eval;
    entry.depth = depth;
    entry.hash = board.hash;
    entry.move = move;
    entry.node = node;
}

uint64_t TT::genHash(BitBoard const& board) const 
{
    using namespace BitBoardState;
    uint64_t hash;
    hash = board.turn == WHITE ? TURN_HASH_WHITE : 0;

    hash ^= EN_PASSANT_HASH * board.s[WHITE].enPassantSquare;
    hash ^= EN_PASSANT_HASH * board.s[BLACK].enPassantSquare;

    hash ^= WHITE_CASTLE_LONG_HASH * board.s[WHITE].castleLong;
    hash ^= WHITE_CASTLE_SHORT_HASH * board.s[WHITE].castleShort;
    hash ^= BLACK_CASTLE_LONG_HASH * board.s[BLACK].castleLong;
    hash ^= BLACK_CASTLE_SHORT_HASH * board.s[BLACK].castleShort;

    for (uint8_t c = 0; c < 2; c++) {
        for (uint8_t pos = 0; pos < 64; pos++) {
            uint8_t pieceData = board.getPiece(board.p[c], pos);
            if (pieceData) {
                hash ^= BOARDPOS_HASH[c][pieceData][pos];
            }
        }
    }
    return hash;
}
