#ifndef __EVALUATE_INC_GUARD__
#define __EVALUATE_INC_GUARD__

#include "boardState.hpp"

#include <cstdint>
#include <iostream>
#include <assert.h>
#include <vector>
#include <math.h>

namespace Evaluate {
    static int32_t const MOBILITY_FACTOR = 4;
    static int32_t const TEMPO_ADDER = 7;
    static int32_t const CASTLE_FACTOR = 16;

    static inline int32_t evaluatePosition(BoardState::BoardType const& board) 
    {
        using namespace BoardState;
        int32_t evaluation = board.value;

        evaluation += TEMPO_ADDER;
        evaluation += MOBILITY_FACTOR * ((int)sqrt(calculateWeightedMobility(board)));
        int32_t castle = (board.cached.whiteLong + board.cached.whiteShort) 
                         - (board.cached.blackLong + board.cached.blackShort)
                         + (board.cached.whiteCastled ? 3 : 0)
                         - (board.cached.blackCastled ? 3 : 0);
        evaluation += ((board.turn == BLACK) ? -1 : 1) * castle * CASTLE_FACTOR;

        return evaluation;
    }
};

#endif