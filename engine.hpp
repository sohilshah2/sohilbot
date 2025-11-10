#ifndef __ENGINE_INC_GUARD__
#define __ENGINE_INC_GUARD__

#include "bitboard.hpp"
#include <array>
#include <chrono>
#include "sohilbot.hpp"

class Engine {
    public:
        struct PerftResult {
            uint64_t nodes;
            uint64_t captures;
            uint64_t enpassants;
            uint64_t castles;
            uint64_t promotions;
            uint64_t checks;
            uint64_t mates;

            bool operator==(const PerftResult& other) const {
                return nodes == other.nodes &&
                       captures == other.captures &&
                       enpassants == other.enpassants &&
                       castles == other.castles &&
                       promotions == other.promotions &&
                       checks == other.checks &&
                       mates == other.mates;
            }

            friend std::ostream& operator<<(std::ostream& os, const PerftResult& result) {
                os << "{" << std::endl;
                os << "    nodes:       " << result.nodes << std::endl;
                os << "    captures:    " << result.captures << std::endl;
                os << "    enpassants:  " << result.enpassants << std::endl;
                os << "    castles:     " << result.castles << std::endl;
                os << "    promotions:  " << result.promotions << std::endl;
                os << "    checks:      " << result.checks << std::endl;
                os << "    mates:       " << result.mates << std::endl;
                os << "}" << std::endl;
                return os;
            }
        };

        Engine(SohilBot* pSohilBot) : cmd(pSohilBot) { };
        ~Engine() { };

        int32_t searchBestMove(BitBoard& board, BitBoard::Move& move, 
                               uint8_t depth, uint32_t time);
        uint64_t perft(PerftResult& result, BitBoard& board, uint8_t depth, bool divide=true);
        void stop() { shouldStop = true; };
        void setNumPvs(uint8_t pvs) { numPvs = pvs; }

    private:
        struct Line {
            std::array<BitBoard::Move, MAX_DEPTH> moves;
            int32_t eval;
        };

        int32_t recursiveDepthSearch(BitBoard& board,
                                     int32_t alpha, int32_t beta, 
                                     uint8_t maxdepth, uint8_t const currdepth);
        int32_t searchPv(BitBoard& board,
                         int32_t alpha, int32_t const beta, 
                         uint8_t const maxdepth, uint8_t const currdepth);
        int32_t quiesce(BitBoard& board, int32_t alpha, int32_t const beta, uint8_t const currdepth);

        void sendEngineInfo(uint8_t depth);
        void printSearchStats() const;
        void extendSearch(uint8_t& depth, bool inCheck) const;
        uint8_t reduce(uint8_t const currdepth, uint8_t const maxdepth, uint8_t movesSearched);
        bool updatePvs(int32_t& alpha, BitBoard::Move* move,
                       int32_t newEval, uint8_t const currdepth);

        struct Line currPvs[MAX_PVS][MAX_DEPTH];
        struct Line pvs[MAX_PVS];

        uint64_t npos;
        uint64_t branches;
        uint32_t numRedos=0;
        uint32_t numReductions=0;
        uint32_t numTTLookups=0;
        uint32_t numTTHits=0;
        uint32_t numTTSoftmiss=0;
        uint32_t numTTEvictions=0;
        uint32_t numTTFills=0;
        uint32_t numNullReductions=0;
        uint32_t aspirationRetries=0;
        uint8_t depthIter=0;
        uint8_t seldepth=0;
        uint8_t quiesceDepth=0;
        uint32_t timelimit=3000;
        std::chrono::time_point<std::chrono::steady_clock> timeStart;
        float prevTime;
        std::atomic<bool> shouldStop;
        uint8_t numPvs=1;
        SohilBot* cmd;
};

#endif