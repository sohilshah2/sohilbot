#ifndef __DEFINES_INC_GUARD__
#define __DEFINES_INC_GUARD__

#include <cstdint>

//#define ASSERT_ON

#define MAX_MOVES 128
#define MAX_DEPTH 40
#define DEFAULT_DEPTH 40

#define LATE_MOVE_CUTOFF 2
#define LATE_MOVE_CUTOFF_2 4

#define DEPTH_MOVE_CUTOFF 4
#define DEPTH_MOVE_CUTOFF_2 2

#define ASPIRATION_WINDOW 15

#define MATE(n) ((BoardState::KING_VALUE)-(n))

// In num positions
#define TT_SIZE_LOG2 23
#define TT_SIZE (1<<TT_SIZE_LOG2)

// Completely random numbers for hashing
#define TURN_HASH_WHITE 0x3DB924635E0F0898
#define EN_PASSANT_HASH 0xD12D774B6F44144F
#define CASTLE_HASH 0x4C27AC1F35B77EBA

namespace BoardState {
    enum Piece {EMPTY=0,PAWN=1,ROOK=2,KNIGHT=3,BISHOP=4,QUEEN=5,KING=6};

    enum ColorPiece {WEMPTY=0,WPAWN=1,WROOK=2,WKNIGHT=3,WBISHOP=4,WQUEEN=5,WKING=6, 
                     BEMPTY=8,BPAWN=9,BROOK=10,BKNIGHT=11,BBISHOP=12,BQUEEN=13,BKING=14};

    enum Color {WHITE=0, BLACK=0x8};

    static int32_t const PAWN_VALUE = 100;
    static int32_t const KNIGHT_VALUE = 305;
    static int32_t const BISHOP_VALUE = 333;
    static int32_t const ROOK_VALUE = 563;
    static int32_t const QUEEN_VALUE = 950;
    static int32_t const KING_VALUE = 50000;

    static uint8_t const pieceColorMask = 0xf;
    static uint8_t const pieceMask = 0x7;
    static uint8_t const colorMask = 0x8;
    
    typedef struct BoardType {
        union Board {
            uint64_t data64[4];
            uint8_t data8[32];
        } b;

        struct CachedState {
            //uint64_t whiteOccupancy;
            //uint64_t blackOccupancy;
            //uint64_t whiteThreats;
            //uint64_t blackThreats;
            uint8_t enPassantSquare;
            bool whiteShort;
            bool whiteLong;
            bool blackShort;
            bool blackLong;
            bool whiteCastled;
            bool blackCastled;
        } cached ={.enPassantSquare=0,.whiteShort=true,.whiteLong=true,.blackShort=true,
                   .blackLong=true,.whiteCastled=false,.blackCastled=false};
        uint64_t hash = 0;
        int32_t value = 0;

        Color turn=WHITE;
        bool operator==(const BoardType &other) const { 
            return b.data64[0] == other.b.data64[0] 
                   && b.data64[1] == other.b.data64[1]
                   && b.data64[2] == other.b.data64[2]
                   && b.data64[3] == other.b.data64[3]
                   && turn==other.turn
                   /*&& cached.whiteShort==other.cached.whiteShort
                   && cached.whiteLong==other.cached.whiteLong
                   && cached.blackShort==other.cached.blackShort
                   && cached.blackLong==other.cached.blackLong*/
                   && cached.enPassantSquare==other.cached.enPassantSquare;
        };
    } BoardType;

    typedef struct Move {
        uint8_t from;
        uint8_t to;
        ColorPiece promote;
        Move(uint8_t _from=0, uint8_t _to=0, ColorPiece _promote=WEMPTY) 
            : from(_from), to(_to), promote(_promote) {};
        inline bool operator==(Move const& other) const {
            return from == other.from && to == other.to && promote == other.promote;
        }
        inline bool valid() const {
            return !(from == to && from == 0);
        }
    } MoveType;
}

#endif