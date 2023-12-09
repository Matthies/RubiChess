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

using namespace rubichess;

namespace rubichess {

/* A small noncryptographic PRNG                       */
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


// A FRC startposition generator
// Credits to rosettacode.org
static void frcPlaceRandomly(char* p, char c)
{
    int loc = rand() % 8;
    if (!p[loc])
        p[loc] = c;
    else
        frcPlaceRandomly(p, c);    // try again
}

static int frcPlaceFirst(char* p, char c, int loc = 0)
{
    while (p[loc]) ++loc;
    p[loc] = c;
    return loc;
}

static int frcPlaceAtX(char* p, char c, int x, int loc = 0)
{
    while (true) {
        while (p[loc]) ++loc;
        if (x == 0)
            break;
        x--;
        loc++;
    }
    p[loc] = c;
    return loc;
}

//  https://en.wikipedia.org/wiki/Fischer_random_chess_numbering_scheme
string frcPositionFen(int num)
{
    char p[8] = { 0 };
    int b1, b2, n1;

    // bishops on opposite color
    if (num < 0)
    {
        b1 = 2 * (rand() % 4) + 1;
        b2 = 2 * (rand() % 4);
    }
    else {
        b1 = 2 * (num % 4) + 1;
        num /= 4;
        b2 = 2 * (num % 4);
        num /= 4;
    }

    p[b1] = 'B';
    p[b2] = 'B';

    // queen knight knight, anywhere
    if (num < 0)
    {
        for (char c : "QNN")
            frcPlaceRandomly(p, c);
    }
    else
    {
        frcPlaceAtX(p, 'Q', num % 6);
        num /= 6;
        // num = 0..9
        int f1 = num < 4 ? 0 : num < 7 ? 1 : num < 9 ? 2 : 3;
        n1 = frcPlaceAtX(p, 'N', f1);
        int f2 = num < 7 ? num % 4 : (num - 1) % 2;
        frcPlaceAtX(p, 'N', f2, n1);
    }
    // rook king rook, in that order

    int l;
    l = frcPlaceFirst(p, 'R');
    l = frcPlaceFirst(p, 'K', l);

    frcPlaceFirst(p, 'R', l);

    return string(p, 8);
}


string frcStartFen(int numWhite, int numBlack)
{
    string wPieceStr = frcPositionFen(numWhite);
    string bPieceStr;
    if (numWhite == numBlack)
        bPieceStr = wPieceStr;
    else
        bPieceStr = frcPositionFen(numBlack);
    transform(bPieceStr.begin(), bPieceStr.end(), bPieceStr.begin(), ::tolower);

    return bPieceStr + "/pppppppp/8/8/8/8/PPPPPPPP/" + wPieceStr + " w KQkq - 0 1";
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


U64 calc_key_from_str(const char* str)
{
    int pcs[16];
    getPcsFromStr(str, pcs);
    return calc_key_from_pcs(pcs, 0);
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
            moveliststr += p->AlgebraicFromShort(fv[i]);
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


PieceType GetPieceType(char c)
{
    switch (c)
    {
    case 'n':
    case 'N':
        return KNIGHT;
    case 'b':
    case 'B':
        return BISHOP;
    case 'r':
    case 'R':
        return ROOK;
    case 'q':
    case 'Q':
        return QUEEN;
    case 'k':
    case 'K':
        return KING;
    default:
        break;
    }
    return BLANKTYPE;
}


char PieceChar(PieceCode c, bool lower)
{
    PieceType p = (PieceType)(c >> 1);
    int color = (c & 1);
    char o;
    switch (p)
    {
    case PAWN:
        o = 'p';
        break;
    case KNIGHT:
        o = 'n';
        break;
    case BISHOP:
        o = 'b';
        break;
    case ROOK:
        o = 'r';
        break;
    case QUEEN:
        o = 'q';
        break;
    case KING:
        o = 'k';
        break;
    default:
        o = ' ';
        break;
    }
    if (!lower && !color)
        o = (char)(o + ('A' - 'a'));
    return o;
}


string moveToString(uint32_t mc)
{
    char s[8];

    if (mc == 0)
        return "(none)";

    int from, to;
    PieceCode promotion;
    from = GETFROM(mc);
    if (!en.chess960)
        to = GETCORRECTTO(mc);
    else
        to = GETTO(mc);
    promotion = GETPROMOTION(mc);

    snprintf(s, 8, "%c%d%c%d", (from & 0x7) + 'a', ((from >> 3) & 0x7) + 1, (to & 0x7) + 'a', ((to >> 3) & 0x7) + 1);
    if (promotion)
    {
        string ps(1, PieceChar(promotion, true));
        return s + ps;
    }

    return s;
}


string chessposition::AlgebraicFromShort(string s)
{
    string retval = "";
    int castle0 = 0;
    PieceType promotion = BLANKTYPE;
    chessmovelist ml;
    prepareStack();
    ml.length = CreateMovelist<ALL>(&ml.move[0]);
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
        from = kingpos[state & S2MMASK];
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
            if (playMove<true>(ml.move[i].code))
            {
                unplayMove<true>(ml.move[i].code);
                retval = ml.move[i].toString();
                break;
            }
        }
    }
    return retval;
}

string compilerinfo::PrintCpuFeatures(U64 f, bool onlyHighest)
{
    string s = "";
    for (int i = 0; f; i++, f = f >> 1)
        if (f & 1) s = (onlyHighest ? "" : ((s != "") ? s + " " : "")) + strCpuFeatures[i];

    return s;
}


#if defined(_M_X64) || defined(__amd64) || defined(_M_IX86) || defined(__i386)

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
            if (CPUInfo[2] & (1 <<  9)) machineSupports |= CPUSSSE3;
        }

        if (i == 7)
        {
            if (CPUInfo[1] & (1 <<  3)) machineSupports |= CPUBMI1;
            if (CPUInfo[1] & (1 <<  8)) machineSupports |= CPUBMI2;
            if (CPUInfo[1] & (1 <<  5)) machineSupports |= CPUAVX2;
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
        // Extended CPU features
        if (i == 0x80000001)
            if (CPUInfo[2] & (1 << 5)) machineSupports |= CPULZCNT;
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
        cout << "info string Error! Binary is not compatible with this machine. Missing cpu features: " + PrintCpuFeatures(notSupported) + ". Please use correct binary.\n";
        exit(-1);
    }

    if (cpuVendor == CPUVENDORAMD && cpuFamily < 25 && (machineSupports & CPUBMI2))
    {
        // No real BMI2 support on AMD cpu before Zen3
        machineSupports ^= CPUBMI2;
        if (binarySupports & CPUBMI2)
            cout << "info string Warning! You are running the BMI2 binary on an AMD cpu which is known for bad performance. Please use avx2 binary for best performance.\n";
    }

    U64 supportedButunused = machineSupports & ~binarySupports;
    if (supportedButunused)
        cout << "info string Warning! Binary not optimal for this machine. Unused cpu features: " + PrintCpuFeatures(supportedButunused) + ". Please use correct binary for best performance.\n";
}

#else
void compilerinfo::GetSystemInfo()
{
#if defined(__arm__) || (defined(USE_NEON) && !defined(USE_ARM64))
    system = "ArmV7 platform supprting NEON";
    machineSupports = CPUNEON;
#elif defined(__aarch64__) || defined(USE_ARM64)
    system = "ArmV8 (AArch64) platform supprting NEON";
    machineSupports = CPUNEON | CPUARM64;
#else
    system = "Some non-x86-64-non-arm platform.";
    machineSupports = 0ULL;
#endif
}

#endif



#ifdef _WIN32
#include <process.h>
int compilerinfo::GetProcessId()
{
    return _getpid();
}

U64 getTime()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
#if 0   // enable this to debug timer overflow after 10 seconds
    static U64 offset;
    if (!offset) {
        offset = 0xffffffffffffffff - now.QuadPart - 10 * en.frequency;
    }
    return now.QuadPart + offset;
#else
    return now.QuadPart;
#endif
}

static int UseLargePages = -1;
size_t largePageSize = 0;

void* my_large_malloc(size_t s)
{
    void* mem = nullptr;
    bool allowlp = en.allowlargepages;
    
    if (allowlp && UseLargePages < 0)
    {
        // Check and preparations for use of large pages... only once
        HANDLE hProcessToken{ };
        LUID luid{ };
        largePageSize = GetLargePageMinimum();
        UseLargePages = (bool)largePageSize;
        
        // Activate SeLockMemoryPrivilege
        UseLargePages = UseLargePages && OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hProcessToken);
        UseLargePages = UseLargePages && LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &luid);
        if (UseLargePages)
        {
            TOKEN_PRIVILEGES  tokenPriv{ };
            TOKEN_PRIVILEGES prevTp{ };
            DWORD prevTpLen = 0;
            
            tokenPriv.PrivilegeCount = 1;
            tokenPriv.Privileges[0].Luid = luid;
            tokenPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            
            // Try to enable SeLockMemoryPrivilege. Note that even if AdjustTokenPrivileges() succeeds,
            // we still need to query GetLastError() to ensure that the privileges were actually obtained.
            UseLargePages = UseLargePages && AdjustTokenPrivileges(hProcessToken, FALSE, &tokenPriv, sizeof(TOKEN_PRIVILEGES), &prevTp, &prevTpLen);
            UseLargePages = UseLargePages && (GetLastError() == ERROR_SUCCESS);
            CloseHandle(hProcessToken);
        }
        
        guiCom << (UseLargePages ? "info string Allocation of memory uses large pages.\n" : "info string Allocation of memory: Large pages not available.\n");
    }
    
    if (allowlp && UseLargePages)
    {
        // Round up size to full pages and allocate
        s = (s + largePageSize - 1) & ~size_t(largePageSize - 1);
        mem = VirtualAlloc(
                           NULL, s, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
        
        if (!mem)
        {
            UseLargePages = -1;
            guiCom << "info string Allocation of memory: Large pages not available for this size. Disabled for now.\n";
        }
    }
    
    if (!mem)
        mem = _aligned_malloc(s, 64);
    
    if (!mem)
        cerr << "Cannot allocate memory (" << s << " bytes)\n";
    
    return mem;
}


void my_large_free(void* m)
{
    if (!m)
        return;
    
    if (en.allowlargepages && UseLargePages > 0)
        VirtualFree(m, 0, MEM_RELEASE);
    else
        _aligned_free(m);
}

#include <direct.h>
#define MYCWD(x,y) _getcwd(x,y)
const char kPathSeparator = '\\';

#else

#include <unistd.h>
int compilerinfo::GetProcessId()
{
    return getpid();
}



U64 getTime()
{
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return (U64)((1000000000LL >> 9) * now.tv_sec + (now.tv_nsec >> 9));
}

void Sleep(long x)
{
    timespec now;
    now.tv_sec = 0;
    now.tv_nsec = x * 1000000;
    nanosleep(&now, NULL);
}

#define MYCWD(x,y) getcwd(x,y)
const char kPathSeparator = '/';

#endif


string CurrentWorkingDir()
{
    char* cwd = MYCWD( 0, 0 );
    string working_directory(cwd);
    free(cwd) ;
    return working_directory + kPathSeparator;
}


#ifdef STATISTICS
#define NODBZ(x) (double)(max(1ULL, x))
void statistic::output(vector<string> args)
{

    U64 n, i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11;;
    double f0, f1, f2, f3, f4, f5, f6, f7, f10, f11;
    char str[512];

    size_t cs = args.size();
    size_t ci = 0;
    while (ci < cs)
    {
        string cmd = args[ci++];
        if (cmd == "reset")
        {
            memset(this, 0, sizeof(*this));
            guiCom << "[STATS] counters are reset.\n";
            return;
        }
        else if (cmd == "print")
        {
            // add some parameter like 'all', 'ms', ...
        }
        else
        {
            cout << "Unknown command " << cmd << "\n";
            return;
        }
    }

    guiCom << "[STATS] ==================================================================================================================================================================\n";

    // quiescense search statistics
    i1 = qs_n[0];
    i2 = qs_n[1];
    i4 = qs_mindepth;
    n = i1 + i2;
    f0 = 100.0 * i2 / NODBZ(n);
    f1 = 100.0 * qs_tt / NODBZ(n);
    f2 = 100.0 * qs_pat / NODBZ(n);
    f3 = 100.0 * qs_delta / NODBZ(n);
    i3 = qs_move_delta + qs_moves;
    f4 = i3 / NODBZ(qs_loop_n);
    f5 = 100.0 * qs_move_delta / NODBZ(i3);
    f6 = 100.0 * qs_moves_fh / NODBZ(qs_moves);
    snprintf(str, 512, "[STATS] QSearch: %12lld   %%InCheck:  %5.2f   %%TT-Hits:  %5.2f   %%Std.Pat: %5.2f   %%DeltaPr: %5.2f   Mvs/Lp: %5.2f   %%DlPrM: %5.2f   %%FailHi: %5.2f   mindepth: %3lld\n", n, f0, f1, f2, f3, f4, f5, f6, i4);
    guiCom << str;

    // general aplhabeta statistics
    n = ab_n;
    f0 = 100.0 * ab_pv / NODBZ(n);
    f1 = 100.0 * ab_tt / NODBZ(n);
    f2 = 100.0 * ab_tb / NODBZ(n);
    f3 = 100.0 * ab_qs / NODBZ(n);
    f4 = 100.0 * ab_draw_or_win / NODBZ(n);
    snprintf(str, 512, "[STATS] Total AB:%12lld   %%PV-Nodes: %5.2f   %%TT-Hits:  %5.2f   %%TB-Hits: %5.2f   %%QSCalls: %5.2f   %%Draw/Mates: %5.2f\n", n, f0, f1, f2, f3, f4);
    guiCom << str;

    // node pruning
    f0 = 100.0 * prune_futility / NODBZ(n);
    f1 = 100.0 * prune_nm / NODBZ(n);
    f2 = 100.0 * prune_probcut / NODBZ(n);
    f3 = 100.0 * prune_multicut / NODBZ(n);
    f4 = 100.0 * prune_threat / NODBZ(n);
    f5 = 100.0 * (prune_futility + prune_nm + prune_probcut + prune_multicut + prune_threat) / (double)n;
    snprintf(str, 512, "[STATS] Node pruning            %%Futility: %5.2f   %%NullMove: %5.2f   %%ProbeC.: %5.2f   %%MultiC.: %7.5f   %%Threat.: %7.5f Total:  %5.2f\n", f0, f1, f2, f3, f4, f5);
    guiCom << str;

    // move statistics
    i1 = moves_n[0]; // quiet moves
    i2 = moves_n[1]; // tactical moves
    n = i1 + i2;
    f0 = 100.0 * i1 / NODBZ(n);
    f1 = 100.0 * i2 / NODBZ(n);
    f2 = 100.0 * moves_pruned_lmp / NODBZ(n);
    f3 = 100.0 * moves_pruned_futility / NODBZ(n);
    f4 = 100.0 * moves_pruned_badsee / NODBZ(n);
    f5 = n / NODBZ(moves_loop_n);
    i3 = moves_played[0] + moves_played[1];
    f6 = 100.0 * moves_fail_high / NODBZ(i3);
    f7 = 100.0 * moves_bad_hash / NODBZ(i2);
    snprintf(str, 512, "[STATS] Moves:   %12lld   %%Quiet-M.: %5.2f   %%Tact.-M.: %5.2f   %%BadHshM: %5.2f   %%LMP-M.:  %5.2f   %%FutilM.: %5.2f   %%BadSEE: %5.2f  Mvs/Lp: %5.2f   %%FailHi: %5.2f\n", n, f0, f1, f7, f2, f3, f4, f5, f6);
    guiCom << str;

    // late move reduction statistics
    U64 red_n = red_pi[0] + red_pi[1];
    f10 = red_lmr[0] / NODBZ(red_pi[0]);
    f11 = red_lmr[1] / NODBZ(red_pi[1]);
    f1 = (red_lmr[0] + red_lmr[1]) / NODBZ(red_n);
    f2 = red_history / NODBZ(red_n);
    f6 = red_historyabs / NODBZ(red_n);
    f3 = red_pv / NODBZ(red_n);
    f4 = red_correction / NODBZ(red_n);
    f5 = red_total / NODBZ(red_n);
    snprintf(str, 512, "[STATS] Reduct.  %12lld   lmr[0]: %4.2f   lmr[1]: %4.2f   lmr: %4.2f   hist: %4.2f   |hst|:%4.2f   pv: %4.2f   corr: %4.2f   total: %4.2f\n", red_n, f10, f11, f1, f2, f6, f3, f4, f5);
    guiCom << str;

    // extensions
    f0 = 100.0 * extend_singular / NODBZ(n);
    f1 = 100.0 * extend_endgame / NODBZ(n);
    f2 = 100.0 * extend_history / NODBZ(n);
    snprintf(str, 512, "[STATS] Extensions: %%singular: %7.4f   %%endgame: %7.4f   %%history: %7.4f\n", f0, f1, f2);
    guiCom << str;

    // accumulator updates
    n = nnue_accupdate_all;
    f0 = 100.0 * nnue_accupdate_cache / NODBZ(n);
    f1 = 100.0 * nnue_accupdate_inc / NODBZ(n);
    f2 = 100.0 * nnue_accupdate_full / NODBZ(n);
    f3 = 100.0 * nnue_accupdate_spec / NODBZ(n);

    snprintf(str, 512, "[STATS] AccuUpdate:   Cached: %10lld (%7.4f%%)   Increm.: %10lld (%7.4f%%)      Full: %10lld (%7.4f%%)      Spec: %10lld (%7.4f%%)\n",
        nnue_accupdate_cache, f0, nnue_accupdate_inc, f1, nnue_accupdate_full, f2, nnue_accupdate_spec, f3);
    guiCom << str;

    int p, d, l;
    // effective branching factor
    f0 = 0;
    n = 0;
    string ebfstr = "";
    for (p = 9; p < MAXSTATDEPTH; p++)
    {
        f0 += ebf_per_depth_sum[p];
        n += ebf_per_depth_n[p];
        snprintf(str, 512, "dp=%02d  n=%lld  ebf=%3.2f   ", p, ebf_per_depth_n[p], ebf_per_depth_sum[p] / NODBZ(ebf_per_depth_n[p]));
        ebfstr = ebfstr + str;
    }
    snprintf(str, 512, "total:  n=%lld  ebf=%3.2f   ", n, f0 / NODBZ(n));
    ebfstr = "[STATS] EBF:  " + string(str) + ebfstr;
    guiCom << ebfstr + "\n";

    // Move selector
    for (p = 0; p < 2; p++)
    {
        n = i1 = i2 = i3 = i4 = i5 = i6 = i7 = i8 = i9 = i10 = i11 = 0ULL;
        guiCom << "[STATS] Statistics of move selector " + (p ? string("PV node") : string("non-PV node")) + "\n";
        for (d = 0; d < MAXSTATDEPTH; d++) {
            n += ms_n[p][d];
        }
        for (d = 0; d < MAXSTATDEPTH; d++) {
            if (ms_n[p][d] == 0)
                continue;
            string depthStr = (d == 0 ? "QSearch " : d == MAXSTATDEPTH - 1 ? "ProbCut " : "Depth#" + (d < 10 ? string(" ") : "") + to_string(d));
            snprintf(str, 512, "n=%12lld   (%6.2f%%)  %%TctStg:%5.1f Mvs:%4.1f  %%SpcStg:%5.1f Mvs:%4.1f  %%QteStg:%5.1f Mvs:%4.1f  %%BdTStg:%5.1f Mvs:%4.1f  %%EvsStg:%5.1f Mvs:%4.1f\n",
                ms_n[p][d], 100.0 * ms_n[p][d] / NODBZ(n),
                100.0 * ms_tactic_stage[p][d][0] / NODBZ(ms_n[p][d]), ms_tactic_moves[p][d][0] / NODBZ(ms_tactic_stage[p][d][0]),
                100.0 * ms_spcl_stage[p][d] / NODBZ(ms_n[p][d]), ms_spcl_moves[p][d] / NODBZ(ms_spcl_stage[p][d]),
                100.0 * ms_quiet_stage[p][d][0] / NODBZ(ms_n[p][d]), ms_quiet_moves[p][d][0] / NODBZ(ms_quiet_stage[p][d][0]),
                100.0 * ms_badtactic_stage[p][d] / NODBZ(ms_n[p][d]), ms_badtactic_moves[p][d] / NODBZ(ms_badtactic_stage[p][d]),
                100.0 * ms_evasion_stage[p][d][0] / NODBZ(ms_n[p][d]), ms_evasion_moves[p][d][0] / NODBZ(ms_evasion_stage[p][d][0]));
            guiCom << "[STATS] " + depthStr + "  " + str;
            i1 += ms_n[p][d];
            i2 += ms_tactic_stage[p][d][0];
            i3 += ms_tactic_moves[p][d][0];
            i4 += ms_spcl_stage[p][d];
            i5 += ms_spcl_moves[p][d];
            i6 += ms_quiet_stage[p][d][0];
            i7 += ms_quiet_moves[p][d][0];
            i8 += ms_badtactic_stage[p][d];
            i9 += ms_badtactic_moves[p][d];
            i10 += ms_evasion_stage[p][d][0];
            i11 += ms_evasion_moves[p][d][0];
        }
        snprintf(str, 512, "n=%12lld   (%6.2f%%)  %%TctStg:%5.1f Mvs:%4.1f  %%SpcStg:%5.1f Mvs:%4.1f  %%QteStg:%5.1f Mvs:%4.1f  %%BdTStg:%5.1f Mvs:%4.1f  %%EvsStg:%5.1f Mvs:%4.1f\n",
            i1, 100.0 * i1 / NODBZ(n),
            100.0 * i2 / NODBZ(n), i3 / NODBZ(i2),
            100.0 * i4 / NODBZ(n), i5 / NODBZ(i4),
            100.0 * i6 / NODBZ(n), i7 / NODBZ(i6),
            100.0 * i8 / NODBZ(n), i9 / NODBZ(i8),
            100.0 * i10 / NODBZ(n), i11 / NODBZ(i10));
        guiCom << string("[STATS] Total:    ") + str;

        guiCom << "[STATS]  Quiets per movelist lenth\n";
        for (l = 1; l < MAXSTATMOVES; l++)
        {
            i1 = i2 = 0;
            for (d = 0; d < MAXSTATDEPTH; d++) {
                i1 += ms_quiet_stage[p][d][l];
                i2 += ms_quiet_moves[p][d][l];
            }
            if (i1 == 0)
                continue;
            snprintf(str, 512, "length %2d  n=%12lld   Total MvsAvg:%4.1f   ", l, i1, i2 / NODBZ(i1));
            string lengthStr = string("[STATS] ") + str;
            for (d = 1; d < MAXSTATDEPTH && ms_quiet_stage[p][d][0]; d++) {
                snprintf(str, 512, "  %4.1f (%2d)", ms_quiet_moves[p][d][l] / NODBZ(ms_quiet_stage[p][d][l]), d);
                lengthStr += str;
            }
            guiCom << lengthStr + "\n";
        }
    }
    NnueCurrentArch->Statistics(true, false);
    guiCom << "[STATS] ==================================================================================================================================================================\n";
    outputDone = true;
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
} // namespace rubichess

