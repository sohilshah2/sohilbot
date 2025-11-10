#include "bitboard.hpp"
#include "evaluate.hpp"
#include "transpositionTables.hpp"

using namespace BitBoardState;

const BitBoard::MoveData BitBoard::DEFAULT_MOVE = {.isCapture=false, .isCastle=false, .isEnPassant=false, .isPromotion=false};
const BitBoard::MoveData BitBoard::CAPTURE_MOVE = {.isCapture=true, .isCastle=false, .isEnPassant=false, .isPromotion=false};
const BitBoard::MoveData BitBoard::EN_PASSANT_MOVE = {.isCapture=true, .isCastle=false, .isEnPassant=true, .isPromotion=false};
const BitBoard::MoveData BitBoard::CASTLE_MOVE = {.isCapture=false, .isCastle=true, .isEnPassant=false, .isPromotion=false};
const BitBoard::MoveData BitBoard::PROMOTION_MOVE = {.isCapture=false, .isCastle=false, .isEnPassant=false, .isPromotion=true};
const BitBoard::MoveData BitBoard::PROMOTE_CAPTURE = {.isCapture=true, .isCastle=false, .isEnPassant=false, .isPromotion=true};

inline uint64_t shiftNorth(uint64_t const b) { return b << 8; }
inline uint64_t shiftSouth(uint64_t const b) { return b >> 8; }

inline uint64_t shiftEast(uint64_t const b) {return (b << 1) & notAFile;}
inline uint64_t shiftNoEast(uint64_t const b) {return (b << 9) & notAFile;}
inline uint64_t shiftSoEast(uint64_t const b) {return (b >> 7) & notAFile;}
inline uint64_t shiftWest(uint64_t const b) {return (b >> 1) & notHFile;}
inline uint64_t shiftSoWest(uint64_t const b) {return (b >> 9) & notHFile;}
inline uint64_t shiftNoWest(uint64_t const b) {return (b << 7) & notHFile;}

BitBoard::BitBoard(TT* _tt, bool startpos) {
    tt = _tt;

    if (tt) {
        tt->clear();
    }

    moves = 0;
    if (startpos) {
        p[0] = {.pawn=0xff00,.knight=0x42,.bishop=0x24,.rook=0x81,.queen=0x8,.king=0x10};
        p[1] = {.pawn=0xff000000000000ull,.knight=0x4200000000000000ull,
                .bishop=0x2400000000000000ull,.rook=0x8100000000000000ull,
                .queen=0x800000000000000ull,.king=0x1000000000000000ull};

        s[0] = s[1] = {.occupancy=0,.scope=0,.mobility=0,.enPassantSquare=0,
                        .castleShort=true,.castleLong=true};
    } else {
        p[0] = p[1] = {.pawn=0,.knight=0,.bishop=0,.rook=0,.queen=0,.king=0};
        s[0] = s[1] = {.occupancy=0,.scope=0,.mobility=0,.enPassantSquare=0,
                        .castleShort=false,.castleLong=false};
    }

    turn = WHITE;
    if (tt) {
        hash = tt->genHash(*this);
    }
    value = 0;

    for (uint8_t pos = 0; pos < 64; pos++) {
        uint64_t attacks = 0;
        uint64_t sftBoard = 1ull << pos;

        attacks |= shiftNorth(shiftNoEast(sftBoard));
        attacks |= shiftNorth(shiftNoWest(sftBoard));

        attacks |= shiftEast(shiftNoEast(sftBoard));
        attacks |= shiftEast(shiftSoEast(sftBoard));
        
        attacks |= shiftSouth(shiftSoEast(sftBoard));
        attacks |= shiftSouth(shiftSoWest(sftBoard));
        
        attacks |= shiftWest(shiftNoWest(sftBoard));
        attacks |= shiftWest(shiftSoWest(sftBoard));

        knightAttacks[pos] = attacks;

        // Pawn angle attacks
        attacks = shiftNoEast(sftBoard);
        attacks |= shiftNoWest(sftBoard);
        pawnAttacks[0][pos] = attacks;
        attacks = shiftSoEast(sftBoard);
        attacks |= shiftSoWest(sftBoard);
        pawnAttacks[1][pos] = attacks;

        // King attacks
        attacks = shiftNorth(sftBoard);
        attacks |= shiftSoEast(sftBoard);
        attacks |= shiftEast(sftBoard);
        attacks |= shiftNoEast(sftBoard);
        attacks |= shiftNoWest(sftBoard);
        attacks |= shiftSouth(sftBoard);
        attacks |= shiftWest(sftBoard);
        attacks |= shiftSoWest(sftBoard);
        kingAttacks[pos] = attacks;
    }
    recalculateOccupancy();
    recalculateThreats();
}

void BitBoard::recalculateOccupancy() {
    for (uint8_t turn = WHITE; turn <= BLACK; turn++) {
        s[turn].occupancy = p[turn].pawn | p[turn].bishop 
                                        | p[turn].knight | p[turn].rook
                                        | p[turn].queen | p[turn].king;
    }

    assert((s[0].occupancy ^ s[1].occupancy) 
            == (s[0].occupancy | s[1].occupancy));
}

void BitBoard::recalculateThreats() {
    for (uint8_t c = WHITE; c <= BLACK; c++) {
        s[c].mobility = 0;
        s[c].scope = 0;
        uint64_t friendly = ~s[c].occupancy;
        uint64_t enemy = ~s[!c].occupancy;

        uint64_t sliding = p[c].rook | p[c].queen;
        uint64_t angle = p[c].bishop | p[c].queen;

        uint64_t bb = p[c].knight;
        while (bb) {
            // Isolate the least significant bit and find it
            uint8_t pos = __builtin_ctzll(bb);
            uint64_t attacks = knightAttacks[pos];
            s[c].mobility |= attacks;
            // Clear last least significant bit
            s[c].scope += __builtin_popcountll(attacks & (~enemy | ~friendly));
            bb &= bb - 1;
        }

        bb = p[c].king;
        while (bb) {
            uint8_t pos = __builtin_ctzll(bb);
            uint64_t attacks = kingAttacks[pos];
            s[c].mobility |= attacks;
            s[c].scope += __builtin_popcountll(attacks & (~enemy | ~friendly));
            bb &= bb - 1;
        }

        bb = p[c].pawn;
        while (bb) {
            uint8_t pos = __builtin_ctzll(bb);
            uint64_t attacks = pawnAttacks[c][pos];
            s[c].mobility |= attacks;
            s[c].scope += __builtin_popcountll(attacks & (~enemy | ~friendly));
            bb &= bb - 1;
        }


        auto slidingFuncs = {&shiftNorth, &shiftEast, &shiftSouth, &shiftWest};
        for (auto func : slidingFuncs) {
            bb = sliding;
            while (bb) {
                bb = func(bb);
                s[c].mobility |= bb;
                s[c].scope += __builtin_popcountll(bb & (~enemy | ~friendly));
                bb &= friendly;
                bb &= enemy;
            }
        }

        auto angleFuncs = {&shiftNoEast, &shiftNoWest, &shiftSoEast, &shiftSoWest};
        for (auto func : angleFuncs) {
            uint64_t bb = angle;
            while (bb) {
                bb = func(bb);
                s[c].mobility |= bb;
                s[c].scope += __builtin_popcountll(bb & (~enemy | ~friendly));
                bb &= friendly;
                bb &= enemy;
            }
        }
    }
}

int32_t BitBoard::getPieceValue(Piece const& piece) const
{
    int32_t value;
    bool isEndgame = moves > ENDGAME_CUTOFF;
    switch(piece) {
        case PAWN:
            value = isEndgame ? PAWN_VALUE_EG : PAWN_VALUE_MG;
            break;
        case KNIGHT:
            value = isEndgame ? KNIGHT_VALUE_EG : KNIGHT_VALUE_MG;
            break;
        case BISHOP:
            value = isEndgame ? BISHOP_VALUE_EG : BISHOP_VALUE_MG;
            break;
        case ROOK:
            value = isEndgame ? ROOK_VALUE_EG : ROOK_VALUE_MG;
            break;
        case QUEEN:
            value = isEndgame ? QUEEN_VALUE_EG : QUEEN_VALUE_MG;
            break;
        case KING:
            value = KING_STRENGTH_VALUE;
            break;
        case EMPTY:
            value = 0;
            break;
        default:
            assert(0);
    }
    return value;
}

enum Piece BitBoard::getPiece(struct Pieces const& p, uint8_t pos) const
{
    return ((p.pawn >> pos) & 0x1) ? PAWN :
            ((p.knight >> pos) & 0x1) ? KNIGHT :
            ((p.bishop >> pos) & 0x1) ? BISHOP :
            ((p.rook >> pos) & 0x1) ? ROOK :
            ((p.king >> pos) & 0x1) ? KING :
            ((p.queen >> pos) & 0x1) ? QUEEN : EMPTY;
}

void BitBoard::maskClearBitBoard(enum Piece piece, bool c, uint64_t m) {
    struct Pieces& bitboard = p[c];
    switch(piece) {
        case PAWN:
            bitboard.pawn &= ~m;
            break;
        case KNIGHT:
            bitboard.knight &= ~m;
            break;
        case BISHOP:
            bitboard.bishop &= ~m;
            break;
        case ROOK:
            bitboard.rook &= ~m;
            break;
        case KING:
            bitboard.king &= ~m;
            break;
        case QUEEN:
            bitboard.queen &= ~m;
            break;
        default:
            break;
    }
}

void BitBoard::maskSetBitBoard(enum Piece piece, bool c, uint64_t m) {
    struct Pieces& bitboard = p[c];
    switch(piece) {
        case PAWN:
            bitboard.pawn |= m;
            break;
        case KNIGHT:
            bitboard.knight |= m;
            break;
        case BISHOP:
            bitboard.bishop |= m;
            break;
        case ROOK:
            bitboard.rook |= m;
            break;
        case KING:
            bitboard.king |= m;
            break;
        case QUEEN:
            bitboard.queen |= m;
            break;
        default:
            assert(0);
            break;
    }
}

void BitBoard::changeTurn() {
    hash ^= TURN_HASH_WHITE;
    turn = static_cast<Color>(turn ^ BLACK); // BLACK == 1
}

void BitBoard::strToMove(std::string const& moveText, struct Move& move) {
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
                move.promote = static_cast<Piece>(KNIGHT);
                break;
            case 'b':
                move.promote = static_cast<Piece>(BISHOP);
                break;
            case 'r':
                move.promote = static_cast<Piece>(ROOK);
                break;
            case 'q':
                move.promote = static_cast<Piece>(QUEEN);
                break;
            default:
                assert(0);
        }
    } else {
        move.promote = EMPTY;
    }
}

std::string BitBoard::moveToStr(struct Move const& move) {
    char colF = 'a' + (move.from & 7);
    char rowF = '1' + (move.from >> 3);
    char colT = 'a' + (move.to & 7);
    char rowT = '1' + (move.to >> 3);
    std::string moveStr;
    moveStr.append(1, colF);
    moveStr.append(1, rowF);
    moveStr.append(1, colT);
    moveStr.append(1, rowT);
    if (move.promote != EMPTY) {
        switch(move.promote) {
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
std::string BitBoard::pieceToUnicode(enum Piece piece, enum Color c) const {
    std::string pieceChar = "?";
    if (c == WHITE) {
        switch(piece) {
            case EMPTY:
                pieceChar = ".";
                break;
            case PAWN:
                pieceChar = "\u265f";
                break;
            case ROOK:
                pieceChar = "\u265c";
                break;
            case BISHOP:
                pieceChar = "\u265d";
                break;
            case KNIGHT:
                pieceChar = "\u265e";
                break;
            case KING:
                pieceChar = "\u265a";
                break;
            case QUEEN:
                pieceChar = "\u265b";
                break;
            default:
                assert(0);
                break;
        }
    } else if (c == BLACK) {
        switch(piece) {
            case EMPTY:
                pieceChar = ".";
                break;
            case PAWN:
                pieceChar = "\u2659";
                break;
            case ROOK:
                pieceChar = "\u2656";
                break;
            case BISHOP:
                pieceChar = "\u2657";
                break;
            case KNIGHT:
                pieceChar = "\u2658";
                break;
            case KING:
                pieceChar = "\u2654";
                break;
            case QUEEN:
                pieceChar = "\u2655";
                break;
            default:
                assert(0);
                break;
        }
    }
    return pieceChar;
}

int32_t BitBoard::getBoardValue() const {
    int32_t value = 0;
    for (uint8_t c = WHITE; c <= BLACK; c++) {
        int8_t count = __builtin_popcountll(p[c].pawn);
        value += getPieceValue(PAWN)*count*(c==turn ? 1 : -1);

        count = __builtin_popcountll(p[c].bishop);
        value += getPieceValue(BISHOP)*count*(c==turn ? 1 : -1);

        count = __builtin_popcountll(p[c].knight);
        value += getPieceValue(KNIGHT)*count*(c==turn ? 1 : -1);

        count = __builtin_popcountll(p[c].rook);
        value += getPieceValue(ROOK)*count*(c==turn ? 1 : -1);

        count = __builtin_popcountll(p[c].queen);
        value += getPieceValue(QUEEN)*count*(c==turn ? 1 : -1);
    }
    return value;
}

void BitBoard::printBoard() const {
    std::cout << std::endl;
    for (uint8_t row = 8; row > 0; row--) {
        std::cout << std::to_string(row) << " ";
        for (uint8_t col = 0; col < 8; col++) {
            uint8_t pos = ((row-1)<<3) | col;
            enum Piece piece = static_cast<Piece>(getPiece(p[0], pos) | getPiece(p[1], pos));
            Color turn = ((s[0].occupancy >> pos) & 0x1) ? WHITE : BLACK;
            std::cout << pieceToUnicode(piece, turn) << " ";
        }
        std::cout << std::endl;
    }
    
    std::cout << "* ";
    for (uint8_t col = 0; col < 8; col++) {
        char colChar = 'a' + col;
        std::cout << colChar << " ";
    }
    std::cout << std::endl;
    std::cout << "Material Imbalance: " << std::to_string(value) << std::endl;
    std::cout << "Turn: " << (turn == WHITE ? "White" : "Black") << std::endl;
    std::cout << "Board hash: 0x" << std::hex << hash << std::endl;
}

void BitBoard::printMobility() const {
    for (uint8_t c = WHITE; c <= BLACK; c++) {
        std::cout << "Mobility[" << (c == WHITE ? "white" : "black") << "]: 0x" << std::hex << s[c].mobility << std::endl;
        std::cout << "Scope[" << (c == WHITE ? "white" : "black") << "]: " << std::dec << s[c].scope << std::endl;
        std::cout << "Occupancy[" << (c == WHITE ? "white" : "black") << "]: 0x" << std::hex << s[c].occupancy << std::endl;

        for (uint8_t row = 8; row > 0; row--) {
            std::cout << std::to_string(row) << " ";
            for (uint8_t col = 0; col < 8; col++) {
                uint8_t pos = ((row-1)<<3) | col;
                std::cout << (((s[c].mobility >> pos) & 0x1) ? "X" : " ") << " ";
            }
            std::cout << std::endl;
        }
        
        std::cout << "* ";
        for (uint8_t col = 0; col < 8; col++) {
            char colChar = 'a' + col;
            std::cout << colChar << " ";
        }
        std::cout << std::endl;
        std::cout << std::endl;
    }
}

void BitBoard::validateBitBoard() const {
    #ifdef ASSERT_ON
    assert((hash == tt->genHash(*this)));
    #endif
}

void BitBoard::movePiece(struct Move const& move) {
    uint8_t from = move.from;
    uint8_t to = move.to;
    uint64_t fromBitboard = 1ull << from;
    uint64_t toBitboard = 1ull << to;

    enum Piece fromPiece = getPiece(p[turn], from);
    enum Piece toPiece = getPiece(p[!turn], to);

    hash ^= tt->BOARDPOS_HASH[turn][fromPiece][from];
    if (toPiece) hash ^= tt->BOARDPOS_HASH[!turn][toPiece][to];

    maskClearBitBoard(fromPiece, turn, fromBitboard);
    maskClearBitBoard(toPiece, !turn, toBitboard);

    // Promote
    if (move.promote != EMPTY) {
        fromPiece = move.promote;
    }
    
    hash ^= tt->BOARDPOS_HASH[turn][fromPiece][to];
    maskSetBitBoard(fromPiece, turn, toBitboard);

    // En Passant
    if (fromPiece == PAWN && s[turn].enPassantSquare != 0 && s[turn].enPassantSquare == to) {
        maskClearBitBoard(PAWN, !turn, turn ? toBitboard << 8ull : toBitboard >> 8ull);
        hash ^= tt->BOARDPOS_HASH[!turn][PAWN][turn ? to+8 : to-8];
    }

    hash ^= EN_PASSANT_HASH*s[turn].enPassantSquare;
    s[turn].enPassantSquare = 0;
    if (fromPiece == PAWN && (to-from) == 16) {
        s[BLACK].enPassantSquare = from + 8;
    } else if (fromPiece == PAWN && (from-to) == 16) {
        s[WHITE].enPassantSquare = to + 8;
    }
    hash ^= EN_PASSANT_HASH*s[!turn].enPassantSquare;

    // Castling
    if (fromPiece == KING && s[turn].castleShort && to-from == 2) {
        if (s[turn].castleShort) hash ^= turn ? BLACK_CASTLE_SHORT_HASH : WHITE_CASTLE_SHORT_HASH;
        if (s[turn].castleLong) hash ^= turn ? BLACK_CASTLE_LONG_HASH : WHITE_CASTLE_LONG_HASH;
        s[turn].castleShort = false;
        s[turn].castleLong = false;

        maskClearBitBoard(ROOK, turn, toBitboard<<1ull);
        maskSetBitBoard(ROOK, turn, toBitboard>>1ull);
        hash ^= tt->BOARDPOS_HASH[turn][ROOK][to-1];
        hash ^= tt->BOARDPOS_HASH[turn][ROOK][to+1];
    } else if (fromPiece == KING && s[turn].castleLong && from-to == 2) {
        if (s[turn].castleShort) hash ^= turn ? BLACK_CASTLE_SHORT_HASH : WHITE_CASTLE_SHORT_HASH;
        if (s[turn].castleLong) hash ^= turn ? BLACK_CASTLE_LONG_HASH : WHITE_CASTLE_LONG_HASH;
        s[turn].castleShort = false;
        s[turn].castleLong = false;

        maskClearBitBoard(ROOK, turn, toBitboard>>2ull);
        maskSetBitBoard(ROOK, turn, toBitboard<<1ull);
        hash ^= tt->BOARDPOS_HASH[turn][ROOK][to-2];
        hash ^= tt->BOARDPOS_HASH[turn][ROOK][to+1];
    } 

    // Can no longer castle because moved king or rook:
    else if (fromPiece == KING) {
        if (s[turn].castleShort) hash ^= turn ? BLACK_CASTLE_SHORT_HASH : WHITE_CASTLE_SHORT_HASH;
        if (s[turn].castleLong) hash ^= turn ? BLACK_CASTLE_LONG_HASH : WHITE_CASTLE_LONG_HASH;
        s[turn].castleShort = false;
        s[turn].castleLong = false;
    } else if (s[turn].castleShort && fromPiece == ROOK && (from % 8 == 7) && (from / 8 == turn*7)) {
        s[turn].castleShort = false;
        hash ^= turn ? BLACK_CASTLE_SHORT_HASH : WHITE_CASTLE_SHORT_HASH;
    } else if (s[turn].castleLong && fromPiece == ROOK && (from % 8 == 0) && (from / 8 == turn*7)) {
        s[turn].castleLong = false;
        hash ^= turn ? BLACK_CASTLE_LONG_HASH : WHITE_CASTLE_LONG_HASH;
    }

    // Can no longer castle becaue our rook was captured
    if (s[!turn].castleShort && toPiece == ROOK && (to % 8 == 7) && (to / 8 == (!turn)*7)) {
        s[!turn].castleShort = false;
        hash ^= turn ? WHITE_CASTLE_SHORT_HASH : BLACK_CASTLE_SHORT_HASH;
    } else if (s[!turn].castleLong && toPiece == ROOK && (to % 8 == 0) && (to / 8 == (!turn)*7)) {
        s[!turn].castleLong = false;
        hash ^= turn ? WHITE_CASTLE_LONG_HASH : BLACK_CASTLE_LONG_HASH;
    }

    changeTurn();
    moves++;

    recalculateOccupancy();
    recalculateThreats();
    validateBitBoard();
}

bool BitBoard::isAvailable(uint8_t const pos) const {
    return !((s[turn].occupancy >> pos) & 0x1);
}

bool BitBoard::isOccupied(uint8_t const pos) const {
    return ((s[0].occupancy >> pos) & 0x1) || ((s[1].occupancy >> pos) & 0x1);
}

void BitBoard::maskIfPositionAttacked(uint64_t& bb, bool c) const {
    uint64_t occupied = ~(s[c].occupancy | s[!c].occupancy);

    uint8_t pos = __builtin_ctzll(bb);
    
    uint64_t sliding = p[!c].rook | p[!c].queen;
    uint64_t angle = p[!c].bishop | p[!c].queen;

    uint64_t knight = knightAttacks[pos] & p[!c].knight;
    uint64_t pawn = pawnAttacks[c][pos] & p[!c].pawn;
    uint64_t kingEnemy = kingAttacks[pos] & p[!c].king;

    if (knight | pawn | kingEnemy) {
        bb = 0;
        return;
    }

    auto slidingFuncs = {&shiftNorth, &shiftEast, &shiftSouth, &shiftWest};
    for (auto func : slidingFuncs) {
        uint64_t sft = bb;
        while (sft) {
            sft = func(sft);
            if (sft & sliding) {
                bb = 0;
                return;
            }
            sft &= occupied;
        }
    }

    auto angleFuncs = {&shiftNoEast, &shiftNoWest, &shiftSoEast, &shiftSoWest};
    for (auto func : angleFuncs) {
        uint64_t sft = bb;
        while (sft) {
            sft = func(sft);
            if (sft & angle) {
                bb = 0;
                return;
            }
            sft &= occupied;
        }
    }
}

inline uint32_t BitBoard::searchStraight(uint64_t bitboard, uint64_t shiftFunc(uint64_t const b), 
                                        std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const
{
    uint8_t pos, newpos;
    uint32_t numMoves = 0;

    uint64_t friendly = ~s[turn].occupancy;
    uint64_t enemy = ~s[!turn].occupancy;

    while (bitboard) {
        // Isolate the least significant bit and find it
        uint64_t sftBoard = bitboard & -bitboard;
        pos = __builtin_ctzll(bitboard);

        while((sftBoard = shiftFunc(sftBoard))) {
            // Piece is blocked by our own
            if (!(sftBoard = sftBoard & friendly)) break;

            if (!capturesOnly || (sftBoard & ~enemy)) {
                newpos = __builtin_ctzll(sftBoard);
                *(moves++) = Move(pos, newpos, (sftBoard & ~enemy) ? CAPTURE_MOVE : DEFAULT_MOVE);
                numMoves++;
            }

            // Cut path at enemy pieces
            sftBoard &= enemy;
        }
        // Clear last least significant bit
        bitboard &= bitboard - 1;
    }
    return numMoves;
}

uint32_t BitBoard::getPawnMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const {
    uint8_t pos, newpos;
    uint64_t bb = p[turn].pawn;
    
    uint64_t ep = 1ull << s[turn].enPassantSquare;
    // Magic to get rid of the last bit because that means invalid ep
    ep >>= 1;
    ep <<= 1;
    
    auto shiftFunc = (turn == WHITE) ? &shiftNorth : &shiftSouth;
    uint32_t numMoves = 0;
    uint64_t friendly = ~s[turn].occupancy;
    uint64_t enemy = ~s[!turn].occupancy;

    while (bb) {
        // Isolate the least significant bit and find it
        uint64_t sftBoard = bb & -bb;
        pos = __builtin_ctzll(bb);

        if (!capturesOnly) {
            // Up/down one
            uint64_t s1 = shiftFunc(sftBoard);
            if (s1 & friendly & enemy) {
                newpos = __builtin_ctzll(s1);
                // Promotion
                if ((newpos / 8 == 0) || (newpos / 8 == 7)) {
                    *(moves++) = Move(pos, newpos, PROMOTION_MOVE, QUEEN);
                    *(moves++) = Move(pos, newpos, PROMOTION_MOVE, KNIGHT);
                    *(moves++) = Move(pos, newpos, PROMOTION_MOVE, ROOK);
                    *(moves++) = Move(pos, newpos, PROMOTION_MOVE, BISHOP);
                    numMoves += 4;
                } else {
                    *(moves++) = Move(pos, newpos);
                    numMoves++;
                    // Move up 2
                    if ((pos / 8 == 1 && turn == WHITE) 
                        || (pos / 8 == 6 && turn == BLACK)) 
                    {
                        s1 = shiftFunc(s1);
                        newpos = __builtin_ctzll(s1);
                        if (s1 & friendly & enemy) {
                            *(moves++) = Move(pos, newpos);
                            numMoves++;
                        }
                    }
                }
            }
        }

        // Kill adjacent piece
        uint64_t attacks = pawnAttacks[turn][pos];
        attacks &= (~enemy | ep);
        while (attacks) {
            uint64_t m = attacks & -attacks;
            newpos = __builtin_ctzll(m);
            if (newpos / 8 == 0 || newpos / 8 == 7) {
                *(moves++) = Move(pos, newpos, PROMOTE_CAPTURE, QUEEN);
                *(moves++) = Move(pos, newpos, PROMOTE_CAPTURE, KNIGHT);
                *(moves++) = Move(pos, newpos, PROMOTE_CAPTURE, ROOK);
                *(moves++) = Move(pos, newpos, PROMOTE_CAPTURE, BISHOP);
                numMoves += 4;
            } else {
                *(moves++) = Move(pos, newpos, (m & ep) ? EN_PASSANT_MOVE : CAPTURE_MOVE);
                numMoves++;
            }
            attacks &= attacks - 1;
        }

        // Move onto the next pawn
        bb &= bb - 1;
    }
    return numMoves;
}

uint32_t BitBoard::getKingMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const {
    uint64_t bb = p[turn].king;
    uint8_t pos, newpos;
    uint32_t numMoves = 0;
    uint64_t friendly = ~s[turn].occupancy;
    uint64_t enemy = ~s[!turn].occupancy;
    pos = __builtin_ctzll(bb);

    if (!capturesOnly) {
        // Castling
        if (s[turn].castleShort) {
            uint64_t sftBoard = bb;
            maskIfPositionAttacked(sftBoard, turn);
            sftBoard = shiftEast(sftBoard);
            sftBoard &= friendly & enemy;
            if (sftBoard) maskIfPositionAttacked(sftBoard, turn);
            sftBoard = shiftEast(sftBoard);
            sftBoard &= friendly & enemy;
            if (sftBoard) maskIfPositionAttacked(sftBoard, turn);
            if (sftBoard) {
                newpos = __builtin_ctzll(sftBoard);
                *(moves++) = Move(pos, newpos, CASTLE_MOVE);
                numMoves++;
            }
        }
        if (s[turn].castleLong) {
            uint64_t sftBoard = bb;
            maskIfPositionAttacked(sftBoard, turn);
            sftBoard = shiftWest(sftBoard);
            sftBoard &= friendly & enemy;
            if (sftBoard) maskIfPositionAttacked(sftBoard, turn);
            sftBoard = shiftWest(sftBoard);
            sftBoard &= friendly & enemy;
            if (sftBoard) maskIfPositionAttacked(sftBoard, turn);
            newpos = __builtin_ctzll(sftBoard);
            sftBoard = shiftWest(sftBoard);
            sftBoard &= friendly & enemy;
            if (sftBoard) {
                *(moves++) = Move(pos, newpos, CASTLE_MOVE);
                numMoves++;
            }
        }
    }

    uint64_t attacks = kingAttacks[pos];
    attacks &= friendly;
    if (capturesOnly) attacks &= ~enemy;
    while (attacks) {
        uint64_t m = attacks & -attacks;
        newpos = __builtin_ctzll(m);
        *(moves++) = Move(pos, newpos, (m & ~enemy) ? CAPTURE_MOVE : DEFAULT_MOVE);
        numMoves++;
        attacks &= attacks - 1;
    }

    return numMoves;
}

uint32_t BitBoard::getKnightMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const {
    uint8_t pos, newpos;
    uint64_t bb = p[turn].knight;
    uint32_t numMoves = 0;
    uint64_t friendly = ~s[turn].occupancy;
    uint64_t enemy = ~s[!turn].occupancy;

    while (bb) {
        // Isolate the least significant bit and find it
        uint64_t sftBoard = bb & -bb;
        pos = __builtin_ctzll(sftBoard);
        uint64_t attacks = knightAttacks[pos];
        attacks &= friendly;
        if (capturesOnly) attacks &= s[!turn].occupancy;
        while (attacks) {
            uint64_t m = attacks & -attacks;
            newpos = __builtin_ctzll(m);
            *(moves++) = Move(pos, newpos, (m & ~enemy) ? CAPTURE_MOVE : DEFAULT_MOVE);
            numMoves++;
            attacks &= attacks - 1;
        }
        // Clear last least significant bit
        bb &= bb - 1;
    }
    return numMoves;
}

uint32_t BitBoard::getRookMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const {
    uint64_t bb = p[turn].rook | p[turn].queen;
    uint32_t numMoves = 0;
    numMoves += searchStraight(bb, &shiftNorth, moves, capturesOnly);
    numMoves += searchStraight(bb, &shiftSouth, moves, capturesOnly);
    numMoves += searchStraight(bb, &shiftWest, moves, capturesOnly);
    numMoves += searchStraight(bb, &shiftEast, moves, capturesOnly);
    return numMoves;
}

uint32_t BitBoard::getBishopMoves(std::array<Move,MAX_MOVES>::iterator& moves, bool capturesOnly) const {
    uint64_t bb = p[turn].bishop | p[turn].queen;
    uint32_t numMoves = 0;
    numMoves += searchStraight(bb, &shiftNoEast, moves, capturesOnly);
    numMoves += searchStraight(bb, &shiftNoWest, moves, capturesOnly);
    numMoves += searchStraight(bb, &shiftSoEast, moves, capturesOnly);
    numMoves += searchStraight(bb, &shiftSoWest, moves, capturesOnly);
    return numMoves;
}

uint8_t BitBoard::getAvailableMoves(std::array<Move,MAX_MOVES>& movesAvailable, bool capturesOnly) const {
    std::array<Move,MAX_MOVES>::iterator moves = movesAvailable.begin();
    uint32_t numMoves = 0;

    numMoves += getPawnMoves(moves, capturesOnly);
    numMoves += getKingMoves(moves, capturesOnly);
    numMoves += getKnightMoves(moves, capturesOnly);
    numMoves += getRookMoves(moves, capturesOnly);
    numMoves += getBishopMoves(moves, capturesOnly);

    assert(moves < movesAvailable.end());
    return numMoves;
}

bool BitBoard::testInCheck(bool c) const {
    uint64_t king = p[c].king;
    maskIfPositionAttacked(king, c);
    return !king;
}

int32_t BitBoard::evaluateKingSafety() const {
    int32_t numPos = 0;
    // King safety. Pretend king was a queen and see how far it can go. Penalize more available moves.
    numPos = 0;
    uint64_t occupied = ~(s[turn].occupancy | s[!turn].occupancy);

    auto slidingFuncs = {&shiftNorth, &shiftEast, &shiftSouth, &shiftWest, &shiftNoEast, &shiftNoWest, &shiftSoEast, &shiftSoWest};
    for (auto func : slidingFuncs) {
        uint64_t sft = p[turn].king;
        while (sft) {
            sft = func(sft);
            sft &= occupied;
            if (sft) numPos++;
        }
    }

    return numPos;
}

int32_t BitBoard::estimateMoveValue(Move const& move) const {
    static constexpr int32_t CAPTURE_BONUS = 75;
    static constexpr int32_t CASTLE_BONUS = 100;

    int32_t value = 0;
    uint64_t to_bb = 1ull << move.to;
    bool target_defended = s[!turn].mobility & to_bb;

    value += move.moveData.isCapture * CAPTURE_BONUS;
    value += move.moveData.isCastle * CASTLE_BONUS;
    value += move.moveData.isEnPassant * PAWN_VALUE_MG;

    // Benefit to put our piece on a good square
    value += pstDelta(move);
    
    // Most valuable target, least valuable attacker. Don't care about attacker if target is not defended
    value += getPieceValue(getPiece(p[!turn], move.to));
    value -= target_defended * getPieceValue(getPiece(p[turn], move.from));
    
    value += getPieceValue(move.promote);

    return value;
}

inline int32_t BitBoard::pstDelta(Move const& move) const {
    enum Piece source = getPiece(p[turn], move.from);
    int32_t pst;
    uint8_t from = move.from;
    uint8_t to = move.to;
    if (!turn) {
        uint8_t col = to % 8;
        uint8_t row = 7 - (to / 8);
        to = (row * 8 + col);

        col = from % 8;
        row = 7 - (from / 8);
        from = (row * 8 + col);
    }

    float egBlend = calculateEndgameBlendFactor();
    float mgBlend = 1-egBlend;

    switch (source) {
        case PAWN:
            pst = mgBlend*(PST_MG_P[to]-PST_MG_P[from]) + egBlend*(PST_EG_P[to]-PST_EG_P[from]);
            break;
        case KNIGHT:
            pst = mgBlend*(PST_MG_N[to]-PST_MG_N[from]) + egBlend*(PST_EG_N[to]-PST_EG_N[from]);
            break;
        case BISHOP:
            pst = mgBlend*(PST_MG_B[to]-PST_MG_B[from]) + egBlend*(PST_EG_B[to]-PST_EG_B[from]);
            break;
        case ROOK:
            pst = mgBlend*(PST_MG_R[to]-PST_MG_R[from]) + egBlend*(PST_EG_R[to]-PST_EG_R[from]);
            break;
        case QUEEN:
            pst = mgBlend*(PST_MG_Q[to]-PST_MG_Q[from]) + egBlend*(PST_EG_Q[to]-PST_EG_Q[from]);
            break;
        case KING:
            pst = mgBlend*(PST_MG_K[to]-PST_MG_K[from]) + egBlend*(PST_EG_K[to]-PST_EG_K[from]);
            break;
        default:
            assert(0);

    }

    return Evaluate::PST_FACTOR * pst;
}

float BitBoard::calculateEndgameBlendFactor() const {
    uint32_t material = 0;
    static constexpr uint32_t maxMaterial = QUEEN_VALUE_MG + (2*KNIGHT_VALUE_MG) + (2*BISHOP_VALUE_MG) + (2*ROOK_VALUE_MG);
    static constexpr uint32_t egCutoff = QUEEN_VALUE_MG - PAWN_VALUE_MG;

    // Only count opponents non-pawn material
    material += __builtin_popcountll(p[!turn].queen) * QUEEN_VALUE_MG;
    material += __builtin_popcountll(p[!turn].knight) * KNIGHT_VALUE_MG;
    material += __builtin_popcountll(p[!turn].bishop) * BISHOP_VALUE_MG;
    material += __builtin_popcountll(p[!turn].rook) * ROOK_VALUE_MG;

    if (material <= egCutoff) return 1;

    return 1 - (static_cast<float>(material - egCutoff) / static_cast<float>(maxMaterial - egCutoff));
}

void BitBoard::sortMoves(std::array<Move,MAX_MOVES>& moves,
                                uint8_t numMoves, Move const& ttMove) const 
{
    for (uint8_t i = 0; i < numMoves; i++) {
        moves[i].value = estimateMoveValue(moves[i]);
        if (moves[i] == ttMove) moves[i].value += 10000;
        #ifdef HISTORY_HEURISTIC
        moves[i].value += tt->getHistoryScore(turn, moves[i]);
        #endif
    }
    std::sort(moves.begin(), moves.begin() + numMoves,
            [&] (Move m1, Move m2) { return m1.value > m2.value; });
}