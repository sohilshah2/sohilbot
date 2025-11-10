#ifndef __DEFINES_INC_GUARD__
#define __DEFINES_INC_GUARD__

#include <cstdint>

//#define ASSERT_ON

//#define SEARCH_STATS_ON
//#define LOG_ON

//#define ENABLE_ASPIRATION
#define ENABLE_NULL_MOVE
#define ENABLE_LMR
#define ENABLE_QUIESCE
#define ENABLE_TT
#define ENABLE_CONTEMPT
//#define HISTORY_HEURISTIC

// 1 hour in milliseconds
#define INFINITE_TIMELIMIT 3600000
#define TIME_BUFFER 100

#define DRAW_THRESHHOLD 60

#define MAX_PVS 5

#define MAX_MOVES 226
#define MAX_DEPTH 64
#define DEFAULT_DEPTH 64

#define LATE_MOVE_CUTOFF 2
#define LATE_MOVE_CUTOFF_2 4
#define REDUCE1(x) (((x)*3)/4)
#define REDUCE2(x) (((x)*2)/3)

#define ASPIRATION_START 35
#define ASPIRATION_DELTA 25

// For scoring moves for move ordering
#define HISTORY_HEURISTIC_MAX_VALUE  (300)
#define HISTORY_HEURISTIC_MIN_VALUE  (-300)

#define ENDGAME_CUTOFF 60

#define MATE(n) ((BitBoardState::KING_VALUE)-(n))

// In num entries. Entry size 32B
#define TT_SIZE_LOG2 22
#define TT_SIZE (1<<TT_SIZE_LOG2)

#define INF 1000000
#define NEG_INF -1000000

// Random numbers for hashing
#define TURN_HASH_WHITE 0x3DB924635E0F0898
#define WHITE_CASTLE_LONG_HASH 0xD1719119D848F035
#define WHITE_CASTLE_SHORT_HASH 0x8C75D1AE197EAB21
#define BLACK_CASTLE_LONG_HASH 0x41920FB2610B9534
#define BLACK_CASTLE_SHORT_HASH 0x4C0961A2EAEFA070
#define EN_PASSANT_HASH 0xD196E5F6169A70A7

namespace BitBoardState {
    enum Piece {EMPTY=0,PAWN=1,ROOK=2,KNIGHT=3,BISHOP=4,QUEEN=5,KING=6};

    enum Color {WHITE=0, BLACK=1};

    static int32_t const KING_VALUE = 50000;
    static int32_t const KING_STRENGTH_VALUE = 430;

    static int32_t const PAWN_VALUE_MG = 90;
    static int32_t const KNIGHT_VALUE_MG = 320;
    static int32_t const BISHOP_VALUE_MG = 340;
    static int32_t const ROOK_VALUE_MG = 480;
    static int32_t const QUEEN_VALUE_MG = 950;

    static int32_t const PAWN_VALUE_EG = 120;
    static int32_t const KNIGHT_VALUE_EG = 320;
    static int32_t const BISHOP_VALUE_EG = 335;
    static int32_t const ROOK_VALUE_EG = 530;
    static int32_t const QUEEN_VALUE_EG = 1100; 

    static const uint64_t notAFile = 0xfefefefefefefefe;
    static const uint64_t notHFile = 0x7f7f7f7f7f7f7f7f;

    static int32_t const PST_MG_P[64] = {
        0,   0,   0,   0,   0,   0,  0,   0,
        98, 134,  61,  95,  68, 126, 34, -11,
        -6,   7,  26,  31,  65,  56, 25, -20,
        -14,  13,   9,  25,  27,  12, 17, -23,
        -27,  -2,   7,  21,  23,   6, 10, -25,
        -26,  -4,  -4, -10,   3,   3, 33, -12,
        -35,  -1, -20, -23, -15,  24, 38, -22,
        0,   0,   0,   0,   0,   0,  0,   0,
    };

    static int32_t const PST_EG_P[64] = {
        0,   0,   0,   0,   0,   0,   0,   0,
        178, 173, 158, 134, 147, 132, 165, 187,
        94, 100,  85,  67,  56,  53,  82,  84,
        32,  24,  13,   5,  -2,   4,  17,  17,
        13,   9,  -3,  -7,  -7,  -8,   3,  -1,
        4,   7,  -6,   1,   0,  -5,  -1,  -8,
        13,   8,   8,  10,  13,   0,   2,  -7,
        0,   0,   0,   0,   0,   0,   0,   0,
    };

    static int32_t const PST_MG_N[64] = {
        -167, -89, -34, -49,  61, -97, -15, -107,
        -73, -41,  72,  36,  23,  62,   7,  -17,
        -47,  60,  37,  65,  84, 129,  73,   44,
        -9,  17,  19,  53,  37,  69,  18,   22,
        -13,   4,  16,  13,  28,  19,  21,   -8,
        -23,  -9,  10,  10,  19,  14,  25,  -16,
        -29, -53, -12,  -3,  -1,  18, -14,  -19,
        -105, -21, -58, -33, -17, -28, -19,  -23,
    };

    static int32_t const PST_EG_N[64] = {
        -58, -38, -13, -28, -31, -27, -63, -99,
        -25,  -8, -25,  -2,  -9, -25, -24, -52,
        -24, -20,  10,   9,  -1,  -9, -19, -41,
        -17,   3,  22,  22,  22,  11,   8, -18,
        -18,  -6,  16,  25,  16,  17,   4, -18,
        -23,  -3,  -1,  15,  10,  -3, -20, -22,
        -42, -20, -10,  -5,  -2, -20, -23, -44,
        -29, -51, -23, -15, -22, -18, -50, -64,
    };

    static int32_t const PST_MG_B[64] = {
        -29,   4, -82, -37, -25, -42,   7,  -8,
        -26,  16, -18, -13,  30,  59,  18, -47,
        -16,  37,  43,  40,  35,  50,  37,  -2,
        -4,   5,  19,  50,  37,  37,   7,  -2,
        -6,  13,  13,  26,  34,  12,  10,   4,
        0,  15,  15,  15,  14,  27,  18,  10,
        4,  15,  16,   0,   7,  21,  33,   1,
        -33,  -3, -14, -21, -13, -12, -39, -21,
    };

    static int32_t const PST_EG_B[64] = {
        -14, -21, -11,  -8, -7,  -9, -17, -24,
        -8,  -4,   7, -12, -3, -13,  -4, -14,
        2,  -8,   0,  -1, -2,   6,   0,   4,
        -3,   9,  12,   9, 14,  10,   3,   2,
        -6,   3,  13,  19,  7,  10,  -3,  -9,
        -12,  -3,   8,  10, 13,   3,  -7, -15,
        -14, -18,  -7,  -1,  4,  -9, -15, -27,
        -23,  -9, -23,  -5, -9, -16,  -5, -17,
    };

    static int32_t const PST_MG_R[64] = {
        32,  42,  32,  51, 63,  9,  31,  43,
        27,  32,  58,  62, 80, 67,  26,  44,
        -5,  19,  26,  36, 17, 45,  61,  16,
        -24, -11,   7,  26, 24, 35,  -8, -20,
        -36, -26, -12,  -1,  9, -7,   6, -23,
        -45, -25, -16, -17,  3,  0,  -5, -33,
        -44, -16, -20,  -9, -1, 11,  -6, -71,
        -19, -13,   1,  12, 10,  7, -37, -26,
    };

    static int32_t const PST_EG_R[64] = {
        13, 10, 18, 15, 12,  12,   8,   5,
        11, 13, 13, 11, -3,   3,   8,   3,
        7,  7,  7,  5,  4,  -3,  -5,  -3,
        4,  3, 13,  1,  2,   1,  -1,   2,
        3,  5,  8,  4, -5,  -6,  -8, -11,
        -4,  0, -5, -1, -7, -12,  -8, -16,
        -6, -6,  0,  2, -9,  -9, -11,  -3,
        -9,  2,  3, -1, -5, -13,   4, -20,
    };

    static int32_t const PST_MG_Q[64] = {
        -28,   0,  29,  12,  59,  44,  43,  45,
        -24, -39,  -5,   1, -16,  57,  28,  54,
        -13, -17,   7,   8,  29,  56,  47,  57,
        -27, -27, -16, -16,  -1,  17,  -2,   1,
        -9, -26,  -9, -10,  -2,  -4,   3,  -3,
        -14,   2, -11,  -2,  -5,   2,  14,   5,
        -35,  -8,  11,   2,   8,  15,  -3,   1,
        -1, -18,  -9,  10, -15, -25, -31, -50,
    };

    static int32_t const PST_EG_Q[64] = {
        -9,  22,  22,  27,  27,  19,  10,  20,
        -17,  20,  32,  41,  58,  25,  30,   0,
        -20,   6,   9,  49,  47,  35,  19,   9,
        3,  22,  24,  45,  57,  40,  57,  36,
        -18,  28,  19,  47,  31,  34,  39,  23,
        -16, -27,  15,   6,   9,  17,  10,   5,
        -22, -23, -30, -16, -16, -23, -36, -32,
        -33, -28, -22, -43,  -5, -32, -20, -41,
    };

    static int32_t const PST_MG_K[64] = {
        -65,  23,  16, -15, -56, -34,   2,  13,
        29,  -1, -20,  -7,  -8,  -4, -38, -29,
        -9,  24,   2, -16, -20,   6,  22, -22,
        -17, -20, -12, -27, -30, -25, -14, -36,
        -49,  -1, -27, -39, -46, -44, -33, -51,
        -14, -14, -22, -46, -44, -30, -15, -27,
        1,   7,  -8, -64, -43, -16,   9,   8,
        -15,  36,  12, -54,   8, -28,  24,  14,
    };

    static int32_t const PST_EG_K[64] = {
        -74, -35, -18, -18, -11,  15,   4, -17,
        -12,  17,  14,  17,  17,  38,  23,  11,
        10,  17,  23,  15,  20,  45,  44,  13,
        -8,  22,  24,  27,  26,  33,  26,   3,
        -18,  -4,  21,  24,  27,  23,   9, -11,
        -19,  -3,  11,  21,  23,  16,   7,  -9,
        -27, -11,   4,  13,  14,   4,  -5, -17,
        -53, -34, -21, -11, -28, -14, -24, -43
    };

}

#endif