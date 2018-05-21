CXX=g++
CXXFLAGS=-g -std=c++11 -Wall -O3 $(EXTRACXXFLAGS)
LDFLAGS=-pthread  $(EXTRALDFLAGS)

profile_make = gcc-profile-make
profile_use = gcc-profile-use


ifeq ($(OS),Windows_NT)
    LDFLAGS += -static
endif
DEPS = RubiChess.h
OBJBOOARD88 = board-Board88.o eval-Board88.o main-Board88.o search-Board88.o transposition-Board88.o uci-Board88.o utils-Board88.o
OBJBITBOARD = board-Bitboard.o eval-Bitboard.o main-Bitboard.o search-Bitboard.o transposition-Bitboard.o uci-Bitboard.o utils-Bitboard.o tbprobe-Bitboard.o
EXE1 = RubiChess-Bitboard
EXE2 = RubiChess-Board88
PGOBENCH1 = ./$(EXE1) -bench
PGOBENCH2 = ./$(EXE2) -bench

.PHONY: clean profile-build gcc-profile-make gcc-profile-use all

%-Board88.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(EXTRACXXFLAGS)

%-Bitboard.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(EXTRACXXFLAGS) -DBITBOARD

all: RubiChess-Bitboard

RubiChess-Board88: $(OBJBOOARD88)
	$(CXX) -o $@ $^ $(LDFLAGS) $(EXTRALDFLAGS)

RubiChess-Bitboard: $(OBJBITBOARD)
	$(CXX) -o $@ $^ $(LDFLAGS) $(EXTRALDFLAGS)

objclean:
	$(RM) RubiChess-Bitboard RubiChess-Board88 *.o

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
	# $(PGOBENCH2) > /dev/null
	@echo "Building optimized executable ..."
	$(MAKE) objclean
	$(MAKE) $(profile_use)
	@echo "Deleting profile data ..."
	$(MAKE) profileclean



