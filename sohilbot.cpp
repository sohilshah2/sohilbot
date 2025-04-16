#include <iostream>
#include <thread>
#include <string>
#include <fstream>
#include <ctime>
#include "sohilbot.hpp"
#include "commandParser.hpp"

SohilBot::SohilBot() {
    // Open log file with timestamp in name
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "logs/sohilbot_debug_log%Y%m%d_%H%M%S.txt");
    SohilBot::logFile.open(oss.str());
    
    if (!SohilBot::logFile.is_open()) {
        throw std::runtime_error("Failed to open log file: " + oss.str());
    }

    parser = new CommandParser(this);
}

SohilBot::~SohilBot() {
    SohilBot::logFile.close();
    delete parser;
}

// Helper function to log to both stdout and file
void SohilBot::uciOutput(const std::string& message, bool addTimestamp, bool logToStdout) {
    #ifdef LOG_ON
    if (addTimestamp) {
        SohilBot::logFile << "[" << getTimestamp() << "] ";
    }
    SohilBot::logFile << message << std::endl;
    #endif
    if (logToStdout) {
        std::cout << message << std::endl;
    }
}

// Function to get current timestamp
std::string SohilBot::getTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void SohilBot::commandLineThread() {
    std::string line;
    std::thread commandParserThread;

    bool threadActive = false;

    while (true) {
        if (getline(std::cin, line)) {
            // Log the command to file 
            uciOutput("Received command: " + line, true, false);

            if (line == "quit" || line == "stop") {
                if (threadActive) {
                    parser->stop();
                    commandParserThread.join();
                    threadActive = false;
                }
                if (line == "quit") break;
            } else {
                if (line == "isready") {
                    if (threadActive) commandParserThread.join();
                    SohilBot::uciOutput("readyok");
                    threadActive = false;
                } else {
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
    SohilBot* sohilbot = new SohilBot();
    
    // Create the command line input thread
    std::thread cmdLineThread(&SohilBot::commandLineThread, sohilbot);
    
    // Wait for the command line thread to finish
    cmdLineThread.join();
    
    delete sohilbot;
    return 0;
}
