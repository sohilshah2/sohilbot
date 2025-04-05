#ifndef __TRANSPOSITION_TABLES_INC_GUARD__
#define __TRANSPOSITION_TABLES_INC_GUARD__

#include <iostream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <array>

#include "defines.hpp"

class TT {
    public:
        typedef struct TTEntry {
            BoardState::Move move;
            uint64_t hash;
            int32_t eval;
            uint8_t depth;
        } TTEntry;

        TT();
        TTEntry const& lookupHash(uint64_t const hash) const;
        void updateEntry(BoardState::BoardType const& board, BoardState::Move const move,
                         int32_t const eval, uint8_t const depth);
        uint64_t genHash(BoardState::BoardType const& board) const;
        void clear();
        void printEstimatedOccupancy() const;

        uint64_t BOARDPOS_HASH[64][16];

    private:
        typedef struct TTKey {
            BoardState::BoardType board;
        
            bool operator==(const TTKey &other) const {
                return board == other.board;
            }
        } TTKey;
        
        struct Hash {
            inline uint64_t operator()(const TTKey& k) const
            {
                return k.board.hash;
            }
        };

        static inline size_t getIdx(uint64_t hash) {
            return hash & (0xffffffffffffffffull >> (64 - TT_SIZE_LOG2));
        };

        TTEntry table[TT_SIZE];
};

#endif