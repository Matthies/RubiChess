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


void engine::send(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    va_end(argptr);

    //cout << s;
}

GuiToken engine::parse(vector<string>* args, string ss)
{
    bool firsttoken = false;

    if (ss == "")
        getline(cin, ss);

    if (cin.eof())
        exit(EXIT_SUCCESS);

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
