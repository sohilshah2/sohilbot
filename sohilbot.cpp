#include <iostream>
#include <thread>
#include "commandParser.hpp"
#include "engine.hpp"

int main() {
    CommandParser *parser = new CommandParser();
    std::thread commandParserThread(CommandParser::process, parser);
    std::thread engineThread;

    commandParserThread.join();
    return 1;
}
