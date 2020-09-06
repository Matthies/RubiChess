#
# Makefile to compile RubiChess in a standard GNU/Makefile environment
#

MAKEFLAGS += --no-print-directory


ifeq ($(shell uname -m),x86_64)
	# BMI2-build
	BMI2EXE=RubiChess-BMI2
	BMI2CPUFEATURE=-DUSE_BMI2 -DUSE_AVX2 -DUSE_SSSE3 -DUSE_SSE2 -DUSE_MMX -DUSE_POPCNT
	BMI2ARCHFLAGS=-mbmi2 -mavx2 -mssse3 -msse2 -mmmx -mpopcnt

	# AVX2-build
	AVX2EXE=RubiChess-AVX2
	AVX2CPUFEATURE=-DUSE_AVX2 -DUSE_SSSE3 -DUSE_SSE2 -DUSE_MMX -DUSE_POPCNT
	AVX2ARCHFLAGS=-mavx2 -mssse3 -msse2 -mmmx -mpopcnt

	# Modern-build
	MODERNEXE=RubiChess
	MODERNCPUFEATURE=-DUSE_SSSE3 -DUSE_SSE2 -DUSE_MMX -DUSE_POPCNT
	MODERNARCHFLAGS=-mssse3 -msse2 -mmmx -mpopcnt

	# Legacy-build
	LEGACYEXE=RubiChess-Legacy
	LEGACYCPUFEATURE=
	LEGACYARCHFLAGS=
endif

ifeq ($(shell uname -m),armv7l)
	# Popcount-build
	MODERNEXE=RubiChess
	MODERNCPUFEATURE=-DUSE_NEON
	MODERNARCHFLAGS=-mthumb -march=armv7-a -mfpu=neon
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

GITVER = $(shell 2>/dev/null git show --name-only --abbrev-commit --date=format:%Y%m%d%H%M%S | grep -i "date:" | grep -o -E '[0-9]+')
GITID = $(shell 2>/dev/null git show --name-only --abbrev-commit | grep -i -o -E "ommit[[:blank:]]+[0-9a-f]+" | grep -o -E '[0-9a-f]+')
ifneq ($(GITVER),)
	GITDEFINE = -D GITVER=\"$(GITVER)\"
endif
ifneq ($(GITID),)
	GITDEFINE += -D GITID=\"$(GITID)\"
endif

.PHONY: clean profile-build gcc-profile-make clang-profile-make all

default: clean
	@$(MAKE) compile ARCHFLAGS="$(MODERNARCHFLAGS)" CPUFEATURE="$(MODERNCPUFEATURE)"

all: RubiChess-BMI2 RubiChess-AVX2 RubiChess RubiChess-Legacy

compile:
	@echo   \  Compiling $(EXE)...
	$(CXX) $(CXXFLAGS) $(EXTRACXXFLAGS) $(ARCHFLAGS) *.cpp $(LDFLAGS) $(EXTRALDFLAGS) $(GITDEFINE) $(CPUFEATURE) -o $(EXE)

RubiChess-AVX2:
	@$(MAKE) compile ARCHFLAGS="$(AVX2ARCHFLAGS)" EXE=$(AVX2EXE) CPUFEATURE="$(AVX2CPUFEATURE)"

RubiChess-BMI2:
	@$(MAKE) compile ARCHFLAGS="$(BMI2ARCHFLAGS)" EXE=$(BMI2EXE) CPUFEATURE="$(BMI2CPUFEATURE)"

RubiChess:
	@$(MAKE) compile ARCHFLAGS="$(MODERNARCHFLAGS)" EXE=$(MODERNEXE) CPUFEATURE="$(MODERNCPUFEATURE)"

RubiChess-Legacy:
	@$(MAKE) compile ARCHFLAGS="$(LEGACYARCHFLAGS)" EXE=$(LEGACYEXE) CPUFEATURE="$(LEGACYCPUFEATURE)"

objclean:
	$(RM) $(BMI2EXE) $(AVX2EXE) $(MODERNEXE) $(LEGACYEXE) *.o

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
	@if [ "$(BMI2EXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(BMI2EXE); fi
	@if [ "$(AVX2EXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(AVX2EXE); fi
	@if [ "$(MODERNEXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(MODERNEXE); fi
	@if [ "$(LEGACYEXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(LEGACYEXE); fi
