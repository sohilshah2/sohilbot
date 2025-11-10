#ifndef __EVALUATE_INC_GUARD__
#define __EVALUATE_INC_GUARD__

#include "bitboard.hpp"

#include <cstdint>
#include <iostream>
#include <assert.h>
#include <vector>
#include <math.h>

namespace Evaluate {
    using namespace BitBoardState;

    static float constexpr MOBILITY_FACTOR = 4;
    static float constexpr SCOPE_FACTOR = 1;
    static float constexpr TEMPO_ADDER = 29;
    static float constexpr PST_FACTOR = 1;
    static float constexpr KING_SAFETY_FACTOR = 22;
    static float constexpr PASSED_PAWN_FACTOR = 1;
    static float constexpr CONNECTED_PAWN_FACTOR = 10;

    static inline int32_t evaluatePieceSquareTables(BitBoard const& board, float egBlend) {
        int32_t evaluation = 0;
        
        uint64_t const bb[2][6] = {{board.p[WHITE].pawn, board.p[WHITE].knight, board.p[WHITE].bishop, 
                                    board.p[WHITE].rook, board.p[WHITE].queen, board.p[WHITE].king},
                                   {board.p[BLACK].pawn, board.p[BLACK].knight, board.p[BLACK].bishop, 
                                    board.p[BLACK].rook, board.p[BLACK].queen, board.p[BLACK].king}};
        int32_t const (*pst_mg[6])[64];
        int32_t const (*pst_eg[6])[64];

        pst_mg[0] = &PST_MG_P;
        pst_eg[0] = &PST_EG_P;

        pst_mg[1] = &PST_MG_N;
        pst_eg[1] = &PST_EG_N;
        
        pst_mg[2] = &PST_MG_B;
        pst_eg[2] = &PST_EG_B;
        
        pst_mg[3] = &PST_MG_R;
        pst_eg[3] = &PST_EG_R;
        
        pst_mg[4] = &PST_MG_Q;
        pst_eg[4] = &PST_EG_Q;
        
        pst_mg[5] = &PST_MG_K;
        pst_eg[5] = &PST_EG_K;

        float mgBlend = 1-egBlend;

        for (Color turn : {WHITE, BLACK}) {
            for (uint8_t p = 0; p < 6; p++) {
                uint64_t sftBoard = bb[turn][p];
                while (sftBoard) {
                    uint8_t pos = __builtin_ctzll(sftBoard);
                    if (!turn) {
                        uint8_t col = pos % 8;
                        uint8_t row = 7 - (pos / 8);
                        pos = (row * 8 + col);
                    }
                    int32_t pstEval = mgBlend*(*pst_mg[p])[pos] + egBlend*(*pst_eg[p])[pos];
                    evaluation += board.turn == turn ? pstEval : -pstEval;
                    sftBoard &= sftBoard - 1;
                }
            }
        }

        return evaluation;
    }

    static inline int32_t evaluatePassedPawns(BitBoard const& board) {
        return 0;
    }

    static inline int32_t evaluateConnectedPawns(BitBoard const& board) {
        // 0th column bb
        constexpr uint64_t colbb = 0x0101010101010101;
        int32_t eval = 0;
        
        for (Color c : {WHITE, BLACK}) {
            uint64_t sftBoard = colbb;
            uint8_t chain = 0;
            uint8_t numIslands = 0;
            uint8_t numConnected = 0;

            for (uint8_t col = 0; col < 8; col++) {
                if (sftBoard & board.p[c].pawn) {
                    if (chain) {
                        numConnected++;
                        if (chain == 1) numConnected++;
                        chain++;
                    } else {
                        numIslands++;
                        chain++;
                    }
                } else {
                    chain = 0;
                }
                sftBoard <<= 1;
            }
            eval += (c == board.turn) ? numConnected - numIslands : numIslands - numConnected;
        }

        return eval;
    }

    static inline int32_t evaluatePosition(BitBoard const& board) {
        int32_t evaluation = board.getBoardValue();

        evaluation += TEMPO_ADDER;

        int32_t scope = static_cast<int32_t>(board.s[board.turn].scope)
                        - static_cast<int32_t>(board.s[!board.turn].scope);
        int32_t mobility = (static_cast<int32_t>(__builtin_popcountll(board.s[board.turn].mobility)))
                            - (static_cast<int32_t>(__builtin_popcountll(board.s[!board.turn].mobility)));
        
        float egBlend = board.calculateEndgameBlendFactor();
        float mgBlend = 1-egBlend;

        evaluation += MOBILITY_FACTOR * mobility;
        evaluation += SCOPE_FACTOR * scope;

        evaluation += PST_FACTOR * evaluatePieceSquareTables(board, egBlend);
        evaluation -= mgBlend * KING_SAFETY_FACTOR * board.evaluateKingSafety();

        //evaluation += egBlend * (PASSED_PAWN_FACTOR * evaluatePassedPawns(board));
        //evaluation += egBlend * (CONNECTED_PAWN_FACTOR * evaluateConnectedPawns(board));

        return evaluation;
    }
};

#endif