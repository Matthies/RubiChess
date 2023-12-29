#
# Makefile to compile RubiChess in a standard GNU/Makefile environment
#

MAKEFLAGS += --no-print-directory

ANDROIDEMU = ~/Android/Sdk/emulator/emulator

ifeq ($(OS), Windows_NT)
	EXEEXT=.exe
else
	EXEEXT=
endif

CPUTEST = cputest
ARCH = native
PROFDIR = OPT
PROFEXE = RubiChess
STRIP = strip

sse2 = no
ssse3 = no
popcnt = no
bmi1 = no
lzcnt = no
avx2 = no
bmi2 = no
avx512 = no
neon = no
arm64 = no
dotprod = no
zlib = no
debug = no
bits = 64

PTHREADLIB=-pthread

UNAME_S=$(shell uname -s)

ifeq ($(UNAME_S),Darwin)
	# Use clang compiler for MacOS
	COMP=clang
endif

ifeq ($(COMP),)
	COMP=gcc
endif

ifeq ($(EXE),)
	EXE=RubiChess
endif

CXXFLAGS=-std=c++11 -Wall -pedantic -Wextra -Wshadow
CFLAGS=-Wall -pedantic -Wextra -Wshadow -Wno-implicit-function-declaration


ifeq ($(debug),yes)
	CXXFLAGS += -g -O0
	CFLAGS += -g -O0
else
	CXXFLAGS += -O3 -flto
	CFLAGS += -O3
endif

ifneq (, $(findstring MINGW64,$(UNAME_S)))
	# Always do a static built with Mingw
	LDFLAGS += -static
endif
ifneq (, $(findstring MINGW32,$(UNAME_S)))
	# Always do a static built with Mingw
	LDFLAGS += -static
	bits = 32
endif
ifeq ($(COMP),$(filter $(COMP),gcc))
	CXX=g++
	# Workaround OB giving wrong CC=g++
	MYCC=gcc
	INSTRUMENTEDEXTRACXXFLAGS='-fprofile-generate=$(PROFDIR)'
	INSTRUMENTEDEXTRALDFLAGS='-lgcov'
	PGOEXTRACXXFLAGS='-fprofile-use=$(PROFDIR) -fno-peel-loops -fno-tracer -Wno-coverage-mismatch -fprofile-correction'
	PGOEXTRALDFLAGS='-lgcov'
	PROFMERGE=
endif

ifeq ($(COMP),$(filter $(COMP), clang ndk icx))
	CXX=clang++
	MYCC=clang
ifeq ($(COMP),$(filter $(COMP), icx))
	CXX=icpx
	MYCC=icx
endif
	LDFLAGS += $(shell type lld 1>/dev/null 2>/dev/null && echo "-fuse-ld=lld")
	INSTRUMENTEDEXTRACXXFLAGS='-fprofile-instr-generate=$(EXE).clangprof-raw'
	INSTRUMENTEDEXTRALDFLAGS=
	PGOEXTRACXXFLAGS='-fprofile-instr-use=$(EXE).profdata'
	PGOEXTRALDFLAGS=
	PROFMERGE=llvm-profdata merge -output=$(EXE).profdata $(EXE).clangprof-raw
endif

ifeq ($(shell uname -m),armv7l)
	# FIXME: Hardcoded -mfpu=neon for Neon support
	CXXFLAGS += -mthumb -march=armv7-a -mfpu=neon
	ARCHFAMILY=arm32
endif

ifeq ($(shell uname -m),aarch64)
	# ARM64... any better flags that work with gcc and clang??
	CXXFLAGS += -mcpu=native
	ARCHFAMILY=arm64
endif

ifeq ($(shell uname -m),x86_64)
	ARCHFAMILY=x86
endif

ifeq ($(COMP),ndk)
	ARCHFAMILY=android
	STRIP=llvm-strip
	AR=llvm-ar
	EXEEXT=
	ifeq ($(ARCH),armv7)
		CXX=armv7a-linux-androideabi19-clang++
		MYCC=armv7a-linux-androideabi19-clang
		CXXFLAGS += -mthumb -march=armv7-a -mfloat-abi=softfp -mfpu=neon
	endif
	ifeq ($(ARCH),armv8)
		CXX=aarch64-linux-android21-clang++
		MYCC=aarch64-linux-android21-clang
	endif
	ifeq ($(ARCH),x86-32)
		CXX=i686-linux-android21-clang++
		MYCC=i686-linux-android21-clang
		CPUFLAGS = "ssse3 sse2"
		bits = 32
	endif
	ifeq ($(ARCH),x86-64)
		CXX=x86_64-linux-android21-clang++
		MYCC=x86_64-linux-android21-clang
		CPUFLAGS = "popcnt lzcnt ssse3 sse2"
	endif
	LDFLAGS += -static-libstdc++
endif


ZLIBDIR=zlib
CXXFLAGS += -Izlib
LDFLAGS += -Lzlib
zlib = yes

ifeq (,$(CPUFLAGS))
ifeq ($(ARCH),native)
ifeq ($(wildcard ./$(CPUTEST)),)
$(shell $(CXX) $(CXXFLAGS) $(LDFLAGS) -DCPUTEST $(CPUTEST).cpp -o $(CPUTEST))
endif
CPUFLAGS = "$(shell ./$(CPUTEST))"
endif

# Common supported x86 and ARM architectures
ifeq ($(findstring x86-32,$(ARCH)),x86-32)
bits = 32
endif
ifeq ($(findstring x86-64,$(ARCH)),x86-64)
bits = 64
endif
ifneq (,$(findstring -sse2,$(ARCH)))
CPUFLAGS = "sse2"
endif
ifneq (,$(findstring -avx512,$(ARCH)))
CPUFLAGS = "avx512 bmi2 avx2 bmi1 lzcnt popcnt ssse3 sse2"
endif
ifneq (,$(findstring -bmi2,$(ARCH)))
CPUFLAGS = "bmi2 avx2 bmi1 lzcnt popcnt ssse3 sse2"
endif
ifneq (,$(findstring -avx2,$(ARCH)))
CPUFLAGS = "avx2 bmi1 lzcnt popcnt ssse3 sse2"
endif
ifneq (,$(findstring -modern,$(ARCH)))
CPUFLAGS = "popcnt ssse3 sse2"
endif
ifneq (,$(findstring -ssse3,$(ARCH)))
CPUFLAGS = "ssse3 sse2"
endif
ifneq (,$(findstring -sse3-popcnt,$(ARCH)))
CPUFLAGS = "popcnt sse2"
endif
ifneq (,$(findstring armv7,$(ARCH)))
CPUFLAGS = "neon"
bits = 32
endif
ifneq (,$(findstring armv8,$(ARCH)))
CPUFLAGS = "neon arm64"
bits = 64
endif
ifneq (,$(findstring -dotprod,$(ARCH)))
CPUFLAGS += " dotprod"
endif

ifneq (,$(findstring avx512,$(CPUFLAGS)))
	avx512 = yes
endif
ifneq (,$(findstring bmi2,$(CPUFLAGS)))
	bmi2 = yes
endif
ifneq (,$(findstring avx2,$(CPUFLAGS)))
	avx2 = yes
endif
ifneq (,$(findstring bmi1,$(CPUFLAGS)))
	bmi1 = yes
endif
ifneq (,$(findstring lzcnt,$(CPUFLAGS)))
	lzcnt = yes
endif
ifneq (,$(findstring popcnt,$(CPUFLAGS)))
	popcnt = yes
endif
ifneq (,$(findstring ssse3,$(CPUFLAGS)))
	ssse3 = yes
endif
ifneq (,$(findstring sse2,$(CPUFLAGS)))
	sse2 = yes
endif
ifneq (,$(findstring neon,$(CPUFLAGS)))
	neon = yes
endif
ifneq (,$(findstring arm64,$(CPUFLAGS)))
	arm64 = yes
endif
ifneq (,$(findstring dotprod,$(CPUFLAGS)))
	dotprod = yes
endif

ifeq ($(neon),no)
	ARCHFLAGS = -m$(bits)
endif
ifeq ($(bits),64)
	CXXFLAGS += -DIS_64BIT
endif

ifeq ($(avx512),yes)
	ARCHFLAGS += -DUSE_AVX512 -mavx512f -mavx512bw
endif
ifeq ($(bmi2),yes)
	ARCHFLAGS += -DUSE_BMI2 -mbmi2
endif
ifeq ($(avx2),yes)
	ARCHFLAGS += -DUSE_AVX2 -mavx2
endif
ifeq ($(bmi1)$(lzcnt),yesyes)
	ARCHFLAGS += -DUSE_BMI1 -mbmi -mlzcnt
endif
ifeq ($(popcnt),yes)
	ARCHFLAGS += -DUSE_POPCNT -mpopcnt -msse3
endif
ifeq ($(ssse3),yes)
	ARCHFLAGS += -DUSE_SSSE3 -mssse3
endif
ifeq ($(sse2),yes)
	ARCHFLAGS += -DUSE_SSE2 -msse2
endif
ifeq ($(neon),yes)
	ARCHFLAGS += -DUSE_NEON
endif
ifeq ($(arm64),yes)
	ARCHFLAGS += -DUSE_ARM64
endif
ifeq ($(dotprod),yes)
	ARCHFLAGS += -DUSE_DOTPROD
endif
ifeq ($(zlib),yes)
	CXXFLAGS += -DUSE_ZLIB
	LDFLAGS += -lz
endif

DEPS = RubiChess.h

GITVER = $(shell 2>/dev/null git show --name-only --abbrev-commit --date=format:%Y%m%d | grep -i "date:" | grep -o -E '[0-9]+')
GITID = $(shell 2>/dev/null git show --name-only --abbrev-commit | grep -i -o -E "ommit[[:blank:]]+[0-9a-f]{6}" | grep -o -E '[0-9a-f]+')
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

NETBIN = net.nnue
ifneq ($(EVALFILE),)
ifeq ($(EVALFILE),default)
	EMBEDFILE = $(RUBINET)
else
	EMBEDFILE = $(EVALFILE)
endif
	NETDEF = -DNNUEINCLUDED=$(EMBEDFILE)
	NETOBJ = net.o
endif

ifeq ($(GITVER),)
	MAJORVERSION = $(shell grep "\#define VERNUMLEGACY " RubiChess.h | awk '{print $$3}')
else
	MAJORVERSION = $(GITVER)
endif
MINORVERSION = ""
VERSION=$(MAJORVERSION)$(MINORVERSION)

.PHONY: clean profile-build gcc-profile-make clang-profile-make net arch compile pgo profile-build pgo-rename release_x86 release_arm32 release_arm64 release

default: net arch
	@$(MAKE) compile MESSAGE='Compiling standard build ...'
ifneq ($(debug),yes)
	@$(STRIP) $(EXE)$(EXEEXT)
endif

build: default

arch: libclean
	@echo
	@echo "Compiler: $(COMP)"
	@echo "Arch: $(ARCH)"
	@echo "Bits: $(bits)"
	@echo "CPU features:"
	@echo "============="
	@echo "avx512 : $(avx512)"
	@echo "bmi2   : $(bmi2)"
	@echo "avx2   : $(avx2)"
	@echo "bmi1   : $(bmi1)"
	@echo "lzcnt  : $(lzcnt)"
	@echo "popcnt : $(popcnt)"
	@echo "ssse3  : $(ssse3)"
	@echo "sse2   : $(sse2)"
	@echo "neon   : $(neon)"
	@echo "arm64  : $(arm64)"
	@echo "dotprod: $(dotprod)"
	@echo "zlib   : $(zlib)"
	@echo "debug  : $(debug)"

net:
ifeq ($(EVALFILE),$(filter $(EVALFILE),default))
ifeq ($(RUBINET),)
		echo "Network not found in header"
else
		@if test -f $(RUBINET); then echo "$(RUBINET) already exists."; else echo "Downloading $(RUBINET)..."; $(WGETCMD) $(NETURL)$(RUBINET) > $(RUBINET); fi;
		$(eval shasum_command := $(shell if hash shasum 2>/dev/null; then echo "shasum -a 256 "; elif hash sha256sum 2>/dev/null; then echo "sha256sum "; fi))
		@if [ "$(RUBINETHASH)" != `$(shasum_command) $(RUBINET) | cut -c1-10` ]; then echo "Failed download or $(RUBINET) corrupted, please delete!"; exit 1; else echo "$(RUBINET) has correct hash."; fi
endif
endif
ifneq ($(EVALFILE),)
		@echo Embedding networkfile $(EMBEDFILE)
		@cp $(EMBEDFILE) $(NETBIN)
		@ld -r -b binary $(NETBIN) -o $(NETOBJ)
endif

$(ZLIBDIR)/libz.a:
	@echo Compiling zlib...
	@cd $(ZLIBDIR); $(MYCC) $(CFLAGS) $(ARCHFLAGS) -w -c *.c; $(AR) rcs libz.a *.o; cd ..

compile: $(ZLIBDIR)/libz.a
	@echo $(MESSAGE)
	$(CXX) $(CXXFLAGS) $(EXTRACXXFLAGS) $(ARCHFLAGS) *.cpp $(LDFLAGS) $(PTHREADLIB) $(EXTRALDFLAGS) $(GITDEFINE) $(NETDEF) $(NETOBJ) -o $(EXE)

objclean:
	@$(RM) *.o $(AVX512EXE) $(BMI2EXE) $(AVX2EXE) $(DEFAULTEXE) $(SSSE3EXE) $(SSE2POPCNTEXE) $(LEGACYEXE) $(PROFEXE) $(CPUTEST) $(NETBIN) || @echo $(RM) not available.

libclean:
	@$(RM) zlib/*.o zlib/*.a || @echo $(RM) not available.

profileclean: libclean
	@$(RM) -rf $(PROFDIR) || @echo $(RM) not available.
	@$(RM) *.clangprof-raw *.profdata || @echo $(RM) not available.

clean: objclean profileclean

pgo: arch
	@$(MAKE) compile EXTRACXXFLAGS=$(INSTRUMENTEDEXTRACXXFLAGS) EXTRALDFLAGS=$(INSTRUMENTEDEXTRALDFLAGS) EXE=$(PROFEXE) MESSAGE='Compiling instrumented build ...'
ifeq ($(COMP),ndk)
	@if !(test -f $(ANDROIDEMU)); then echo "Android emulator not found at $(ANDROIDEMU). Exit!"; exit 1; fi
	@$(ANDROIDEMU) -avd $(ARCH) -no-snapshot-load -no-qt 1>/dev/null &
	@echo Wait 60 seconds for emulator startup...
	@sleep 60
	@adb shell "rm -rf /data/RubiChess"
	@adb shell "mkdir /data/RubiChess"
	@adb push $(PROFEXE) /data/RubiChess 1>/dev/null
	@adb push $(RUBINET) /data/RubiChess 1>/dev/null
	@echo "Running bench on emulator to generate profiling data..."
	@adb shell "cd /data/RubiChess && ./$(PROFEXE) -bench 1>/dev/null"
	@adb pull /data/RubiChess/$(EXE).clangprof-raw .  1>/dev/null
	@if test -f $(EXE).clangprof-raw; then echo "$(RUBINET) already exists."; else echo "Downloading $(RUBINET)..."; $(WGETCMD) $(NETURL)$(RUBINET) > $(RUBINET); fi;
	@adb emu kill
else
	@echo "Running bench to generate profiling data..." && ./$(PROFEXE) -bench 1>/dev/null && ([ $$? -eq 0 ] && echo " Profiling successful!") \
	|| (echo " Profiling failed!" && [ "$(SDE)" != "" ] && echo " Trying to use SDE..." && $(SDE) -icx -- ./$(PROFEXE) -bench 1>/dev/null && [ $$? -eq 0 ] && echo " Profiling successful!") \
	|| (echo " SDE not available or profiling with SDE failed! Profiling with native build..." && $(MAKE) profileclean && $(MAKE) compile ARCH=native EXTRACXXFLAGS=$(INSTRUMENTEDEXTRACXXFLAGS) EXTRALDFLAGS=$(INSTRUMENTEDEXTRALDFLAGS) EXE=$(PROFEXE) MESSAGE='Compiling instrumented build ...' && ./$(PROFEXE) -bench 1>/dev/null)
endif
	@$(PROFMERGE)
	@$(RM) ./$(PROFEXE)
	@$(MAKE) compile EXTRACXXFLAGS=$(PGOEXTRACXXFLAGS) EXTRALDFLAGS=$(PGOEXTRALDFLAGS) MESSAGE='Compiling optimized build ...'
	@$(MAKE) profileclean
ifneq ($(debug),yes)
	@$(STRIP) $(EXE)$(EXEEXT)
endif
	@echo Binary $(EXE) created successfully.

profile-build: net
	@$(MAKE) pgo

pgo-rename: pgo
	@mv $(EXE) $(EXE)-$(VERSION)_$(ARCH)
	@echo Successfully created $(EXE)-$(VERSION)_$(ARCH)

release_x86:
	@$(MAKE) pgo-rename  ARCH=x86-$(bits)-avx512
	@$(MAKE) pgo-rename  ARCH=x86-$(bits)-bmi2
	@$(MAKE) pgo-rename  ARCH=x86-$(bits)-avx2
	@$(MAKE) pgo-rename  ARCH=x86-$(bits)-modern
	@$(MAKE) pgo-rename  ARCH=x86-$(bits)-ssse3
	@$(MAKE) pgo-rename  ARCH=x86-$(bits)-sse3-popcnt
	@$(MAKE) pgo-rename  ARCH=x86-$(bits)-sse2
	@$(MAKE) pgo-rename  ARCH=x86-$(bits)

release_arm64:
	@$(MAKE) pgo-rename  ARCH=armv8

release_arm32:
	@$(MAKE) pgo-rename  ARCH=armv7

release_android:
	@$(MAKE) pgo-rename  ARCH=armv8 COMP=$(COMP)
	@$(MAKE) pgo-rename  ARCH=armv7 COMP=$(COMP)
	@$(MAKE) pgo-rename  ARCH=x86-64 COMP=$(COMP)
	@$(MAKE) pgo-rename  ARCH=x86-32 COMP=$(COMP)

release: net
	@$(MAKE) release_$(ARCHFAMILY)

help:
	@echo ""
	@echo "Compile RubiChess with following command:"
	@echo "make target [ARCH=arch] [COMP=compiler] [EVALFILE=networkfile|default]"
	@echo "Supported targets:"
	@echo "build             standard build (default target)"
	@echo "profile-build     profiling optimized build (pgo)"
	@echo "release           build all pgo optimized binaries CPU family"
	@echo ""
	@echo "ARCH should only be set when building a binary for a hardware different from the host"
	@echo "COMP can be gcc (default) or clang (usually faster but has some more dependencies)"
	@echo "Setting EVALFILE will build binaries with network included"
	@echo ""
