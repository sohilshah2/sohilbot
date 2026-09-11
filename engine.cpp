#include <cstring>

#include "engine.hpp"
#include "evaluate.hpp"
#include "sohilbot.hpp"
#include "transpositionTables.hpp"

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
    numNullAttempts=0;
    aspirationRetries=0;
    numSearches = 0;
    timelimit = time;
    seldepth = 0;
    shouldStop = false;
    branchFactor = 0;

    uint32_t prevNodes = 0;

    depth = std::min(static_cast<int>(depth), MAX_DEPTH-1);

    int32_t alpha = NEG_INF;
    int32_t beta = INF;

    BitBoard oldBoard = board;

    memset(&currPvs, 0, sizeof(Line)*MAX_DEPTH*MAX_PVS);
    memset(&pvs, 0, sizeof(Line)*MAX_PVS);
    memset(&bestMoves, 0, sizeof(uint32_t)*8);

    board.tt->clearHistory();

    // Initialize evals to -INF
    for (uint8_t pv = 0; pv < numPvs; pv++) {
        pvs[pv].eval = NEG_INF;
        for (uint8_t depth = 0; depth < MAX_DEPTH; depth++) {
            currPvs[pv][depth].eval = NEG_INF;
        }
    }

    timeStart = std::chrono::steady_clock::now();
    for (depthIter = iterStart; depthIter <= depth; depthIter++) {
        aspirationRetries = 1;

        quiesceDepth = std::min(depthIter * 2, MAX_DEPTH-1);

        board.tt->clearHistory();
        do {
            #ifdef ENABLE_ASPIRATION
            if (aspirationRetries > 2) {
                // If we tried 2 times, search full window
                alpha = NEG_INF;
                beta = INF;
            } else {
                if (eval >= beta) beta += ASPIRATION_DELTA * aspirationRetries;
                if (eval <= alpha) alpha -= ASPIRATION_DELTA * aspirationRetries;
            }
            #endif
            eval = recursiveDepthSearch(board, alpha, beta, depthIter, 0);
            aspirationRetries++;
        } while (!shouldStop && (eval > beta || eval < alpha));

        #ifdef ENABLE_ASPIRATION
        beta = eval + ASPIRATION_START;
        alpha = eval - ASPIRATION_START;
        #endif

        for (uint8_t pv = 0; pv < numPvs; pv++) {
            // Could've stopped before we found all PVs, then don't copy over yet
            if (currPvs[pv][0].moves[0].valid()) {
                memcpy(&pvs[pv], &currPvs[pv][0], sizeof(Line));
            }
        }

        // Ran out of time or got a stop command
        if (shouldStop) {
            shouldStop = false;
            break;
        }

        sendEngineInfo(depthIter);

        // Update branch factor
        uint8_t numLoops = depthIter - iterStart;
        uint32_t currNodes = npos - prevNodes;
        if (numLoops > 0) {
            assert(prevNodes > 0);
            float currBranchFactor = static_cast<float>(currNodes) / static_cast<float>(prevNodes);
            branchFactor = (((numLoops-1) * branchFactor) + currBranchFactor) / (numLoops);
        }
        prevNodes = currNodes;
        if (abs(eval) > MATE(MAX_DEPTH)) break;
    }

    board = oldBoard;

#ifdef SEARCH_STATS_ON
    board.tt->printEstimatedOccupancy();
    printSearchStats();
#endif

    move = pvs[0].moves[0];
    return pvs[0].eval;
}

int32_t Engine::quiesce(BitBoard& board, int32_t alpha, int32_t const beta, uint8_t const currdepth) {
    using namespace BitBoardState;

    npos++;
    if (currdepth > seldepth) {
        seldepth = currdepth;
    }

    int32_t staticEval = Evaluate::evaluatePosition(board);
    #ifndef ENABLE_QUIESCE
    return staticEval;
    #endif
    if (currdepth >= quiesceDepth) return staticEval;
    if (staticEval >= beta) return staticEval;
    if (staticEval > alpha) alpha = staticEval;

    std::array<BitBoard::Move,MAX_MOVES> moves = {0};
    int32_t bestEval = staticEval;

    uint8_t numCaptures = board.getAvailableMoves(moves, true /* capturesOnly */);
    board.sortMoves(moves, numCaptures, BitBoard::Move());

    for (uint8_t i = 0; i < numCaptures; i++) {
        BitBoard oldboard = board;
        board.movePiece(moves[i]);

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

inline void Engine::extendSearch(uint8_t& depth, bool inCheck) const {
    if (depth >= quiesceDepth-1) return;
    if (depth < quiesceDepth-1 && inCheck) depth+=2;
}

inline uint8_t Engine::reduce(uint8_t const currdepth, uint8_t const maxdepth, uint8_t movesSearched) {
    uint8_t newdepth = maxdepth;
    #ifdef ENABLE_LMR
    // Reduce late moves if we can
    if (currdepth + 1 < maxdepth) {
        if ((movesSearched > LATE_MOVE_CUTOFF)) {
            if (currdepth < REDUCE1(maxdepth)) newdepth=REDUCE1(maxdepth);
            else newdepth = maxdepth-1;
            numReductions++;
        } else if ((movesSearched > LATE_MOVE_CUTOFF_2)) {
            if (currdepth < REDUCE2(maxdepth)) newdepth=REDUCE2(maxdepth);
            else newdepth = maxdepth-1;
            numReductions++;
        }
    }
    #endif
    return newdepth;
}

inline bool Engine::updatePvs(int32_t& alpha, BitBoard::Move* move,
                              int32_t newEval, uint8_t const currdepth) {
    bool raisedAlpha = false;

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

    if (currdepth == 0 && numPvs > 1) {
        raisedAlpha = true;
        // For MultiPV, we want a wider window from the root node so we don't beta-cutoff other PVs
        alpha = currPvs[numPvs-1][0].eval;
    } else {
        if (newEval > alpha) {
            alpha = newEval;
            raisedAlpha = true;
        }
    }

    return raisedAlpha;
}

int32_t Engine::recursiveDepthSearch(BitBoard& board,
                                     int32_t alpha, int32_t beta, 
                                     uint8_t maxdepth, uint8_t const currdepth)
{
    using namespace BitBoardState;
    npos++;

    if (shouldStop) {
        return NEG_INF;
    }
    
    for (uint8_t pv = 0; pv < numPvs; pv++) {
        currPvs[pv][currdepth].eval = NEG_INF;
        memset(&currPvs[pv][currdepth].moves[currdepth], 0, sizeof(BitBoard::Move)*(MAX_DEPTH-currdepth-1));
    }

    // Check the TT for hits
    BitBoard::Move ttMove = BitBoard::Move();
    #ifdef ENABLE_TT
    TT::TTEntry entry = board.tt->lookupHash(board.hash);
    numTTLookups++;
    if (entry.hash == board.hash) {
        ttMove = entry.move;
        if (entry.depth >= (maxdepth-currdepth)) {
            numTTHits++;
            if (entry.node == TT::PV || (entry.node == TT::ALL && entry.eval <= alpha)) {
                currPvs[0][currdepth].eval = entry.eval;
                currPvs[0][currdepth].moves[currdepth] = entry.move;
                return entry.eval;
            } else if (entry.node == TT::CUT && entry.eval >= beta) {
                return entry.eval;
            }
        } else {
            numTTSoftmiss++;
        }
    } else {
        if (entry.hash == 0) numTTFills++;
        else numTTEvictions++;
    }
    #endif

    // Base case
    if (currdepth == maxdepth) {
        const auto end = std::chrono::steady_clock::now();
        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - timeStart);
        if (time.count() >= (timelimit)) {
            shouldStop = true;
        }

        // Don't double count nodes in the quiesce function
        npos--; 
        
        int32_t eval = quiesce(board, alpha, beta, currdepth);

        // Leaf of search tree is PV node
        board.tt->updateEntry(board, BitBoard::Move(), eval, 0, TT::PV);

        return eval;
    }

    std::array<BitBoard::Move,MAX_MOVES> moves = {0};
    BitBoard oldboard = board;

    bool inCheck = board.testInCheck(board.turn);
    // Increase depth of search while still in check
    if (currdepth == maxdepth - 1) extendSearch(maxdepth, inCheck);
    uint8_t newdepth = maxdepth;

#ifdef ENABLE_NULL_MOVE
    if (maxdepth > currdepth+3 && !inCheck && board.moves < ENDGAME_CUTOFF) {
        // Null move reduction: try a Null move and use it to reduce search depth
        numNullAttempts++;
        board.changeTurn();
        board.s[board.turn].enPassantSquare = 0;
        board.s[!board.turn].enPassantSquare = 0;
        newdepth = maxdepth-3;
        int32_t eval = -recursiveDepthSearch(board, -beta, -beta + 1, newdepth, currdepth+1);
        board = oldboard;
        if (eval >= beta) {
            numNullReductions++;
            // Doing nothing is already better for us than making a move, just return beta
            board.tt->updateEntry(board, BitBoard::Move(), beta, maxdepth-currdepth, TT::CUT);
            return beta;
        }
    }
#endif

    bool foundLegalMove = false;
    [[maybe_unused]] bool raisedAlpha = false;
    int32_t bestEval = NEG_INF;
    BitBoard::Move bestMove = BitBoard::Move();

    uint8_t numMoves = board.getAvailableMoves(moves);
    assert(numMoves);
    board.sortMoves(moves, numMoves, ttMove);

    uint8_t movesSearched = 0;
    numSearches++;
    for (auto move = moves.begin(); move != moves.begin() + numMoves; move++) {
        bool depthReduced = false;

        newdepth = maxdepth;

        board.movePiece(*move);
        // We are in check after moving
        if (board.testInCheck(!board.turn)) {
            // Illegal move, continue
            board = oldboard;
            continue;
        }

        int32_t newEval = 0;

        // 3-fold repetition detection
        if (currdepth > 0 && board.history.isRepeat(board.hash)) {
            if (currdepth % 2 == 0) 
                #ifdef ENABLE_CONTEMPT
                newEval = -DRAW_THRESHHOLD;
                #else
                newEval = 0;
                #endif
            else newEval = 0;
        } else {
            board.history.insert(board.hash);

            // We found a move letting us live next turn
            foundLegalMove = true;

            movesSearched++;
            branches++;
            if (!inCheck && !move->moveData.isCapture && !board.testInCheck(board.turn)) {
                newdepth = reduce(currdepth, maxdepth, movesSearched);
                depthReduced = newdepth != maxdepth;
            }

            newEval = -recursiveDepthSearch(board, -beta, -alpha, newdepth, currdepth+1);

            if (depthReduced) {
                if (newEval > alpha) {
                    // Redo search at full depth
                    numRedos++;
                    newdepth = maxdepth;
                    newEval = -recursiveDepthSearch(board, -beta, -alpha, maxdepth, currdepth+1);
                }
            }
        }

        // Undo move
        board = oldboard;

        // Prune tree if adjacent branch is already < this branch
        if (newEval >= beta) {
            #ifdef ENABLE_TT
            board.tt->updateEntry(board, *move, beta, maxdepth-currdepth, TT::CUT);
            #endif
            #ifdef HISTORY_HEURISTIC
            if (!move->moveData.isCapture) {
                int32_t historyBonus = (maxdepth-currdepth)*(maxdepth-currdepth);
                board.tt->updateHistoryScore(board.turn, *move, historyBonus);
                for (auto m = moves.begin(); m != move; m++) {
                    if (!m->moveData.isCapture) {
                        board.tt->updateHistoryScore(board.turn, *m, -historyBonus/5);
                    }
                }
            }
            #endif
            bestMoves[std::min(7u, static_cast<uint>(movesSearched-1))]++;
            return newEval;
        }

        if (newEval > bestEval) {
            bestEval = newEval;
            bestMove = *move;
        }

        raisedAlpha |= updatePvs(alpha, move, newEval, currdepth);
    }

    if (!foundLegalMove && !inCheck) {
        // Stalemate
        bestEval = 0;
    } else if (!foundLegalMove) {
        // In check but we have no legal moves
        bestEval = -MATE(currdepth+1);
    }

    #ifdef ENABLE_TT
    board.tt->updateEntry(board, bestMove, bestEval, maxdepth-currdepth, raisedAlpha ? TT::PV : TT::ALL);
    #endif
    
    bestMoves[std::min(7u, static_cast<uint>(movesSearched-1))]++;

    return bestEval;
}

uint64_t Engine::perft(PerftResult& result, BitBoard& board, uint8_t depth, bool divide) {
    if (depth == 0 && board.testInCheck(board.turn)) result.checks++;
    if (depth == 0) {
        result.nodes++;
        return 1;
    }

    using namespace BitBoardState;

    std::array<BitBoard::Move,MAX_MOVES> moves;
    BitBoard oldboard = board;
    uint8_t numMoves = board.getAvailableMoves(moves);

    bool foundLegalMove = false;
    uint64_t nodes = 0;

    for (auto move = moves.begin(); move != moves.begin() + numMoves; move++) {
        board.movePiece(*move);
        if (!board.testInCheck(!board.turn)) {
            foundLegalMove = true;
            if (depth == 1 && move->promote) result.promotions++;
            if (depth == 1 && move->moveData.isCapture) result.captures++;
            if (depth == 1 && move->moveData.isCastle) result.castles++;
            if (depth == 1 && move->moveData.isEnPassant) result.enpassants++;

            uint64_t childNodes = perft(result, board, depth-1, false);
            nodes += childNodes;
            if (divide) {
                std::cout << BitBoard::moveToStr(*move) << ": " << std::to_string(childNodes) << std::endl;
            }
        }
        board = oldboard;
    }

    if (depth == 1 && !foundLegalMove && board.testInCheck(board.turn)) {
        result.mates++;
    }

    return nodes;
}

void Engine::sendEngineInfo(uint8_t depth) {
    const auto end = std::chrono::steady_clock::now();
    const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - timeStart);
    uint32_t evalRate = (time.count() == 0) ? 0 : (uint32_t)(npos / ((float)time.count()/1000));

    std::string evalString;
    
    for (uint8_t pv = 0; pv < numPvs; pv++) {
        if (pvs[pv].eval == NEG_INF) break;
        
        if (abs(pvs[pv].eval) >= MATE(MAX_DEPTH)) {
            if (pvs[pv].eval < 0) {
                evalString = std::string("mate ").append(std::to_string(-(pvs[pv].eval + BitBoardState::KING_VALUE)/2));
            } else {
                evalString = std::string("mate ").append(std::to_string((BitBoardState::KING_VALUE-pvs[pv].eval)/2));
            }
        } else {
            evalString = std::string("cp ").append(std::to_string(pvs[pv].eval));
        }

        std::string infoString = "info score " + evalString + " depth " + std::to_string(depth)
                                + " seldepth " + std::to_string(seldepth) + " nodes " + std::to_string(npos) 
                                + " time " + std::to_string((uint32_t)time.count())
                                + " nps " + std::to_string(evalRate)
                                + " multipv " + std::to_string(pv+1) + " pv ";
        for (uint8_t idx = 0; idx < MAX_DEPTH; idx++) {
            if (!pvs[pv].moves[idx].valid()) break;
            infoString += BitBoard::moveToStr(pvs[pv].moves[idx]) + " ";
        }
        cmd->uciOutput(infoString);
    }
}

void Engine::printSearchStats() const {
    std::cout << "Searched total number of nodes: " << std::to_string(npos) << std::endl;
    std::cout << "Branch Factor: " << std::to_string(branchFactor) << std::endl;
    
    std::cout << "Aspiration retries: " << std::to_string(aspirationRetries-1) << std::endl;
    
    std::cout << "LMR Reduction rate: " << std::to_string((float)numReductions*100/branches) << "%" << std::endl;
    std::cout << "LMR Redo rate: " << std::to_string((float)numRedos*100/numReductions) << "%" << std::endl;
    std::cout << "NULL reduction rate: " << std::to_string((float)numNullReductions*100/numNullAttempts) << "%" << std::endl;

    std::cout << "TT Hitrate: " << std::to_string((float)numTTHits*100/numTTLookups) << "%" << std::endl;
    std::cout << "TT Evictionrate: " << std::to_string((float)numTTEvictions*100/numTTLookups) << "%" << std::endl;
    std::cout << "TT Depth miss: " << std::to_string((float)numTTSoftmiss*100/numTTLookups) << "%" << std::endl;
    std::cout << "TT Entries filled: " << std::to_string(numTTFills) << std::endl;

    std::cout << "Move order hitrate: [";
    for (uint i = 0; i < 8; i++) {
        std::cout << std::to_string(i) << " (" << std::to_string((float)bestMoves[i]*100/numSearches) << "%)  ";
    }
    std::cout << "]" << std::endl;
}