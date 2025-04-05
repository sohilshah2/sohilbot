SohilBot Chess Engine
===================

A chess engine written in C++ that uses alpha-beta pruning with various optimizations including:
- Move Ordering
- Late Move Reductions
- Transposition Tables
- Quiescence Search
- Null Move Pruning

Features:
---------
- UCI (Universal Chess Interface) compatible
- Bitboard-based board representation for efficient move generation
- Configurable search depth and time limits
- Detailed search statistics and performance metrics
- Multiple principal variations

Building:
---------
The engine can be built using make:
    make

This will create an executable named 'sohilbot'.

Usage:
------
The engine communicates through UCI protocol. You can use it with any UCI-compatible chess GUI (like Arena, Cutechess, etc.).

When running the engine, you can configure:
- Search depth
- Time limit (in milliseconds)
- Hash table size

The engine will output:
- Best move found
- Current search depth
- Position evaluation
- Principal variation (best line of play)
- Search statistics including:
  * Nodes searched
  * Branch factor
  * Late Move Reduction rate
  * Transposition table hit rate
  * And more...

Example UCI commands:
    uci                    - Initialize UCI
    isready               - Check if engine is ready
    position startpos     - Set starting position
    go depth 10          - Search to depth 10
    go movetime 1000     - Search for 1 second
    stop                 - Stop current search