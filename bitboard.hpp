#ifndef __BIT_BOARD_INC_GUARD__
#define __BIT_BOARD_INC_GUARD__

#include <iostream>
#include <assert.h>
#include <vector>
#include <array>
#include <algorithm>
#include <cstring>

#include "defines.hpp"

class TT;

class BitBoard {
    public:
        struct MoveData {
            bool isCapture;
            bool isCastle;
            bool isEnPassant;
            bool isPromotion;
        };

        struct Move {
            int32_t value;
            uint8_t from;
            uint8_t to;
            BitBoardState::Piece promote;
            MoveData moveData;

            Move(uint8_t _from=0, uint8_t _to=0, const MoveData& _moveData=DEFAULT_MOVE, 
                 BitBoardState::Piece _promote=BitBoardState::EMPTY) 
                : from(_from), to(_to), promote(_promote), moveData(_moveData) {};
            bool valid() const {
                return from != to;
            }
            bool operator==(const Move& other) const {
                return from == other.from && to == other.to && promote == other.promote;
            }
        };

        struct Pieces {
            uint64_t pawn;
            uint64_t knight;
            uint64_t bishop;
            uint64_t rook;
            uint64_t queen;
            uint64_t king;
        } p[2]; // White, black

        struct CachedState {
            uint64_t occupancy;
            uint64_t mobility;
            uint32_t scope;
            uint8_t enPassantSquare;
            bool castleShort;
            bool castleLong;
        } s[2];

        class History {
            public:
                History() : h(), idx(0) { }
                bool isRepeat(uint64_t hash) {
                    for (uint8_t i = 0; i < 4; i++) {
                        if (hash == h[i]) {
                            return true;
                        }
                    }
                    return false;
                }

                void insert(uint64_t hash) {
                    h[idx] = hash;
                    idx++;
                    idx &= 0x3;
                }
            private:
                uint64_t h[4];
                uint8_t idx;
        } history;

        static const MoveData DEFAULT_MOVE;
        static const MoveData CAPTURE_MOVE;
        static const MoveData EN_PASSANT_MOVE;
        static const MoveData CASTLE_MOVE;
        static const MoveData PROMOTION_MOVE;
        static const MoveData PROMOTE_CAPTURE;

        uint64_t knightAttacks[64];
        uint64_t pawnAttacks[2][64];
        uint64_t kingAttacks[64];

        uint64_t hash;
        int32_t value;
        uint32_t moves;
        bool turn;

        TT* tt;

        bool operator==(const BitBoard &other) const { 
            assert(0); // Should never need to deep compare boards, only hashes
            return hash == other.hash;
        }

        BitBoard(TT* _tt=nullptr, bool startpos=true);
        int32_t getBoardValue() const;
        void changeTurn();
        int32_t getPieceValue(BitBoardState::Piece const& piece) const;
        void printBoard() const;
        void printMobility() const;
        static void strToMove(std::string const& moveText, struct Move& move);
        static std::string moveToStr(struct Move const& move);
        void movePiece(struct Move const& move);
        void sortMoves(std::array<Move,MAX_MOVES>& moves, uint8_t numMoves, struct Move const& ttMove) const;
        uint8_t getAvailableMoves(std::array<Move,MAX_MOVES>& movesAvailable, bool capturesOnly=false) const;
        bool testInCheck(bool c) const;
        int32_t estimateMoveValue(struct Move const& move) const;
        void recalculateOccupancy();
        void recalculateThreats();
        int32_t evaluateKingSafety() const;
        float calculateEndgameBlendFactor() const;
        enum BitBoardState::Piece getPiece(struct Pieces const& p, uint8_t pos) const;

    private:
        void maskClearBitBoard(enum BitBoardState::Piece piece, bool c, uint64_t m);
        void maskSetBitBoard(enum BitBoardState::Piece piece, bool c, uint64_t m);
        std::string pieceToUnicode(enum BitBoardState::Piece piece, enum BitBoardState::Color c) const;
        void validateBitBoard() const;
        bool isAvailable(uint8_t const pos) const;
        bool isOccupied(uint8_t const pos) const;

        uint32_t searchStraight(uint64_t bitboard, uint64_t shiftFunc(uint64_t const b), 
                                       std::array<Move,MAX_MOVES>::iterator& moves, 
                                       bool capturesOnly=false) const;
        int32_t pstDelta(struct Move const& move) const;
        uint32_t getPawnMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const;
        uint32_t getKingMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const;
        uint32_t getKnightMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const;
        uint32_t getRookMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const;
        uint32_t getBishopMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const;
        void maskIfPositionAttacked(uint64_t& bb, bool c) const;
};
#endif