#ifndef __EVALUATE_INC_GUARD__
#define __EVALUATE_INC_GUARD__

#include "bitboard.hpp"

#include <cstdint>
#include <iostream>
#include <assert.h>
#include <vector>
#include <math.h>

namespace Evaluate {
    static int32_t const MOBILITY_FACTOR = 6;
    static int32_t const SCOPE_FACTOR = 8;
    static int32_t const TEMPO_ADDER = 29;
    static int32_t const PST_FACTOR = 1;
    static int32_t const KING_SAFETY_FACTOR = 15;

    static inline int32_t evaluatePieceSquareTables(BitBoard const& board) {
        using namespace BitBoardState;
        int32_t evaluation = 0;
        
        enum Color const turns[] = {WHITE, BLACK};
        uint64_t const bb[2][6] = {{board.p[WHITE].pawn, board.p[WHITE].knight, board.p[WHITE].bishop, 
                                    board.p[WHITE].rook, board.p[WHITE].queen, board.p[WHITE].king},
                                   {board.p[BLACK].pawn, board.p[BLACK].knight, board.p[BLACK].bishop, 
                                    board.p[BLACK].rook, board.p[BLACK].queen, board.p[BLACK].king}};
        int32_t const (*pst[6])[64];

        if (board.moves < ENDGAME_CUTOFF) {
            pst[0] = &PST_MG_P;
            pst[1] = &PST_MG_N;
            pst[2] = &PST_MG_B;
            pst[3] = &PST_MG_R;
            pst[4] = &PST_MG_Q;
            pst[5] = &PST_MG_K;
        } else {
            pst[0] = &PST_EG_P;
            pst[1] = &PST_EG_N;
            pst[2] = &PST_EG_B;
            pst[3] = &PST_EG_R;
            pst[4] = &PST_EG_Q;
            pst[5] = &PST_EG_K;
        }

        for (enum Color turn : turns) {
            for (uint8_t p = 0; p < 6; p++) {
                uint64_t sftBoard = bb[turn][p];
                while (sftBoard) {
                    uint8_t pos = __builtin_ctzll(sftBoard);
                    if (!turn) {
                        uint8_t col = pos % 8;
                        uint8_t row = 7 - (pos / 8);
                        pos = (row * 8 + col);
                    }
                    evaluation += (board.turn == turn) ? (*pst[p])[pos] : -(*pst[p])[pos];
                    sftBoard &= sftBoard - 1;
                }
            }
        }

        return evaluation;
    }

    static inline int32_t evaluatePosition(BitBoard const& board) 
    {
        int32_t evaluation = board.getBoardValue();

        evaluation += TEMPO_ADDER;

        int32_t scope = static_cast<int32_t>(board.s[board.turn].scope)
                                - static_cast<int32_t>(board.s[!board.turn].scope);
        int32_t mobility = (static_cast<int32_t>(__builtin_popcountll(board.s[board.turn].mobility)))
                                - (static_cast<int32_t>(__builtin_popcountll(board.s[!board.turn].mobility)));
        
        evaluation += MOBILITY_FACTOR * mobility;
        evaluation += SCOPE_FACTOR * scope;

        evaluation += PST_FACTOR * evaluatePieceSquareTables(board);
        evaluation -= KING_SAFETY_FACTOR * board.evaluateKingSafety();

        return evaluation;
    }
};

#endif