#include <assert.h>
#include <iostream>
#include <sstream>
#include <string>
#include <random>

#include "commandParser.hpp"
#include "boardState.hpp"
#include "transpositionTables.hpp"
#include "defines.hpp"

using namespace std;

int CommandParser::process() {
    int success = 1;
    uint32_t timelimit = 5000;
    string line;

    while (getline(cin, line)) {
        if (line == "quit") {
            break;
        }

        cout << "// [DEBUG; " << 
             (isActive ? "ACTIVE" : "INACTIVE") 
             << "] got command \"" << line << "\"" << endl;
        if(isActive) {
            std::stringstream ss (line);
            std::string command;
            getline (ss, command, ' ');

            if (command == "isready") {
                cout << "readyok" << endl;
            } else if (command == "ucinewgame") {
                board = BoardState::initialBoard;
                board.hash = tt->genHash(board);
                BoardState::printBoard(board);
            } else if (command == "move") {
                using namespace BoardState;
                std::string args;
                MoveType move;
                getline(ss, args,' ');
                strToMove(board, args, move);
                oldPiece = movePiece(board, move, tt);
                lastmove = move;
                printBoard(board);
                pEngine->searchBestMove(board, move, depth, timelimit);
                oldPiece = movePiece(board, move, tt);
                printBoard(board);
                lastmove = move;
            } else if (command == "moves") {
                using namespace BoardState;
                std::array<Move,MAX_MOVES> moves;
                uint8_t numMoves = getAvailableMoves(board, moves);
                sortMoves(moves,Move(),Move(),numMoves,board);
                cout << "legal moves:" << endl;
                for (auto move = moves.begin(); move != moves.begin() + numMoves; move++) {
                    cout << moveToStr(*move) << endl;
                }
            } else if (command == "position") {
                getline(ss, command, ' ');
                if (command == "startpos") {
                    board = BoardState::initialBoard;
                    board.hash = tt->genHash(board);
                    getline(ss, command, ' ');
                    if (command == "moves") {
                        std::vector<std::string> moves;
                        std::string move;
                        while(getline(ss, move, ' ')) { 
                            moves.push_back(move); 
                        }
                        handleNewPosition(moves, board);
                    }
                } else if (command == "fen") {
                    getline(ss, command, ' ');
                    board = BoardState::emptyBoard;
                    uint8_t pos = 56;
                    for (char c : command) {
                        if (c == '/') { pos -= 16; continue; }
                        else if (int8_t numEmpty = atoi(&c)) { pos += numEmpty; continue; }

                        if (c == 'r') board.b.data8[pos>>1] |= BoardState::BROOK << ((pos & 0x1) ? 4 : 0);
                        else if (c == 'n') board.b.data8[pos>>1] |= BoardState::BKNIGHT << ((pos & 0x1) ? 4 : 0);
                        else if (c == 'b') board.b.data8[pos>>1] |= BoardState::BBISHOP << ((pos & 0x1) ? 4 : 0);
                        else if (c == 'q') board.b.data8[pos>>1] |= BoardState::BQUEEN << ((pos & 0x1) ? 4 : 0);
                        else if (c == 'k') board.b.data8[pos>>1] |= BoardState::BKING << ((pos & 0x1) ? 4 : 0);
                        else if (c == 'p') board.b.data8[pos>>1] |= BoardState::BPAWN << ((pos & 0x1) ? 4 : 0);

                        else if (c == 'R') board.b.data8[pos>>1] |= BoardState::WROOK << ((pos & 0x1) ? 4 : 0);
                        else if (c == 'N') board.b.data8[pos>>1] |= BoardState::WKNIGHT << ((pos & 0x1) ? 4 : 0);
                        else if (c == 'B') board.b.data8[pos>>1] |= BoardState::WBISHOP << ((pos & 0x1) ? 4 : 0);
                        else if (c == 'Q') board.b.data8[pos>>1] |= BoardState::WQUEEN << ((pos & 0x1) ? 4 : 0);
                        else if (c == 'K') board.b.data8[pos>>1] |= BoardState::WKING << ((pos & 0x1) ? 4 : 0);
                        else if (c == 'P') board.b.data8[pos>>1] |= BoardState::WPAWN << ((pos & 0x1) ? 4 : 0);
                        pos++;
                    }

                    getline(ss, command, ' ');
                    if (command[0] == 'w') board.turn = BoardState::WHITE;
                    else board.turn = BoardState::BLACK;

                    getline(ss, command, ' ');
                    for (char c : command) {
                        if (c == 'K') board.cached.whiteShort = true;
                        else if (c == 'k') board.cached.blackShort = true;
                        else if (c == 'Q') board.cached.whiteLong = true;
                        else if (c == 'q') board.cached.blackLong = true;
                    }

                    getline(ss, command, ' ');
                    if (command != "-") {
                        uint8_t col = command[0] - 'a';
                        uint8_t row = command[1] - '0' - 1;
                        uint8_t idx = (row<<3) | col;
                        board.cached.enPassantSquare = idx;
                    }

                    // halfmoves
                    getline(ss, command, ' ');
                    // fullmoves
                    getline(ss, command, ' ');

                    getline(ss, command, ' ');
                    if (command == "moves") {
                        std::vector<std::string> moves;
                        std::string move;
                        while(getline(ss, move, ' ')) { 
                            moves.push_back(move); 
                        }
                        handleNewPosition(moves, board);
                    }

                    board.hash = tt->genHash(board);
                    board.value = BoardState::getBoardValue(board);
                    BoardState::printBoard(board);
                }
            } else if (command == "go") {
                getline(ss, command, ' ');
                if (command == "infinite") {
                    using namespace BoardState;
                    besteval = pEngine->searchBestMove(board, bestmove, depth, timelimit);
                    oldPiece = movePiece(board, bestmove, tt);
                    cout << "bestmove " << BoardState::moveToStr(bestmove) << endl;
                } else if (command == "movetime") {
                    getline(ss, command, ' ');
                    using namespace BoardState;
                    besteval = pEngine->searchBestMove(board, bestmove, depth, stoi(command));
                    oldPiece = movePiece(board, bestmove, tt);
                    cout << "bestmove " << BoardState::moveToStr(bestmove) << endl;
                } else if (command == "depth") {
                    getline(ss, command, ' ');
                    using namespace BoardState;
                    besteval = pEngine->searchBestMove(board, bestmove, stoi(command), timelimit);
                    oldPiece = movePiece(board, bestmove, tt);
                    cout << "bestmove " << BoardState::moveToStr(bestmove) << endl;
                }
            } else if (command == "setoption") {
                getline(ss, command, ' ');
                assert(command == "name");
                getline(ss, command, ' ');
                if (command == "depth") {
                    getline(ss, command, ' ');
                    assert(command == "value");
                    getline(ss, command, ' ');
                    depth = stoi(command);
                } else if (command == "timelimit") {
                    getline(ss, command, ' ');
                    assert(command == "value");
                    getline(ss, command, ' ');
                    timelimit = stoi(command);
                }
            } else if (command == "perft") {
                getline(ss, command, ' ');
                const auto start = std::chrono::high_resolution_clock::now();
                uint32_t npos = pEngine->perft(board, stoi(command), true);
                cout << "Total positions searched: " << to_string(npos) << endl;
                const auto end = std::chrono::high_resolution_clock::now();
                const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                cout << "Elapsed time: " << to_string(time.count()) << "ms" << endl;
                uint32_t rate = npos / time.count();
                cout << "Searchrate: " << to_string(rate) << "k nodes/sec" << endl;
            } else if (command == "stop") {
                    //lastmove = bestmove;
                    //cout << "bestmove " << BoardState::moveToStr(bestmove) << endl;
            } else {
                cout << "// [DEBUG; ACTIVE] got unhandled command \""
                     << line << "\", ignoring" << endl;
            }
        } else {
            if (line == "uci") {
                cout << "// [DEBUG; ACTIVE] got command \"" 
                     << line << "\", setting to ACTIVE" << endl;
                tt = new TT();
                pEngine = new Engine(tt);
                isActive = true;
                sendEngineInfo();
            } else {
                cout << "// [DEBUG; INACTIVE] got command \"" 
                     << line << "\" while not active, ignoring" << endl;
            }
        }
    }

    cout << "Exiting with code " << to_string(success) << endl;
    return success;
}

void CommandParser::sendEngineInfo() {
    cout << "id name sohilbot" << endl;
    cout << "id author Sohil Shah" << endl;
    cout << "option name depth type spin default " 
         << to_string(depth) 
         << " min 2 max " << to_string(MAX_DEPTH)
         << endl;
    cout << "option name timelimit type spin default " 
         << to_string(5000) 
         << " min 1000 max 1000000"
         << endl;
    cout << "uciok" << endl;
}

int CommandParser::handleNewPosition(std::vector<std::string>& moves, 
                                     BoardState::BoardType& board) {
    int success = 0;
    using namespace BoardState;

    for (auto args : moves) {
        MoveType move;
        strToMove(board, args, move);
        movePiece(board, move, tt);
    }

    BoardState::printBoard(board);
    return success;
}
