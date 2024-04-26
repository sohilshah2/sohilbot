#ifndef __COMMAND_PARSER_INC_GUARD__
#define __COMMAND_PARSER_INC_GUARD__

#include <vector>
#include "engine.hpp"
#include "defines.hpp"

class CommandParser {
    public:
        CommandParser() { };
        ~CommandParser() { };

        // Return 1 on successful exit
        int process();
        void sendBestMove(std::string const& bestmove);
        void sendPrincipalVariation(int depth, int score, int time, int nodes,
                                    int nps, std::string const& pv);

    private:
        void sendEngineInfo();

        int handleDebug(bool debugMode);
        int handleIsReady();
        int handleSetOption(std::string const& name, std::string const& value);
        int handleRegister(std::string const& registerMsg);
        int handleNewGame();
        int handleFENPosition(std::string const& fen, std::vector<std::string>& moves);
        int handleNewPosition(std::vector<std::string>& moves, BoardState::BoardType& board);
        int handleGo(int depth, int nodes, bool infinite);
        int handleStop();

        bool isActive = false;

        BoardState::BoardType board = BoardState::initialBoard;
        BoardState::ColorPiece oldPiece = BoardState::WEMPTY;
        BoardState::Move lastmove;
        BoardState::Move bestmove;
        int32_t besteval;
        uint8_t depth = DEFAULT_DEPTH;

        Engine *pEngine;
        TT *tt;
};

#endif