#include <cstring>

#include "engine.hpp"
#include "evaluate.hpp"

int32_t Engine::searchBestMove(BitBoard& board, BitBoard::Move& move, 
                               uint8_t depth, uint32_t time)
{
    uint8_t const iterStart = 1;
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
    seldepth = 0;
    shouldStop = false;

    int32_t alpha = NEG_INF;
    int32_t beta = INF;

    //table->clear();

    BitBoard oldBoard = board;

    memset(&currPvs, 0, sizeof(Line)*MAX_DEPTH*MAX_PVS);
    memset(&pvs, 0, sizeof(Line)*MAX_PVS);


    // Initialize evals to -INF
    for (uint8_t pv = 0; pv < numPvs; pv++) {
        pvs[pv].eval = NEG_INF;
        for (uint8_t depth = 0; depth < MAX_DEPTH; depth++) {
            currPvs[pv][depth].eval = NEG_INF;
        }
    }

    timeStart = std::chrono::high_resolution_clock::now();
    for (depthIter = iterStart; depthIter <= depth; depthIter++) {
        aspirationRetries = 1;
        numReductions = 0;
        numNullReductions = 0;
        numRedos = 0;
        branches = 0;
        do {
            if (eval >= beta) beta += ASPIRATION_DELTA * aspirationRetries * aspirationRetries;
            if (eval <= alpha) alpha -= ASPIRATION_DELTA * aspirationRetries * aspirationRetries;
            #ifdef ENABLE_PV_SEARCH
            // If we have a valid PV, search it first
            if (pvs[0].moves[0].valid()) {
                eval = searchPv(board, alpha, beta, depthIter, 0);
            } else {
                eval = recursiveDepthSearch(board, alpha, beta, depthIter, 0);
            }
            #else
            eval = recursiveDepthSearch(board, alpha, beta, depthIter, 0);
            #endif
            aspirationRetries++;
        } while (!shouldStop && (eval >= beta || eval <= alpha));

        #ifdef ENABLE_ASPIRATION
        beta = eval + ASPIRATION_START;
        alpha = eval - ASPIRATION_START;
        #endif

        // Ran out of time or got a stop command
        if (shouldStop) {
            shouldStop = false;
            break;
        }

        for (uint8_t pv = 0; pv < numPvs; pv++) {
            memcpy(&pvs[pv], &currPvs[pv][0], sizeof(Line));
        }
        sendEngineInfo(depthIter);

        if (abs(eval) > MATE(MAX_DEPTH)) break;
    }

    board = oldBoard;

    printSearchStats();

    move = pvs[0].moves[0];
    return pvs[0].eval;
}

int32_t Engine::searchPv(BitBoard& board,
                         int32_t alpha, int32_t const beta, 
                         uint8_t const maxdepth, uint8_t const currdepth)
{
    using namespace BitBoardState;

    if (shouldStop) {
        printSearchStats();
        return NEG_INF;
    }

    uint8_t newdepth = (maxdepth-2 >= currdepth) ? maxdepth-2 : currdepth;

    // If we still have a valid PV, search that move first. Otherwise, search the rest of the moves.
    if (currdepth == maxdepth-1 || !pvs[0].moves[currdepth].valid()) {
        // Reduce non PV search depth
        return recursiveDepthSearch(board, alpha, beta, newdepth, currdepth);
    }

    npos++;
    branches++;

    BitBoard oldboard = board;
    Piece oldPiece = board.movePiece(pvs[0].moves[currdepth]);
    (void)oldPiece;
    int32_t pvEval = -searchPv(board, -beta, -alpha, maxdepth, currdepth+1);
    board = oldboard;

    // Update pvs
    // We only need to update the 0th pv because this level only looks at 1 move
    currPvs[0][currdepth].eval = pvEval;
    currPvs[0][currdepth].moves[currdepth] = pvs[0].moves[currdepth];
    // Copy best pv from depth+1 to current depth's 0th pv
    memcpy(&currPvs[0][currdepth].moves[currdepth+1], &currPvs[0][currdepth+1].moves[currdepth+1], 
            sizeof(BitBoard::Move)*(seldepth-currdepth));

    int32_t eval = recursiveDepthSearch(board, pvEval, beta, newdepth, currdepth);

    return std::max(eval, pvEval);
}

int32_t Engine::quiesce(BitBoard& board, int32_t alpha, int32_t const beta, uint8_t const currdepth) {
    using namespace BitBoardState;

    npos++;
    if (currdepth > seldepth) {
        seldepth = currdepth;
    }

    // Null move test to see if current position already beats beta
    int32_t staticEval = Evaluate::evaluatePosition(board);
    #ifndef ENABLE_QUIESCE
    return staticEval;
    #endif
    if (currdepth == MAX_DEPTH-1) return staticEval;
    if (staticEval >= beta) return staticEval;
    if (staticEval > alpha) alpha = staticEval;

    branches++;
    std::array<BitBoard::Move,MAX_MOVES> moves = {0};
    int32_t bestEval = staticEval;

    uint8_t numCaptures = board.getAvailableMoves(moves, true /* capturesOnly */);
    board.sortMoves(moves, numCaptures);

    for (uint8_t i = 0; i < numCaptures; i++) {
        BitBoard oldboard = board;
        Piece oldPiece = board.movePiece(moves[i]);
        (void)oldPiece;

        // Illegal move check
        if (board.testInCheck(!board.turn)) {
            board = oldboard;
            continue;
        }

        int32_t eval = -quiesce(board, -beta, -alpha, currdepth+1);
        board = oldboard;

        if (eval >= beta) return eval;
        if (eval > alpha) alpha = eval;
        if (eval > bestEval) bestEval = eval;
    }

    return bestEval;
}

int32_t Engine::recursiveDepthSearch(BitBoard& board,
                                     int32_t alpha, int32_t const beta, 
                                     uint8_t maxdepth, uint8_t const currdepth)
{
    using namespace BitBoardState;

    if (shouldStop) {
        printSearchStats();
        return NEG_INF;
    }
    
    for (uint8_t pv = 0; pv < numPvs; pv++) {
        currPvs[pv][currdepth].eval = NEG_INF;
        memset(&currPvs[pv][currdepth].moves[currdepth], 0, sizeof(BitBoard::Move)*(MAX_DEPTH-currdepth-1));
    }

    // Base case
    if (currdepth == maxdepth) {
        const auto end = std::chrono::high_resolution_clock::now();
        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - timeStart);
        if (time.count() >= (timelimit)) {
            shouldStop = true;
        }

        return quiesce(board, alpha, beta, currdepth);
    }

    npos++;
    branches++;
    std::array<BitBoard::Move,MAX_MOVES> moves = {0};
    BitBoard oldboard = board;

    bool inCheck = board.testInCheck(board.turn);
    uint8_t newdepth = maxdepth;

    // Increase depth of search while still in check
    if (inCheck) maxdepth++;

#ifdef ENABLE_NULL_MOVE
    if (!inCheck && board.moves < ENDGAME_CUTOFF) {
        // Null move reduction: try a Null move and use it to reduce search depth
        board.changeTurn();
        board.s[board.turn].enPassantSquare = 0;
        board.s[!board.turn].enPassantSquare = 0;
        newdepth = std::min(static_cast<uint>(currdepth+3), static_cast<uint>(maxdepth));
        int32_t eval = -recursiveDepthSearch(board, -beta, -alpha, newdepth, currdepth+1);
        board = oldboard;
        if (eval >= beta) {
            numNullReductions++;
            if (maxdepth - currdepth > 4) {
                maxdepth -= 4;
                board = oldboard;
            } else {
                // Doing nothing is already better for us than making a move, just return beta
                return beta;
            }
        }
    }
#endif

    bool foundLegalMove = false;
    int32_t bestEval = NEG_INF;

    uint8_t numMoves = board.getAvailableMoves(moves);
    assert(numMoves);
    board.sortMoves(moves, numMoves);

    uint8_t movesSearched = 0;
    for (auto move = moves.begin(); move != moves.begin() + numMoves; move++) {
        bool depthReduced = false;

        newdepth = maxdepth;

        Piece oldPiece = board.movePiece(*move);
        (void)oldPiece;
        if (board.testInCheck(!board.turn)) {
            // Illegal move, continue
            board = oldboard;
            continue;
        }
        // We found a move letting us live next turn
        foundLegalMove = true;

        movesSearched++;

        #ifdef ENABLE_LMR
        // Reduce late moves if we can
        if ((movesSearched > LATE_MOVE_CUTOFF) && !inCheck) {
            if ((maxdepth - currdepth) > 3) newdepth-=3;
            else newdepth = currdepth+1;
            numReductions++;
            depthReduced = true;
        } else if ((movesSearched > LATE_MOVE_CUTOFF_2) && !inCheck) {
            if ((maxdepth - currdepth) > 6) newdepth-=6;
            else newdepth = currdepth+1;
            numReductions++;
            depthReduced = true;
        }
        #endif

        int32_t newEval = -recursiveDepthSearch(board, -beta, -alpha, newdepth, currdepth+1);

        if (depthReduced) {
            if (newEval >= beta || newEval > alpha) {
                // Redo search at full depth
                numRedos++;
                newdepth = maxdepth;
                newEval = -recursiveDepthSearch(board, -beta, -alpha, maxdepth, currdepth+1);
            }
        }

        // Undo move
        board = oldboard;

        // Prune tree if adjacent branch is already < this branch
        if (newEval >= beta) {
            // Not exact score, can't use it so set depth to 0
            //table->updateEntry(board, *move, newEval, 0);
            return newEval;
        }

        // Update pvs
        uint8_t numPvsToUpdate = (currdepth == 0) ? numPvs : 1;
        for (uint8_t pv = 0; pv < numPvsToUpdate; pv++) {
            if (newEval > currPvs[pv][currdepth].eval) {
                // Shift all pvs after this one down
                for (uint8_t sft = numPvs-1; sft > pv; sft--) {
                    currPvs[sft][currdepth].eval = currPvs[sft-1][currdepth].eval;
                    memcpy(&currPvs[sft][currdepth].moves[currdepth], &currPvs[sft-1][currdepth].moves[currdepth], 
                            sizeof(BitBoard::Move)*(MAX_DEPTH-currdepth-1));
                }
                currPvs[pv][currdepth].eval = newEval;
                currPvs[pv][currdepth].moves[currdepth] = *move;
                // Copy best pv from depth+1 to current depth's nth pv
                memcpy(&currPvs[pv][currdepth].moves[currdepth+1], &currPvs[0][currdepth+1].moves[currdepth+1], 
                        sizeof(BitBoard::Move)*(MAX_DEPTH-currdepth-2));
                break;
            }
        }

        if (newEval > bestEval) {
            bestEval = newEval;
        }

        if (currdepth == 0 && numPvs > 1) {
            // For MultiPV, we want a wider window from the root node so we don't beta-cutoff other PVs
            alpha = currPvs[numPvs-1][0].eval;
        } else {
            if (newEval > alpha) {
                alpha = newEval;
            }
        }
    }

    if (!foundLegalMove && !inCheck) {
        // Stalemate
        bestEval = 0;
    } else if (!foundLegalMove) {
        // In check but we have no legal moves
        bestEval = -MATE(currdepth+1);
    }

    return bestEval;
}

uint64_t Engine::perft(BitBoard& board, uint8_t depth, bool divide) {
    static uint64_t numCaptures = 0;
    static uint64_t numChecks = 0;
    static uint64_t numPromotes = 0;

    if (divide) {
        numCaptures = 0;
        numChecks = 0;
        numPromotes = 0;
    }
    if (depth == 0 && board.testInCheck(board.turn)) numChecks++;
    if (depth == 0) return 1;

    using namespace BitBoardState;

    std::array<BitBoard::Move,MAX_MOVES> moves;
    BitBoard oldboard = board;
    if (depth == 1) numCaptures += board.getAvailableMoves(moves, true);
    uint8_t numMoves = board.getAvailableMoves(moves);
    uint64_t npos = 0;
    for (auto move = moves.begin(); move != moves.begin() + numMoves; move++) {
        Piece oldPiece = board.movePiece(*move);
        if (static_cast<Piece>(oldPiece) == KING) {
            // Should not be able to capture king
            assert(0);
            return 0;
        }
        uint64_t nodes = 0;
        if (!board.testInCheck(!board.turn)) {
            if (depth == 1 && move->promote) numPromotes++;
            nodes = perft(board, depth-1, false);
            npos += nodes;
            if (divide) {
                std::cout << BitBoard::moveToStr(*move) << ": " << std::to_string(nodes) << std::endl;
            }
        } else if (depth == 1 && oldPiece != EMPTY) {
            numCaptures--;
        }
        board = oldboard;
    }
    if (divide) {
        std::cout << "Total captures: " << std::to_string(numCaptures) << std::endl;
        std::cout << "Total checks: " << std::to_string(numChecks) << std::endl;
        std::cout << "Total promotes: " << std::to_string(numPromotes) << std::endl;
    }
    return npos;
}

void Engine::sendEngineInfo(uint8_t depth) {
    const auto end = std::chrono::high_resolution_clock::now();
    const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - timeStart);
    uint32_t evalRate = (time.count() == 0) ? 0 : (uint32_t)(npos / ((float)time.count()/1000));

    std::string evalString;
    
    for (uint8_t pv = 0; pv < numPvs; pv++) {
        if (pvs[pv].eval == NEG_INF) break;
        
        if (abs(pvs[pv].eval) >= MATE(MAX_DEPTH)) {
            if (pvs[pv].eval < 0) {
                evalString = std::string("mate ").append(std::to_string(-(pvs[pv].eval + BoardState::KING_VALUE)/2));
            } else {
                evalString = std::string("mate ").append(std::to_string((BoardState::KING_VALUE-pvs[pv].eval)/2));
            }
        } else {
            evalString = std::string("cp ").append(std::to_string(pvs[pv].eval));
        }

        std::cout << "info score " << evalString << " depth " << std::to_string(depth)
                  << " seldepth " << std::to_string(seldepth) << " nodes " << std::to_string(npos) 
                  << " time " << std::to_string((uint32_t)time.count())
                  << " nps " << std::to_string(evalRate)
                  << " multipv " << std::to_string(pv+1) << " pv ";
        for (uint8_t idx = 0; idx < MAX_DEPTH; idx++) {
            if (!pvs[pv].moves[idx].valid()) break;
            std::cout << BitBoard::moveToStr(pvs[pv].moves[idx]) << " ";
        }
        std::cout << std::endl;
    }
}

void Engine::printSearchStats() const {
    float branchFactor = std::log2((float)branches)/ std::log2(depthIter-1);

    std::cout << "Searched total number of nodes: " << std::to_string(npos) << std::endl;
    std::cout << "Branch Factor: " << std::to_string(branchFactor) << std::endl;
    std::cout << "Aspiration retries: " << std::to_string(aspirationRetries-1) << std::endl;
    std::cout << "LMR Redo rate: " << std::to_string((float)numRedos*100/numReductions) << "%" << std::endl;
    std::cout << "TT Hitrate: " << std::to_string((float)numTTHits*100/numTTLookups) << "%" << std::endl;
    std::cout << "TT Evictionrate: " << std::to_string((float)numTTEvictions*100/numTTLookups) << "%" << std::endl;
    std::cout << "TT Depth miss: " << std::to_string((float)numTTSoftmiss*100/numTTLookups) << "%" << std::endl;
    std::cout << "TT Entries filled: " << std::to_string(numTTFills) << std::endl;
    std::cout << "NULL reduction rate: " << std::to_string((float)numNullReductions*100/branches) << "%" << std::endl;
    //table->printEstimatedOccupancy();
}