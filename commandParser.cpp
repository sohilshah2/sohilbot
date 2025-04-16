/**
 * @file commandParser.cpp
 * @brief Implementation of the UCI (Universal Chess Interface) command parser
 */

#include <assert.h>
#include <iostream>
#include <sstream>
#include <string>
#include <random>
#include <chrono>

#include "commandParser.hpp"
#include "bitboard.hpp"
#include "defines.hpp"
#include "evaluate.hpp"
#include "transpositionTables.hpp"
#include "perftTests.hpp"

using namespace std;

CommandParser::CommandParser(SohilBot* pSohilBot) {
    cmd = pSohilBot;
    isActive = false;
    debugMode = false;
    pEngine = new Engine(cmd);
    tt = new TT();
    board = BitBoard(tt, true);
}

CommandParser::~CommandParser() { 
    delete pEngine;
    delete tt;
};

/**
 * @brief Main command processing loop that handles UCI protocol commands
 * @param line The command line to process
 * @return int Success status (1 for success)
 */
void CommandParser::process(string line) {
    logCommand(line);
    
    if(isActive) {
        handleActiveCommand(line);
    } else {
        handleInactiveCommand(line);
    }
}

/**
 * @brief Logs the received command with current active status
 * @param line The command line to log
 */
void CommandParser::logCommand(const string& line) {
    if (debugMode) {
        cout << "// [DEBUG; " << 
             (isActive ? "ACTIVE" : "INACTIVE") 
             << "] got command \"" << line << "\"" << endl;
    }
}

/**
 * @brief Handles commands when the engine is in active state
 * @param line The command line to process
 */
void CommandParser::handleActiveCommand(const string& line) {
    std::stringstream ss(line);
    std::string command;
    getline(ss, command, ' ');

    if (command == "isready") {
        handleIsReady();
    } else if (command == "ucinewgame") {
        handleNewGame();
    } else if (command == "move") {
        handleMove(ss);
    } else if (command == "moves") {
        handleListMoves();
    } else if (command == "captures") {
        handleListCaptures();
    } else if (command == "position") {
        handlePosition(ss);
    } else if (command == "go") {
        handleGo(ss);
    } else if (command == "setoption") {
        handleSetOption(ss);
    } else if (command == "perft") {
        handlePerft(ss);
    } else if (command == "test") {
        handleTest();
    } else if (command == "debug") {
        handleDebug(ss);
    } else if (command == "eval") {
        handleEval();
    } else {
        logUnhandledCommand(line);
    }
}

/**
 * @brief Handles commands when the engine is in inactive state
 * @param line The command line to process
 */
void CommandParser::handleInactiveCommand(const string& line) {
    if (line == "uci") {
        initializeEngine();
    } else {
        cout << "// [DEBUG; INACTIVE] got command \"" 
             << line << "\" while not active, ignoring" << endl;
    }
}

/**
 * @brief Handles the "isready" command
 */
void CommandParser::handleIsReady() {
    cmd->uciOutput("readyok");
}

/**
 * @brief Handles the "ucinewgame" command
 */
void CommandParser::handleNewGame() {
    board = BitBoard(tt, true);
}

/**
 * @brief Handles the "eval" command
 */
void CommandParser::handleEval() {
    board.printBoard();
    board.printMobility();
    float egBlend = board.calculateEndgameBlendFactor();
    std::cout << "Evaluation: " << std::dec << Evaluate::evaluatePosition(board) << std::endl;
    std::cout << "EG Blend Factor: " << egBlend << std::endl;
}

/**
 * @brief Handles the "move" command
 * @param ss String stream containing the move parameters
 */
void CommandParser::handleMove(std::stringstream& ss) {
    using namespace BitBoardState;
    std::string args;
    BitBoard::Move move;
    getline(ss, args,' ');
    board.strToMove(args, move);
    board.movePiece(move);
    board.history.insert(board.hash);
    board.printBoard();
}

/**
 * @brief Handles the "moves" command to list all legal moves
 */
void CommandParser::handleListMoves() {
    using namespace BitBoardState;
    std::array<BitBoard::Move,MAX_MOVES> moves;
    uint8_t numMoves = board.getAvailableMoves(moves);
    board.sortMoves(moves, numMoves, BitBoard::Move());
    cout << "legal moves:" << endl;
    for (auto move = moves.begin(); move != moves.begin() + numMoves; move++) {
        cout << BitBoard::moveToStr(*move) << " est val: " << board.estimateMoveValue(*move) << endl;
    }
}

/**
 * @brief Handles the "captures" command to list all legal captures
 */
void CommandParser::handleListCaptures() {
    using namespace BitBoardState;
    std::array<BitBoard::Move,MAX_MOVES> moves;
    uint8_t numMoves = board.getAvailableMoves(moves, true);
    cout << "legal captures:" << endl;
    for (auto move = moves.begin(); move != moves.begin() + numMoves; move++) {
        cout << BitBoard::moveToStr(*move) << endl;
    }
}

/**
 * @brief Handles the "position" command
 * @param ss String stream containing the position parameters
 */
void CommandParser::handlePosition(std::stringstream& ss) {
    std::string command;
    getline(ss, command, ' ');
    if (command == "startpos") {
        handleStartPosition(ss);
    } else if (command == "fen") {
        handleFENPosition(ss);
    }
}

/**
 * @brief Handles the "go" command with various parameters
 * @param ss String stream containing the go parameters
 */
void CommandParser::handleGo(std::stringstream& ss) {
    std::string command;
    uint32_t time = INFINITE_TIMELIMIT;
    uint8_t depth = MAX_DEPTH;
    uint32_t timeLeft = 0;
    uint32_t inc = 0;

    while(getline(ss, command, ' ')) {
        if (command == "infinite") {
            time = INFINITE_TIMELIMIT;
            depth = MAX_DEPTH;
            break;
        } else if (command == "movetime") {
            getline(ss, command, ' ');
            time = stoi(command);
        } else if (command == "depth") {
            getline(ss, command, ' ');
            depth = stoi(command);
        } else if (command == "wtime") {
            getline(ss, command, ' ');
            if (board.turn == BitBoardState::WHITE) timeLeft = stoi(command);
        } else if (command == "btime") {
            getline(ss, command, ' ');
            if (board.turn == BitBoardState::BLACK) timeLeft = stoi(command);
        } else if (command == "winc") {
            getline(ss, command, ' ');
            if (board.turn == BitBoardState::WHITE) inc = stoi(command);
        } else if (command == "binc") {
            getline(ss, command, ' ');
            if (board.turn == BitBoardState::BLACK) inc = stoi(command);
        } else if (command == "movestogo") {
            getline(ss, command, ' ');
            // Do nothing
        } else if (command == "nodes") {
            getline(ss, command, ' ');
            // Do nothing
        } else if (command == "mate") {
            getline(ss, command, ' ');
            // Do nothing
        } else if (command == "ponder") {
            // Do nothing
        }
    }

    if (timeLeft > 0) {
        // Calculate how much time we should spend on this move
        time = timeLeft / 50;
        time += inc;
        time -= TIME_BUFFER;
    }

    besteval = pEngine->searchBestMove(board, bestmove, depth, time);
    cmd->uciOutput("bestmove " + BitBoard::moveToStr(bestmove));
}

/**
 * @brief Handles the "setoption" command
 * @param ss String stream containing the option parameters
 */
void CommandParser::handleSetOption(std::stringstream& ss) {
    std::string command;
    getline(ss, command, ' ');
    assert(command == "name");
    getline(ss, command, ' ');
    
    if (command == "MultiPV") {
        handleMultiPVOption(ss);
    }
}

/**
 * @brief Handles the "perft" command for performance testing
 * @param ss String stream containing the perft parameters
 */
void CommandParser::handlePerft(std::stringstream& ss) {
    std::string command;
    getline(ss, command, ' ');
    const auto start = std::chrono::high_resolution_clock::now();
    
    Engine::PerftResult result;
    std::memset(&result, 0, sizeof(Engine::PerftResult));
    pEngine->perft(result, board, stoi(command), true);
    
    printPerftResults(result, start);
}

/**
 * @brief Handles the "test" command
 * @param ss String stream containing the test parameters
 */
void CommandParser::handleTest() {
    Engine::PerftResult result;
    const auto start = std::chrono::high_resolution_clock::now();

    for (auto test : PerftTests::tests) {
        std::stringstream fen;
        fen << test.fen;
        handleFENPosition(fen);
        std::memset(&result, 0, sizeof(Engine::PerftResult));
        const auto tempStart = std::chrono::high_resolution_clock::now();
        pEngine->perft(result, board, test.depth, false);
        const auto end = std::chrono::high_resolution_clock::now();
        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - tempStart);
       
        PerftTests::printStatus(test, result);
        std::cout << "took " << to_string(time.count()) << "ms";
        uint64_t rate = result.nodes / time.count();
        std::cout << "  |  Searchrate: " << to_string(rate) << "k nodes/sec" << endl;
        std::cout << endl;
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Total time: " << to_string(time.count()) << "ms" << endl;

}

/**
 * @brief Handles the "debug" command
 * @param ss String stream containing the debug parameters
 */
void CommandParser::handleDebug(std::stringstream& ss) {
    std::string mode;
    getline(ss, mode, ' ');
    
    if (mode == "on") {
        debugMode = true;
        std::cout << "// [DEBUG; ACTIVE] Debug mode enabled" << std::endl;
    } else if (mode == "off") {
        debugMode = false;
        std::cout << "// [DEBUG; ACTIVE] Debug mode disabled" << std::endl;
    } else {
        std::cout << "// [DEBUG; ACTIVE] Invalid debug mode. Use 'debug on' or 'debug off'" << std::endl;
    }
}

/**
 * @brief Initializes the chess engine and sends engine info
 */
void CommandParser::initializeEngine() {
    if (debugMode) {
        std::cout << "// [DEBUG; ACTIVE] got command \"uci\", setting to ACTIVE" << endl;
    }
    isActive = true;
    sendEngineInfo();
}

/**
 * @brief Logs an unhandled command
 * @param line The unhandled command line
 */
void CommandParser::logUnhandledCommand(const string& line) {
    if (debugMode) {
        std::cout << "// [DEBUG; ACTIVE] got unhandled command \""
             << line << "\", ignoring" << endl;
    }
}

/**
 * @brief Sends engine info to the GUI
 */
void CommandParser::sendEngineInfo() {
    cmd->uciOutput("id name sohilbot");
    cmd->uciOutput("id author Sohil Shah");
    cmd->uciOutput("option name MultiPV type spin default 1" 
                        " min 1 max " + to_string(MAX_PVS));
    cmd->uciOutput("uciok");
}

/**
 * @brief Handles a new position from a list of moves
 * @param moves List of moves to apply to the board
 * @param board Board to apply the moves to
 * @return Success status (1 for success)
 */
int CommandParser::handleNewPosition(std::vector<std::string>& moves) {
    int success = 0;
    using namespace BitBoardState;

    for (auto args : moves) {
        BitBoard::Move move;
        BitBoard::strToMove(args, move);
        board.movePiece(move);
        board.history.insert(board.hash);
    }
    return success;
}

/**
 * @brief Handles a new position from a FEN string
 * @param ss String stream containing the FEN string
 * @return Success status (1 for success)
 */
int CommandParser::handleFENPosition(std::stringstream& ss) {
    std::string command;
    getline(ss, command, ' ');
    board = BitBoard(tt, false); // Empty board
    uint8_t pos = 56; 
    for (char c : command) {
        if (c == '/') { pos -= 16; continue; }
        else if (int8_t numEmpty = atoi(&c)) { pos += numEmpty; continue; }

        if (c == 'r') board.p[1].rook |= 1ull << pos;
        else if (c == 'n') board.p[1].knight |= 1ull << pos;
        else if (c == 'b') board.p[1].bishop |= 1ull << pos;
        else if (c == 'q') board.p[1].queen |= 1ull << pos;
        else if (c == 'k') board.p[1].king |= 1ull << pos;
        else if (c == 'p') board.p[1].pawn |= 1ull << pos;

        else if (c == 'R') board.p[0].rook |= 1ull << pos;
        else if (c == 'N') board.p[0].knight |= 1ull << pos;
        else if (c == 'B') board.p[0].bishop |= 1ull << pos;
        else if (c == 'Q') board.p[0].queen |= 1ull << pos;
        else if (c == 'K') board.p[0].king |= 1ull << pos;
        else if (c == 'P') board.p[0].pawn |= 1ull << pos;
        pos++;
    }

    getline(ss, command, ' ');
    if (command[0] == 'w') board.turn = BitBoardState::WHITE;
    else board.turn = BitBoardState::BLACK;

    getline(ss, command, ' ');
    for (char c : command) {
        if (c == 'K') board.s[0].castleShort = true;
        else if (c == 'k') board.s[1].castleShort = true;
        else if (c == 'Q') board.s[0].castleLong = true;
        else if (c == 'q') board.s[1].castleLong = true;
    }

    getline(ss, command, ' ');
    if (command != "-") {
        uint8_t col = command[0] - 'a';
        uint8_t row = command[1] - '0' - 1;
        uint8_t idx = (row<<3) | col;
        board.s[board.turn].enPassantSquare = idx;
    }

    // halfmoves
    getline(ss, command, ' ');
    if (command != "") board.moves = stoi(command);
    // fullmoves
    getline(ss, command, ' ');

    board.hash = tt->genHash(board);

    getline(ss, command, ' ');
    if (command == "moves") {
        std::vector<std::string> moves;
        std::string move;
        while(getline(ss, move, ' ')) { 
            moves.push_back(move); 
        }
        handleNewPosition(moves);
    }

    board.value = board.getBoardValue();
    board.recalculateOccupancy();
    board.recalculateThreats();
    return 1;
}

/**
 * @brief Handles setting the multipv option
 * @param ss String stream containing the multipv value
 */
void CommandParser::handleMultiPVOption(std::stringstream& ss) {
    std::string command;
    getline(ss, command, ' ');
    assert(command == "value");
    getline(ss, command, ' ');
    pEngine->setNumPvs(stoi(command));
    cout << "Setting MultiPV to " << command << endl;
}

/**
 * @brief Handles setting up a new position from startpos
 * @param ss String stream containing the position parameters
 */
void CommandParser::handleStartPosition(std::stringstream& ss) {
    board = BitBoard(tt, true);
    std::string command;
    getline(ss, command, ' ');
    if (command == "moves") {
        std::vector<std::string> moves;
        std::string move;
        while(getline(ss, move, ' ')) { 
            moves.push_back(move); 
        }
        handleNewPosition(moves);
    }
}

/**
 * @brief Prints the results of a perft test
 * @param npos Number of positions searched
 * @param start Start time of the search
 */
void CommandParser::printPerftResults(struct Engine::PerftResult const& result, 
                                     std::chrono::high_resolution_clock::time_point start) 
{
    std::cout << "Total captures: " << std::to_string(result.captures) << std::endl;
    std::cout << "Total checks: " << std::to_string(result.checks) << std::endl;
    std::cout << "Total promotes: " << std::to_string(result.promotions) << std::endl;
    std::cout << "Total castles: " << std::to_string(result.castles) << std::endl;
    std::cout << "Total en passant: " << std::to_string(result.enpassants) << std::endl;
    
    std::cout << "Total positions searched: " << to_string(result.nodes) << endl;
    const auto end = std::chrono::high_resolution_clock::now();
    const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Elapsed time: " << to_string(time.count()) << "ms" << endl;
    uint64_t rate = result.nodes / time.count();
    std::cout << "Searchrate: " << to_string(rate) << "k nodes/sec" << endl;
}

/**
 * @brief Stops the command parser thread
 */
void CommandParser::stop() {
    pEngine->stop();
}