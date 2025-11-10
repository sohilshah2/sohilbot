# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -g 
LDFLAGS = -pthread

# Source files
SRCS = sohilbot.cpp commandParser.cpp engine.cpp bitboard.cpp transpositionTables.cpp
OBJS = $(SRCS:.cpp=.o)
HEADERS = bitboard.hpp evaluate.hpp defines.hpp perftTests.hpp

# Target executable
TARGET = sohilbot

# Default target
all: $(TARGET)

# Debug target
debug: CXXFLAGS = -DASSERT_ON -std=c++17 -O0 -g3 -Wall -Wextra
debug: clean $(TARGET)

# Linking
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compilation
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean debug 