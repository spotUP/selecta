/*  ExperimentalScene DarkWave Studio
    Copyright (C) 2004-2012 Daniel Werner
    http://www.experimentalscene.com/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdarg.h>
#include <windows.h>
#include "Error.h"
#include "Core.h"

void ThrowString(char *format, ...)
{
  int     count;
  va_list list;
  char*   string;

  va_start(list, format);
  count = _vscprintf(format, list);
  string = new char[count + 1];
  vsprintf(string, format, list);
  va_end(list);
  throw string;
}

void DisplayError(char *format, ...)
{
  int     count;
  va_list list;
  char*   string;

  va_start(list, format);
  count = _vscprintf(format, list);
  string = new char[count + 1];
  vsprintf(string, format, list);
  va_end(list);
  MessageBoxA(0, string, "DarkWave", MB_ICONERROR);
  delete string;
}

void DebugPrint(char *format, ...)
{
  int     count;
  va_list list;
  char*   string;

  va_start(list, format);
  count = _vscprintf(format, list);
  string = new char[count + 1];
  vsprintf(string, format, list);
  va_end(list);

  OutputDebugStringA(string);
  
  delete string;
}
