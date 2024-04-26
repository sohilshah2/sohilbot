#include <cstring>
#include <assert.h>
#include "transpositionTables.hpp"
#include "boardState.hpp"

TT::TT()
{
    for (uint8_t pos = 0; pos < 64; pos++) {
        for (uint8_t piece = 0; piece < 16; piece++) {
            uint64_t random =
                (((uint64_t) rand() <<  0) & 0x000000000000FFFFull) ^ 
                (((uint64_t) rand() << 16) & 0x00000000FFFF0000ull) ^ 
                (((uint64_t) rand() << 32) & 0x0000FFFF00000000ull) ^
                (((uint64_t) rand() << 48) & 0xFFFF000000000000ull);

            BOARDPOS_HASH[pos][piece] = random;
        }
    }
}

TT::TTEntry const& TT::lookupHash(uint64_t const hash) const 
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

void TT::updateEntry(BoardState::BoardType const& board, BoardState::Move const move,
                 int32_t const eval, uint8_t const depth)
{
    size_t idx = getIdx(board.hash);
    TTEntry& entry = table[idx];
    entry.eval = eval;
    entry.depth = depth;
    entry.hash = board.hash;
    entry.move = move;
}

uint64_t TT::genHash(BoardState::BoardType const& board) const 
{
    using namespace BoardState;
    uint64_t hash;
    hash = board.turn == BoardState::WHITE ? TURN_HASH_WHITE : 0;
    hash ^= EN_PASSANT_HASH*board.cached.enPassantSquare;
    for (uint8_t idx = 0; idx < 32; idx++) {
        // Are both packed squares empty?
        if (!board.b.data8[idx]) continue;

        uint8_t data = board.b.data8[idx];
        for (uint8_t nib = 0; nib < 2; nib++) {
            uint8_t pos = (idx<<1) | nib;
            uint8_t pieceData = getColorPiece(board, pos);
        
            // Is the square empty?
            if (!(pieceData)) continue;

            hash ^= BOARDPOS_HASH[pos][pieceData];
        }
    }
    return hash;
}
