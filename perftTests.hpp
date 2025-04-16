#ifndef PERFTTESTS_H
#define PERFTTESTS_H

#include "engine.hpp"

namespace PerftTests {
    struct Test {
        std::string fen;
        uint8_t depth;
        Engine::PerftResult expected;
    };

    static const std::vector<Test> tests = {
        { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, {.nodes = 4865609, .captures = 82719, .checks = 27351, 
                                                                          .promotions = 0, .castles = 0, .enpassants = 258, .mates = 8}},
        { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, {.nodes = 4085603, .captures = 757163, .checks = 25523, 
                                                                                   .promotions = 15172, .castles = 128013, .enpassants = 1929, .mates = 1}},
        { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1 ", 6, {.nodes = 11030083, .captures = 940350, .checks = 452473, 

                                                                                   .promotions = 7552, .castles = 0, .enpassants = 33325, .mates = 0}},
        { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, {.nodes = 15833292, .captures = 2046173, .checks = 200568, 
                                                                                   .promotions = 329464, .castles = 0, .enpassants = 6512, .mates = 5}},
        { "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 5, {.nodes = 15833292, .captures = 2046173, .checks = 200568, 
                                                                                   .promotions = 329464, .castles = 0, .enpassants = 6512, .mates = 5}},

        { "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 5, {.nodes = 164075551, .captures = 19528068, .checks = 2998380, 
                                                                                   .promotions = 0, .castles = 0, .enpassants = 122, .mates = 0}}
                                                                                
    };

    static void printStatus(const Test& test, const Engine::PerftResult& result) {
        std::cout << "FEN: " << test.fen << std::endl;
        std::cout << "--------------------------------" << std::endl;
        if (result == test.expected) {
            std::cout << "Test PASSED" << std::endl;
        } else {
            std::cout << "Test FAILED" << std::endl;
            std::cout << std::endl;
            std::cout << "Expected: " << test.expected << std::endl;
            std::cout << "Result: " << result << std::endl;
        }
        std::cout << "--------------------------------" << std::endl;
    }
};

#endif

