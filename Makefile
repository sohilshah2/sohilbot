# Compiler settings
CXX = g++
# EXTRA_FLAGS: e.g. -DDISABLE_TT -DENABLE_ASPIRATION
EXTRA_FLAGS ?=
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -g $(EXTRA_FLAGS)
LDFLAGS = -pthread

# Source files
SRCS = sohilbot.cpp commandParser.cpp engine.cpp bitboard.cpp transpositionTables.cpp
OBJS = $(SRCS:.cpp=.o)
HEADERS = bitboard.hpp evaluate.hpp defines.hpp perftTests.hpp \
          engine.hpp sohilbot.hpp commandParser.hpp transpositionTables.hpp

# Target executable
TARGET = sohilbot

TOURNAMENT_DIR = chess-tournament
TEST_NAME ?= sohilbot_test
BASELINE_NAME ?= sohilbot_baseline
GAMES ?= 100
THREADS ?= 4
MOVETIME ?= 50

# Convert DISABLE="TT LMR" -> -DDISABLE_TT -DDISABLE_LMR
DISABLE_FLAGS = $(foreach f,$(DISABLE),-DDISABLE_$(f))
ENABLE_FLAGS = $(foreach f,$(ENABLE),-DENABLE_$(f))
AB_FLAGS = $(DISABLE_FLAGS) $(ENABLE_FLAGS) $(EXTRA_FLAGS)

# Default target
all: $(TARGET)

# Debug target
debug: CXXFLAGS = -DASSERT_ON -std=c++17 -O0 -g3 -Wall -Wextra $(EXTRA_FLAGS)
debug: clean $(TARGET)

# Linking
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compilation
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Copy current binary into the tournament directory for A/B testing
deploy-test: $(TARGET)
	mkdir -p $(TOURNAMENT_DIR)/logs
	cp $(TARGET) $(TOURNAMENT_DIR)/$(TEST_NAME)

# Promote current binary to the new baseline (only after a clear Elo gain)
promote-baseline: $(TARGET)
	mkdir -p $(TOURNAMENT_DIR)/logs
	cp $(TARGET) $(TOURNAMENT_DIR)/$(BASELINE_NAME)

# One-shot: rebuild with flag overrides, deploy, run vs baseline.
# Examples:
#   make ab-test DISABLE=TT
#   make ab-test DISABLE="TT LMR" GAMES=200 MOVETIME=50
#   make ab-test ENABLE=ASPIRATION
#   make ab-test DISABLE="QS_SEE QS_DELTA QS_CHECK"
#   make ab-test DISABLE=PVS
ab-test:
	$(MAKE) clean
	$(MAKE) EXTRA_FLAGS="$(AB_FLAGS)"
	$(MAKE) deploy-test
	cd "$(TOURNAMENT_DIR)" && python3 run_tournament.py \
		-g $(GAMES) -t $(THREADS) -m1 $(MOVETIME) -m2 $(MOVETIME) \
		$(BASELINE_NAME) $(TEST_NAME)

# Clean
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean debug deploy-test promote-baseline ab-test
