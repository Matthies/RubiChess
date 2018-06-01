CXX=g++
CXXFLAGS=-g -std=c++11 -Wall -O3 $(EXTRACXXFLAGS)
LDFLAGS=-pthread  $(EXTRALDFLAGS)

profile_make = gcc-profile-make
profile_use = gcc-profile-use


ifeq ($(OS),Windows_NT)
    LDFLAGS += -static
endif
DEPS = RubiChess.h
OBJBITBOARD = board-Bitboard.o eval-Bitboard.o main-Bitboard.o search-Bitboard.o transposition-Bitboard.o uci-Bitboard.o utils-Bitboard.o tbprobe-Bitboard.o
EXE1 = RubiChess-Bitboard
PGOBENCH1 = ./$(EXE1) -bench

.PHONY: clean profile-build gcc-profile-make gcc-profile-use all

%-Board88.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(EXTRACXXFLAGS)

%-Bitboard.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(EXTRACXXFLAGS)

all: RubiChess-Bitboard

RubiChess-Bitboard: $(OBJBITBOARD)
	$(CXX) -o $@ $^ $(LDFLAGS) $(EXTRALDFLAGS)

objclean:
	$(RM) RubiChess-Bitboard *.o

profileclean:
	$(RM) -f *.gcda

clean: objclean profileclean

gcc-profile-make:
	$(MAKE) \
	EXTRACXXFLAGS='-fprofile-generate' \
	EXTRALDFLAGS='-lgcov' \
	all

gcc-profile-use:
	$(MAKE) \
	EXTRACXXFLAGS='-fprofile-use -fno-peel-loops -fno-tracer' \
	EXTRALDFLAGS='-lgcov' \
	all

profile-build: clean
	@echo "Building instrumented executable ..."
	$(MAKE) $(profile_make)
	@echo "Running benchmark for pgo-build ..."
	$(PGOBENCH1) > /dev/null
	@echo "Building optimized executable ..."
	$(MAKE) objclean
	$(MAKE) $(profile_use)
	@echo "Deleting profile data ..."
	$(MAKE) profileclean
