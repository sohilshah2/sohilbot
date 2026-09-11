# SohilBot — Agent Context

UCI-compatible chess engine in C++ (C++17). Goal: iterate on search/eval optimizations and measure Elo via self-play tournaments.

## Quick start

```bash
make                    # builds ./sohilbot (-O3)
make debug              # ASSERT_ON, -O0 -g3
make clean
```

### A/B Elo test (preferred workflow)

Do **not** hand-edit feature flags in `defines.hpp` for experiments. Use compile-time overrides:

```bash
# From repo root — rebuild with TT off, deploy, play vs baseline
make ab-test DISABLE=TT

# Or via helper script
./scripts/ab_test.sh --disable TT
./scripts/ab_test.sh --disable TT,LMR --games 200 --movetime 50 --threads 4
./scripts/ab_test.sh --enable ASPIRATION --games 100

# Manual pieces
make EXTRA_FLAGS="-DDISABLE_TT" && make deploy-test
cd chess-tournament && python3 run_tournament.py -g 100 -t 4 -m1 50 -m2 50 sohilbot_baseline sohilbot_test
```

- `-g` / `GAMES`, `-t` / `THREADS`, `-m` / `MOVETIME` (ms, min 20)
- Starts from random FENs in `chess-tournament/positions`
- Prints W/D/L and Elo difference with 95% CI
- Baseline binary: `chess-tournament/sohilbot_baseline`
- After a clear gain: `make promote-baseline`

Shell note: always `cd` with quoted paths (repo path contains a space); prefer invoking `make ab-test` / `scripts/ab_test.sh` from repo root.

## Architecture

| File | Role |
|------|------|
| `sohilbot.cpp/hpp` | `main`, stdin UCI loop, optional file logging (`LOG_ON`) |
| `commandParser.cpp/hpp` | UCI commands; owns `BitBoard`, `Engine`, `TT` |
| `engine.cpp/hpp` | Iterative deepening αβ search, quiescence, perft |
| `bitboard.cpp/hpp` | Bitboard state, move gen, make-move, move ordering |
| `evaluate.hpp` | Static evaluation (header-only) |
| `transpositionTables.cpp/hpp` | Zobrist TT + history heuristic table |
| `defines.hpp` | Feature flags, constants, piece values, PSTs |
| `perftTests.hpp` | Built-in perft suite (`test` UCI command) |
| `scripts/ab_test.sh` | One-shot build + tournament helper |

**Data flow:** stdin → `SohilBot` → `CommandParser` → `Engine::searchBestMove` → `BitBoard` + `Evaluate` + `TT`.

Board is copy-make (save `oldboard`, restore after recurse). Illegal moves filtered by checking whether the mover’s king is in check after the move.

## Feature flags (`defines.hpp`)

Defaults ON features are wrapped as `#ifndef DISABLE_<NAME>`. Override without editing the file:

| Flag | Default | Disable / enable |
|------|---------|------------------|
| `ENABLE_NULL_MOVE` | on | `-DDISABLE_NULL_MOVE` |
| `ENABLE_LMR` | on | `-DDISABLE_LMR` |
| `ENABLE_QUIESCE` | on | `-DDISABLE_QUIESCE` |
| `ENABLE_TT` | on | `-DDISABLE_TT` |
| `ENABLE_CONTEMPT` | on | `-DDISABLE_CONTEMPT` |
| `ENABLE_ASPIRATION` | off | `-DENABLE_ASPIRATION` |
| `HISTORY_HEURISTIC` | off | `-DHISTORY_HEURISTIC` |
| `ASSERT_ON` / `SEARCH_STATS_ON` / `LOG_ON` | off | `-DASSERT_ON` etc. |

Also in `defines.hpp`: piece MG/EG values, PSTs, LMR cutoffs (`LATE_MOVE_CUTOFF`, `REDUCE1`/`REDUCE2`), `ENDGAME_CUTOFF` (60 halfmoves), mate helpers, TT size (`TT_SIZE_LOG2`).

## Search (`engine.cpp`)

1. Iterative deepening from depth 1 → `depth` (or until time / mate)
2. Optional aspiration window around previous iter eval
3. αβ with MultiPV at root (`numPvs`, UCI `MultiPV`)
4. TT probe (PV/ALL/CUT); TT move ordered first
5. Null-move prune when safe
6. Move loop: LMR on late quiet non-check moves; re-search full depth if fails high
7. Leaf → `quiesce` (static eval stand-pat + captures)
8. Check extension near horizon; stalemate = 0; mate = `-MATE(depth)`
9. 3-fold: short history ring (4 hashes) on the board

Time: `go movetime` / clock (`timeLeft/50 + inc - TIME_BUFFER`). Soft stop checked at leaves.

## Evaluation (`evaluate.hpp`)

Side-to-move relative: material, tempo, mobility/scope, blended PSTs, middlegame king-safety penalty. Connected/passed pawn stubs exist but are commented out.

## Board / moves (`bitboard`)

Separate bitboards per piece×color; incremental Zobrist; move ordering via TT move + `estimateMoveValue` [+ history if enabled]. Squares: a1=0 … h8=63.

## UCI surface

Standard: `uci`, `isready`, `ucinewgame`, `position startpos|fen …`, `go …`, `stop`, `quit`, `setoption name MultiPV`.

Debug/dev: `move`, `moves`, `captures`, `perft <d>`, `test` (perft suite), `eval`, `debug on|off`.

## Conventions for agents

- Prefer `make ab-test DISABLE=...` for strength experiments; use `test`/`perft` for correctness
- Keep default flags in `defines.hpp` as the “production” config; experiment via `EXTRA_FLAGS` / `DISABLE` / `ENABLE`
- Match existing style: C++17, bitboard-centric, copy-make search, header-only eval
- Ignore `lichess-bot-master/`, `Old files/`, and `logs/` unless asked
