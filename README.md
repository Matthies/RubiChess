# RubiChess
Just another UCI compliant chess engine. Have a look at the ChangeLog for a detailed feature list.

'UCI compliant' means that for best user experience you need a chess GUI like Arena, CuteChess or BanksiaGUI (just to name some free programs) and install RubiChess as an engine in this GUI.

RubiChess development started in 2016 as a private hobby project to practise programming in C++ and to see the engine improving compared
to earlier releases. Meanwhile some years later RubiChess got pretty competitive and is listed in most of the rankings and plays
a lot of even big tournaments.

I'm still not very good in C++ using a C-style code most of the time but the whole project was and is a lot of fun.

Many thanks to the excellent documentation at https://chessprogramming.org.
Also many thanks to Bluefever and his video tutorial https://www.youtube.com/user/BlueFeverSoft/videos
A special thank you goes to open source engine Olithink. I had a look at its source code or even two.
And while improving RubiChess more and more I looked at several open source engines like
Ethereal, Stockfish, Pirarucu, Laser, Koivisto, Berserk, ...
Thank you for the great list of engines at http://www.computerchess.org.uk/ccrl/4040/
Not mentioned all the other documentation and tools freely available.

Also a big thank you goes to the guys at http://chess.grantnet.us/ especially Andrew Grant for running and improving this testing framework and to Bojun Guo (noobpwnftw) for spending all the hardware resources for testing.
## NNUE
Starting with version 1.9 RubiChess supports evaluation using NNUE weight files. With version 2.0 NNUE evaluation becomes the default.

Disable the 'Use_NNUE' option for so called handcrafted evaluation.

Use the 'NNUENetpath' option to switch to a different network weight file.

You can download network files from my repository https://github.com/Matthies/NN and put it in the same folder as the executable.

Current default net will be downloaded automatically when compiling the engine and is also included in Windows release packages.

## Binaries and hints to build some
I provide release binary packages for Windows x64 only. Depending on the type of your x86-64 CPU you can choose from
- __RubiChess-x86-64-avx512__: For best performance on new Intel CPUs supporting the AVX512 extensions.
- __RubiChess-x86-64-bmi2__: For best performance on modern Intel CPUs and AMD Ryzen starting from Zen3/5?00X CPU
- __RubiChess-x86-64-avx2__: For best performance on modern AMD Ryzen Zen/Zen2
- __RubiChess-x86-64-modern__: For older CPUs that support POPCNT but no AVX2
- __RubiChess-x86-64-ssse3__: For even older CPUs with SSSE3 but no POPCNT
- __RubiChess-x86-64-sse3-popcount__: For old AMD CPUs supporting POPCNT and SSE3 but no SSSE3 like Phenom II
- __RubiChess-x86-64-sse2__: This should run on even oldest x86-64 CPU
- __RubiChess-x86-64__: Native and slowest build without SIMD support, just for debugging purposes

You will get a warning at startup if the selected binary doesn't match your CPU or it will just crash.

RubiChess should build successfully on any x64 Linux, on MacOS (x64 and ARM64/M1) and on Raspbian (at least up to Raspi 3 and 4 which I own and tested) using ```make``` from inside the src subfolder.

For fastest binaries you should use the Intel icx compiler (based on Clang/LLVM but with Intel's optimizations) and the following build command

```make profile-build COMP=icx```

or native Clang which is a little bit slower:

```make profile-build COMP=clang```

You may need to install some additional packages like the Intel compiler (https://www.intel.com/content/www/us/en/developer/articles/tool/oneapi-standalone-components.html#dpcpp-cpp) or clang, lld and llvm and add bin paths of compiler and profiler to your PATH to make this work.

You can also use the (default) gcc/g++ compiler ```make profile-build``` which probably works without additional packages but the binaries will be a little bit slower.

Some more notes about compiling (for me and maybe others):

- For a profile-build in MacOS (Darwin) you have to include the folder containing the llvm-profdata tool in your PATH:

```export PATH=$PATH:/Library/Developer/CommandLineTools/usr/bin/```

- For a build using the Intel icx compiler (unzipped to your home folder) add two folders to your path

```export PATH=~/intel/oneapi/compiler/latest/linux/bin/:~/intel/oneapi/compiler/latest/linux/bin-llvm/:$PATH```

- By default a 'native' binary is compiled by detecting the CPU features of the computer. If you want to create a binary with a special set of CPU features, append ARCH=x86-64-... parameter to make. For a list of supported archs look above.

- For profiling an arch not supported by your CPU (like x86-64-avx512 in my case), you can use Intel's SDE by giving SDE=/path/to/sde to the make process:

```make release COMP=icx SDE=~/sde-external-9.14.0-2022-10-25-lin/sde```

