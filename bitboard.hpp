#ifndef __BIT_BOARD_INC_GUARD__
#define __BIT_BOARD_INC_GUARD__

#include <iostream>
#include <assert.h>
#include <vector>
#include <array>
#include <algorithm>
#include <cstring>

#include "transpositionTables.hpp"
#include "defines.hpp"

class BitBoard {
    public:
        typedef struct Move {
            uint8_t from;
            uint8_t to;
            BitBoardState::Piece promote;
            Move(uint8_t _from=0, uint8_t _to=0, BitBoardState::Piece _promote=BitBoardState::EMPTY) 
                : from(_from), to(_to), promote(_promote) {};
            bool operator==(Move const& other) const {
                return from == other.from && to == other.to && promote == other.promote;
            }
            bool valid() const {
                return !(from == to && from == 0);
            }
        } MoveType;

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
            bool castled;
        } s[2];

        uint64_t knightAttacks[64];
        uint64_t pawnAttacks[2][64];
        uint64_t kingAttacks[64];

        uint64_t hash;
        int32_t value;
        uint32_t moves;
        bool turn;

        bool operator==(const BitBoard &other) const { 
            assert(0); // Should never need to deep compare boards, only hashes
            return hash == other.hash;
        }

        BitBoard(bool startpos=true);
        int32_t getBoardValue() const;
        void changeTurn();
        int32_t getPieceValue(BitBoardState::Piece const& piece) const;
        void printBoard() const;
        void printMobility() const;
        static void strToMove(std::string moveText, MoveType& move);
        static std::string moveToStr(MoveType& move);
        enum BitBoardState::Piece movePiece(MoveType const& move);
        void sortMoves(std::array<Move,MAX_MOVES>& moves, uint8_t numMoves) const;
        bool canReduce(Move const& move) const;
        uint8_t getAvailableMoves(std::array<Move,MAX_MOVES>& movesAvailable, bool capturesOnly=false) const;
        bool testInCheck(bool c) const;
        int32_t estimateMoveValue(Move const& move) const;
        void recalculateOccupancy();
        void recalculateThreats();
        int32_t evaluateKingSafety() const;

    private:
        enum BitBoardState::Piece getPiece(struct Pieces const& p, uint8_t pos) const;
        void maskClearBitBoard(enum BitBoardState::Piece piece, bool c, uint64_t m);
        void maskSetBitBoard(enum BitBoardState::Piece piece, bool c, uint64_t m);
        std::string pieceToUnicode(enum BitBoardState::Piece piece, enum BitBoardState::Color c) const;
        void validateBitBoard() const;
        bool isAvailable(uint8_t const pos) const;
        bool isOccupied(uint8_t const pos) const;

        uint32_t searchStraight(uint64_t bitboard, uint64_t shiftFunc(uint64_t const b), 
                                       std::array<Move,MAX_MOVES>::iterator& moves, 
                                       bool capturesOnly=false) const;
        int32_t pstDelta(Move const& move) const;
        uint32_t getPawnMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const;
        uint32_t getKingMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const;
        uint32_t getKnightMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const;
        uint32_t getRookMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const;
        uint32_t getBishopMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const;
        void maskIfPositionAttacked(uint64_t& bb, bool c) const;
};
#endif