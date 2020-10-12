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


/* A small noncryptographic PRNG */
/* http://www.burtleburtle.net/bob/rand/smallprng.html */

#define rot(x,k) (((x)<<(k))|((x)>>(64-(k))))

U64 ranval(ranctx* x)
{
    U64 e = x->a - rot(x->b, 7);
    x->a = x->b ^ rot(x->c, 13);
    x->b = x->c + rot(x->d, 37);
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

void raninit(ranctx* x, U64 seed)
{
    U64 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i = 0; i < 20; ++i) {
        (void)ranval(x);
    }
}

// Produce a 64-bit material key corresponding to the material combination
// defined by pcs[16], where pcs[1], ..., pcs[6] is the number of white
// pawns, ..., kings and pcs[9], ..., pcs[14] is the number of black
// pawns, ..., kings.
// Again no need to be efficient here.
U64 calc_key_from_pcs(int *pcs, int mirror)
{
    int color;
    PieceType pt;
    int i;
    U64 key = 0;

    color = !mirror ? 0 : 8;
    for (pt = PAWN; pt <= KING; pt++)
        for (i = 0; i < pcs[color + pt]; i++)
            key ^= zb.boardtable[(i << 4) | (pt << 1)];
    color ^= 8;
    for (pt = PAWN; pt <= KING; pt++)
        for (i = 0; i < pcs[color + pt]; i++)
            key ^= zb.boardtable[(i << 4) | (pt << 1) | 1];

    return key;
}


// Order of pieces taken from tbcore!
void getPcsFromStr(const char* str, int *pcs)
{
    for (int i = 0; i < 16; i++)
        pcs[i] = 0;
    int color = 0;
    for (const char *s = str; *s; s++)
        switch (*s) {
        case 'P':
            pcs[PAWN | color]++;
            break;
        case 'N':
            pcs[KNIGHT | color]++;
            break;
        case 'B':
            pcs[BISHOP | color]++;
            break;
        case 'R':
            pcs[ROOK | color]++;
            break;
        case 'Q':
            pcs[QUEEN | color]++;
            break;
        case 'K':
            pcs[KING | color]++;
            break;
        case 'v':
            color = 0x08;
            break;
        }
}


void getFenAndBmFromEpd(string input, string *fen, string *bm, string *am)
{
    *fen = "";
    string f;
    vector<string> fv = SplitString(input.c_str());
    for (int i = 0; i < min(4, (int)fv.size()); i++)
        f = f + fv[i] + " ";

    chessposition *p = &en.sthread[0].pos;
    if (p->getFromFen(f.c_str()) < 0)
        return;

    *fen = f;
    *bm = *am = "";
    bool searchbestmove = false;
    bool searchavoidmove = false;
    string moveliststr;
    for (unsigned int i = 4; i < fv.size(); i++)
    {
        if (searchbestmove || searchavoidmove)
        {
            size_t smk = fv[i].find(';');
            if (smk != string::npos)
                fv[i] = fv[i].substr(0, smk);
            if (moveliststr != "")
                moveliststr += " ";
            moveliststr += AlgebraicFromShort(fv[i], p);
            if (smk != string::npos)
            {
                if (searchbestmove)
                {
                    *bm = moveliststr;
                    searchbestmove = false;
                }
                else if (searchavoidmove)
                {
                    *am = moveliststr;
                    searchavoidmove = false;
                }
            }
        }
        if (strstr(fv[i].c_str(), "bm") != NULL)
        {
            searchbestmove = true;
            moveliststr = "";
        }
        if (strstr(fv[i].c_str(), "am") != NULL)
        {
            searchavoidmove = true;
            moveliststr = "";
        }
    }
}


vector<string> SplitString(const char* c)
{
    string ss(c);
    vector<string> result;
    istringstream iss(ss);
    bool quotes = false;
    string sout;
    for (string s; iss >> s;)
    {
        if (s[0] == '"' && !quotes)
        {
            sout = "";
            quotes = true;
        }

        if (quotes)
            sout += s;
        else
            sout = s;

        if (s[s.length() - 1] == '"' && quotes)
        {
            quotes = false;
        }

        if (!quotes)
            result.push_back(sout);
    }
    return result;
}

string IndexToAlgebraic(int i)
{
    string s;
    s.push_back((char)(FILE(i) + 'a'));
    s.push_back((char)(RANK(i) + '1'));
    return s;
}

unsigned char AlgebraicToIndex(string s)
{
    char file = (char)(s[0] - 'a');
    char rank = (char)(s[1] - '1');

    return (unsigned char)(rank << 3 | file);
}

void BitboardDraw(U64 b)
{
    U64 mask;
    printf("/--------\\\n");
    for (int r = 7; r >= 0; r--)
    {
        printf("|");
        mask = (1ULL << (r * 8));
        for (int f = 0; f < 8; f++)
        {
            if (b & mask)
                printf("x");
            else
                printf(" ");
            mask <<= 1;
        }
        printf("|\n");
    }
    printf("\\--------/\n\n");
}


string AlgebraicFromShort(string s, chessposition *pos)
{
    string retval = "";
    int castle0 = 0;
    PieceType promotion = BLANKTYPE;
    chessmovelist ml;
    pos->prepareStack();
    ml.length = CreateMovelist<ALL>(pos, &ml.move[0]);
    PieceType pt = PAWN;
    int to = 0xc0, from = 0xc0;
    int i = (int)s.size() - 1;
    // Skip check +
    if (i >= 0 && s[i] == '+')
        i--;
    // Castle
    while (i >= 0 && (s[i] == '0' || s[i] == 'O'))
    {
        i -= 2;
        castle0++;
    }
    if (castle0 >= 2)
    {
        pt = KING;
        from = pos->kingpos[pos->state & S2MMASK];
        to = (from & 0x38) | castlerookfrom[castle0 == 2];
    }
    if (i >= 0 && s[i] >= 'A')
    {
        // Promotion
        promotion = GetPieceType(s[i--]);
        if (i >= 0 && s[i] == '=')
            // Skip promotion '='
            i--;
    }
    if (i > 0 && s[i] >= '1' && s[i] <= '8' && s[i - 1] >= 'a' && s[i - 1] <= 'h')
    {
        // Target field
        to = AlgebraicToIndex(&s[i - 1]);
        i -= 2;
    }
    // Skip the capture x
    if (i >= 0 && s[i] == 'x')
        i--;
    // rank or file for ambiguous moves
    if (i >= 0 && s[i] >= '1' && s[i] <= '8')
        from = (from & 0x47) | ((s[i--] - '1') << 3);
    if (i >= 0 && s[i] >= 'a' && s[i] <= 'h')
        from = (from & 0xb8) | (s[i--] - 'a');

    // The Figure
    if (i >= 0 && s[i] >= 'A')
        pt = GetPieceType(s[i--]);

    // i < 0 hopefully
    // get the correct move
    for (i = 0; i < ml.length; i++)
    {
        if (pt == (GETPIECE(ml.move[i].code) >> 1)
            && promotion == (GETPROMOTION(ml.move[i].code) >> 1)
            && ((from & 0x80) || ((from & 0x38) == (GETFROM(ml.move[i].code) & 0x38)))
            && ((from & 0x40) || ((from & 0x07) == (GETFROM(ml.move[i].code) & 0x07)))
            && ((to & 0x80) || ((to & 0x38) == (GETTO(ml.move[i].code) & 0x38)))
            && ((to & 0x40) || ((to & 0x07) == (GETTO(ml.move[i].code) & 0x07))))
        {
            // test if the move is legal; otherwise we need to search further
            if (pos->playMove(&ml.move[i]))
            {
                pos->unplayMove(&ml.move[i]);
                retval = ml.move[i].toString();
                break;
            }
        }
    }
    return retval;
}


#if defined(_M_X64) || defined(__amd64)

#if defined _MSC_VER && !defined(__clang_major__)
#define CPUID(x,i) __cpuid(x, i)
#endif

#if defined(__MINGW64__) || defined(__gnu_linux__) || defined(__clang_major__)
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
    for (i = 0; i <= nIds; ++i)
    {
        CPUID(CPUInfo, i);
        // Interpret CPU feature information.
        if (i == 1)
        {
            if (CPUInfo[3] & (1 << 23)) machineSupports |= CPUMMX;
            if (CPUInfo[3] & (1 << 26)) machineSupports |= CPUSSE2;
            if (CPUInfo[2] & (1 << 23)) machineSupports |= CPUPOPCNT;
            if (CPUInfo[2] & (1 <<  0)) machineSupports |= CPUSSSE3;
        }

        if (i == 7)
        {
            if (CPUInfo[1] & (1 <<  8)) machineSupports |= CPUBMI2;
            if (CPUInfo[1] & (1 <<  5)) machineSupports |= CPUAVX2;
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

    //maxHWSupport = bPOPCNT ? (bBMI2 ? CPUBMI2 : CPUPOPCOUNT) : CPULEGACY;
    system = CPUBrandString;

    U64 notSupported = binarySupports & ~machineSupports;

    if (notSupported)
    {
        cout << "info string Error! Binary is not compatible with this machine. Missing cpu features:";
        cout << PrintCpuFeatures(notSupported) << ". Please use correct binary.\n";
        exit(0);
    }
    
    if (cpuVendor == CPUVENDORAMD && (machineSupports & CPUBMI2))
    {
        // No BMI2 build on AMD cpu
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
    system = "Some non-x86-64 platform.";
}

string compilerinfo::PrintCpuFeatures(U64 f, bool onlyHighest)
{
    return onlyHighest ? "" : "unknown";
}
#endif


#ifdef _WIN32
U64 getTime()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

#else

U64 getTime()
{
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return (U64)(1000000000LL * now.tv_sec + now.tv_nsec);
}

void Sleep(long x)
{
    timespec now;
    now.tv_sec = 0;
    now.tv_nsec = x * 1000000;
    nanosleep(&now, NULL);
}

#endif


#ifdef STACKDEBUG
// Thanks to http://blog.aaronballman.com/2011/04/generating-a-stack-crawl/ for the following stacktracer
void GetStackWalk(chessposition *pos, const char* message, const char* _File, int Line, int num, ...)
{
    va_list args;
    va_start(args, num);
    string values = "Values: ";
    for (int i = 0; i < num; i++)
    {
        values = values + " " + to_string(va_arg(args, int));
    }
    va_end(args);

    ofstream ofile;
    bool bFileAssert = (en.assertfile != "");
    if (bFileAssert)
    {
        ofile.open(en.assertfile, fstream::out | fstream::app);
    }

    cout << "Assertion failed: " + string(message) + ", file " + string(_File) + ", line " + to_string(Line) + "\n";
    cout << values + "\n";
    if (pos)
        pos->print();
    if (bFileAssert)
    {
        ofile << "Assertion failed: " + string(message) + ", file " + string(_File) + ", line " + to_string(Line) + "\n";
        ofile << values + "\n";
        pos->print(&ofile);
    }

    std::string outWalk;
    // Set up the symbol options so that we can gather information from the current
    // executable's PDB files, as well as the Microsoft symbol servers.  We also want
    // to undecorate the symbol names we're returned.  If you want, you can add other
    // symbol servers or paths via a semi-colon separated list in SymInitialized.

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    HANDLE hProcess = GetCurrentProcess();
    if (!SymInitialize(hProcess, NULL, TRUE))
    {
        cout << "info string Cannot initialize symbols.\n";
        return;
    }

    // Capture up to 25 stack frames from the current call stack.  We're going to
    // skip the first stack frame returned because that's the GetStackWalk function
    // itself, which we don't care about.
    PVOID addrs[25] = { 0 };
    USHORT frames = CaptureStackBackTrace(1, 25, addrs, NULL);
    for (USHORT i = 0; i < frames; i++) {
        // Allocate a buffer large enough to hold the symbol information on the stack and get 
        // a pointer to the buffer.  We also have to set the size of the symbol structure itself
        // and the number of bytes reserved for the name.
        ULONG64 buffer[(sizeof(SYMBOL_INFO) + 1024 + sizeof(ULONG64) - 1) / sizeof(ULONG64)] = { 0 };
        SYMBOL_INFO *info = (SYMBOL_INFO *)buffer;
        info->SizeOfStruct = sizeof(SYMBOL_INFO);
        info->MaxNameLen = 1024;

        // Attempt to get information about the symbol and add it to our output parameter.
        DWORD64 displacement64 = 0;
        if (SymFromAddr(hProcess, (DWORD64)addrs[i], &displacement64, info)) {
            outWalk.append(info->Name, info->NameLen);

            DWORD dwDisplacement;
            IMAGEHLP_LINE64 line;

            if (SymGetLineFromAddr64(hProcess, (DWORD64)addrs[i], &dwDisplacement, &line))
            {
                outWalk.append(":" + to_string(line.LineNumber));
            }
            else
            {
                cout << "SymGetLineFromAddr64 failed.\n";
            }

            outWalk.append("\n");
        }
        else
        {
            cout << "SymFromAddr failed with " + to_string(GetLastError()) + "\n";
        }
    }

    SymCleanup(::GetCurrentProcess());

    cout << outWalk;
    if (bFileAssert)
    {
        ofile << outWalk;
        ofile.close();
    }

}
#endif
