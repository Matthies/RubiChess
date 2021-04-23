# RubiChess
Just another UCI compliant chess engine. Have a look at the ChangeLog for a detailed feature list.

This started in 2016 as a private hobby project to practise programming in C++ and to see the engine improving compared
to earlier releases. Meanwhile some years later RubiChess got pretty competitive and is listed in most of the rankings and plays
a lot of even big tournaments.

I'm still not very good in C++ using a C-style code most of the time but the whole project was and is a lot of fun.

Many thanks to the excellent documentation at https://chessprogramming.org.
Also many thanks to Bluefever and his video tutorial https://www.youtube.com/user/BlueFeverSoft/videos
A special thank you goes to open source engine Olithink. I had a look at its source code or even two.
And while improving RubiChess more and more I looked at several open source engines like
Ethereal, Stockfish, Pirarucu, Laser, ...
Thank you for the great list of engines at http://www.computerchess.org.uk/ccrl/4040/
Not mentioned all the other documentation and tools freely available.
## NNUE
Starting with version 1.9 RubiChess supports evaluation using NNUE weight files. With version 2.0 NNUE evaluation becomes the default.

Disable the 'Use NNUE' option for so called handcrafted evaluation.

Use the 'NNUENetpath' option to switch to a different network weight file.

You can download network files from my repository https://github.com/Matthies/NN and put it in the same folder as the executable.
Default net (and default value of NNUENetpath) is now nn-cf8c56d366-20210326.nnue which is also included in Windows release package.

## Binaries and hints to build some
I provide release binary packages for Windows x64 only. Depending on the type of your x86-64 CPU you can choose from
1. RubiChess-BMI2: For best performance on modern intel CPUs and probably also new AMD Ryzen Zen3 / 5?00X CPU
2. RubiChess-AVX2: For best performance on modern AMD Ryzen Zen/Zen2
3. RubiChess: For older CPUs that support POPCNT but no AVX2
4. RubiChess-SSSE3: For even older CPUs with SSSE3 but no POPCNT
5. RubiChess-SSE2POPCNT: For old AMD CPUs supporting POPCNT but no SSSE3 like Phemon II
6. RubiChess-Legacy: For very old x86-64 CPU without SSSE3 support (are there any?)
7. New: RubiChess-AVX512: For best performance on new Intel CPUs supporting the AVX512 extensions. Thanks Ipman for kicking me to implement it.

You will get a warning at startup if the selected binary doesn't match your CPU or it will just crash.

RubiChess should build successfully on any x64 Linux, on MacOS (x64 and ARM64/M1) and on Raspbian (at least up to Raspi 3 and 4 which I own and tested) using ```make``` from inside the src subfolder.

For fastest binaries you should use the Clang compiler and the following build command

```make profile-build COMP=clang```

You may need to install some additional packages like clang, lld and llvm to make this work.
You can also use the (default) gcc/g++ compiler ```make profile-build``` which probably works without additional packages but the binaries will be a little bit slower.

Note 1: For a profile-build in MacOS (Darwin) you have to include the folder containing the llvm-profdata tool in your PATH:
```export PATH=$PATH:/Library/Developer/CommandLineTools/usr/bin/```

Note 2: With current Makefile a profile-build always builds all the different binaries for the x86-64 plattform. Selecting a special one with the ARCH variable is not supported. Maybe later...
