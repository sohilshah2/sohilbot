#ifndef __SOHILBOT_INC_GUARD__
#define __SOHILBOT_INC_GUARD__

#include <string>
#include <fstream>

class CommandParser;

class SohilBot {
    public:
        SohilBot();
        ~SohilBot();

        void uciOutput(const std::string& message, bool addTimestamp = true, bool logToStdout = true);
        void commandLineThread();

    private:
        std::string getTimestamp();
        
        std::ofstream logFile;
        CommandParser* parser;
};

#endif