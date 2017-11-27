
#include "RubiChess.h"


void uci::send(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    va_end(argptr);

    //cout << s;
}

GuiToken uci::parse(vector<string>* args, string ss)
{
    bool firsttoken = false;

    if (ss == "")
        getline(cin, ss);

    GuiToken result = UNKNOWN;
    istringstream iss(ss);
    for (string s; iss >> s; )
    {
        if (!firsttoken)
        {
            if (GuiCommandMap.find(s.c_str()) != GuiCommandMap.end())
            {
                result = GuiCommandMap.find(s.c_str())->second;
                firsttoken = true;
            }
        }
        else {
            args->push_back(s);
        }
    }

    return result;
}
