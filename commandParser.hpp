#ifndef __COMMAND_PARSER_INC_GUARD__
#define __COMMAND_PARSER_INC_GUARD__

#include <vector>
#include <string>
#include <sstream>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "engine.hpp"
#include "defines.hpp"

class CommandParser {
    public:
        CommandParser() : isActive(false), debugMode(false) { };
        ~CommandParser() { 
            stop(); // Ensure thread is stopped on destruction
        };

        // Return 1 on successful exit
        void process(std::string line);
        void sendBestMove(std::string const& bestmove);
        void sendPrincipalVariation(int depth, int score, int time, int nodes,
                                    int nps, std::string const& pv);
        
        // Thread control
        void stop();

        // Debug control
        void setDebugMode(bool mode) { debugMode = mode; }
        bool isDebugMode() const { return debugMode; }

    private:
        // Command handling functions
        void logCommand(const std::string& line);
        void handleActiveCommand(const std::string& line);
        void handleInactiveCommand(const std::string& line);
        void handleIsReady();
        void handleNewGame();
        void handleMove(std::stringstream& ss);
        void handleListMoves();
        void handleListCaptures();
        void handlePosition(std::stringstream& ss);
        void handleGo(std::stringstream& ss);
        void handleSetOption(std::stringstream& ss);
        void handlePerft(std::stringstream& ss);
        void handleDebug(std::stringstream& ss);
        void handleMultiPVOption(std::stringstream& ss);
        void initializeEngine();
        void logUnhandledCommand(const std::string& line);
        void handleEval();
        
        // Search handling functions
        void handleInfiniteSearch();
        void handleMoveTimeSearch(std::stringstream& ss);
        void handleDepthSearch(std::stringstream& ss);
        void handleDepthOption(std::stringstream& ss);
        void handleTimeLimitOption(std::stringstream& ss);
        void handleStartPosition(std::stringstream& ss);
        void printPerftResults(uint64_t npos, std::chrono::high_resolution_clock::time_point start);

        void sendEngineInfo();
        int handleDebug(bool debugMode);
        int handleSetOption(std::string const& name, std::string const& value);
        int handleRegister(std::string const& registerMsg);
        int handleFENPosition(std::stringstream& ss);
        int handleNewPosition(std::vector<std::string>& moves, BitBoard& board);

        bool isActive;
        bool debugMode;

        BitBoard board;
        BitBoardState::Piece oldPiece = BitBoardState::EMPTY;
        BitBoard::MoveType lastmove;
        BitBoard::MoveType bestmove;
        int32_t besteval;
        uint8_t depth = DEFAULT_DEPTH;

        Engine *pEngine;
        TT *tt;
};

#endif