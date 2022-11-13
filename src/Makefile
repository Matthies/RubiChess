#
# Makefile to compile RubiChess in a standard GNU/Makefile environment
#

MAKEFLAGS += --no-print-directory

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
zlib = no

PTHREADLIB=-pthread
# Use workaround for weak symbols problem in pthreads; https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58909
PTHREADSTATICLIB="-static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive"
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

CXXFLAGS=-std=c++11 -Wall -O3 -flto -pedantic -Wextra -Wshadow
CFLAGS=-Wall -O3 -pedantic -Wextra -Wshadow
ifneq (, $(findstring MINGW64,$(UNAME_S)))
	# Always do a static built with Mingw
	LDFLAGS += -static
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

ifeq ($(COMP),$(filter $(COMP), clang ndk))
	CXX=clang++
	MYCC=clang
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
	CXXFLAGS += -stdlib=libc++ -fPIE
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
	LDFLAGS += -static-libstdc++ -pie -lm -latomic
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

# Common supported x64 and ARM architectures
ifneq (,$(findstring x86-64,$(ARCH)))
CPUFLAGS = "sse2"
endif
ifneq (,$(findstring x86-64-avx512,$(ARCH)))
CPUFLAGS = "avx512 bmi2 avx2 bmi1 lzcnt popcnt ssse3 sse2"
endif
ifneq (,$(findstring x86-64-bmi2,$(ARCH)))
CPUFLAGS = "bmi2 avx2 bmi1 lzcnt popcnt ssse3 sse2"
endif
ifneq (,$(findstring x86-64-avx2,$(ARCH)))
CPUFLAGS = "avx2 bmi1 lzcnt popcnt ssse3 sse2"
endif
ifneq (,$(findstring x86-64-modern,$(ARCH)))
CPUFLAGS = "popcnt ssse3 sse2"
endif
ifneq (,$(findstring x86-64-ssse3,$(ARCH)))
CPUFLAGS = "ssse3 sse2"
endif
ifneq (,$(findstring x86-64-sse3-popcnt,$(ARCH)))
CPUFLAGS = "popcnt sse2"
endif
ifneq (,$(findstring neon,$(ARCH)))
CPUFLAGS = "popcnt neon"
endif
ifneq (,$(findstring arm,$(ARCH)))
CPUFLAGS = "popcnt neon"
endif
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

ARCHFLAGS=
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
ifeq ($(zlib),yes)
	CXXFLAGS += -DUSE_ZLIB
	LDFLAGS += -lz
endif

DEPS = RubiChess.h

GITVER = $(shell 2>/dev/null git show --name-only --abbrev-commit --date=format:%Y%m%d | grep -i "date:" | grep -o -E '[0-9]+')
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
	$(STRIP) $(EXE)$(EXEEXT)

build: default

arch: libclean
	@echo
	@echo "Compiler: $(COMP)"
	@echo "Arch: $(ARCH)"
	@echo "CPU features:"
	@echo "============="
	@echo "avx512: $(avx512)"
	@echo "bmi2  : $(bmi2)"
	@echo "avx2  : $(avx2)"
	@echo "bmi1  : $(bmi1)"
	@echo "lzcnt : $(lzcnt)"
	@echo "popcnt: $(popcnt)"
	@echo "ssse3 : $(ssse3)"
	@echo "sse2  : $(sse2)"
	@echo "neon  : $(neon)"
	@echo "zlib  : $(zlib)"

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
	cd $(ZLIBDIR); $(MYCC) $(CFLAGS) $(ARCHFLAGS) -w -c *.c; $(AR) rcs libz.a *.o; cd ..

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
	@./$(PROFEXE) -bench 1>/dev/null && ([ $$? -eq 0 ] && echo "Running bench to generate profiling data... successful!") || (echo "Running bench to generate profiling data... failed! Profiling with native build..." && $(MAKE) profileclean && $(MAKE) compile ARCH=native EXTRACXXFLAGS=$(INSTRUMENTEDEXTRACXXFLAGS) EXTRALDFLAGS=$(INSTRUMENTEDEXTRALDFLAGS) EXE=$(PROFEXE) MESSAGE='Compiling instrumented build ...' && ./$(PROFEXE) -bench 1>/dev/null)
	@$(PROFMERGE)
	@$(RM) ./$(PROFEXE)
	@$(MAKE) compile EXTRACXXFLAGS=$(PGOEXTRACXXFLAGS) EXTRALDFLAGS=$(PGOEXTRALDFLAGS) MESSAGE='Compiling optimized build ...'
	@$(MAKE) profileclean
	@$(STRIP) $(EXE)$(EXEEXT)
	@echo Binary $(EXE) created successfully.

profile-build: net
	@$(MAKE) pgo

pgo-rename: pgo
	@mv $(EXE) $(EXE)-$(VERSION)_$(ARCH)
	@echo Successfully created $(EXE)-$(VERSION)_$(ARCH)

release_x86:
	@$(MAKE) pgo-rename  ARCH=x86-64-avx512 PTHREADLIB=$(PTHREADSTATICLIB)
	@$(MAKE) pgo-rename  ARCH=x86-64-bmi2 PTHREADLIB=$(PTHREADSTATICLIB)
	@$(MAKE) pgo-rename  ARCH=x86-64-avx2 PTHREADLIB=$(PTHREADSTATICLIB)
	@$(MAKE) pgo-rename  ARCH=x86-64-modern PTHREADLIB=$(PTHREADSTATICLIB)
	@$(MAKE) pgo-rename  ARCH=x86-64-ssse3 PTHREADLIB=$(PTHREADSTATICLIB)
	@$(MAKE) pgo-rename  ARCH=x86-64-sse3-popcnt PTHREADLIB=$(PTHREADSTATICLIB)
	@$(MAKE) pgo-rename  ARCH=x86-64 PTHREADLIB=$(PTHREADSTATICLIB)

release_arm64:
	@$(MAKE) pgo-rename  ARCH=arm64_neon PTHREADLIB=$(PTHREADSTATICLIB)

release_arm32:
	@$(MAKE) pgo-rename  ARCH=arm32_neon PTHREADLIB=$(PTHREADSTATICLIB)

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
