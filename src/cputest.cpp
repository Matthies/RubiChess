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


#include "RubiChess.h"

#ifdef CPUTEST

const string strCpuFeatures[] = STRCPUFEATURELIST;

compilerinfo::compilerinfo()
{
    GetSystemInfo();
}

string compilerinfo::SystemName()
{
    return system;
}


#if defined(_M_X64) || defined(__amd64)

#if defined _MSC_VER && !defined(__clang_major__)
#define CPUID(x,i) __cpuid(x, i)
#endif

#if defined(__MINGW64__) || defined(__gnu_linux__) || defined(__clang_major__) || defined(__GNUC__)
#include <cpuid.h>
#define CPUID(x,i) cpuid(x, i)
static void cpuid(int32_t out[4], int32_t x) {
    __cpuid_count(x, 0, out[0], out[1], out[2], out[3]);
}
#endif


string compilerinfo::PrintCpuFeatures(U64 f, bool onlyHighest)
{
    string s = "";
    for (int i = 0; f; i++, f = f >> 1)
        if (f & 1) s = (onlyHighest ? "" : ((s != "") ? s + " " : "")) + strCpuFeatures[i];

    return s;
}

void compilerinfo::GetSystemInfo()
{
    machineSupports = 0ULL;

    // shameless copy from MSDN example explaining __cpuid
    char CPUBrandString[0x40];
    char CPUString[0x10];
    int CPUInfo[4] = { -1 };

    unsigned    nIds, nExIds, i;

    CPUID(CPUInfo, 0);

    memset(CPUString, 0, sizeof(CPUString));
    memcpy(CPUString, &CPUInfo[1], 4);
    memcpy(CPUString + 4, &CPUInfo[3], 4);
    memcpy(CPUString + 8, &CPUInfo[2], 4);

    string vendor = string(CPUString);

    if (vendor == "GenuineIntel")
        cpuVendor = CPUVENDORINTEL;
    else if (vendor == "AuthenticAMD")
        cpuVendor = CPUVENDORAMD;
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
            if (CPUInfo[1] & (1 << 8)) machineSupports |= CPUBMI2;
            if (CPUInfo[1] & (1 << 5)) machineSupports |= CPUAVX2;
            if (CPUInfo[1] & ((1 << 16) | (1 << 30))) machineSupports |= CPUAVX512; // AVX512F + AVX512BW needed
        }
    }

    // Calling __cpuid with 0x80000000 as the InfoType argument
    // gets the number of valid extended IDs.
    CPUID(CPUInfo, 0x80000000);
    nExIds = CPUInfo[0];
    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    // Get the information associated with each extended ID.
    for (i = 0x80000000; i <= nExIds; ++i)
    {
        CPUID(CPUInfo, i);

        // Interpret CPU brand string and cache information.
        if (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }

    system = CPUBrandString;
    system += "  Family: " + to_string(cpuFamily) + "  Model: " + to_string(cpuModel);

    U64 notSupported = binarySupports & ~machineSupports;

    if (notSupported)
    {
        cout << "info string Error! Binary is not compatible with this machine. Missing cpu features:";
        cout << PrintCpuFeatures(notSupported) << ". Please use correct binary.\n";
        exit(0);
    }

    if (cpuVendor == CPUVENDORAMD && cpuFamily < 25 && (machineSupports & CPUBMI2))
    {
        // No real BMI2 support on AMD cpu before Zen3
        machineSupports ^= CPUBMI2;
        if (binarySupports & CPUBMI2)
            cout << "info string Warning! You are running the BMI2 binary on an AMD cpu which is known for bad performance. Please use the different binary for best performance.\n";
    }

    U64 supportedButunused = machineSupports & ~binarySupports;
    if (supportedButunused)
    {
        cout << "info string Warning! Binary not optimal for this machine. Unused cpu features:";
        cout << PrintCpuFeatures(supportedButunused) << ". Please use correct binary for best performance.\n";
    }
}

#else
void compilerinfo::GetSystemInfo()
{
#ifdef __ARM_NEON__
    system = "ARM platform supprting NEON";
    machineSupports = CPUNEON;
#else
    system = "Some non-x86-64 platform.";
    machineSupports = 0ULL;
#endif
}

string compilerinfo::PrintCpuFeatures(U64 f, bool onlyHighest)
{
    return onlyHighest ? "" : "unknown";
}
#endif


int main()
{
    compilerinfo ci;
    ci.GetSystemInfo();
    ci.PrintCpuFeatures(ci.machineSupports);
}

#endif