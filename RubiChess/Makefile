CXX=g++
CXXFLAGS=-g -std=c++11 -Wall -O3 $(EXTRACXXFLAGS)
POPCNTFLAGS=-msse3 -mpopcnt
LDFLAGS=-pthread  $(EXTRALDFLAGS)

profile_make = gcc-profile-make
profile_use = gcc-profile-use


ifeq ($(OS),Windows_NT)
    LDFLAGS += -static
endif
DEPS = RubiChess.h
EXE1 = RubiChess-Bitboard
EXE2 = RubiChess-Bitboard-oldcpu
PGOBENCH1 = ./$(EXE1) -bench
PGOBENCH2 = ./$(EXE2) -bench

.PHONY: clean profile-build gcc-profile-make gcc-profile-use all

all: RubiChess-Bitboard RubiChess-Bitboard-oldcpu

RubiChess-Bitboard:
	$(CXX) $(CXXFLAGS) $(EXTRACXXFLAGS) $(POPCNTFLAGS) *.cpp $(LDFLAGS) $(EXTRALDFLAGS) -o RubiChess-Bitboard

RubiChess-Bitboard-oldcpu:
	$(CXX) $(CXXFLAGS) $(EXTRACXXFLAGS) *.cpp $(LDFLAGS) $(EXTRALDFLAGS) -o RubiChess-Bitboard-oldcpu

objclean:
	$(RM) RubiChess-Bitboard RubiChess-Bitboard-oldcpu *.o

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
	$(PGOBENCH2) > /dev/null
	@echo "Building optimized executable ..."
	$(MAKE) objclean
	$(MAKE) $(profile_use)
	@echo "Deleting profile data ..."
	$(MAKE) profileclean
