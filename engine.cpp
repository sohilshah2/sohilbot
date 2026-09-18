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
    numAspirationRetries=0;
    numPvsScouts=0;
    numPvsResearches=0;
    numQsDeltaPrunes=0;
    numQsSeePrunes=0;
    numSearches = 0;
    timelimit = time;
    seldepth = 0;
    shouldStop = false;
    branchFactor = 0;

    uint64_t prevCumulativeNodes = 0;
    uint64_t prevIterationNodes = 0;

    depth = std::min(static_cast<int>(depth), MAX_DEPTH-1);

    int32_t alpha = NEG_INF;
    int32_t beta = INF;

    BitBoard oldBoard = board;

    memset(&currPvs, 0, sizeof(Line)*MAX_DEPTH*MAX_PVS);
    memset(&pvs, 0, sizeof(Line)*MAX_PVS);
    memset(&bestMoves, 0, sizeof(uint32_t)*8);
    memset(&killers, 0, sizeof(killers));

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
        [[maybe_unused]] uint32_t aspirationAttempts = 0;

        quiesceDepth = std::min(depthIter + QS_EXTRA_PLIES, MAX_DEPTH-1);

#ifdef HISTORY_WIPE_EACH_ID
        board.tt->clearHistory();
#endif
        do {
            #ifdef ENABLE_ASPIRATION
            if (aspirationAttempts >= 2) {
                // If we tried 2 times, search full window
                alpha = NEG_INF;
                beta = INF;
            } else if (aspirationAttempts > 0) {
                const int32_t expansion = ASPIRATION_DELTA * (aspirationAttempts + 1);
                if (eval >= beta) beta += expansion;
                if (eval <= alpha) alpha -= expansion;
            }
            #endif
            eval = recursiveDepthSearch(board, alpha, beta, depthIter, 0, true);
            aspirationAttempts++;
            if (!shouldStop && (eval > beta || eval < alpha)) {
                numAspirationRetries++;
            }
        } while (!shouldStop && (eval > beta || eval < alpha));

        #ifdef ENABLE_ASPIRATION
        beta = eval + ASPIRATION_START;
        alpha = eval - ASPIRATION_START;
        #endif

        // Incomplete iteration: keep the last fully-searched PV
        if (shouldStop) {
            shouldStop = false;
            break;
        }

        for (uint8_t pv = 0; pv < numPvs; pv++) {
            if (currPvs[pv][0].moves[0].valid()) {
                memcpy(&pvs[pv], &currPvs[pv][0], sizeof(Line));
            }
        }

        sendEngineInfo(depthIter);

        // Update branch factor
        const uint8_t numLoops = depthIter - iterStart;
        const uint64_t cumulativeNodes = npos;
        const uint64_t currIterationNodes = cumulativeNodes - prevCumulativeNodes;
        if (numLoops > 0 && prevIterationNodes > 0) {
            float currBranchFactor = static_cast<float>(currIterationNodes)
                                   / static_cast<float>(prevIterationNodes);
            branchFactor = (((numLoops-1) * branchFactor) + currBranchFactor) / (numLoops);
        }
        prevCumulativeNodes = cumulativeNodes;
        prevIterationNodes = currIterationNodes;
        // Only stop ID once we have searched at least as many plies as the
        // mate distance. A TT mate at depth 1 must not abort the iteration
        // that would actually play the mating line.
        if (abs(eval) >= MATE(MAX_DEPTH)) {
            const int32_t matePlies = BitBoardState::KING_VALUE - abs(eval);
            if (depthIter >= matePlies) break;
        }
    }

    board = oldBoard;

#ifdef SEARCH_STATS_ON
    board.tt->printEstimatedOccupancy();
    printSearchStats();
#endif

    move = pvs[0].moves[0];
    if (!move.valid()) {
        std::array<BitBoard::Move, MAX_MOVES> fallback = {0};
        uint8_t numMoves = board.getAvailableMoves(fallback);
        for (uint8_t i = 0; i < numMoves; i++) {
            BitBoard tmp = board;
            tmp.movePiece(fallback[i]);
            if (!tmp.testInCheck(!tmp.turn)) {
                move = fallback[i];
                break;
            }
        }
    }
    return pvs[0].eval;
}

int32_t Engine::quiesce(BitBoard& board, int32_t alpha, int32_t const beta, uint8_t const currdepth) {
    using namespace BitBoardState;

    npos++;
    if (currdepth > seldepth) {
        seldepth = currdepth;
    }
    if (shouldStop) {
        return 0;
    }

    int32_t staticEval = Evaluate::evaluatePosition(board);
    #ifndef ENABLE_QUIESCE
    return staticEval;
    #endif

    const bool inCheck = board.testInCheck(board.turn);

#ifdef ENABLE_QS_CHECK
    if (!inCheck && currdepth >= quiesceDepth) return staticEval;
    if (currdepth >= MAX_DEPTH - 1) return staticEval;
#else
    if (currdepth >= quiesceDepth) return staticEval;
#endif

    int32_t bestEval;
#ifdef ENABLE_QS_CHECK
    if (inCheck) {
        bestEval = NEG_INF;
    } else
#endif
    {
        if (staticEval >= beta) return staticEval;
        if (staticEval > alpha) alpha = staticEval;
        bestEval = staticEval;
    }

    std::array<BitBoard::Move,MAX_MOVES> moves = {0};
#ifdef ENABLE_QS_CHECK
    const bool capturesOnly = !inCheck;
#else
    const bool capturesOnly = true;
#endif
    uint8_t numMoves = board.getAvailableMoves(moves, capturesOnly);
    board.sortMoves(moves, numMoves, BitBoard::Move());

    bool foundLegalMove = false;
    for (uint8_t i = 0; i < numMoves; i++) {
        if (!inCheck) {
#if defined(ENABLE_QS_DELTA) || defined(ENABLE_QS_SEE)
            const Piece victim = moves[i].moveData.isEnPassant
                ? PAWN
                : board.getPiece(board.p[!board.turn], moves[i].to);
#endif
#ifdef ENABLE_QS_DELTA
            if (!moves[i].moveData.isPromotion
                && staticEval + SEE_VALUE[victim] + QS_DELTA_MARGIN < alpha) {
                numQsDeltaPrunes++;
                continue;
            }
#endif
#ifdef ENABLE_QS_SEE
            if (moves[i].moveData.isCapture) {
                Piece attacker = board.getPiece(board.p[board.turn], moves[i].from);
                if (moves[i].moveData.isPromotion) attacker = moves[i].promote;
                // Equal/winning MVV cannot have negative SEE.
                if (SEE_VALUE[victim] < SEE_VALUE[attacker] && board.see(moves[i]) < 0) {
                    numQsSeePrunes++;
                    continue;
                }
            }
#endif
        }

        BitBoard oldboard = board;
        board.movePiece(moves[i]);

        if (board.testInCheck(!board.turn)) {
            board = oldboard;
            continue;
        }
        foundLegalMove = true;

        int32_t eval = -quiesce(board, -beta, -alpha, currdepth+1);
        board = oldboard;

        if (eval >= beta) return eval;
        if (eval > alpha) alpha = eval;
        if (eval > bestEval) bestEval = eval;
    }

#ifdef ENABLE_QS_CHECK
    if (inCheck && !foundLegalMove) {
        return -MATE(currdepth+1);
    }
#else
    (void)foundLegalMove;
#endif
    return bestEval;
}

inline void Engine::extendSearch(uint8_t& depth, bool inCheck) const {
    if (depth >= quiesceDepth-1) return;
    if (depth < quiesceDepth-1 && inCheck) depth+=2;
}

inline uint8_t Engine::reduce(uint8_t const currdepth, uint8_t const maxdepth, uint8_t movesSearched) {
    uint8_t newdepth = maxdepth;
    #ifdef ENABLE_LMR
    const uint8_t remaining = (maxdepth > currdepth) ? static_cast<uint8_t>(maxdepth - currdepth) : 0;
    // remaining >= 2 and R < remaining ⇒ child gets currdepth+1 <= newdepth and hits >= leaf
    if (remaining >= 2 && movesSearched > LMR_MIN_MOVES) {
        uint8_t r = (movesSearched > LMR_AGGRESSIVE_MOVES) ? LMR_R2 : LMR_R1;
        if (r >= remaining) r = remaining - 1;
        if (r > 0) {
            newdepth = maxdepth - r;
            numReductions++;
        }
    }
    #endif
    return newdepth;
}

void Engine::storeKiller(uint8_t ply, BitBoard::Move const& move) {
#ifdef ENABLE_KILLERS
    if (ply >= MAX_DEPTH || move.moveData.isCapture) return;
    if (killers[ply][0] == move) return;
    killers[ply][1] = killers[ply][0];
    killers[ply][0] = move;
#endif
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
                                     uint8_t maxdepth, uint8_t const currdepth,
                                     bool onPV)
{
    using namespace BitBoardState;
    npos++;

    if (shouldStop) {
        return 0;
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
        // Never return a TT score at the root: the same Zobrist can be mate
        // on one visit and a draw by repetition on the next, and a 1-move PV
        // from a hash hit then aborts iterative deepening on the mate score.
        if (currdepth > 0 && entry.depth >= (maxdepth-currdepth)) {
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

    // Base case (>= so over-reduced LMR / extensions cannot walk past the horizon)
    if (currdepth >= maxdepth) {
        const auto end = std::chrono::steady_clock::now();
        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - timeStart);
        if (time.count() >= (timelimit)) {
            shouldStop = true;
        }

        // Don't double count nodes in the quiesce function
        npos--; 
        
        int32_t eval = quiesce(board, alpha, beta, currdepth);
        return eval;
    }

    std::array<BitBoard::Move,MAX_MOVES> moves = {0};
    BitBoard oldboard = board;

    bool inCheck = board.testInCheck(board.turn);
    // Increase depth of search while still in check
    if (currdepth == maxdepth - 1) extendSearch(maxdepth, inCheck);
    uint8_t newdepth = maxdepth;

#ifdef ENABLE_NULL_MOVE
    if (maxdepth > currdepth+3 && !inCheck && board.calculateEndgameBlendFactor() < 0.85f) {
        // Null move reduction: try a Null move and use it to reduce search depth
        numNullAttempts++;
        if (board.s[WHITE].enPassantSquare) {
            board.hash ^= EN_PASSANT_HASH * board.s[WHITE].enPassantSquare;
            board.s[WHITE].enPassantSquare = 0;
        }
        if (board.s[BLACK].enPassantSquare) {
            board.hash ^= EN_PASSANT_HASH * board.s[BLACK].enPassantSquare;
            board.s[BLACK].enPassantSquare = 0;
        }
        board.changeTurn();
#ifdef ASSERT_ON
        assert(board.hash == board.tt->genHash(board));
#endif
        newdepth = maxdepth-3;
        int32_t eval = -recursiveDepthSearch(board, -beta, -beta + 1, newdepth, currdepth+1, false);
        board = oldboard;
        if (!shouldStop && eval >= beta) {
            numNullReductions++;
            // Doing nothing is already better for us than making a move, just return beta
            #ifdef ENABLE_TT
            board.tt->updateEntry(board, BitBoard::Move(), beta, maxdepth-currdepth, TT::CUT);
            #endif
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
    if (!ttMove.valid() && onPV && pvs[0].moves[currdepth].valid()) {
        ttMove = pvs[0].moves[currdepth];
    }
#ifdef ENABLE_KILLERS
    board.sortMoves(moves, numMoves, ttMove, killers[currdepth][0], killers[currdepth][1]);
#else
    board.sortMoves(moves, numMoves, ttMove);
#endif

#ifdef SEARCH_STATS_ON
    if (currdepth == 0) {
        std::cout << "root-order d=" << std::to_string(maxdepth)
                  << " tt=" << (ttMove.valid() ? BitBoard::moveToStr(ttMove) : "-")
                  << " prevPV=" << (pvs[0].moves[0].valid() ? BitBoard::moveToStr(pvs[0].moves[0]) : "-")
                  << " first=";
        for (uint8_t i = 0; i < std::min<uint8_t>(8, numMoves); i++) {
            std::cout << BitBoard::moveToStr(moves[i]) << "(" << moves[i].value << ") ";
        }
        std::cout << std::endl;
    }
#endif

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

        foundLegalMove = true;
        int32_t newEval = 0;

        // Draw if this position already occurred (game or search path).
        // Must run at the root too: a TT mate move can be a repetition.
        if (board.history.isRepeat(board.hash)) {
            if (currdepth % 2 == 0) 
                #ifdef ENABLE_CONTEMPT
                newEval = -DRAW_THRESHHOLD;
                #else
                newEval = 0;
                #endif
            else newEval = 0;
        } else {
            board.history.insert(board.hash);

            movesSearched++;
            branches++;
            const bool isKiller = (killers[currdepth][0] == *move) || (killers[currdepth][1] == *move);
            if (!inCheck && !isKiller && !move->moveData.isCapture && !board.testInCheck(board.turn)) {
                newdepth = reduce(currdepth, maxdepth, movesSearched);
                depthReduced = newdepth != maxdepth;
            }

            const bool childOnPV = onPV && (*move == pvs[0].moves[currdepth]);
            #ifdef ENABLE_PVS
            const bool useScout = numPvs == 1 && movesSearched > 1;
            #else
            const bool useScout = false;
            #endif

            if (useScout) {
                // A later move only needs to prove that it can beat alpha.
                // Re-search a fail-high inside the full window to recover its
                // exact score and principal variation.
                numPvsScouts++;
                newEval = -recursiveDepthSearch(board, -alpha-1, -alpha,
                                                newdepth, currdepth+1, false);

                if (depthReduced && !shouldStop && newEval > alpha) {
                    // Confirm an LMR fail-high at full depth, still with the
                    // cheap scout window.
                    numRedos++;
                    newdepth = maxdepth;
                    numPvsScouts++;
                    newEval = -recursiveDepthSearch(board, -alpha-1, -alpha,
                                                    newdepth, currdepth+1, false);
                }

                if (!shouldStop && newEval > alpha && newEval < beta) {
                    numPvsResearches++;
                    newEval = -recursiveDepthSearch(board, -beta, -alpha,
                                                    newdepth, currdepth+1, childOnPV);
                }
            } else {
                newEval = -recursiveDepthSearch(board, -beta, -alpha,
                                                newdepth, currdepth+1, childOnPV);

                if (depthReduced && !shouldStop && newEval > alpha) {
                    // Redo search at full depth
                    numRedos++;
                    newdepth = maxdepth;
                    newEval = -recursiveDepthSearch(board, -beta, -alpha,
                                                    newdepth, currdepth+1, childOnPV);
                }
            }
        }

        // Undo move
        board = oldboard;

        if (shouldStop) {
            break;
        }

        // Prune tree if adjacent branch is already < this branch
        if (newEval >= beta) {
            #ifdef ENABLE_TT
            board.tt->updateEntry(board, *move, beta, maxdepth-currdepth, TT::CUT);
            #endif
            if (!move->moveData.isCapture) {
                storeKiller(currdepth, *move);
#ifdef HISTORY_HEURISTIC
                int32_t historyBonus = (maxdepth-currdepth)*(maxdepth-currdepth);
                board.tt->updateHistoryScore(board.turn, *move, historyBonus);
                for (auto m = moves.begin(); m != move; m++) {
                    if (!m->moveData.isCapture) {
                        board.tt->updateHistoryScore(board.turn, *m, -historyBonus/5);
                    }
                }
#endif
            }
            bestMoves[std::min(7u, static_cast<uint>(movesSearched-1))]++;
            return newEval;
        }

        if (newEval > bestEval) {
            bestEval = newEval;
            bestMove = *move;
        }

        raisedAlpha |= updatePvs(alpha, move, newEval, currdepth);
    }

    if (shouldStop) {
        return bestEval;
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
    
    std::cout << "Aspiration retries: " << std::to_string(numAspirationRetries) << std::endl;
    std::cout << "PVS scouts: " << std::to_string(numPvsScouts) << std::endl;
    std::cout << "PVS re-searches: " << std::to_string(numPvsResearches)
              << " (" << std::to_string(numPvsScouts
                                        ? static_cast<float>(numPvsResearches)*100/numPvsScouts
                                        : 0.0f)
              << "%)" << std::endl;
    std::cout << "QS delta prunes: " << std::to_string(numQsDeltaPrunes) << std::endl;
    std::cout << "QS SEE prunes: " << std::to_string(numQsSeePrunes) << std::endl;
    
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