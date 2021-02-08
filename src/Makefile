#
# Makefile to compile RubiChess in a standard GNU/Makefile environment
#

MAKEFLAGS += --no-print-directory

# Default build (Popcount)
DEFAULTEXE=RubiChess
DEFAULTCPUFEATURE=-DUSE_SSSE3 -DUSE_SSE2 -DUSE_POPCNT
DEFAULTARCHFLAGS=-mssse3 -msse2 -mpopcnt


ifeq ($(shell uname -m),x86_64)
	# BMI2-build
	BMI2EXE=RubiChess-BMI2
	BMI2CPUFEATURE=-DUSE_BMI2 -DUSE_AVX2 -DUSE_SSSE3 -DUSE_SSE2 -DUSE_POPCNT
	BMI2ARCHFLAGS=-mbmi2 -mavx2 -mssse3 -msse2 -mpopcnt

	# AVX2-build
	AVX2EXE=RubiChess-AVX2
	AVX2CPUFEATURE=-DUSE_AVX2 -DUSE_SSSE3 -DUSE_SSE2 -DUSE_POPCNT
	AVX2ARCHFLAGS=-mavx2 -mssse3 -msse2 -mpopcnt

	# SSSE3-build
	SSSE3EXE=RubiChess-SSSE3
	SSSE3CPUFEATURE=-DUSE_SSSE3 -DUSE_SSE2
	SSSE3ARCHFLAGS=-mssse3 -msse2

	# Legacy-build
	LEGACYEXE=RubiChess-Legacy
	LEGACYCPUFEATURE=
	LEGACYARCHFLAGS=
endif

ifeq ($(shell uname -m),armv7l)
	# Default build for Raspi
	DEFAULTEXE=RubiChess
	DEFAULTCPUFEATURE=-DUSE_NEON
	DEFAULTARCHFLAGS=-mthumb -march=armv7-a -mfpu=neon
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

RUBINET = $(shell grep "NNUEDEFAULT " RubiChess.h | awk '{print $$3}')
RUBINETHASH = $(shell echo $(RUBINET) | awk -F'-' '{print $$2}')
NETURL = https://github.com/Matthies/NN/raw/main/
$(eval WGETCMD := $(shell if hash wget 2>/dev/null; then echo "wget -qO-"; elif hash curl 2>/dev/null; then echo "curl -skL"; fi))
ifneq ($(PROXY),)
	WGETCMD += -e https_proxy=$(PROXY)
endif

.PHONY: clean profile-build gcc-profile-make clang-profile-make net all

default:
	@$(MAKE) compile ARCHFLAGS="$(DEFAULTARCHFLAGS)" CPUFEATURE="$(DEFAULTCPUFEATURE)"

all: net RubiChess-BMI2 RubiChess-AVX2 RubiChess RubiChess-SSSE3 RubiChess-Legacy

net:
ifeq ($(RUBINET),)
		echo "Network not found in header"
else
		@if test -f $(RUBINET); then echo "$(RUBINET) already exists."; else echo "Downloading $(RUBINET)..."; $(WGETCMD) $(NETURL)$(RUBINET) > $(RUBINET); fi;
		$(eval shasum_command := $(shell if hash shasum 2>/dev/null; then echo "shasum -a 256 "; elif hash sha256sum 2>/dev/null; then echo "sha256sum "; fi))
		@if [ "$(RUBINETHASH)" != `$(shasum_command) $(RUBINET) | cut -c1-10` ]; then echo "Failed download or $(RUBINET) corrupted, please delete!"; exit 1; fi
endif


compile:
	@echo   \  Compiling $(EXE)...
	$(CXX) $(CXXFLAGS) $(EXTRACXXFLAGS) $(ARCHFLAGS) *.cpp $(LDFLAGS) $(EXTRALDFLAGS) $(GITDEFINE) $(CPUFEATURE) -o $(EXE)

RubiChess-AVX2:
	@$(MAKE) compile ARCHFLAGS="$(AVX2ARCHFLAGS)" EXE=$(AVX2EXE) CPUFEATURE="$(AVX2CPUFEATURE)"

RubiChess-BMI2:
	@$(MAKE) compile ARCHFLAGS="$(BMI2ARCHFLAGS)" EXE=$(BMI2EXE) CPUFEATURE="$(BMI2CPUFEATURE)"

RubiChess:
	@$(MAKE) compile ARCHFLAGS="$(DEFAULTARCHFLAGS)" EXE=$(DEFAULTEXE) CPUFEATURE="$(DEFAULTCPUFEATURE)"

RubiChess-SSSE3:
	@$(MAKE) compile ARCHFLAGS="$(SSSE3ARCHFLAGS)" EXE=$(SSSE3EXE) CPUFEATURE="$(SSSE3CPUFEATURE)"

RubiChess-Legacy:
	@$(MAKE) compile ARCHFLAGS="$(LEGACYARCHFLAGS)" EXE=$(LEGACYEXE) CPUFEATURE="$(LEGACYCPUFEATURE)"

objclean:
	@$(RM) $(BMI2EXE) $(AVX2EXE) $(DEFAULTEXE) $(SSSE3EXE) $(LEGACYEXE) *.o || @echo $(RM) not available.

profileclean:
	@$(RM) -rf $(PROFDIR) || @echo $(RM) not available.
	@$(RM) *.clangprof-raw *.profdata || @echo $(RM) not available.

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


profile-build: clean net
	@if [ "$(BMI2EXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(BMI2EXE); fi
	@if [ "$(AVX2EXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(AVX2EXE); fi
	@if [ "$(DEFAULTEXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(DEFAULTEXE); fi
	@if [ "$(SSSE3EXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(SSSE3EXE); fi
	@if [ "$(LEGACYEXE)" != "" ]; then $(MAKE) $(profile_make) PROFEXE=$(LEGACYEXE); fi
