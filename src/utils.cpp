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


#ifdef EVALTUNE

chessposition pos;
U64 fenWritten;

static void writeFenToFile(ofstream *fenfile, string fenlines[], int gamepositions, int ppg)
{
    int fentowrite = (ppg ? ppg : gamepositions);

    int i = 0;
    while (gamepositions && fentowrite--)
    {
        if (ppg)
            i = rand() % gamepositions;
        *fenfile << fenlines[i];
        fenWritten++;
        if (ppg)
            fenlines[i] = fenlines[--gamepositions];
        else
            i++;
    }
}


bool PGNtoFEN(string pgnfilename, bool quietonly, int ppg)
{
    pos.tps.count = 0;
    int gamescount = 0;
    fenWritten = 0ULL;
    bool mateFound = false;
    string line, newline;
    string line1, line2;
    string rest_of_last_line = "";
    string fenfilename = pgnfilename + ".fen";
    string fenmovefilename = pgnfilename + ".fenmove";

    // The following is Windows-only; FIXME: C++--17 offers portable filesystem handling
    WIN32_FIND_DATA fd;
    HANDLE pgnhandle = INVALID_HANDLE_VALUE;
    bool folderMode = (GetFileAttributes(pgnfilename.c_str()) & FILE_ATTRIBUTE_DIRECTORY);
    if (folderMode)
    {
        if (pgnfilename.substr(pgnfilename.length() - 1, 1) != "\\")
            pgnfilename += "\\";

        pgnhandle = FindFirstFile((pgnfilename + "*.pgn").c_str(), &fd);
        if (pgnhandle == INVALID_HANDLE_VALUE)
            return false;
    }


    ofstream fenfile(fenfilename);
    if (!fenfile.is_open())
    {
        printf("Cannot open %s for writing.\n", fenfilename.c_str());
        return false;
    }
    ofstream fenmovefile;
    if (!quietonly)
    {
        fenmovefile.open(fenmovefilename);
        if (!fenmovefile.is_open())
        {
            printf("Cannot open %s for writing.\n", fenmovefilename.c_str());
            return false;
        }
    }
    do
    {
        string pgnfullname = (folderMode ? pgnfilename + fd.cFileName : pgnfilename);
        // Open the pgn file for reading
        ifstream pgnfile(pgnfullname);

        if (!pgnfile.is_open())
        {
            printf("Cannot open %s for reading.\n", pgnfullname.c_str());
            return false;
        }
        printf("Reading %s ...\n", pgnfullname.c_str());
        int newgamestarts = 0;
        int result = 0;
        string newfen;
        string moves;
        string lastmove;
        bool valueChecked = true;;
        int gamepositions = 0;
        string fenlines[2048];
        string scoreBracket;
        while (getline(pgnfile, newline))
        {
            line2 = line1;
            line1 = newline;
            line = line + newline;

            smatch match;
            string fen;
            int score;
            // We assume that the [Result] section comes first, then the [FEN] section
            if (regex_search(line, match, regex("\\[Result\\s+\"(.*)\\-(.*)\"")))
            {
                gamescount++;
                if (gamepositions)
                    writeFenToFile(&fenfile, fenlines, gamepositions, ppg);
                gamepositions = 0;
                // Write last game
                if (!quietonly && !ppg && newgamestarts >= 2)
                    fenmovefile << to_string(result) + "#" + newfen + " moves " + moves + "\n";

                if (match.str(1) == "0")
                    result = -1;
                else if (match.str(1) == "1")
                    result = 1;
                else
                    result = 0;
                newgamestarts = 1;
                moves = "";
                mateFound = false;
            }
            bool fenFound;
            if (newgamestarts == 1 && 
                ((fenFound = regex_search(line, match, regex("\\[FEN\\s+\"(.*)\"")))
                    || !regex_search(line, match, regex("\\["))))
            {
                newfen = fenFound ? match.str(1) : STARTFEN;
                newgamestarts++;
                valueChecked = true;
                fen = newfen;
                cout << "Reading game " << gamescount << "\n";
                pos.getFromFen(fen.c_str());
                pos.ply = 0;
                // Skip positions inside TB area
                if (POPCOUNT(pos.occupied00[0] | pos.occupied00[1]) >= 7)
                    fenlines[gamepositions++] = fen + " " + (result == 0 ? "1/2" : (result > 0 ? "1-0" : "0-1")) + "\n";
            }
            // Don't export games that were lost on time or by stalled connection
            if (regex_search(line, match, regex("\\[Termination\\s+\".*(forfeit|stalled|unterminated).*\"")))
            {
                printf("Skip this match: %s\n", line.c_str());
                newgamestarts = 0;
                valueChecked = true;
            }

            if (regex_search(line, match, regex("^\\[.*\\]$")))
                line = "";

            // search for the moves
            if (newgamestarts == 2 && !regex_search(line, match, regex("^\\[.*\\]$")))
            {
                bool foundInLine;
                do
                {
                    if ((foundInLine = regex_search(line, match, regex("^(\\s*\\d+\\.\\s*)"))))
                    {
                        // skip move number
                        line = match.suffix();
                    }
                    else if ((foundInLine = regex_search(line, match, regex("^\\s*(([O\\-]{3,5})\\+?|([KQRBN]?[a-h]*[1-8]*x?[a-h]+[1-8]+(=[QRBN])?)\\+?)"))))
                    {
                        // Found move
                        if (!valueChecked)
                        {
                            // Score tag of last move missing; just output without score
                            if (fen != "")
                                fenlines[gamepositions++] = fen + " " + (result == 0 ? "1/2" : (result > 0 ? "1-0" : "0-1")) + " 0\n";
                            moves = moves + lastmove + " ";
                        }
                        valueChecked = false;
                        lastmove = AlgebraicFromShort(match.str(1), &pos);
                        //printf("%5s  %5s\n", match.str(1).c_str(), lastmove.c_str());
                        if (lastmove == "" || !pos.applyMove(lastmove))
                        {
                            printf("Alarm (game %d): %s\n", gamescount, match.str(1).c_str());
                            pos.print();
                            printf("last Lines:\n%s\n%s\n\n", line2.c_str(), line1.c_str());
                        }
                        pos.isQuiet = !pos.isCheckbb;
                        if (quietonly && pos.isQuiet)
                            pos.getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                        if (!quietonly || pos.isQuiet)
                            fen = pos.toFen();
                        else
                            fen = "";
                        line = match.suffix();
                    }
                    else if ((foundInLine = regex_search(line, match, regex("\\s*(\\{[^\\}]*\\})"))))
                    {
                        scoreBracket = match.str(1);
                        line = match.suffix();
                        if (!valueChecked)
                        {
                            string scorestr;
                            bool foundValue;
                            if ((foundValue = regex_search(scoreBracket, match, regex("\\{(\\(.*\\))*(\\+|\\-)(M?)(\\d+(\\.?)\\d*)"))))
                            {
                                // cutechess pgn
                                scorestr = match.str(2) + match.str(4);
                                double dScore = stod(scorestr);
                                if (match.str(5) == ".")
                                    dScore *= 100;
                                score = int(dScore);
                                // Only output if no mate score detected
                                if (match.str(3) == "M" || abs(score) >= 3000)
                                    mateFound = true;
                            }
                            else if ((foundValue = regex_search(scoreBracket, match, regex("(\\(.*\\))*(eval\\s+)([\\+\\-]?)(M?)(\\d+(\\.?)\\d*)"))))
                            {
                                // CCRL pgn
                                scorestr = match.str(3) + match.str(5);
                                score = stoi(scorestr);
                                // Only output if no mate score detected
                                if (match.str(4) == "M" || abs(score) >= 3000)
                                    mateFound = true;
                            }
                            if (foundValue)
                            {
                                foundInLine = true;
                                if (!mateFound && fen != "")
                                {
                                    fenlines[gamepositions++] = fen + " " + (result == 0 ? "1/2" : (result > 0 ? "1-0" : "0-1")) + " " + to_string(score) + "\n";
                                    moves = moves + lastmove + " ";
                                }
                                valueChecked = true;
                            }
                        }
                    }
                } while (foundInLine);
            }

            if (pos.ply > 500 && newgamestarts == 2)
                // Too many plies; abort game
                newgamestarts = 3;

            if (newgamestarts != 2)
                // not in moves section delete rest of line
                line = "";
        }
        if (gamepositions)
            writeFenToFile(&fenfile, fenlines, gamepositions, ppg);

        printf("Position so far:  %9lld\n", fenWritten);

    } while (folderMode && FindNextFile(pgnhandle, &fd));

    return true;
}



static string getValueStringValue(eval *e)
{
    if (e->type == 0)
    {
        string sm = to_string(GETMGVAL(*e));
        if (sm.length() < 4)
            sm.insert(sm.begin(), 4 - sm.length(), ' ');
        string se = to_string(GETEGVAL(*e));
        if (se.length() < 4)
            se.insert(se.begin(), 4 - se.length(), ' ');
        return "VALUE(" + sm + "," + se + ")";
    }
    else if (e->type == 1)
    {
        string si = to_string(*e);
        if (si.length() < 4)
            si.insert(si.begin(), 4 - si.length(), ' ');
        return "CVALUE(" + si + ")";
    }
    else if (e->type == 2)
    {
        string si = to_string(e->groupindex);
        if (si.length() < 4)
            si.insert(si.begin(), 4 - si.length(), ' ');
        string sv = to_string(e->v);
        if (sv.length() < 4)
            sv.insert(sv.begin(), 4 - sv.length(), ' ');
        return "SQVALUE(" + si + "," + sv + ")";
    }
    else
    {
        string si = to_string(*e);
        if (si.length() < 4)
            si.insert(si.begin(), 4 - si.length(), ' ');
        return "EVALUE(" + si + ")";
    }
}


static string nameTunedParameter(chessposition *p, int i)
{
    string name = p->tps.name[i];
    if (p->tps.bound2[i] > 0)
    {
        name += "[" + to_string(p->tps.index2[i]) + "][" + to_string(p->tps.index1[i]) + "]";
    }
    else if (p->tps.bound1[i] > 0)
    {
        name += "[" + to_string(p->tps.index1[i]) + "] = { ";
    }
    return name;
}


static void printTunedParameters(chessposition *p)
{
    string lastname = "";
    string output = "";
    for (int i = 0; i < p->tps.count; i++)
    {
        if (lastname != p->tps.name[i])
        {
            if (output != "")
            {
                output += ";\n";
                printf("%s", output.c_str());
                output = "";
            }
            lastname = p->tps.name[i];
            output = "    eval " + lastname;
            if (p->tps.bound2[i] > 0)
            {
                output += "[" + to_string(p->tps.bound2[i]) + "][" + to_string(p->tps.bound1[i]) + "] = {\n        { ";
            }
            else if (p->tps.bound1[i] > 0)
            {
                output += "[" + to_string(p->tps.bound1[i]) + "] = { ";
            }
            else {
                output += " = ";
            }
        }

        output = output + " " + getValueStringValue(p->tps.ev[i]);

        if (p->tps.index1[i] < p->tps.bound1[i] - 1)
        {
            output += ",";
            if (!((p->tps.index1[i] + 1) & (p->tps.bound2[i] ? 0x7 : 0x7)))
                output += "\n          ";
        }
        else if (p->tps.index1[i] == p->tps.bound1[i] - 1)
        {
            output += "  }";
            if (p->tps.index2[i] < p->tps.bound2[i] - 1)
                output += ",\n        { ";
            else if (p->tps.index2[i] == p->tps.bound2[i] - 1)
                output += "\n    }";
        }
    }
    output += ";\n";
    printf("%s", output.c_str());
}


int tuningratio = 1;

char *texelpts = NULL;
U64 texelptsnum;
int iThreads;


static int getGradientValue(eval *ev, positiontuneset *p, evalparam *e, bool debug = false, int corParam = -1)
{
    int v = 0;
    int complexity = 0;
    int sqsum[4][2] = { { 0 } };
    for (int i = 0; i < p->num; i++, e++)
    {
        if (corParam >= 0 && corParam != e->index)
            continue;
        int type = ev[e->index].type;
        if (debug)
            printf("%30s ", nameTunedParameter(&pos, e->index).c_str());
        if (type <= 1)
        {
            v += ev[e->index] * e->g[0];
            if (debug)
                printf(" %08x * %3d = %08x \n", ev[e->index].v, e->g[0], ev[e->index] * e->g[0]);
        }
        else if (type == 2)
        {
            int sqindex = ev[e->index].groupindex;
            sqsum[sqindex][0] += ev[e->index] * e->g[0];
            sqsum[sqindex][1] += ev[e->index] * e->g[1];
            if (debug) {
                printf(" sq0: %d * %d = %d \n", ev[e->index].v, e->g[0], ev[e->index] * e->g[0]);
                printf(" sq1: %d * %d = %d \n", ev[e->index].v, e->g[1], ev[e->index] * e->g[1]);
            }
        }
        else {
            complexity += ev[e->index] * e->g[0];
            if (debug)
                printf(" Compl: %08x * %3d = %08x \n", ev[e->index].v, e->g[0], ev[e->index] * e->g[0]);
        }
    }
    if (debug)
        printf("linear gesamt: %08x\n", v);
    for (int i = 0; i < 4; i++)
    {
        if (sqsum[i][0] == 0 && sqsum[i][1] == 0) continue;
        v += SQRESULT(sqsum[i][0], 0) + SQRESULT(sqsum[i][1], 1);
        if (debug)
            printf("sq-gesamt: %3d + %3d = %3d\n", SQRESULT(sqsum[i][0], 0), SQRESULT(sqsum[i][1], 1), SQRESULT(sqsum[i][0], 0) + SQRESULT(sqsum[i][1], 1));
    }

    // compelexity
    int evaleg = GETEGVAL(v);
    int sign = (evaleg > 0) - (evaleg < 0);
    v += sign * max(complexity, -abs(evaleg));
    if (debug)
        printf("gesamt: %08x\n", v);

    return v;
}

double texel_k = 1.121574;

static double TexelEvalError(tuner *tn, double k = texel_k)
{
    double Ri, Qi;
    double E = 0.0;

    positiontuneset *p = (positiontuneset*)texelpts;
    for (U64 i = 0; i < texelptsnum; i++)
    {
        evalparam *e = (evalparam *)((char*)p + sizeof(positiontuneset));

        Ri = p->R / 2.0;
        if (p->sc == SCALE_DRAW)
            Qi = SCOREDRAW;
        else
            Qi = TAPEREDANDSCALEDEVAL(getGradientValue(tn->ev, p, e), p->ph, p->sc);
        double sigmoid = 1 / (1 + pow(10.0, - k * Qi / 400.0));
        E += (Ri - sigmoid) * (Ri - sigmoid);
        p = (positiontuneset*)((char*)p + sizeof(positiontuneset) + p->num * sizeof(evalparam));
    }

    return E / texelptsnum;
}

static void getGradsFromFen(string fenfilenames)
{
    int n = 0;
    while (fenfilenames != "")
    {
        size_t spi = fenfilenames.find('*');
        string filename = (spi == string::npos) ? fenfilenames : fenfilenames.substr(0, spi);
        fenfilenames = (spi == string::npos) ? "" : fenfilenames.substr(spi + 1, string::npos);

        int fentype = -1;
        int gamescount = 0;
        bool fenmovemode = (filename.find(".fenmove") != string::npos);
        string line;
        smatch match;
        int c;
        int bw;
        char R;
        string fen;
        int Qi, Qr;
        int Q[4];
        U64 buffersize;
        char *pnext;
        long long minfreebuffer = sizeof(positiontuneset) + NUMOFEVALPARAMS * sizeof(evalparam) * 1024;
        int msb;
        GETMSB(msb, minfreebuffer);
        minfreebuffer = (1ULL << (msb + 1));
        const U64 maxbufferincrement = minfreebuffer << 10;

        bw = 0;
        c = tuningratio;
        ifstream fenfile(filename);
        if (!fenfile.is_open())
        {
            printf("\nCannot open %s for reading.\n", filename.c_str());
            continue;
        }
        buffersize = minfreebuffer;
        texelpts = (char*)malloc(buffersize);
        pnext = (char*)texelpts;
        printf("\nReading positions from %s\n", filename.c_str());
        while (getline(fenfile, line))
        {
            if (texelpts + buffersize - pnext < minfreebuffer)
            {
                buffersize = min(buffersize + maxbufferincrement, buffersize * 2);
                char *oldtexelpts = texelpts;
                texelpts = (char*)realloc(texelpts, buffersize);
                pnext += (texelpts - oldtexelpts);
            }
            if (!fenmovemode)
            {
                // "(.*)\\s+(c9\\s+)?\"?((1\\-0)|(0\\-1)|(1/2))\"?"
                fen = "";
                R = 1; // draw as default
                if ((fentype < 0 || fentype == 1) && regex_search(line, match, regex("(.*)#(.*)#(.*)")))
                {
                    // my own fen format
                    if (fentype < 0)
                    {
                        printf("Format: score#fen#eval (from pgn2fen)\n");
                        fentype = 1;
                    }
                    fen = match.str(2);
                    R = (stoi(match.str(1)) + 1);
                }
                else if ((fentype < 0 || fentype == 2) && regex_search(line, match, regex("(.*)\\s+((1\\-0)|(0\\-1)|(1/2))")))
                {
                    // FENS_JEFFREY
                    if (fentype < 0)
                    {
                        printf("Format: fen 1-0|0-1|1/2 (from FENS_JEFFREY)\n");
                        fentype = 2;
                    }
                    fen = match.str(1);
                    R = (match.str(2) == "1-0" ? 2 : (match.str(2) == "0-1" ? 0 : 1));
                }
                else if ((fentype < 0 || fentype == 3) && regex_search(line, match, regex("(.*)\\s+c9\\s+\"((1\\-0)|(0\\-1)|(1/2))")))
                {
                    // quiet-labled (zurichess)
                    if (fentype < 0)
                    {
                        printf("Format: fen c9 \"1-0|0-1|1/2\" (from quiet-labled)\n");
                        fentype = 3;
                    }
                    fen = match.str(1);
                    R = (match.str(2) == "1-0" ? 2 : (match.str(2) == "0-1" ? 0 : 1));
                }
                else if ((fentype < 0 || fentype == 4) && regex_search(line, match, regex("(.*)\\s+c1(.*)c2\\s+\"((1.0)|(0.5)|(0.0))")))
                {
                    // big3
                    if (fentype < 0)
                    {
                        printf("Format: fen c1 ... c2 \"1.0|0.5|0.0\" (from big3)\n");
                        fentype = 4;
                    }
                    fen = match.str(1);
                    R = (match.str(3) == "1.0" ? 2 : (match.str(3) == "0.0" ? 0 : 1));
                }
                else if ((fentype < 0 || fentype == 5) && regex_search(line, match, regex("(.*)\\|(.*)")))
                {
                    // lichess
                    if (fentype < 0)
                    {
                        printf("Format: fen |White|Black|Draw (from lichess-quiet)\n");
                        fentype = 5;
                    }
                    fen = match.str(1);
                    R = (match.str(2) == "White" ? 2 : (match.str(2) == "Black" ? 0 : 1));
                }
                if (fen != "")
                {
                    bw = 1 - bw;
                    if (bw)
                        c++;
                    if (c > tuningratio)
                        c = 1;
                    if (c == tuningratio)
                    {
                        pos.getFromFen(fen.c_str());
                        pos.ply = 0;
                        Qi = pos.getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                        if (!pos.w2m())
                            Qi = -Qi;
                        positiontuneset *nextpts = (positiontuneset*)pnext;
                        *nextpts = pos.pts;
                        nextpts->R = R;
                        Q[0] = Q[1] = Q[2] = Q[3] = 0;
                        evalparam *e = (evalparam *)(pnext + sizeof(positiontuneset));
                        int sqsum[4][2] = { { 0 } };
                        for (int i = 0; i < pos.pts.num; i++)
                        {
                            *e = pos.ev[i];
                            int ty = pos.tps.ev[e->index]->type;
                            if (ty != 2)
                            {
                                Q[ty] += e->g[0] * *pos.tps.ev[e->index];
                            }
                            else
                            {
                                int sqindex = pos.tps.ev[e->index]->groupindex;
                                sqsum[sqindex][0] += e->g[0] * *pos.tps.ev[e->index];
                                sqsum[sqindex][1] += e->g[1] * *pos.tps.ev[e->index];
                            }
                            pos.tps.used[e->index]++;
                            e++;
                        }
                        for (int i = 0; i < 4; i++)
                        {
                            if (sqsum[i][0] == 0 && sqsum[i][1] == 0) continue;
                            Q[0] += SQRESULT(sqsum[i][0], 0) + SQRESULT(sqsum[i][1], 1);
                        }
                        int evaleg = GETEGVAL(Q[0]);
                        int sign = (evaleg > 0) - (evaleg < 0);
                        Q[3] = sign * max(Q[3], -abs(evaleg));
                        Qr = TAPEREDANDSCALEDEVAL(Q[0] + Q[3], nextpts->ph, nextpts->sc) + Q[1];
                        if (MATEDETECTED(Qi))
                            n--;
                        else if (Qi != (nextpts->sc == SCALE_DRAW ? SCOREDRAW : Qr))
                        {
                            printf("\n%d  Alarm. Gradient evaluation differs from qsearch value: %d != %d.\nFEN: %s\n", n, Qr, Qi, fen.c_str());
                            getGradientValue(*pos.tps.ev, nextpts, (evalparam *)(pnext + sizeof(positiontuneset)), true);
                        }
                        else
                        {
                            //printf("%d  gesamt: %08x\n", n, Q);
                            pnext = (char*)e;
                            n++;
                            if (n % 0x2000 == 0) printf(".");
                        }
                    }
                }
            }
            else
            {
                if (regex_search(line, match, regex("(.*)#(.*)moves(.*)")))
                {
                    gamescount++;
                    R = (stoi(match.str(1)) + 1);
                    pos.getFromFen(match.str(2).c_str());
                    pos.ply = 0;
                    vector<string> movelist = SplitString(match.str(3).c_str());
                    vector<string>::iterator move = movelist.begin();
                    bool gameend;
                    do
                    {
                        bw = 1 - bw;
                        if (bw)
                            c++;
                        if (c > tuningratio)
                            c = 1;
                        if (c == tuningratio)
                        {
                            Qi = pos.getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                            if (!pos.w2m())
                                Qi = -Qi;
                            positiontuneset *nextpts = (positiontuneset*)pnext;
                            *nextpts = pos.pts;
                            nextpts->R = R;
                            Q[0] = Q[1] = Q[2] = Q[3] = 0;
                            evalparam *e = (evalparam *)(pnext + sizeof(positiontuneset));
                            int sqsum[4][2] = { { 0 } };
                            for (int i = 0; i < pos.pts.num; i++)
                            {
                                *e = pos.ev[i];
                                int ty = pos.tps.ev[e->index]->type;
                                //printf("%20s: %08x  %3d\n", pos->tps.name[e->index].c_str(), *pos.tps.ev[i], e->g);
                                if (ty != 2)
                                {
                                    Q[ty] += e->g[0] * *pos.tps.ev[e->index];
                                    //printf("l %3d: %3d * %08x         = %08x\n", e->index, e->g, (int)*pos.tps.ev[e->index], Qa);
                                }
                                else
                                {
                                    int sqindex = pos.tps.ev[e->index]->groupindex;
                                    sqsum[sqindex][0] += e->g[0] * *pos.tps.ev[e->index];
                                    sqsum[sqindex][1] += e->g[1] * *pos.tps.ev[e->index];
                                }
                                pos.tps.used[e->index]++;
                                e++;
                            }
                            for (int i = 0; i < 4; i++)
                            {
                                if (sqsum[i][0] == 0 && sqsum[i][1] == 0) continue;
                                Q[0] += SQRESULT(sqsum[i][0], 0) + SQRESULT(sqsum[i][1], 1);
                            }
                            int evaleg = GETEGVAL(Q[0]);
                            int sign = (evaleg > 0) - (evaleg < 0);
                            Q[3] = sign * max(Q[3], -abs(evaleg));
                            Qr = TAPEREDANDSCALEDEVAL(Q[0] + Q[3], nextpts->ph, nextpts->sc) + Q[1];
                            if (Qi != (nextpts->sc == SCALE_DRAW ? SCOREDRAW : Qr))
                                printf("Alarm. Gradient evaluation differs from qsearch value.\n");
                            else
                            {
                                //printf("gesamt: %d\n", Qa);
                                pnext = (char*)e;
                                n++;
                                if (n % 0x2000 == 0) printf(".");
                            }
                        }
                        gameend = (move == movelist.end());
                        if (!gameend)
                        {
                            if (!pos.applyMove(*move))
                            {
                                printf("Alarm (game %d)! Move %s seems illegal.\nLine: %s\n", gamescount, move->c_str(), line.c_str());
                                pos.print();
                            }
                            move++;
                        }

                    } while (!gameend);
                }
            }
        }

    }

    texelptsnum = n;
    printf("  got %d positions\n", n);
}



static void copyParams(chessposition *p, tuner *tn)
{
    for (int i = 0; i < p->tps.count; i++)
        tn->ev[i] = *p->tps.ev[i];
    tn->paramcount = p->tps.count;
}


static void tuneParameter(tuner *tn)
{
    tn->busy = true;

    double Error;
    double Emin = -1.0;
    int pmin;

    int tuned = 0;
    int g = 0;
    int subParam = (tn->ev[tn->paramindex].type ? 1 : 2);

    while (true)     // loop over mg/eg parameter while notImproved <=2
    {
        int pa[4];
        int p, lastp;
        tuned++;
        if (tuned > subParam)
            break;

        int pbound[2] = { SHRT_MAX, SHRT_MIN };
        int delta = 1;
        int direction = 0; // direction=0: go right; delta > 0; direction=1: go right; delta
        if (subParam == 2)
        {
            // linear parameter
            int v = tn->ev[tn->paramindex];
            pa[0] = GETEGVAL(v);
            pa[1] = GETMGVAL(v);
            lastp = pa[g];
            p = lastp + delta;
            p = min(512, max(-512, p));
            tn->ev[tn->paramindex].replace(g, lastp);
            pmin = lastp;
        }
        else
        {
            // square parameter
            eval *e = &tn->ev[tn->paramindex];
            pa[0] = e->v;
            lastp = pa[g];
            p = lastp + delta;
            p = min(512, max(-512, p));
            tn->ev[tn->paramindex].replace(lastp);
            pmin = lastp;
        }
        if (Emin < 0)
            tn->starterror = Emin = TexelEvalError(tn);
        do
        {
            if (subParam == 2)
                tn->ev[tn->paramindex].replace(g, p);
            else
                tn->ev[tn->paramindex].replace(p);
            Error = TexelEvalError(tn);
            if (Error >= Emin)
            {
                direction = (p > pmin ? 1 : 0);
                pbound[direction] = p;
                delta = (direction ? -1 : 1);
                p = pmin + delta;
            }
            else
            {
                pbound[direction] = pmin;
                Emin = Error;
                tuned = 1;
                pmin = p;
                delta *= 2;
                p = p + delta;
            }
        } while (abs(pbound[1] - pbound[0]) > 2);
        if (subParam == 2)
            tn->ev[tn->paramindex].replace(g, pmin);
        else
            tn->ev[tn->paramindex].replace(pmin);

        g = (g + 1) % subParam;
    }

    tn->error = Emin;
    tn->busy = false;
}


static void updateTunerPointer(chessposition *p, tunerpool *pool)
{
    int num = p->tps.count;
    int newLowRunning = pool->highRunning;

    for (int i = 0; i < en.Threads; i++)
    {
        tuner *tn = &pool->tn[i];
        int pi = tn->paramindex;

        if (tn->busy)
        {
            // remember for possible lowest runner
            if ((pool->highRunning - pi) % num > (pool->highRunning - newLowRunning) % num)
                newLowRunning = pi;
        }
    }
    pool->lowRunning = newLowRunning;
}

// Collects params of finished tuners, updates 'low' and 'improved' mark and returns free tuner
static void collectTuners(chessposition *p, tunerpool *pool, tuner **freeTuner)
{
    if (freeTuner) *freeTuner = nullptr;
    for (int i = 0; i < en.Threads; i++)
    {
        tuner *tn = &pool->tn[i];
        int pi = tn->paramindex;

        while (!freeTuner && tn->busy)
            Sleep(10);

        if (!tn->busy)
        {
            if (tn->thr.joinable())
                tn->thr.join();

            if (freeTuner && i < iThreads) *freeTuner = tn;

            if (pi >= 0)
            {
                if (tn->ev[pi] != *p->tps.ev[pi])
                {
                    printf("%2d %4d  %9lld   %40s  %0.10f -> %0.10f  %s  -> %s\n", i, pi, p->tps.used[pi], nameTunedParameter(p, pi).c_str(), tn->starterror, tn->error,
                        getValueStringValue(p->tps.ev[pi]).c_str(),
                        getValueStringValue(&(tn->ev[pi])).c_str());
                    pool->lastImproved = pi;
                    *p->tps.ev[pi] = tn->ev[pi];
                }
                else {
                    printf("%2d %4d  %9lld   %40s  %0.10f  %s  constant\n", i, pi, p->tps.used[pi], nameTunedParameter(p, pi).c_str(), tn->error,
                        getValueStringValue(&(tn->ev[pi])).c_str());
                }
            }
            tn->paramindex = -1;
        }
    }
}


static double getAvgParamVal(int iParam)
{
    // Get average
    positiontuneset *p = (positiontuneset*)texelpts;
    int pSum = 0;
    for (U64 i = 0; i < texelptsnum; i++)
    {
        evalparam *e = (evalparam *)((char*)p + sizeof(positiontuneset));
        if (p->sc != SCALE_DRAW)
            pSum += TAPEREDANDSCALEDEVAL(getGradientValue(*pos.tps.ev, p, e, false, iParam), p->ph, p->sc);
        p = (positiontuneset*)((char*)p + sizeof(positiontuneset) + p->num * sizeof(evalparam));
    }
    double pAvg = pSum / (double)texelptsnum;
    return pAvg;
}

static double getCorrelationCoeff(int ix, int iy, double ax, double ay)
{
    // Get correlation
    double counter = 0.0;
    double denominatorfactorx = 0.0;
    double denominatorfactory = 0.0;
    positiontuneset *p = (positiontuneset*)texelpts;
    for (U64 i = 0; i < texelptsnum; i++)
    {
        int px = 0, py = 0;
        evalparam *e = (evalparam *)((char*)p + sizeof(positiontuneset));
        if (p->sc != SCALE_DRAW)
        {
            px = TAPEREDANDSCALEDEVAL(getGradientValue(*pos.tps.ev, p, e, false, ix), p->ph, p->sc);
            py = TAPEREDANDSCALEDEVAL(getGradientValue(*pos.tps.ev, p, e, false, iy), p->ph, p->sc);
        }
        counter += (px - ax) * (py - ay);
        denominatorfactorx += (px - ax) * (px - ax);
        denominatorfactory += (py - ay) * (py - ay);

        p = (positiontuneset*)((char*)p + sizeof(positiontuneset) + p->num * sizeof(evalparam));
    }

    double cor = counter / sqrt(denominatorfactorx * denominatorfactory);
    return cor;
}


void getCorrelation(string correlationParams)
{
    while (correlationParams != "")
    {
        size_t spi = correlationParams.find('*');
        string correlationparam = (spi == string::npos) ? correlationParams : correlationParams.substr(0, spi);
        correlationParams = (spi == string::npos) ? "" : correlationParams.substr(spi + 1, string::npos);
        int x = 0;
        try {
            x = stoi(correlationparam);
        }
        catch (...) {}

        // skip (almost) unused parameters
        if (pos.tps.used[x] * 100000 / texelptsnum < 1)
            continue;

        // Get average of this param
        double xAvg = getAvgParamVal(x);
        struct {
            double avg;
            double coeff;
            int index;
        } correlation[NUMOFEVALPARAMS];
        // loop all parameters and get correlation
        for (int y = 0; y < pos.tps.count; y++)
        {
            correlation[y].index = -1;
            if (pos.tps.ev[y]->type != pos.tps.ev[x]->type)
                continue;
            if (pos.tps.used[y] * 100000 / texelptsnum < 1)
                continue;
            correlation[y].avg = getAvgParamVal(y);
            correlation[y].coeff = getCorrelationCoeff(x, y, xAvg, correlation[y].avg);
            correlation[y].index = y;
        }

        // sort them by absolute correlation
        for (int i = 0; i < pos.tps.count; i++)
        {
            if (correlation[i].index < 0) continue;
            for (int j = i + 1; j < pos.tps.count; j++)
            {
                if (correlation[j].index < 0) continue;
                if (abs(correlation[j].coeff) > abs(correlation[i].coeff))
                    swap(correlation[i], correlation[j]);
            }
            int y = correlation[i].index;
            printf("%3d\t%30s\tavg=\t%.4f\t%3d\t%30s\tavg=\t%.4f\tcor=\t%.4f\n", x, nameTunedParameter(&pos, x).c_str(), xAvg, y, nameTunedParameter(&pos, y).c_str(), correlation[i].avg, correlation[i].coeff);
        }
    }

}

void TexelTune(string fenfilenames, bool noqs, bool bOptimizeK, string correlation)
{
    pos.mtrlhsh.init();
    pos.pwnhsh.setSize(0);
    pos.tps.count = 0;
    registerallevals(&pos);
    pos.noQs = noqs;
    getGradsFromFen(fenfilenames);
    if (!texelptsnum) return;

    if (correlation != "")
    {
        getCorrelation(correlation);
        return;
    }

    tunerpool tpool;
    iThreads = en.Threads;
    tpool.tn = new tuner[en.Threads];
    tpool.lowRunning = -1;
    tpool.highRunning = -1;
    tpool.lastImproved = -1;
    tuner *tn;

    for (int i = 0; i < en.Threads; i++)
    {
        tpool.tn[i].busy = false;
        tpool.tn[i].index = i;
        tpool.tn[i].paramindex = -1;
    }

    if (bOptimizeK)
    {
        printf("Finding optimal tuning constant k for this position set first...\n");

        tn = &tpool.tn[0];
        copyParams(&pos, tn);
        double E[2];
        double Emin, Error;
        double bound[2] = { 0.0, 10.0 };
        double x, lastx;
        int direction = 0;
        //double delta;
        lastx = (bound[0] + bound[1]) / 2;

        E[0] = TexelEvalError(tn, bound[0]);
        E[1] = TexelEvalError(tn, bound[1]);
        Emin = TexelEvalError(tn, lastx);
        if (Emin > E[0] || Emin > E[1])
        {
            printf("Tuning Error! Wrong bounds. E0=%0.10f  E1=%0.10f  Ed=%0.10f\n", E[0], E[1], Emin);
            return;
        }

        while (bound[1] - bound[0] > 0.001)
        {
            x = (lastx + bound[direction]) / 2;
            Error = TexelEvalError(tn, x);
            printf("Tuningscore b0=%0.10f (%0.10f) b1=%0.10f (%0.10f)\n", bound[0], E[0], bound[1], E[1]);
            if (Error > Emin)
            {
                bound[direction] = x;
                E[direction] = Error;
            }
            else {
                E[1 - direction] = Emin;
                bound[1 - direction] = lastx;
                lastx = x;
                Emin = Error;
            }
            direction = 1 - direction;
        }
        texel_k = (bound[0] + bound[1]) / 2.0;
        printf("Best k for this tuning set: %0.10f\n", texel_k);
    }

    bool improved = true;
    bool leaveSoon = false;
    bool leaveNow = false;

    printf("Tuning starts now.\nPress 'P' to output current parameters.\nPress 'B' to break after current tuning loop.\nPress 'S' for immediate break.\nPress '+', '-' to increase/decrease threads to use.\n\n");

    while (improved && !leaveSoon && !leaveNow)
    {
        for (int i = 0; i < pos.tps.count; i++)
        {
            if (leaveNow)
                break;
            if (!pos.tps.tune[i])
                continue;
            if (pos.tps.used[i] * 100000 / texelptsnum < 1)
            {
                printf("   %4d  %9lld   %40s   %s  canceled, too few positions in testset\n", i, pos.tps.used[i],
                    nameTunedParameter(&pos, i).c_str(),
                    getValueStringValue(pos.tps.ev[i]).c_str());
                continue;
            }
            tpool.highRunning = i;
            do
            {
                collectTuners(&pos, &tpool, &tn);
                if (!tn)
                {
                    Sleep(100);
                    if (_kbhit())
                    {
                        char c = _getch();
                        if (c == 'p')
                            printTunedParameters(&pos);
                        if (c == 'b')
                        {
                            printf("Stopping after this tuning loop...\n");
                            leaveSoon = true;
                        }
                        if (c == 's')
                        {
                            printf("Stopping now!\n");
                            leaveNow = true;
                        }
                        if (c == '-')
                        {
                            iThreads = max(1, iThreads - 1);
                            printf("Now using %d threads...\n", iThreads);
                        }
                        if (c == '+')
                        {
                            iThreads = min(en.Threads, iThreads + 1);
                            printf("Now using %d threads...\n", iThreads);
                        }
                    }
                }
            } while (!tn);
            tn->busy = true;
            tn->paramindex = i;
            copyParams(&pos, tn);

            tn->thr = thread(&tuneParameter, tn);

            updateTunerPointer(&pos, &tpool);
            if (tpool.highRunning == tpool.lastImproved)
            {
                while (tn->busy)
                    // Complete loop without improvement... wait for last tuning finish
                    Sleep(100);
                collectTuners(&pos, &tpool, &tn);

                if (tpool.highRunning == tpool.lastImproved)
                {
                    // still no improvement after last finished thread => exit
                    improved = false;
                    break;
                }
            }
        }
    }
    collectTuners(&pos, &tpool, nullptr);
    delete[] tpool.tn;
    free(texelpts);
    pos.mtrlhsh.remove();
    pos.pwnhsh.remove();
    printTunedParameters(&pos);
}

#endif //EVALTUNE


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
