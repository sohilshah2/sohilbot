#ifndef __ENGINE_INC_GUARD__
#define __ENGINE_INC_GUARD__

#include "boardState.hpp"
#include "transpositionTables.hpp"
#include <array>
#include <chrono>

class Engine {
    public:
        Engine(TT *table) : table(table) { };
        ~Engine() { };

        void setBoardState(BoardState::BoardType const& state);
        int32_t searchBestMove(BoardState::BoardType& board, BoardState::Move& move, 
                               uint8_t depth, uint32_t time);
        uint32_t perft(BoardState::BoardType& board, 
                       uint8_t depth, bool divide=true);
    private:
        int32_t recursiveDepthSearch(BoardState::BoardType& board,
                                     std::array<BoardState::Move, MAX_DEPTH>& currVariation,
                                     bool usePrincipalMove, int32_t alpha, int32_t const beta, 
                                     uint8_t maxdepth, uint8_t const currdepth);
        bool testInCheck(BoardState::BoardType& board);
        void sendEngineInfo(uint8_t depth);

        std::array<BoardState::Move, MAX_DEPTH> topLine;
        TT *table;
        int32_t topEval;
        int32_t npos;
        int32_t branches;
        int32_t numRedos=0;
        int32_t numReductions=0;
        uint32_t numTTLookups=0;
        uint32_t numTTHits=0;
        uint32_t numTTSoftmiss=0;
        uint32_t numTTEvictions=0;
        uint32_t numTTFills=0;
        uint32_t numNullReductions=0;
        int32_t aspirationRetries=0;
        int32_t windowAlpha=INT32_MIN;
        int32_t windowBeta=INT32_MIN;
        uint8_t depthIter=0;
        uint32_t timelimit=3000;
        std::chrono::time_point<std::chrono::system_clock> timeStart;
        float prevTime;
};

#endif