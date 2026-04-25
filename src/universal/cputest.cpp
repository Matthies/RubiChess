/*
  RubiChess is a UCI chess playing engine by Andreas Matthies.

  RubiChess is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  RubiChess is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//#include "RubiChess.h"

#include <cpuid.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <cstring>
#include <iostream>

#define DEFINE_BUILD(x) \
    namespace rubichess_##x { \
        extern int main(int argc, char* argv[]); \
    } \
    extern "C" void (*__start_##x##_init[])(void); \
    extern "C" void (*__stop_##x##_init[])(void); \
    int entry_##x(int argc, char* argv[]) { \
        unsigned count = __stop_##x##_init - __start_##x##_init; \
        for (unsigned i = 0; i < count; i++) \
            __start_##x##_init[i](); \
        return rubichess_##x::main(argc, argv); \
    }

//DEFINE_BUILD(x86_64)
DEFINE_BUILD(x86_64_avx2)
DEFINE_BUILD(x86_64_bmi2)
DEFINE_BUILD(x86_64_avx512)

#if defined(_M_X64) || defined(__amd64)

#if defined _MSC_VER && !defined(__clang_major__)
#include <intrin.h>
#define CPUID(x,i) __cpuid(x, i)
#endif

#if defined(__MINGW64__) || defined(__gnu_linux__) || defined(__clang_major__) || defined(__GNUC__)
#include <cpuid.h>
#define CPUID(x,i) cpuid(x, i)
static void cpuid(int32_t out[4], int32_t x) {
    __cpuid_count(x, 0, out[0], out[1], out[2], out[3]);
}
#endif
#endif

#define CPUVENDORUNKNOWN    0
#define CPUVENDORINTEL      1
#define CPUVENDORAMD        2

#define CPUSSE2     (1 << 0)
#define CPUSSSE3    (1 << 1)
#define CPUPOPCNT   (1 << 2)
#define CPULZCNT    (1 << 3)
#define CPUBMI1     (1 << 4)
#define CPUAVX2     (1 << 5)
#define CPUBMI2     (1 << 6)
#define CPUAVX512   (1 << 7)
#define CPUNEON     (1 << 8)
#define CPUARM64    (1 << 9)
#define CPUDOTPROD  (1 << 10)


uint64_t GetSystemInfo_x86_64()
{
    uint64_t machineSupports = 0ULL;
    int cpuVendor;
    int cpuFamily = 0;
    int cpuModel = 0;

    // shameless copy from MSDN example explaining __cpuid
    char CPUBrandString[0x40];
    char CPUString[0x10];
    int CPUInfo[4] = { -1 };

    unsigned    nIds, nExIds, i;

    CPUID(CPUInfo, 0);

#if 0
    memset(CPUString, 0, sizeof(CPUString));
    memcpy(CPUString, &CPUInfo[1], 4);
    memcpy(CPUString + 4, &CPUInfo[3], 4);
    memcpy(CPUString + 8, &CPUInfo[2], 4);
#endif    
    if (CPUInfo[1] == 0x68747541 && CPUInfo[3] == 0x69746e65 && CPUInfo[2] == 0x444d4163)  // "AuthenticAMD"
    {
        std::cout << "detected AMD CPU" << std::endl;
        cpuVendor = CPUVENDORAMD;
    }
    else if (CPUInfo[1] == 0x756e6547 && CPUInfo[3] == 0x49656e69 && CPUInfo[2] == 0x6c65746e)  // "GenuineIntel"
    {
        std::cout << "detected Intel CPU" << std::endl;
        cpuVendor = CPUVENDORINTEL;
    }
    else
        cpuVendor = CPUVENDORUNKNOWN;

    nIds = CPUInfo[0];
    // Get the information associated with each valid Id
    // https://www.sandpile.org/x86/cpuid.htm
    // https://en.wikichip.org/wiki/amd/cpuid
    // https://en.wikichip.org/wiki/intel/cpuid
    for (i = 0; i <= nIds; ++i)
    {
        CPUID(CPUInfo, i);
        // Interpret CPU feature information.
        if (i == 1)
        {
            cpuFamily = ((CPUInfo[0] & (0xf << 8)) >> 8) + ((CPUInfo[0] & (0xff << 20)) >> 20);
            cpuModel = ((CPUInfo[0] & (0xf << 16)) >> 12) + ((CPUInfo[0] & (0xf << 4)) >> 4);
            if (CPUInfo[3] & (1 << 26)) machineSupports |= CPUSSE2;
            if (CPUInfo[2] & (1 << 23)) machineSupports |= CPUPOPCNT;
            if (CPUInfo[2] & (1 << 9)) machineSupports |= CPUSSSE3;
        }

        if (i == 7)
        {
            if (CPUInfo[1] & (1 << 3)) machineSupports |= CPUBMI1;
            if (CPUInfo[1] & (1 << 8)) machineSupports |= CPUBMI2;
            if (CPUInfo[1] & (1 << 5)) machineSupports |= CPUAVX2;
            if (CPUInfo[1] & ((1 << 16) | (1 << 30))) machineSupports |= CPUAVX512; // AVX512F + AVX512BW needed
        }
    }

    // Calling __cpuid with 0x80000000 as the InfoType argument
    // gets the number of valid extended IDs.
    CPUID(CPUInfo, 0x80000000);
    nExIds = CPUInfo[0];
    //memset(CPUBrandString, 0, sizeof(CPUBrandString));

    // Get the information associated with each extended ID.
    for (i = 0x80000000; i <= nExIds; ++i)
    {
        CPUID(CPUInfo, i);
        // Extended CPU features
        if (i == 0x80000001)
            if (CPUInfo[2] & (1 << 5)) machineSupports |= CPULZCNT;
        // Interpret CPU brand string and cache information.
        if (i == 0x80000002)
            ;//memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            ;//memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            ;//memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }

    if (cpuVendor == CPUVENDORAMD && cpuFamily < 25 && (machineSupports & CPUBMI2))
    {
        // No real BMI2 support on AMD cpu before Zen3
        machineSupports ^= CPUBMI2;
    }
    return machineSupports;
}



int main(int argc, char* argv[]) {
    unsigned _;
    uint64_t machine = GetSystemInfo_x86_64();

    if (machine & CPUAVX512)
        return entry_x86_64_avx512(argc, argv);
    else if (machine & CPUBMI2)
        return entry_x86_64_bmi2(argc, argv);
    else if (machine & CPUAVX2)
        return entry_x86_64_avx2(argc, argv);
    else
        return entry_x86_64_avx2(argc, argv);
}

