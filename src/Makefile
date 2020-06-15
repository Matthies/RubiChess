#
# Makefile to compile RubiChess in a standard GNU/Makefile environment
#

MAKEFLAGS += --no-print-directory


ifeq ($(shell uname -m),x86_64)
	# BMI2-build
	BMI2EXE=RubiChess-BMI2
	BMI2CPUFEATURE=-D CPUFEATURE=CPUBMI2
	BMI2ARCHFLAGS=-msse3 -mpopcnt -mbmi2

	# Popcount-build
	POPCOUNTEXE=RubiChess
	POPCOUNTCPUFEATURE=-D CPUFEATURE=CPUPOPCOUNT
	POPCOUNTARCHFLAGS=-msse3 -mpopcnt

	# Legacy-build
	LEGACYEXE=RubiChess-Legacy
	LEGACYCPUFEATURE=-D CPUFEATURE=CPULEGACY
	LEGACYARCHFLAGS=
endif

ifeq ($(shell uname -m),armv7l)
	# Popcount-build
	POPCOUNTEXE=RubiChess
	POPCOUNTCPUFEATURE=-D CPUFEATURE=CPUPOPCOUNT
	POPCOUNTARCHFLAGS=-march=armv7-a
endif


ifeq ($(COMP),)
	COMP=gcc
endif

ifeq ($(EXE),)
	EXE=RubiChess
endif

CXXFLAGS=-std=c++11 -Wall -O3 -flto

ifeq ($(COMP),gcc)
	CXX=g++
	CXXFLAGS += -pedantic -Wextra -Wshadow
	LDFLAGS=-pthread
	profile_make = gcc-profile-make
endif

ifeq ($(COMP),clang)
	CXX=clang++
	CXXFLAGS += -pedantic -Wextra -Wshadow
	LDFLAGS=-lpthread -fuse-ld=lld
	profile_make = clang-profile-make
endif

ifeq ($(COMP),mingw)
	CXX=g++
	CXXFLAGS += -Wextra -Wshadow
	LDFLAGS += -static
	profile_make = gcc-profile-make
endif

DEPS = RubiChess.h
PROFDIR = OPT

GITVER = $(shell 2>/dev/null git show --name-only --abbrev-commit --date=format:%Y%m%d%H%M%S | grep -i "date" | grep -o -E '[0-9]+')
GITID = $(shell 2>/dev/null git show --name-only --abbrev-commit | grep -i -o -E "ommit[[:blank:]]+[0-9a-f]+" | grep -o -E '[0-9a-f]+')
ifneq ($(GITVER),)
	GITDEFINE = -D GITVER=\"$(GITVER)\"
endif
ifneq ($(GITID),)
	GITDEFINE += -D GITID=\"$(GITID)\"
endif

.PHONY: clean profile-build gcc-profile-make clang-profile-make all

default: clean
	@$(MAKE) compile ARCHFLAGS="$(POPCOUNTARCHFLAGS)" CPUFEATURE="$(POPCOUNTCPUFEATURE)"

all: RubiChess-BMI2 RubiChess RubiChess-Legacy

compile:
	@echo   \  Compiling $(EXE)...
	$(CXX) $(CXXFLAGS) $(EXTRACXXFLAGS) $(ARCHFLAGS) *.cpp $(LDFLAGS) $(EXTRALDFLAGS) $(GITDEFINE) $(CPUFEATURE) -o $(EXE)

RubiChess-BMI2:
	@$(MAKE) compile ARCHFLAGS="$(BMI2ARCHFLAGS)" EXE=$(BMI2EXE) CPUFEATURE="$(BMI2CPUFEATURE)"

RubiChess:
	@$(MAKE) compile ARCHFLAGS="$(POPCOUNTARCHFLAGS)" EXE=$(POPCOUNTEXE) CPUFEATURE="$(POPCOUNTCPUFEATURE)"

RubiChess-Legacy:
	@$(MAKE) compile ARCHFLAGS="$(LEGACYARCHFLAGS)" EXE=$(LEGACYEXE) CPUFEATURE="$(LEGACYCPUFEATURE)"

objclean:
	$(RM) $(BMI2EXE) $(POPCOUNTEXE) $(LEGACYEXE) *.o

profileclean:
	$(RM) -rf $(PROFDIR)
	$(RM) *.clangprof-raw *.profdata

clean: objclean profileclean

gcc-profile-make:
	@echo Creating profile build for $(PROFEXE)...
	@$(MAKE) $(PROFEXE) EXTRACXXFLAGS='-fprofile-generate=$(PROFDIR)' EXTRALDFLAGS='-lgcov'
	@echo Bench to generate profiling data...
	./$(PROFEXE) -bench > /dev/null
	$(RM) ./$(PROFEXE)
	@echo Creating optimized build for $(PROFEXE)...
	@$(MAKE) $(PROFEXE) EXTRACXXFLAGS='-fprofile-use=$(PROFDIR) -fno-peel-loops -fno-tracer' EXTRALDFLAGS='-lgcov'
	@$(MAKE) profileclean

clang-profile-make:
	@echo Creating profile build for $(PROFEXE)...
	@$(MAKE) $(PROFEXE) EXTRACXXFLAGS='-fprofile-instr-generate=$(PROFEXE).clangprof-raw'
	@echo Bench to generate profiling data...
	./$(PROFEXE) -bench > /dev/null
	llvm-profdata merge -output=$(PROFEXE).profdata $(PROFEXE).clangprof-raw
	$(RM) ./$(PROFEXE)
	@echo Creating optimized build for $(PROFEXE)...
	@$(MAKE) $(PROFEXE) EXTRACXXFLAGS='-fprofile-instr-use=$(PROFEXE).profdata'
	@$(MAKE) profileclean


profile-build: clean
	@if [ "$(POPCOUNTEXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(POPCOUNTEXE); fi
	@if [ "$(BMI2EXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(BMI2EXE); fi
	@if [ "$(LEGACYEXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(LEGACYEXE); fi
