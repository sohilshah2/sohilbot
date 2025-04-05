#include <iostream>
#include <thread>
#include <string>
#include <fstream>
#include <ctime>
#include "commandParser.hpp"
#include "engine.hpp"

// Global log file stream
std::ofstream logFile;

// Function to get current timestamp
std::string getTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void commandLineThread(CommandParser* parser) {
    std::string line;
    std::thread commandParserThread;
    bool threadActive = false;

    while (true) {
        if (getline(std::cin, line)) {
            // Log the command to file
            logFile << "[" << getTimestamp() << "] Command received: " << line << std::endl;

            if (line == "quit" || line == "stop") {
                if (threadActive) {
                    parser->stop();
                    commandParserThread.join();
                    threadActive = false;
                }
                if (line == "quit") break;
            } else {
                if (line == "isready") std::cout << "readyok" << std::endl;
                else {
                    if (threadActive) commandParserThread.join();
                    // Create the command parser thread to process this line
                    commandParserThread = std::thread(&CommandParser::process, parser, line);
                    threadActive = true;
                }
            }
        } else {
            // Handle EOF or error
            if (threadActive) {
                parser->stop();
                commandParserThread.join();
                threadActive = false;
            }
            break;
        }
    }
}

int main() {
    // Open log file with timestamp in name
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "logs/sohilbot_debug_log%Y%m%d_%H%M%S.txt");
    logFile.open(oss.str());
    
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file!" << std::endl;
        return 1;
    }
    
    logFile << "[" << getTimestamp() << "] Chess engine started" << std::endl;
    
    CommandParser* parser = new CommandParser();
    
    // Create the command line input thread
    std::thread cmdLineThread(commandLineThread, parser);
    
    // Wait for the command line thread to finish
    cmdLineThread.join();
    
    logFile << "[" << getTimestamp() << "] Chess engine stopped" << std::endl;
    logFile.close();
    
    delete parser;
    return 0;
}
