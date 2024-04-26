#include <cstring>

#include "engine.hpp"
#include "evaluate.hpp"

int32_t Engine::searchBestMove(BoardState::BoardType& board, BoardState::Move& move, 
                               uint8_t depth, uint32_t time)
{
    uint8_t const iterStart = 3;
    int32_t eval = 0;
    npos = 0;
    branches = 0;
    prevTime = 0;
    numRedos=0;
    numReductions=0;
    numTTLookups=0;
    numTTHits=0;
    numTTSoftmiss=0;
    numTTEvictions=0;
    numTTFills=0;
    numNullReductions=0;
    aspirationRetries=0;
    timelimit = time;

    int32_t alpha = -100000;
    int32_t beta = 100000;

    table->clear();

    BoardState::BoardType oldBoard = board;

    timeStart = std::chrono::high_resolution_clock::now();
    for (depthIter = iterStart; depthIter <= depth; depthIter++) {
        try {
            do {
                alpha -= ASPIRATION_WINDOW;
                beta += ASPIRATION_WINDOW;
                eval = recursiveDepthSearch(board, topLine, depthIter != iterStart,
                                            alpha, beta, depthIter, 0);
                aspirationRetries++;
            } while (eval <= alpha || eval >= beta);
            alpha = eval;
            beta = -eval;
        } catch (int64_t time) { break; }
        // Ran out of time
        std::cout << "Found best move: " << moveToStr(topLine.at(0)) << std::endl;
        std::cout << "Searched depth: " << std::to_string(depthIter) << std::endl;
        std::cout << "Evaluation: " << std::to_string(eval) << std::endl;
        std::cout << "Aspiration retry rate: " 
                << std::to_string(((float)(aspirationRetries-depthIter+iterStart)*100)/aspirationRetries) 
                << "%" << std::endl;
        sendEngineInfo(depthIter);
        
        if (abs(eval) > MATE(depthIter+1)) break;
    }

    board = oldBoard;

    move = topLine[0];

    return eval;
}

int32_t Engine::recursiveDepthSearch(BoardState::BoardType& board,
                                     std::array<BoardState::Move, MAX_DEPTH>& topVariation,
                                     bool usePrincipalMove, int32_t alpha, int32_t const beta, 
                                     uint8_t maxdepth, uint8_t const currdepth)
{
    using namespace BoardState;

    npos++;
    if (currdepth != maxdepth) branches++;

    Move& principalMove = topLine[currdepth];
    Move const& killerMove = topVariation[currdepth];
    bool chainPrincipalMove = usePrincipalMove;

    TT::TTEntry const& entry = table->lookupHash(board.hash);
    numTTLookups++;
    if (entry.hash == board.hash) {
        if (entry.depth >= (maxdepth-currdepth)) {
            numTTHits++;
            topVariation[currdepth] = entry.move;
            return entry.eval;
        } else {
            if (!usePrincipalMove && entry.move.valid()) {
                usePrincipalMove = true;
                principalMove = entry.move;
            }
            numTTSoftmiss++;
        }
    } else if (entry.hash == 0) {
        numTTFills++;
    } else if (entry.hash != board.hash) {
        numTTEvictions++;
    } 
    
    // Base case
    if (currdepth == maxdepth || currdepth == MAX_DEPTH-1) {
        const auto end = std::chrono::high_resolution_clock::now();
        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - timeStart);
        if (time.count() >= (timelimit)) {
            throw time.count();
        }
        if (time.count() - prevTime > 1000) {
            prevTime = time.count();
            sendEngineInfo(currdepth);
            std::cout << "Alpha: " << std::to_string(alpha) << " | Beta: " << std::to_string(beta) << std::endl;
        }

        int32_t evaluation = Evaluate::evaluatePosition(board);
        table->updateEntry(board, Move(), evaluation, maxdepth-currdepth);
        return evaluation;
    }

    std::array<Move,MAX_DEPTH> currVariation = {0};
    std::array<Move,MAX_MOVES> moves = {0};
    BoardType oldboard = board;

    bool inCheck = false;
    uint8_t newdepth = maxdepth;

    // Null move reduction: try a Null move and use it to reduce search depth
    changeTurn(board);
    board.hash ^= EN_PASSANT_HASH*board.cached.enPassantSquare;
    board.cached.enPassantSquare = 0;
    newdepth = ((maxdepth-currdepth) > 2) ? currdepth+2 : maxdepth;
    int32_t eval = -recursiveDepthSearch(board, currVariation, false,
                                         -beta, -alpha, newdepth, currdepth+1);
    board = oldboard;
    if (eval == -MATE(currdepth+1)) {
        // Reuse null move test to check for in check
        inCheck = true;
    } else if (eval >= beta) {
        numNullReductions++;
        // Doing nothing is already better than making a move, cut depth
        if ((maxdepth-currdepth) > 5) maxdepth -= 4;
        else return beta; 
    }

    // Increase depth of search while still in check
    if (inCheck) {
        maxdepth++;
    }

    int32_t bestEval = INT_MIN;
    bool foundLegalMove = false;

    uint8_t numMoves = getAvailableMoves(board, moves);
    if (numMoves == 0) return 0;
    sortMoves(moves, usePrincipalMove ? principalMove : Move(), killerMove, numMoves, board);

    uint8_t movesSearched = 0;
    for (auto move = moves.begin(); move != moves.begin() + numMoves; move++) {
        bool depthReduced = false;
        bool isCapture = false;
        usePrincipalMove = usePrincipalMove && (move == moves.begin());

        // Reduce late moves if we can
        newdepth = maxdepth;

        ColorPiece oldPiece = movePiece(board, *move, table);
        isCapture = oldPiece != WEMPTY;
       
        int32_t newEval;
        if (static_cast<Piece>(oldPiece&pieceMask) == KING) {
            newEval = MATE(currdepth);
            // We are in illegal state if we can take the king in topmost move
            assert(currdepth != 0);
            board = oldboard;
            table->updateEntry(board, Move() /* no moves */, newEval, 0);
            // We can't take their king
            topVariation[currdepth] = Move();
            // Prev move was illegal because we took their king
            topVariation[currdepth-1] = Move();
            return newEval;
        }

        if (inCheck && testInCheck(board)) {
            // Still in check, illegal move
            board = oldboard;
            chainPrincipalMove = false;
            continue;
        }

        // Illegal to castle in check
        if ((board.cached.blackCastled != oldboard.cached.blackCastled) && inCheck) {
            board = oldboard;
            assert(board.turn == BLACK);
            chainPrincipalMove = false;
            continue;
        } else if ((board.cached.whiteCastled != oldboard.cached.whiteCastled) && inCheck) {
            board = oldboard;
            assert(board.turn == WHITE);
            chainPrincipalMove = false;
            continue;
        }

        movesSearched++;

        currVariation[currdepth] = *move;
        if ((movesSearched > LATE_MOVE_CUTOFF) && ((maxdepth - currdepth) > 2) && !isCapture && !inCheck) {
            newdepth-=2;
            numReductions++;
            depthReduced = true;
        } else if ((movesSearched > LATE_MOVE_CUTOFF_2) && ((maxdepth - currdepth) > 4) && !isCapture && !inCheck) {
            newdepth-=4;
            numReductions++;
            depthReduced = true;
        }

        if (isCapture && (newdepth == currdepth+1)) {
            // Quiesce (condition not possible if depth reduced)
            assert(!depthReduced);
            newdepth += 1;
        }

        newEval = -recursiveDepthSearch(board, currVariation, chainPrincipalMove,
                                        -beta, -alpha, newdepth, currdepth+1);

        if (depthReduced) {
            if (newEval > alpha || newEval >= beta) {
                // Redo search at full depth
                numRedos++;
                newdepth = maxdepth;
                newEval = -recursiveDepthSearch(board, currVariation, chainPrincipalMove,
                                                -beta, -alpha, maxdepth, currdepth+1);
            }
        }

        chainPrincipalMove = false;
        board = oldboard;

        // We found a move letting us live next turn
        if (newEval > -MATE(currdepth+1)) foundLegalMove = true;
        
        // Prune tree if adjacent branch is already < this branch
        if (newEval >= beta) {
            table->updateEntry(board, *move, beta, newdepth-currdepth);
            return beta;
        }

        if (newEval > bestEval) {
            bestEval = newEval;
            if (newEval > alpha) {
                alpha = newEval;
                // We beat alpha, we can update our PV to this one
                for (uint8_t i = currdepth; i < newdepth; i++) {
                    topVariation[i] = currVariation[i];
                }
                for (uint8_t i = newdepth; i < MAX_DEPTH; i++) {
                    topVariation[i] = Move();
                }
                topEval = (currdepth % 2) ? alpha : -alpha;
                if (currdepth < 2) sendEngineInfo(currdepth);
            }
            table->updateEntry(board, *move, bestEval, newdepth-currdepth);
        }
    }

    if (!inCheck && !foundLegalMove) {
        // Stalemate
        return 0;
    } else if (inCheck && !foundLegalMove) {
        // In check but we have no legal moves
        return -MATE(currdepth+1);
    }

    return bestEval;
}

uint32_t Engine::perft(BoardState::BoardType& board, uint8_t depth, bool divide) {
    static int numChecks = 0;
    if (depth == 0) return 1;

    using namespace BoardState;

    std::array<Move,MAX_MOVES> moves;
    BoardType oldboard = board;
    uint8_t numMoves = getAvailableMoves(board, moves);
    uint32_t npos = 0;
    for (auto move = moves.begin(); move != moves.begin() + numMoves; move++) {
        ColorPiece oldPiece = movePiece(board, *move, table);
        if (static_cast<Piece>(oldPiece&pieceMask) == KING) {
            // Illegal move, skip
            numChecks++;
            board = oldboard;
            return 0;
        }
        uint32_t nodes = perft(board, depth-1, false);
        npos += nodes;
        board = oldboard;
        if (divide) {
            std::cout << "move " << moveToStr(*move) << ": " << std::to_string(nodes) << std::endl;
        }
    }
    if (divide) 
        std::cout << "Num checks: " << std::to_string(numChecks) << std::endl;
    return npos;
}

// Note - checks if person who just moved is in check
bool Engine::testInCheck(BoardState::BoardType& board) {
    using namespace BoardState;
    std::array<Move,MAX_MOVES> moves = {0};
    uint8_t numMoves = getAvailableMoves(board, moves);
    BoardType oldBoard = board;
    for (auto move = moves.begin(); move != moves.begin() + numMoves; move++) {
        ColorPiece oldPiece = movePiece(board, *move, table);
        board = oldBoard;
        if (static_cast<Piece>(oldPiece&pieceMask) == KING) {
            return true;
        }
    }
    return false;
}

void Engine::sendEngineInfo(uint8_t depth) {
    static uint8_t seldepth = 0;
    if (depth > seldepth) seldepth = depth;
    uint32_t evalRate = (uint32_t)(npos / (prevTime/1000));
    std::string evalString;
    if (abs(topEval) + depth + 1 >= BoardState::KING_VALUE) {
        if (topEval < 0) {
            evalString = std::string("mate ").append(std::to_string(-(topEval + BoardState::KING_VALUE)/2));
        } else {
            evalString = std::string("mate ").append(std::to_string((BoardState::KING_VALUE-topEval)/2));
        }
    } else {
        evalString = std::string("cp ").append(std::to_string(topEval));
    }

    std::cout << "Searched total number of nodes: " << std::to_string(npos) << std::endl;
    std::cout << "Branch Factor: " << std::to_string((float)npos/branches) << std::endl;
    std::cout << "LMR Rate: " << std::to_string((float)numReductions*100/branches) << "%" << std::endl;
    std::cout << "Redo rate: " << std::to_string((float)numRedos*100/numReductions) << "%" << std::endl;
    std::cout << "TT Hitrate: " << std::to_string((float)numTTHits*100/numTTLookups) << "%" << std::endl;
    std::cout << "TT Evictionrate: " << std::to_string((float)numTTEvictions*100/numTTLookups) << "%" << std::endl;
    std::cout << "TT Depth miss: " << std::to_string((float)numTTSoftmiss*100/numTTLookups) << "%" << std::endl;
    std::cout << "TT Entries filled: " << std::to_string(numTTFills) << std::endl;
    std::cout << "NULL reduction rate: " << std::to_string((float)numNullReductions*100/branches) << "%" << std::endl;
    table->printEstimatedOccupancy();
    std::cout << "info score " << evalString << " depth " << std::to_string(depthIter)
                << " seldepth " << std::to_string(seldepth) << " nodes " << std::to_string(npos) 
                << " time " << std::to_string((uint32_t)prevTime)
                << " pv ";
    for (uint8_t idx = 0; idx < seldepth; idx++) {
        if (!topLine[idx].valid()) break; 
        std::cout << moveToStr(topLine[idx]) << "  ";
    }
    std::cout << std::endl;
    std::cout << "info nps " << std::to_string(evalRate) << std::endl;
}