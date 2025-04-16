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
#include "sohilbot.hpp"

class CommandParser {
    public:
        CommandParser(SohilBot* pSohilBot);
        ~CommandParser();

        // Return 1 on successful exit
        void process(std::string line);

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
        void handleTest();
        void handleMultiPVOption(std::stringstream& ss);
        void initializeEngine();
        void logUnhandledCommand(const std::string& line);
        void handleEval();
        
        // Search handling functions
        void handleStartPosition(std::stringstream& ss);
        void printPerftResults(struct Engine::PerftResult const& result, 
                               std::chrono::high_resolution_clock::time_point start);

        void sendEngineInfo();
        int handleFENPosition(std::stringstream& ss);
        int handleNewPosition(std::vector<std::string>& moves);

        bool isActive;
        bool debugMode;

        BitBoard::Move bestmove;
        int32_t besteval;

        BitBoard board;
        SohilBot* cmd;

        Engine *pEngine;
        TT *tt;
};

#endif