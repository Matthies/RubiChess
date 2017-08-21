CXX=g++
CXXFLAGS=-g -std=c++11 -Wall -O3
LDFLAGS=-pthread 
DEPS = RubiChess.h
OBJ = board.o main.o search.o transposition.o uci.o utils.o

%.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

RubiChess: $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -r RubiChess *.o

