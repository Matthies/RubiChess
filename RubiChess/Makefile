CXX=g++
CXXFLAGS=-g -std=c++11 -Wall -O3
LDFLAGS=-pthread
ifeq ($(OS),Windows_NT)
    LDFLAGS += -static
endif
DEPS = RubiChess.h
OBJBOOARD88 = board-Board88.o eval-Board88.o main-Board88.o search-Board88.o transposition-Board88.o uci-Board88.o utils-Board88.o
OBJBITBOARD = board-Bitboard.o eval-Bitboard.o main-Bitboard.o search-Bitboard.o transposition-Bitboard.o uci-Bitboard.o utils-Bitboard.o

%-Board88.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

%-Bitboard.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS) -DBITBOARD

all: RubiChess-Board88 RubiChess-Bitboard

RubiChess-Board88: $(OBJBOOARD88)
	$(CXX) -o $@ $^ $(LDFLAGS)

RubiChess-Bitboard: $(OBJBITBOARD)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) RubiChess-Bitboard RubiChess-Board88 *.o

