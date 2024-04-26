#ifndef __BOARD_STATE_INC_GUARD__
#define __BOARD_STATE_INC_GUARD__

#include <iostream>
#include <assert.h>
#include <vector>
#include <array>
#include <algorithm>
#include <cstring>

#include "transpositionTables.hpp"
#include "defines.hpp"

namespace BoardState {
    static BoardType const initialBoard = 
        {.b={.data64 = {0x1111111123465432,
                      0, 
                      0, 
                      0xabcedcba99999999}}};

    static BoardType const emptyBoard =
        {.b={.data64 = {0,
                      0, 
                      0, 
                      0}},
         .cached={.whiteShort=false,.whiteLong=false,.blackShort=false,.blackLong=false,}};

    static inline int32_t getPieceValue(Piece const& piece)
    {
        int32_t value;
        switch(piece) {
            case PAWN:
                value = PAWN_VALUE;
                break;
            case KNIGHT:
                value = KNIGHT_VALUE;
                break;
            case BISHOP:
                value = BISHOP_VALUE;
                break;
            case ROOK:
                value = ROOK_VALUE;
                break;
            case QUEEN:
                value = QUEEN_VALUE;
                break;
            case KING:
                value = KING_VALUE;
                break;
            case EMPTY:
                value = 0;
                break;
            default:
                assert(0);
        }
        return value;
    }

    static inline enum ColorPiece getColorPiece(BoardType const& board, uint8_t pos) 
    {
        uint8_t piece = board.b.data8[pos>>1] >> (pos&0x1 ? 4 : 0);
        piece &= pieceColorMask;
        assert(piece != BEMPTY);
        return static_cast<enum ColorPiece>(piece);
    }

    static inline enum Piece getPiece(BoardType const& board, uint8_t pos) 
    {
        return static_cast<enum Piece>(getColorPiece(board,pos) & pieceMask);
    }

    static inline enum Color getColor(BoardType const& board, uint8_t pos) 
    {
        return static_cast<enum Color>((getColorPiece(board,pos) & colorMask) >> 3);
    }
    
    static inline void changeTurn(BoardType& board) {
        board.value = -board.value;
        board.hash ^= TURN_HASH_WHITE;
        if (board.turn == WHITE) board.turn = BLACK;
        else board.turn = WHITE;
    }

    static inline void strToMove(BoardType const& board, std::string moveText, MoveType& move) {
        assert(moveText.size() == 4 || moveText.size() == 5);

        uint8_t col = moveText[0] - 'a';
        uint8_t row = moveText[1] - '0' - 1;
        uint8_t idx = (row<<3) | col;
        assert(idx < 64);
        move.from = idx;

        col = moveText[2] - 'a';
        row = moveText[3] - '0' - 1;
        idx = idx = (row<<3) | col;
        assert(idx < 64);
        move.to = idx;

        if (moveText.size() == 5) {
            switch(moveText[4]) {
                case 'n':
                    move.promote = static_cast<ColorPiece>(board.turn | KNIGHT);
                    break;
                case 'b':
                    move.promote = static_cast<ColorPiece>(board.turn | BISHOP);
                    break;
                case 'r':
                    move.promote = static_cast<ColorPiece>(board.turn | ROOK);
                    break;
                case 'q':
                    move.promote = static_cast<ColorPiece>(board.turn | QUEEN);
                    break;
                default:
                    assert(0);
            }
        } else {
            move.promote = WEMPTY;
        }
    }

    static inline std::string moveToStr(MoveType& move) {
        char colF = 'a' + (move.from & 7);
        char rowF = '1' + (move.from >> 3);
        char colT = 'a' + (move.to & 7);
        char rowT = '1' + (move.to >> 3);
        std::string moveStr;
        moveStr.append(1, colF);
        moveStr.append(1, rowF);
        moveStr.append(1, colT);
        moveStr.append(1, rowT);
        if (move.promote != WEMPTY) {
            switch(move.promote & pieceMask) {
                case KNIGHT:
                    moveStr.append(1, 'n');
                    break;
                case BISHOP:
                    moveStr.append(1, 'b');
                    break;
                case ROOK:
                    moveStr.append(1, 'r');
                    break;
                case QUEEN:
                    moveStr.append(1, 'q');
                    break;
                default:
                    assert(0);
            }
        }
        return moveStr;
    }

    static inline std::string pieceToString(enum ColorPiece piece) {
        std::string pieceString = "INVALID";
        switch(piece) {
            case WEMPTY:
            case BEMPTY:
                pieceString = "Empty";
                break;
            case WPAWN:
                pieceString = "White Pawn";
                break;
            case BPAWN:
                pieceString = "Black Pawn";
                break;
            case WROOK:
                pieceString = "White Rook";
                break;
            case BROOK:
                pieceString = "Black Rook";
                break;
            case WBISHOP:
                pieceString = "White Bishop";
                break;
            case BBISHOP:
                pieceString = "Black Bishop";
                break;
            case WKNIGHT:
                pieceString = "White Knight";
                break;
            case BKNIGHT:
                pieceString = "Black Knight";
                break;
            case WKING:
                pieceString = "White King";
                break;
            case BKING:
                pieceString = "Black King";
                break;
            case WQUEEN:
                pieceString = "White Queen";
                break;
            case BQUEEN:
                pieceString = "Black Queen";
                break;
            default:
                assert(0);
                break;
        }
        return pieceString;
    }

    static inline std::string pieceToUnicode(enum ColorPiece piece) {
        std::string pieceChar = "?";
        switch(piece) {
            case WEMPTY:
                pieceChar = ".";
                break;
            case BPAWN:
                pieceChar = "\u2659";
                break;
            case WPAWN:
                pieceChar = "\u265f";
                break;
            case BROOK:
                pieceChar = "\u2656";
                break;
            case WROOK:
                pieceChar = "\u265c";
                break;
            case BBISHOP:
                pieceChar = "\u2657";
                break;
            case WBISHOP:
                pieceChar = "\u265d";
                break;
            case BKNIGHT:
                pieceChar = "\u2658";
                break;
            case WKNIGHT:
                pieceChar = "\u265e";
                break;
            case BKING:
                pieceChar = "\u2654";
                break;
            case WKING:
                pieceChar = "\u265a";
                break;
            case BQUEEN:
                pieceChar = "\u2655";
                break;
            case WQUEEN:
                pieceChar = "\u265b";
                break;
            default:
                assert(0);
                break;
        }
        return pieceChar;
    }

    static inline int32_t getBoardValue(BoardType const& board) {
        int32_t value = 0;
        for (uint8_t idx = 0; idx < 32; idx++) {
            // Are both packed squares empty?
            if (!board.b.data8[idx]) continue;

            uint8_t data = board.b.data8[idx];
            for (uint8_t nib = 0; nib < 2; nib++) {
                uint8_t pieceData = data >> (nib ? 4 : 0);
                pieceData &= pieceColorMask;
            
                // Is the square empty?
                if (!(pieceData)) continue;

                enum Piece piece = static_cast<enum Piece>(pieceData & pieceMask);
                value += getPieceValue(piece) 
                             * (!(board.turn ^ static_cast<enum Color>(pieceData & colorMask)) ? 1 : -1);
            }
        }
        return value;
    }

    static inline void printBoard(BoardType const& board) {
        std::cout << std::endl;
        for (uint8_t row = 8; row > 0; row--) {
            std::cout << std::to_string(row) << " ";
            for (uint8_t col = 0; col < 8; col++) {
                uint8_t pos = ((row-1)<<3) | col;
                enum ColorPiece piece = getColorPiece(board, pos);
                std::cout << pieceToUnicode(piece) << " ";
            }
            std::cout << std::endl;
        }
        
        std::cout << "* ";
        for (uint8_t col = 0; col < 8; col++) {
            char colChar = 'a' + col;
            std::cout << colChar << " ";
        }
        std::cout << std::endl;
        std::cout << "Material Imbalance: " << std::to_string(board.value) << std::endl;
        std::cout << "Turn: " << (board.turn == WHITE ? "White" : "Black") << std::endl;
        std::cout << "Board hash: 0x" << std::hex << board.hash << std::endl;
    }

    static inline void validateBoardState(BoardType const& board, TT* tt) {
        #ifdef ASSERT_ON
        assert(board.value == getBoardValue(board));
        assert(board.hash == tt->genHash(board));
        #endif
    }

    static inline enum ColorPiece movePiece(BoardType& board, MoveType const& move, TT* tt) {
        uint8_t from = move.from;
        uint8_t to = move.to;
        
        uint8_t fromPiece = getColorPiece(board, from);
        uint8_t toPiece = getColorPiece(board, to);;
        uint8_t toPieceOther = (board.b.data8[to>>1] & (to&0x1 ?  pieceColorMask : (pieceColorMask<<4)));

        board.hash ^= tt->BOARDPOS_HASH[from][fromPiece];

        if (toPiece&pieceMask) {
            board.value += getPieceValue(static_cast<Piece>(toPiece&pieceMask));
            board.hash ^= tt->BOARDPOS_HASH[to][toPiece];
        }

        if (move.promote != WEMPTY) {
            fromPiece = move.promote;
            board.value += getPieceValue(static_cast<Piece>(move.promote&pieceMask));
            board.value -= PAWN_VALUE;
            board.hash ^= tt->BOARDPOS_HASH[to][move.promote];
        } else {
            board.hash ^= tt->BOARDPOS_HASH[to][fromPiece];
        }
        board.b.data8[to>>1] = (fromPiece << (to&0x1 ? 4 : 0)) | toPieceOther;
        
        if (board.cached.enPassantSquare != 0 && board.cached.enPassantSquare == to) {
            uint8_t newTo;
            if (fromPiece== WPAWN) { newTo = to-8; } 
            else if (fromPiece== BPAWN) { newTo = to+8; }
            toPiece = getColorPiece(board, newTo);
            toPieceOther = (board.b.data8[newTo>>1] & (newTo&0x1 ?  pieceColorMask : (pieceColorMask<<4)));
            board.b.data8[newTo>>1] = (WEMPTY << (newTo&0x1 ? 4 : 0)) | toPieceOther;
            board.value += PAWN_VALUE;
            board.hash ^= tt->BOARDPOS_HASH[newTo][toPiece];
        }

        uint8_t fromPieceOther = (board.b.data8[from>>1] & (from&0x1 ? pieceColorMask : (pieceColorMask<<4)));
        board.b.data8[from>>1] = fromPieceOther;
        
        board.hash ^= EN_PASSANT_HASH*board.cached.enPassantSquare;
        board.cached.enPassantSquare = 0;
        // En Passant
        if (fromPiece == WPAWN && (to-from) == 16) {
            board.cached.enPassantSquare = from + 8;
        } else if (fromPiece == BPAWN && (from-to) == 16) {
            board.cached.enPassantSquare = to + 8;
        }
        board.hash ^= EN_PASSANT_HASH*board.cached.enPassantSquare;

        // Castling
        if (fromPiece == WKING) {
            if (board.cached.whiteShort && to == 6)
                { movePiece(board, Move(7,5),tt); changeTurn(board); board.cached.whiteCastled = true; }
            else if (board.cached.whiteLong && to == 2) 
                { movePiece(board, Move(0,3),tt); changeTurn(board); board.cached.whiteCastled = true; }
            // Undo castling
            /*else if (from == 6 && to == 4) { movePiece(board, Move(5,7)); changeTurn(board); }
            else if (from == 2 && to == 4) { movePiece(board, Move(3,0)); changeTurn(board); }*/
            board.cached.whiteShort = false;
            board.cached.whiteLong = false;
        } else if (fromPiece == BKING) {
            if (board.cached.blackShort && to == 62) 
                { movePiece(board, Move(63,61),tt); changeTurn(board); board.cached.blackCastled= true; }
            if (board.cached.blackLong && to == 58) 
                { movePiece(board, Move(56,59),tt); changeTurn(board); board.cached.blackCastled= true; }
            // Undo castling
            /*else if (from == 62 && to == 60) { movePiece(board, Move(61,63)); changeTurn(board); }
            else if (from == 58 && to == 60) { movePiece(board, Move(59,56)); changeTurn(board); }*/
            board.cached.blackShort = false;
            board.cached.blackLong = false;
        } else if (board.cached.whiteShort && fromPiece == WROOK && from == 7) {
            board.cached.whiteShort = false;
        } else if (board.cached.whiteLong && fromPiece == WROOK && from == 0) {
            board.cached.whiteLong = false;
        } else if (board.cached.blackShort && fromPiece == BROOK && from == 63) {
            board.cached.blackShort = false;
        } else if (board.cached.blackLong && fromPiece == BROOK && from == 56) {
            board.cached.blackLong = false;
        } else if (board.cached.whiteShort && toPiece == WROOK && to == 7) {
            board.cached.whiteShort = false;
        } else if (board.cached.whiteLong && toPiece == WROOK && to == 0) {
            board.cached.whiteLong = false;
        } else if (board.cached.blackShort && toPiece == BROOK && to == 63) {
            board.cached.blackShort = false;
        } else if (board.cached.blackLong && toPiece == BROOK && to == 56) {
            board.cached.blackLong = false;
        }

        changeTurn(board);

        validateBoardState(board,tt);
        return static_cast<enum ColorPiece>(toPiece);
    }
/*
    // Currently not used. This is actually slower than just copying the board. 
    // Maybe use this later if implementing bitboards?
    // Does not implement En Passant yet
    static inline void undoMove(BoardType& board, MoveType const& move, 
                                enum ColorPiece oldPiece, BoardType::CachedState oldState) 
    {
        Move tmp = move;
        tmp.from = move.to;
        tmp.to = move.from;
        tmp.promote = WEMPTY;
        movePiece(board, tmp);
        board.b.data8[tmp.from>>1] |= oldPiece << (tmp.from&0x1 ? 4 : 0);
        if (oldPiece&pieceMask) {
            board.value -= getPieceValue(static_cast<Piece>(oldPiece&pieceMask));
            board.hash ^= BOARDPOS_HASH[tmp.from] ^ PIECE_HASH[oldPiece];
        }
        if (move.promote != WEMPTY) {
            board.b.data8[tmp.to>>1] &= pieceColorMask << (tmp.to&0x1 ? 0 : 4);
            board.b.data8[tmp.to>>1] |= (board.turn|PAWN) << (tmp.to&0x1 ? 4 : 0);
            board.value -= getPieceValue(static_cast<Piece>(move.promote&pieceMask));
            board.value += PAWN_VALUE;
            board.hash ^= BOARDPOS_HASH[tmp.to] ^ PIECE_HASH[move.promote];
            board.hash ^= BOARDPOS_HASH[tmp.to] ^ PIECE_HASH[board.turn|PAWN];            
        }
        board.cached = oldState;
        validateBoardState(board);
    }*/

    static inline bool isAvailable(BoardType const& board, uint8_t const pos) {
        uint8_t piece = getColorPiece(board, pos);
        // If empty or opposite color, we can land there
        if (!(piece & pieceMask) || ((piece&colorMask) ^ static_cast<uint8_t>(board.turn))) return true;
        return false;
    }

    static inline bool isOccupied(BoardType const& board, uint8_t const pos) {
        return !!getPiece(board, pos);
    }

    static inline bool posUp(uint8_t const pos, uint8_t& newpos, int8_t n=1) {
        newpos = pos + 8*n;
        if (newpos > 63) return false;
        return true;
    }

    static inline bool posDown(uint8_t const pos, uint8_t& newpos, int8_t n=1) {
        newpos = pos - 8*n;
        if (newpos > 63) return false;
        return true;
    }

    static inline bool posLeft(uint8_t const pos, uint8_t& newpos, int8_t n=1) {
        newpos = pos - n;
        if (newpos >> 3 != pos >> 3) return false;
        return true;
    }

    static inline bool posRight(uint8_t const pos, uint8_t& newpos, int8_t n=1) {
        newpos = pos + n;
        if (newpos >> 3 != pos >> 3) return false;
        return true;
    }

    static inline uint32_t searchStraight(BoardType const& board, uint8_t pos, 
                                      bool searchFunc(uint8_t, uint8_t&, int8_t), 
                                      std::array<Move,MAX_MOVES>::iterator& moves, 
                                      uint8_t lim=0, bool storeMoves=true) 
    {
        uint8_t newpos;
        uint32_t numMoves = 0;
        for (int8_t inc = 1; searchFunc(pos,newpos,inc) && isAvailable(board,newpos); inc++) {
            if (storeMoves) *(moves++) = Move(pos, newpos);
            numMoves++;
            // Break if we terminate on an opponent's piece
            if (isOccupied(board,newpos)) break;
            if (inc==lim) break;
        }
        return numMoves;
    }

    static inline uint32_t searchAngle(BoardType const& board, uint8_t pos, 
                                   bool searchFunc1(uint8_t, uint8_t&, int8_t), 
                                   bool searchFunc2(uint8_t, uint8_t&, int8_t),
                                   std::array<Move,MAX_MOVES>::iterator& moves, 
                                   uint8_t lim=0, bool storeMoves=true) 
    {
        uint8_t newpos;
        uint8_t newnewpos;
        uint32_t numMoves = 0;
        for (int8_t inc = 1; searchFunc1(pos,newpos,inc)
                             && searchFunc2(newpos,newnewpos,inc)
                             && isAvailable(board,newnewpos); inc++) 
        {
            if (storeMoves) *(moves++) = Move(pos, newnewpos);
            numMoves++;
            // Break if we terminate on an opponent's piece
            if (isOccupied(board,newnewpos)) break;
            if (inc==lim) break;
        }
        return numMoves;
    }

    static inline int32_t calculateWeightedMobility(BoardType const& board) {
        int32_t mobility = 0;
        std::array<Move,MAX_MOVES>::iterator moves;

        int32_t const knightFactor = 11;
        int32_t const bishopFactor = 7;
        int32_t const queenFactor = 1;
        int32_t const rookFactor = 1;
        
        for (uint8_t idx = 0; idx < 32; idx++) {
            // Are both packed squares empty?
            if (!board.b.data8[idx]) continue;

            uint8_t data = board.b.data8[idx];
            for (uint8_t nib = 0; nib < 2; nib++) {
                uint8_t pieceData = data >> (nib ? 4 : 0);
                pieceData &= pieceColorMask;
            
                // Is the square empty?
                if (!(pieceData)) continue;

                enum Piece piece = static_cast<enum Piece>(pieceData & pieceMask);
                enum Color color = static_cast<enum Color>(pieceData & colorMask);
                uint8_t pos = (idx<<1) | nib;
                uint8_t newpos = pos;

                int32_t inc = !(board.turn ^ static_cast<enum Color>(pieceData & colorMask)) ? 1 : -1;
                int32_t numMoves = 0;

                switch(piece) {
                    case ROOK:
                        // Prefer vertical mobility
                        numMoves += 2*searchStraight(board, pos, &posUp, moves, 0, false);
                        numMoves += 2*searchStraight(board, pos, &posDown, moves, 0, false);
                        numMoves += searchStraight(board, pos, &posLeft, moves, 0, false);
                        numMoves += searchStraight(board, pos, &posRight, moves, 0, false);
                        mobility += inc * rookFactor * numMoves;
                        break;
                    case KNIGHT:
                        for (int8_t verDir = -1; verDir <= 1; verDir+=2) {
                            for (int8_t horDir = -1; horDir <= 1; horDir+=2) {
                                for (uint8_t horLen = 1; horLen < 3; horLen++) {
                                    uint8_t newnewpos;
                                    if (posUp(pos, newpos, verDir*(horLen==1 ? 2 : 1)) 
                                        && posLeft(newpos, newnewpos, horDir*horLen)
                                        && isAvailable(board, newnewpos))
                                    {
                                        numMoves++;
                                    }
                                }
                            }
                        }
                        mobility += inc * knightFactor * numMoves;
                        break;
                    case BISHOP:
                        numMoves += (board, pos, &posUp, &posLeft, moves, 0, false);
                        numMoves += searchAngle(board, pos, &posUp, &posRight, moves, 0, false);
                        numMoves += searchAngle(board, pos, &posDown, &posLeft, moves, 0, false);
                        numMoves += searchAngle(board, pos, &posDown, &posRight, moves, 0, false);
                        mobility += inc * bishopFactor * numMoves;
                        break;
                    /*case QUEEN:
                        numMoves += searchStraight(board, pos, &posUp, moves, 0, false);
                        numMoves += searchStraight(board, pos, &posDown, moves, 0, false);
                        numMoves += searchStraight(board, pos, &posLeft, moves, 0, false);
                        numMoves += searchStraight(board, pos, &posRight, moves, 0, false);
                        numMoves += searchAngle(board, pos, &posUp, &posLeft, moves, 0, false);
                        numMoves += searchAngle(board, pos, &posUp, &posRight, moves, 0, false);
                        numMoves += searchAngle(board, pos, &posDown, &posLeft, moves, 0, false);
                        numMoves += searchAngle(board, pos, &posDown, &posRight, moves, 0, false);
                        mobility += inc * queenFactor* numMoves;
                        break;*/
                    default:
                        break;
                }
            }
        }

        return mobility;
    }

    static inline uint8_t getAvailableMoves(BoardType const& board, 
                                            std::array<Move,MAX_MOVES>& movesAvailable) {
        std::array<Move,MAX_MOVES>::iterator moves = movesAvailable.begin();
        for (uint8_t idx = 0; idx < 32; idx++) {
            // Are both packed squares empty?
            if (!board.b.data8[idx]) continue;

            uint8_t data = board.b.data8[idx];
            for (uint8_t nib = 0; nib < 2; nib++) {
                uint8_t pieceData = data >> (nib ? 4 : 0);
                pieceData &= pieceColorMask;
            
                // Is the square empty?
                if (!(pieceData)) continue;
                // Is the piece the same color as the board.turn we are searching?
                if (board.turn ^ static_cast<enum Color>(pieceData & colorMask)) continue;

                enum Piece piece = static_cast<enum Piece>(pieceData & pieceMask);
                enum Color color = static_cast<enum Color>(pieceData & colorMask);
                uint8_t pos = (idx<<1) | nib;
                uint8_t newpos = pos;

                switch(piece) {
                    case PAWN:
                    {
                        int8_t direction = (color == BLACK) ? -1 : 1;
                        if (color == WHITE && (pos >> 3) == 6 && !isOccupied(board,pos+8)) {
                            // promotion
                            *(moves++) = Move(pos, pos+8, static_cast<ColorPiece>(WKNIGHT));
                            *(moves++) = Move(pos, pos+8, static_cast<ColorPiece>(WBISHOP));
                            *(moves++) = Move(pos, pos+8, static_cast<ColorPiece>(WROOK));
                            *(moves++) = Move(pos, pos+8, static_cast<ColorPiece>(WQUEEN));
                        } else if (color == BLACK && (pos >> 3) == 1 && !isOccupied(board,pos-8)) {
                            // promotion
                            *(moves++) = Move(pos, pos-8, static_cast<ColorPiece>(BKNIGHT));
                            *(moves++) = Move(pos, pos-8, static_cast<ColorPiece>(BBISHOP));
                            *(moves++) = Move(pos, pos-8, static_cast<ColorPiece>(BROOK));
                            *(moves++) = Move(pos, pos-8, static_cast<ColorPiece>(BQUEEN));
                        } else if (posUp(pos,newpos,direction) && !isOccupied(board,newpos)) {
                            // standard move 1 square up
                            *(moves++) = Move(pos, newpos);
                            // start move 2 squares up if we are on the starting square
                            if (((((pos >> 3) == 1) && board.turn == WHITE) || (((pos >> 3) == 6) && board.turn == BLACK))
                                && posUp(pos,newpos, direction*2) && !isOccupied(board,newpos))
                            {
                                *(moves++) = Move(pos, newpos);;
                            }
                        }

                        // Kill adjacent piece
                        if (!posUp(pos, newpos, direction)) assert(0);
                        uint8_t newnewpos;
                        if (posLeft(newpos, newnewpos) && isAvailable(board, newnewpos) 
                            && (isOccupied(board, newnewpos) || newnewpos == board.cached.enPassantSquare))
                        {
                            if ((newnewpos >> 3) == 0 || (newnewpos >> 3) == 7) {
                                *(moves++) = Move(pos, newnewpos, static_cast<ColorPiece>(color|QUEEN));
                                *(moves++) = Move(pos, newnewpos, static_cast<ColorPiece>(color|KNIGHT));
                                *(moves++) = Move(pos, newnewpos, static_cast<ColorPiece>(color|BISHOP));
                                *(moves++) = Move(pos, newnewpos, static_cast<ColorPiece>(color|ROOK));
                            } else {
                                *(moves++) = Move(pos, newnewpos);
                            }
                        }
                        if (posRight(newpos, newnewpos) && isAvailable(board, newnewpos) 
                            && (isOccupied(board, newnewpos) || newnewpos == board.cached.enPassantSquare))
                        {
                            if ((newnewpos >> 3) == 0 || (newnewpos >> 3) == 7) {
                                *(moves++) = Move(pos, newnewpos, static_cast<ColorPiece>(color|QUEEN));
                                *(moves++) = Move(pos, newnewpos, static_cast<ColorPiece>(color|KNIGHT));
                                *(moves++) = Move(pos, newnewpos, static_cast<ColorPiece>(color|BISHOP));
                                *(moves++) = Move(pos, newnewpos, static_cast<ColorPiece>(color|ROOK));
                            } else {
                                *(moves++) = Move(pos, newnewpos);
                            }
                        }
                        break;
                    }
                    case ROOK:
                        searchStraight(board, pos, &posUp, moves);
                        searchStraight(board, pos, &posDown, moves);
                        searchStraight(board, pos, &posLeft, moves);
                        searchStraight(board, pos, &posRight, moves);
                        break;
                    case KNIGHT:
                        for (int8_t verDir = -1; verDir <= 1; verDir+=2) {
                            for (int8_t horDir = -1; horDir <= 1; horDir+=2) {
                                for (uint8_t horLen = 1; horLen < 3; horLen++) {
                                    uint8_t newnewpos;
                                    if (posUp(pos, newpos, verDir*(horLen==1 ? 2 : 1)) 
                                        && posLeft(newpos, newnewpos, horDir*horLen)
                                        && isAvailable(board, newnewpos))
                                    {
                                        *(moves++) = Move(pos, newnewpos);
                                    }
                                }
                            }
                        }
                        break;
                    case BISHOP:
                        searchAngle(board, pos, &posUp, &posLeft, moves);
                        searchAngle(board, pos, &posUp, &posRight, moves);
                        searchAngle(board, pos, &posDown, &posLeft, moves);
                        searchAngle(board, pos, &posDown, &posRight, moves);
                        break;
                    case QUEEN:
                        searchStraight(board, pos, &posUp, moves);
                        searchStraight(board, pos, &posDown, moves);
                        searchStraight(board, pos, &posLeft, moves);
                        searchStraight(board, pos, &posRight, moves);
                        searchAngle(board, pos, &posUp, &posLeft, moves);
                        searchAngle(board, pos, &posUp, &posRight, moves);
                        searchAngle(board, pos, &posDown, &posLeft, moves);
                        searchAngle(board, pos, &posDown, &posRight, moves);
                        break;
                    case KING:
                        // Castling
                        if ((board.turn == WHITE && board.cached.whiteShort)
                            || (board.turn == BLACK && board.cached.blackShort)) 
                        {
                            if ((posRight(pos, newpos, 1) && !isOccupied(board, newpos))
                                && 
                                (posRight(pos, newpos, 2) && !isOccupied(board, newpos)))
                            {
                                *(moves++) = Move(pos, newpos);
                            }
                        } else if ((board.turn == WHITE && board.cached.whiteLong)
                                   || (board.turn == BLACK && board.cached.blackLong)) 
                        {
                            if ((posLeft(pos, newpos, 1) && !isOccupied(board, newpos))
                                && 
                                (posLeft(pos, newpos, 3) && !isOccupied(board, newpos))
                                && 
                                (posLeft(pos, newpos, 2) && !isOccupied(board, newpos)))
                            {
                                *(moves++) = Move(pos, newpos);
                            }
                        }

                        searchStraight(board, pos, &posUp, moves, 1);
                        searchStraight(board, pos, &posDown, moves, 1);
                        searchStraight(board, pos, &posLeft, moves, 1);
                        searchStraight(board, pos, &posRight, moves, 1);
                        searchAngle(board, pos, &posUp, &posLeft, moves, 1);
                        searchAngle(board, pos, &posUp, &posRight, moves, 1);
                        searchAngle(board, pos, &posDown, &posLeft, moves, 1);
                        searchAngle(board, pos, &posDown, &posRight, moves, 1);
                        break;
                    default:
                        assert(0);
                }
                assert(moves < movesAvailable.end());
            }
        }

        return moves - movesAvailable.begin();
    }

    inline bool preferCenter(Move m1, Move m2) {
        // Multiply by 2 to get round number center
        int16_t r1 = m1.to >> 2;
        int16_t r2 = m2.to >> 2;
        int16_t c1 = (m1.to & 0x7) << 1;
        int16_t c2 = (m2.to & 0x7) << 1;
        
        int16_t dist_r1 = (7-r1) * (7-r1);
        int16_t dist_r2 = (7-r2) * (7-r2);
        int16_t dist_c1 = (7-c1) * (7-c1);
        int16_t dist_c2 = (7-c2) * (7-c2);

        return (dist_r1 + dist_c1) < (dist_r2 + dist_c2);
    }

    static inline void sortMoves(std::array<Move,MAX_MOVES>& moves, 
                                 Move const& principalMove, Move const& killerMove,
                                 uint8_t numMoves, BoardType const& board) 
    {
        std::array<Move,MAX_MOVES> movesTmp = {0};

        uint8_t dst = 0;
        bool killerValid = false;
        bool principalValid = false;
        for (uint8_t src = 0; src < numMoves; src++) {
            if (principalMove.valid() && moves[src] == principalMove) { principalValid = true; continue; }
            if (killerMove.valid() && moves[src] == killerMove) { killerValid = true; continue; }
            movesTmp[dst++] = moves[src];
        }
        assert(!principalMove.valid() || principalValid);

        // This sort will return "prefer center" if there are no captures. 
        // If there are captures, prefer moves that capture high value targets
        // If m1 promotes, prefer it
        std::sort(movesTmp.begin(), movesTmp.begin() + dst, 
                  [&] (Move m1, Move m2) {
                    uint32_t v1_target = getPieceValue(getPiece(board, m1.to));
                    uint32_t v2_target = getPieceValue(getPiece(board, m2.to));
                    if (v1_target == 0 && v2_target == 0) return preferCenter(m1,m2);
                    else if (v1_target > 0 || v2_target > 0) return v1_target > v2_target;
                    uint32_t v1_source = getPieceValue(getPiece(board, m1.from));
                    uint32_t v2_source = getPieceValue(getPiece(board, m2.from));
                    return (v1_target-v1_source) > (v2_target-v2_source);
                  });
        
        dst = 0;
        if (principalValid) moves[dst++] = principalMove;
        if (killerValid) moves[dst++] = killerMove;
        
        std::memcpy(&moves[dst], &movesTmp[0], (numMoves-dst)*sizeof(Move));
    }

    static inline bool canReduce(BoardType const& board, Move const& move) {
        // Do not reduce captures
        return !isOccupied(board, move.to);
    }
};

#endif