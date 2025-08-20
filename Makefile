BIN     = ./bin
SRC     = problem3/251110004.cpp
TARGET  = $(BIN)/problem3.out

CXX      = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2
LIBS     = -pthread

all: $(TARGET)

$(BIN):
	mkdir -p $(BIN)

$(TARGET): $(SRC) problem3/251110004.h | $(BIN)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)

.PHONY: clean
clean:
	rm -rf $(BIN)

