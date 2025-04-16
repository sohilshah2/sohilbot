#ifndef __TRANSPOSITION_TABLES_INC_GUARD__
#define __TRANSPOSITION_TABLES_INC_GUARD__

#include <iostream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <array>

#include "defines.hpp"
#include "bitboard.hpp"

class TT {
    public:
        enum NodeType { PV, CUT, ALL};

        typedef struct TTEntry {
            uint64_t hash;
            BitBoard::Move move;
            int32_t eval;
            uint8_t depth;
            NodeType node;
        } TTEntry;

        TT();
        TTEntry& lookupHash(uint64_t const hash);
        void updateEntry(BitBoard const& board, BitBoard::Move const& move,
                         int32_t const eval, uint8_t const depth, NodeType const node);
        uint64_t genHash(BitBoard const& board) const;
        void clear();
        void printEstimatedOccupancy() const;

        // [turn][piece][square]
        uint64_t BOARDPOS_HASH[2][8][64];

    private:
        static inline size_t getIdx(uint64_t hash) {
            return hash & (0xffffffffffffffffull >> (64 - TT_SIZE_LOG2));
        };

        TTEntry table[TT_SIZE];
};

#endif