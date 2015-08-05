/*
 *  linenoise.hpp -- Multi-platfrom C++ header-only linenoise library.
 *
 *  All credits and commendations have to go to the authors of the
 *  following excellent libraries.
 *
 *  - linenoise.h and linenose.c (https://github.com/antirez/linenoise)
 *  - ANSI.c (https://github.com/adoxa/ansicon)
 *  - Win32_ANSI.h and Win32_ANSI.c (https://github.com/MSOpenTech/redis)
 *
 * ------------------------------------------------------------------------
 *
 *  Copyright (c) 2015 yhirose
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  
 *  1. Redistributions of source code must retain the above copyright notice, this
 *     list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* linenoise.h -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * See linenoise.c for more information.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ANSI.c - ANSI escape sequence console driver.
 *
 * Copyright (C) 2005-2014 Jason Hood
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 * 
 * Jason Hood
 * jadoxa@yahoo.com.au
 */

/*
 * Win32_ANSI.h and Win32_ANSI.c
 *
 * Derived from ANSI.c by Jason Hood, from his ansicon project (https://github.com/adoxa/ansicon), with modifications.
 *
 * Copyright (c), Microsoft Open Technologies, Inc.
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LINENOISE_HPP
#define __LINENOISE_HPP

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#else
#define NOMINMAX
#include <Windows.h>
#include <io.h>
#ifndef STDIN_FILENO
#define STDIN_FILENO (_fileno(stdin))
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#define isatty _isatty
#define write win32_write
#define read _read
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <string>
#include <fstream>
#include <functional>
#include <vector>
#include <iostream>

namespace linenoise {

typedef std::function<void (const char*, std::vector<std::string>&)> CompletionCallback;

#ifdef _WIN32

namespace ansi {

#define lenof(array) (sizeof(array)/sizeof(*(array)))

typedef struct
{
    BYTE foreground;    // ANSI base color (0 to 7; add 30)
    BYTE background;    // ANSI base color (0 to 7; add 40)
    BYTE bold;  // console FOREGROUND_INTENSITY bit
    BYTE underline; // console BACKGROUND_INTENSITY bit
    BYTE rvideo;    // swap foreground/bold & background/underline
    BYTE concealed; // set foreground/bold to background/underline
    BYTE reverse; // swap console foreground & background attributes
} GRM, *PGRM;   // Graphic Rendition Mode


inline bool is_digit(char c) { return '0' <= c && c <= '9'; }

// ========== Global variables and constants

HANDLE    hConOut;      // handle to CONOUT$

const char ESC = '\x1B'; // ESCape character
const char BEL = '\x07';
const char SO = '\x0E';  // Shift Out
const char SI = '\x0F';  // Shift In

const size_t MAX_ARG = 16;  // max number of args in an escape sequence
int   state;                // automata state
TCHAR prefix;               // escape sequence prefix ( '[', ']' or '(' );
TCHAR prefix2;              // secondary prefix ( '?' or '>' );
TCHAR suffix;               // escape sequence suffix
int   es_argc;              // escape sequence args count
int   es_argv[MAX_ARG];     // escape sequence args
TCHAR Pt_arg[MAX_PATH * 2]; // text parameter for Operating System Command
int   Pt_len;
BOOL  shifted;


// DEC Special Graphics Character Set from
// http://vt100.net/docs/vt220-rm/table2-4.html
// Some of these may not look right, depending on the font and code page (in
// particular, the Control Pictures probably won't work at all).
const WCHAR G1[] =
{
    ' ',          // _ - blank
    L'\x2666',    // ` - Black Diamond Suit
    L'\x2592',    // a - Medium Shade
    L'\x2409',    // b - HT
    L'\x240c',    // c - FF
    L'\x240d',    // d - CR
    L'\x240a',    // e - LF
    L'\x00b0',    // f - Degree Sign
    L'\x00b1',    // g - Plus-Minus Sign
    L'\x2424',    // h - NL
    L'\x240b',    // i - VT
    L'\x2518',    // j - Box Drawings Light Up And Left
    L'\x2510',    // k - Box Drawings Light Down And Left
    L'\x250c',    // l - Box Drawings Light Down And Right
    L'\x2514',    // m - Box Drawings Light Up And Right
    L'\x253c',    // n - Box Drawings Light Vertical And Horizontal
    L'\x00af',    // o - SCAN 1 - Macron
    L'\x25ac',    // p - SCAN 3 - Black Rectangle
    L'\x2500',    // q - SCAN 5 - Box Drawings Light Horizontal
    L'_',         // r - SCAN 7 - Low Line
    L'_',         // s - SCAN 9 - Low Line
    L'\x251c',    // t - Box Drawings Light Vertical And Right
    L'\x2524',    // u - Box Drawings Light Vertical And Left
    L'\x2534',    // v - Box Drawings Light Up And Horizontal
    L'\x252c',    // w - Box Drawings Light Down And Horizontal
    L'\x2502',    // x - Box Drawings Light Vertical
    L'\x2264',    // y - Less-Than Or Equal To
    L'\x2265',    // z - Greater-Than Or Equal To
    L'\x03c0',    // { - Greek Small Letter Pi
    L'\x2260',    // | - Not Equal To
    L'\x00a3',    // } - Pound Sign
    L'\x00b7',    // ~ - Middle Dot
};

#define FIRST_G1 '_'
#define LAST_G1  '~'


// color constants

#define FOREGROUND_BLACK 0
#define FOREGROUND_WHITE FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE

#define BACKGROUND_BLACK 0
#define BACKGROUND_WHITE BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE

const BYTE foregroundcolor[8] =
    {
    FOREGROUND_BLACK,                   // black foreground
    FOREGROUND_RED,                     // red foreground
    FOREGROUND_GREEN,                   // green foreground
    FOREGROUND_RED | FOREGROUND_GREEN,  // yellow foreground
    FOREGROUND_BLUE,                    // blue foreground
    FOREGROUND_BLUE | FOREGROUND_RED,   // magenta foreground
    FOREGROUND_BLUE | FOREGROUND_GREEN, // cyan foreground
    FOREGROUND_WHITE                    // white foreground
    };

const BYTE backgroundcolor[8] =
    {
    BACKGROUND_BLACK,           // black background
    BACKGROUND_RED,         // red background
    BACKGROUND_GREEN,           // green background
    BACKGROUND_RED | BACKGROUND_GREEN,  // yellow background
    BACKGROUND_BLUE,            // blue background
    BACKGROUND_BLUE | BACKGROUND_RED,   // magenta background
    BACKGROUND_BLUE | BACKGROUND_GREEN, // cyan background
    BACKGROUND_WHITE,           // white background
    };

const BYTE attr2ansi[8] =       // map console attribute to ANSI number
{
    0,                  // black
    4,                  // blue
    2,                  // green
    6,                  // cyan
    1,                  // red
    5,                  // magenta
    3,                  // yellow
    7                   // white
};

GRM grm;

// saved cursor position
COORD SavePos;

// ========== Print Buffer functions

#define BUFFER_SIZE 2048

int   nCharInBuffer;
WCHAR ChBuffer[BUFFER_SIZE];

//-----------------------------------------------------------------------------
//   FlushBuffer()
// Writes the buffer to the console and empties it.
//-----------------------------------------------------------------------------

inline void FlushBuffer(void)
{
    DWORD nWritten;
    if (nCharInBuffer <= 0) return;
    WriteConsole(hConOut, ChBuffer, nCharInBuffer, &nWritten, NULL);
    nCharInBuffer = 0;
}

//-----------------------------------------------------------------------------
//   PushBuffer( WCHAR c )
// Adds a character in the buffer.
//-----------------------------------------------------------------------------

inline void PushBuffer(WCHAR c)
{
    if (shifted && c >= FIRST_G1 && c <= LAST_G1)
        c = G1[c - FIRST_G1];
    ChBuffer[nCharInBuffer] = c;
    if (++nCharInBuffer == BUFFER_SIZE)
        FlushBuffer();
}

//-----------------------------------------------------------------------------
//   SendSequence( LPTSTR seq )
// Send the string to the input buffer.
//-----------------------------------------------------------------------------

inline void SendSequence(LPTSTR seq)
{
    DWORD out;
    INPUT_RECORD in;
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);

    in.EventType = KEY_EVENT;
    in.Event.KeyEvent.bKeyDown = TRUE;
    in.Event.KeyEvent.wRepeatCount = 1;
    in.Event.KeyEvent.wVirtualKeyCode = 0;
    in.Event.KeyEvent.wVirtualScanCode = 0;
    in.Event.KeyEvent.dwControlKeyState = 0;
    for (; *seq; ++seq)
        {
        in.Event.KeyEvent.uChar.UnicodeChar = *seq;
        WriteConsoleInput(hStdIn, &in, 1, &out);
        }
}

// ========== Print functions

//-----------------------------------------------------------------------------
//   InterpretEscSeq()
// Interprets the last escape sequence scanned by ParseAndPrintANSIString
//   prefix             escape sequence prefix
//   es_argc            escape sequence args count
//   es_argv[]          escape sequence args array
//   suffix             escape sequence suffix
//
// for instance, with \e[33;45;1m we have
// prefix = '[',
// es_argc = 3, es_argv[0] = 33, es_argv[1] = 45, es_argv[2] = 1
// suffix = 'm'
//-----------------------------------------------------------------------------

inline void InterpretEscSeq(void)
{
    int  i;
    WORD attribut;
    CONSOLE_SCREEN_BUFFER_INFO Info;
    CONSOLE_CURSOR_INFO CursInfo;
    DWORD len, NumberOfCharsWritten;
    COORD Pos;
    SMALL_RECT Rect;
    CHAR_INFO  CharInfo;

    if (prefix == '[')
        {
        if (prefix2 == '?' && (suffix == 'h' || suffix == 'l'))
            {
            if (es_argc == 1 && es_argv[0] == 25)
                {
                GetConsoleCursorInfo(hConOut, &CursInfo);
                CursInfo.bVisible = (suffix == 'h');
                SetConsoleCursorInfo(hConOut, &CursInfo);
                return;
                }
            }
        // Ignore any other \e[? or \e[> sequences.
        if (prefix2 != 0)
            return;

        GetConsoleScreenBufferInfo(hConOut, &Info);
        switch (suffix)
            {
                case 'm':
                    if (es_argc == 0) es_argv[es_argc++] = 0;
                    for (i = 0; i < es_argc; i++)
                        {
                        if (30 <= es_argv[i] && es_argv[i] <= 37)
                            grm.foreground = es_argv[i] - 30;
                        else if (40 <= es_argv[i] && es_argv[i] <= 47)
                            grm.background = es_argv[i] - 40;
                        else switch (es_argv[i])
                            {
                                case 0:
                                case 39:
                                case 49:
                                        {
                                        TCHAR def[4];
                                        int   a;
                                        *def = '7'; def[1] = '\0';
                                        GetEnvironmentVariable(L"ANSICON_DEF", def, lenof(def));
                                        a = wcstol(def, NULL, 16);
                                        grm.reverse = FALSE;
                                        if (a < 0)
                                            {
                                            grm.reverse = TRUE;
                                            a = -a;
                                            }
                                        if (es_argv[i] != 49)
                                            grm.foreground = attr2ansi[a & 7];
                                        if (es_argv[i] != 39)
                                            grm.background = attr2ansi[(a >> 4) & 7];
                                        if (es_argv[i] == 0)
                                            {
                                            if (es_argc == 1)
                                                {
                                                grm.bold = a & FOREGROUND_INTENSITY;
                                                grm.underline = a & BACKGROUND_INTENSITY;
                                                }
                                            else
                                                {
                                                grm.bold = 0;
                                                grm.underline = 0;
                                                }
                                            grm.rvideo = 0;
                                            grm.concealed = 0;
                                            }
                                        }
                                        break;

                                case  1: grm.bold = FOREGROUND_INTENSITY; break;
                                case  5: // blink
                                case  4: grm.underline = BACKGROUND_INTENSITY; break;
                                case  7: grm.rvideo = 1; break;
                                case  8: grm.concealed = 1; break;
                                case 21: // oops, this actually turns on double underline
                                case 22: grm.bold = 0; break;
                                case 25:
                                case 24: grm.underline = 0; break;
                                case 27: grm.rvideo = 0; break;
                                case 28: grm.concealed = 0; break;
                            }
                        }
                    if (grm.concealed)
                        {
                        if (grm.rvideo)
                            {
                            attribut = foregroundcolor[grm.foreground]
                                | backgroundcolor[grm.foreground];
                            if (grm.bold)
                                attribut |= FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;
                            }
                        else
                            {
                            attribut = foregroundcolor[grm.background]
                                | backgroundcolor[grm.background];
                            if (grm.underline)
                                attribut |= FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;
                            }
                        }
                    else if (grm.rvideo)
                        {
                        attribut = foregroundcolor[grm.background]
                            | backgroundcolor[grm.foreground];
                        if (grm.bold)
                            attribut |= BACKGROUND_INTENSITY;
                        if (grm.underline)
                            attribut |= FOREGROUND_INTENSITY;
                        }
                    else
                        attribut = foregroundcolor[grm.foreground] | grm.bold
                        | backgroundcolor[grm.background] | grm.underline;
                    if (grm.reverse)
                        attribut = ((attribut >> 4) & 15) | ((attribut & 15) << 4);
                    SetConsoleTextAttribute(hConOut, attribut);
                    return;

                case 'J':
                    if (es_argc == 0) es_argv[es_argc++] = 0; // ESC[J == ESC[0J
                    if (es_argc != 1) return;
                    switch (es_argv[0])
                        {
                            case 0:     // ESC[0J erase from cursor to end of display
                                len = (Info.dwSize.Y - Info.dwCursorPosition.Y - 1) * Info.dwSize.X
                                    + Info.dwSize.X - Info.dwCursorPosition.X - 1;
                                FillConsoleOutputCharacter(hConOut, ' ', len,
                                    Info.dwCursorPosition,
                                    &NumberOfCharsWritten);
                                FillConsoleOutputAttribute(hConOut, Info.wAttributes, len,
                                    Info.dwCursorPosition,
                                    &NumberOfCharsWritten);
                                return;

                            case 1:     // ESC[1J erase from start to cursor.
                                Pos.X = 0;
                                Pos.Y = 0;
                                len = Info.dwCursorPosition.Y * Info.dwSize.X
                                    + Info.dwCursorPosition.X + 1;
                                FillConsoleOutputCharacter(hConOut, ' ', len, Pos,
                                    &NumberOfCharsWritten);
                                FillConsoleOutputAttribute(hConOut, Info.wAttributes, len, Pos,
                                    &NumberOfCharsWritten);
                                return;

                            case 2:     // ESC[2J Clear screen and home cursor
                                Pos.X = 0;
                                Pos.Y = 0;
                                len = Info.dwSize.X * Info.dwSize.Y;
                                FillConsoleOutputCharacter(hConOut, ' ', len, Pos,
                                    &NumberOfCharsWritten);
                                FillConsoleOutputAttribute(hConOut, Info.wAttributes, len, Pos,
                                    &NumberOfCharsWritten);
                                SetConsoleCursorPosition(hConOut, Pos);
                                return;

                            default:
                                return;
                        }

                case 'K':
                    if (es_argc == 0) es_argv[es_argc++] = 0; // ESC[K == ESC[0K
                    if (es_argc != 1) return;
                    switch (es_argv[0])
                        {
                            case 0:     // ESC[0K Clear to end of line
                                len = Info.dwSize.X - Info.dwCursorPosition.X + 1;
                                FillConsoleOutputCharacter(hConOut, ' ', len,
                                    Info.dwCursorPosition,
                                    &NumberOfCharsWritten);
                                FillConsoleOutputAttribute(hConOut, Info.wAttributes, len,
                                    Info.dwCursorPosition,
                                    &NumberOfCharsWritten);
                                return;

                            case 1:     // ESC[1K Clear from start of line to cursor
                                Pos.X = 0;
                                Pos.Y = Info.dwCursorPosition.Y;
                                FillConsoleOutputCharacter(hConOut, ' ',
                                    Info.dwCursorPosition.X + 1, Pos,
                                    &NumberOfCharsWritten);
                                FillConsoleOutputAttribute(hConOut, Info.wAttributes,
                                    Info.dwCursorPosition.X + 1, Pos,
                                    &NumberOfCharsWritten);
                                return;

                            case 2:     // ESC[2K Clear whole line.
                                Pos.X = 0;
                                Pos.Y = Info.dwCursorPosition.Y;
                                FillConsoleOutputCharacter(hConOut, ' ', Info.dwSize.X, Pos,
                                    &NumberOfCharsWritten);
                                FillConsoleOutputAttribute(hConOut, Info.wAttributes,
                                    Info.dwSize.X, Pos,
                                    &NumberOfCharsWritten);
                                return;

                            default:
                                return;
                        }

                case 'X':                 // ESC[#X Erase # characters.
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[X == ESC[1X
                    if (es_argc != 1) return;
                    FillConsoleOutputCharacter(hConOut, ' ', es_argv[0],
                        Info.dwCursorPosition,
                        &NumberOfCharsWritten);
                    FillConsoleOutputAttribute(hConOut, Info.wAttributes, es_argv[0],
                        Info.dwCursorPosition,
                        &NumberOfCharsWritten);
                    return;

                case 'L':                 // ESC[#L Insert # blank lines.
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[L == ESC[1L
                    if (es_argc != 1) return;
                    Rect.Left = 0;
                    Rect.Top = Info.dwCursorPosition.Y;
                    Rect.Right = Info.dwSize.X - 1;
                    Rect.Bottom = Info.dwSize.Y - 1;
                    Pos.X = 0;
                    Pos.Y = Info.dwCursorPosition.Y + es_argv[0];
                    CharInfo.Char.UnicodeChar = ' ';
                    CharInfo.Attributes = Info.wAttributes;
                    ScrollConsoleScreenBuffer(hConOut, &Rect, NULL, Pos, &CharInfo);
                    return;

                case 'M':                 // ESC[#M Delete # lines.
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[M == ESC[1M
                    if (es_argc != 1) return;
                    if (es_argv[0] > Info.dwSize.Y - Info.dwCursorPosition.Y)
                        es_argv[0] = Info.dwSize.Y - Info.dwCursorPosition.Y;
                    Rect.Left = 0;
                    Rect.Top = Info.dwCursorPosition.Y + es_argv[0];
                    Rect.Right = Info.dwSize.X - 1;
                    Rect.Bottom = Info.dwSize.Y - 1;
                    Pos.X = 0;
                    Pos.Y = Info.dwCursorPosition.Y;
                    CharInfo.Char.UnicodeChar = ' ';
                    CharInfo.Attributes = Info.wAttributes;
                    ScrollConsoleScreenBuffer(hConOut, &Rect, NULL, Pos, &CharInfo);
                    return;

                case 'P':                 // ESC[#P Delete # characters.
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[P == ESC[1P
                    if (es_argc != 1) return;
                    if (Info.dwCursorPosition.X + es_argv[0] > Info.dwSize.X - 1)
                        es_argv[0] = Info.dwSize.X - Info.dwCursorPosition.X;
                    Rect.Left = Info.dwCursorPosition.X + es_argv[0];
                    Rect.Top = Info.dwCursorPosition.Y;
                    Rect.Right = Info.dwSize.X - 1;
                    Rect.Bottom = Info.dwCursorPosition.Y;
                    CharInfo.Char.UnicodeChar = ' ';
                    CharInfo.Attributes = Info.wAttributes;
                    ScrollConsoleScreenBuffer(hConOut, &Rect, NULL, Info.dwCursorPosition,
                        &CharInfo);
                    return;

                case '@':                 // ESC[#@ Insert # blank characters.
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[@ == ESC[1@
                    if (es_argc != 1) return;
                    if (Info.dwCursorPosition.X + es_argv[0] > Info.dwSize.X - 1)
                        es_argv[0] = Info.dwSize.X - Info.dwCursorPosition.X;
                    Rect.Left = Info.dwCursorPosition.X;
                    Rect.Top = Info.dwCursorPosition.Y;
                    Rect.Right = Info.dwSize.X - 1 - es_argv[0];
                    Rect.Bottom = Info.dwCursorPosition.Y;
                    Pos.X = Info.dwCursorPosition.X + es_argv[0];
                    Pos.Y = Info.dwCursorPosition.Y;
                    CharInfo.Char.UnicodeChar = ' ';
                    CharInfo.Attributes = Info.wAttributes;
                    ScrollConsoleScreenBuffer(hConOut, &Rect, NULL, Pos, &CharInfo);
                    return;

                case 'k':                 // ESC[#k
                case 'A':                 // ESC[#A Moves cursor up # lines
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[A == ESC[1A
                    if (es_argc != 1) return;
                    Pos.Y = Info.dwCursorPosition.Y - es_argv[0];
                    if (Pos.Y < 0) Pos.Y = 0;
                    Pos.X = Info.dwCursorPosition.X;
                    SetConsoleCursorPosition(hConOut, Pos);
                    return;

                case 'e':                 // ESC[#e
                case 'B':                 // ESC[#B Moves cursor down # lines
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[B == ESC[1B
                    if (es_argc != 1) return;
                    Pos.Y = Info.dwCursorPosition.Y + es_argv[0];
                    if (Pos.Y >= Info.dwSize.Y) Pos.Y = Info.dwSize.Y - 1;
                    Pos.X = Info.dwCursorPosition.X;
                    SetConsoleCursorPosition(hConOut, Pos);
                    return;

                case 'a':                 // ESC[#a
                case 'C':                 // ESC[#C Moves cursor forward # spaces
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[C == ESC[1C
                    if (es_argc != 1) return;
                    Pos.X = Info.dwCursorPosition.X + es_argv[0];
                    if (Pos.X >= Info.dwSize.X) Pos.X = Info.dwSize.X - 1;
                    Pos.Y = Info.dwCursorPosition.Y;
                    SetConsoleCursorPosition(hConOut, Pos);
                    return;

                case 'j':                 // ESC[#j
                case 'D':                 // ESC[#D Moves cursor back # spaces
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[D == ESC[1D
                    if (es_argc != 1) return;
                    Pos.X = Info.dwCursorPosition.X - es_argv[0];
                    if (Pos.X < 0) Pos.X = 0;
                    Pos.Y = Info.dwCursorPosition.Y;
                    SetConsoleCursorPosition(hConOut, Pos);
                    return;

                case 'E':                 // ESC[#E Moves cursor down # lines, column 1.
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[E == ESC[1E
                    if (es_argc != 1) return;
                    Pos.Y = Info.dwCursorPosition.Y + es_argv[0];
                    if (Pos.Y >= Info.dwSize.Y) Pos.Y = Info.dwSize.Y - 1;
                    Pos.X = 0;
                    SetConsoleCursorPosition(hConOut, Pos);
                    return;

                case 'F':                 // ESC[#F Moves cursor up # lines, column 1.
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[F == ESC[1F
                    if (es_argc != 1) return;
                    Pos.Y = Info.dwCursorPosition.Y - es_argv[0];
                    if (Pos.Y < 0) Pos.Y = 0;
                    Pos.X = 0;
                    SetConsoleCursorPosition(hConOut, Pos);
                    return;

                case '`':                 // ESC[#`
                case 'G':                 // ESC[#G Moves cursor column # in current row.
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[G == ESC[1G
                    if (es_argc != 1) return;
                    Pos.X = es_argv[0] - 1;
                    if (Pos.X >= Info.dwSize.X) Pos.X = Info.dwSize.X - 1;
                    if (Pos.X < 0) Pos.X = 0;
                    Pos.Y = Info.dwCursorPosition.Y;
                    SetConsoleCursorPosition(hConOut, Pos);
                    return;

                case 'd':                 // ESC[#d Moves cursor row #, current column.
                    if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[d == ESC[1d
                    if (es_argc != 1) return;
                    Pos.Y = es_argv[0] - 1;
                    if (Pos.Y < 0) Pos.Y = 0;
                    if (Pos.Y >= Info.dwSize.Y) Pos.Y = Info.dwSize.Y - 1;
                    SetConsoleCursorPosition(hConOut, Pos);
                    return;

                case 'f':                 // ESC[#;#f
                case 'H':                 // ESC[#;#H Moves cursor to line #, column #
                    if (es_argc == 0)
                        es_argv[es_argc++] = 1; // ESC[H == ESC[1;1H
                    if (es_argc == 1)
                        es_argv[es_argc++] = 1; // ESC[#H == ESC[#;1H
                    if (es_argc > 2) return;
                    Pos.X = es_argv[1] - 1;
                    if (Pos.X < 0) Pos.X = 0;
                    if (Pos.X >= Info.dwSize.X) Pos.X = Info.dwSize.X - 1;
                    Pos.Y = es_argv[0] - 1;
                    if (Pos.Y < 0) Pos.Y = 0;
                    if (Pos.Y >= Info.dwSize.Y) Pos.Y = Info.dwSize.Y - 1;
                    SetConsoleCursorPosition(hConOut, Pos);
                    return;

                case 's':                 // ESC[s Saves cursor position for recall later
                    if (es_argc != 0) return;
                    SavePos = Info.dwCursorPosition;
                    return;

                case 'u':                 // ESC[u Return to saved cursor position
                    if (es_argc != 0) return;
                    SetConsoleCursorPosition(hConOut, SavePos);
                    return;

                case 'n':                 // ESC[#n Device status report
                    if (es_argc != 1) return; // ESC[n == ESC[0n -> ignored
                    switch (es_argv[0])
                        {
                            case 5:     // ESC[5n Report status
                                SendSequence(L"\33[0n"); // "OK"
                                return;

                            case 6:     // ESC[6n Report cursor position
                                    {
                                    TCHAR buf[32];
                                    wsprintf(buf, L"\33[%d;%dR", Info.dwCursorPosition.Y + 1,
                                        Info.dwCursorPosition.X + 1);
                                    SendSequence(buf);
                                    }
                                    return;

                            default:
                                return;
                        }

                case 't':                 // ESC[#t Window manipulation
                    if (es_argc != 1) return;
                    if (es_argv[0] == 21)   // ESC[21t Report xterm window's title
                        {
                        TCHAR buf[MAX_PATH * 2];
                        DWORD len = GetConsoleTitle(buf + 3, lenof(buf) - 3 - 2);
                        // Too bad if it's too big or fails.
                        buf[0] = ESC;
                        buf[1] = ']';
                        buf[2] = 'l';
                        buf[3 + len] = ESC;
                        buf[3 + len + 1] = '\\';
                        buf[3 + len + 2] = '\0';
                        SendSequence(buf);
                        }
                    return;

                default:
                    return;
            }
        }
    else // (prefix == ']')
        {
        // Ignore any \e]? or \e]> sequences.
        if (prefix2 != 0)
            return;

        if (es_argc == 1 && es_argv[0] == 0) // ESC]0;titleST
            {
            SetConsoleTitle(Pt_arg);
            }
        }
}

//-----------------------------------------------------------------------------
//   ParseAndPrintANSIString(hDev, lpBuffer, nNumberOfBytesToWrite)
// Parses the string lpBuffer, interprets the escapes sequences and prints the
// characters in the device hDev (console).
// The lexer is a three states automata.
// If the number of arguments es_argc > MAX_ARG, only the MAX_ARG-1 firsts and
// the last arguments are processed (no es_argv[] overflow).
//-----------------------------------------------------------------------------

inline BOOL ParseAndPrintANSIString(HANDLE hDev, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten)
{
    DWORD   i;
    LPCSTR s;

    if (hDev != hConOut)    // reinit if device has changed
        {
        hConOut = hDev;
        state = 1;
        shifted = FALSE;
        }
    for (i = nNumberOfBytesToWrite, s = (LPCSTR)lpBuffer; i > 0; i--, s++)
        {
        if (state == 1)
            {
            if (*s == ESC) state = 2;
            else if (*s == SO) shifted = TRUE;
            else if (*s == SI) shifted = FALSE;
            else PushBuffer(*s);
            }
        else if (state == 2)
            {
            if (*s == ESC); // \e\e...\e == \e
            else if ((*s == '[') || (*s == ']'))
                {
                FlushBuffer();
                prefix = *s;
                prefix2 = 0;
                state = 3;
                Pt_len = 0;
                *Pt_arg = '\0';
                }
            else if (*s == ')' || *s == '(') state = 6;
            else state = 1;
            }
        else if (state == 3)
            {
            if (is_digit(*s))
                {
                es_argc = 0;
                es_argv[0] = *s - '0';
                state = 4;
                }
            else if (*s == ';')
                {
                es_argc = 1;
                es_argv[0] = 0;
                es_argv[1] = 0;
                state = 4;
                }
            else if (*s == '?' || *s == '>')
                {
                prefix2 = *s;
                }
            else
                {
                es_argc = 0;
                suffix = *s;
                InterpretEscSeq();
                state = 1;
                }
            }
        else if (state == 4)
            {
            if (is_digit(*s))
                {
                es_argv[es_argc] = 10 * es_argv[es_argc] + (*s - '0');
                }
            else if (*s == ';')
                {
                if (es_argc < MAX_ARG - 1) es_argc++;
                es_argv[es_argc] = 0;
                if (prefix == ']')
                    state = 5;
                }
            else
                {
                es_argc++;
                suffix = *s;
                InterpretEscSeq();
                state = 1;
                }
            }
        else if (state == 5)
            {
            if (*s == BEL)
                {
                Pt_arg[Pt_len] = '\0';
                InterpretEscSeq();
                state = 1;
                }
            else if (*s == '\\' && Pt_len > 0 && Pt_arg[Pt_len - 1] == ESC)
                {
                Pt_arg[--Pt_len] = '\0';
                InterpretEscSeq();
                state = 1;
                }
            else if (Pt_len < lenof(Pt_arg) - 1)
                Pt_arg[Pt_len++] = *s;
            }
        else if (state == 6)
            {
            // Ignore it (ESC ) 0 is implicit; nothing else is supported).
            state = 1;
            }
        }
    FlushBuffer();
    if (lpNumberOfBytesWritten != NULL)
        *lpNumberOfBytesWritten = nNumberOfBytesToWrite - i;
    return (i == 0);
}

} // namespace ansi

HANDLE hOut;
HANDLE hIn;
DWORD consolemode;

inline int win32read(char *c) {

    DWORD foo;
    INPUT_RECORD b;
    KEY_EVENT_RECORD e;
    BOOL altgr;

    while (1) {
        if (!ReadConsoleInput(hIn, &b, 1, &foo)) return 0;
        if (!foo) return 0;

        if (b.EventType == KEY_EVENT && b.Event.KeyEvent.bKeyDown) {

            e = b.Event.KeyEvent;
            *c = b.Event.KeyEvent.uChar.AsciiChar;

            altgr = e.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED);

            if (e.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED) && !altgr) {

                /* Ctrl+Key */
                switch (*c) {
                    case 'D':
                        *c = 4;
                        return 1;
                    case 'C':
                        *c = 3;
                        return 1;
                    case 'H':
                        *c = 8;
                        return 1;
                    case 'T':
                        *c = 20;
                        return 1;
                    case 'B': /* ctrl-b, left_arrow */
                        *c = 2;
                        return 1;
                    case 'F': /* ctrl-f right_arrow*/
                        *c = 6;
                        return 1;
                    case 'P': /* ctrl-p up_arrow*/
                        *c = 16;
                        return 1;
                    case 'N': /* ctrl-n down_arrow*/
                        *c = 14;
                        return 1;
                    case 'U': /* Ctrl+u, delete the whole line. */
                        *c = 21;
                        return 1;
                    case 'K': /* Ctrl+k, delete from current to end of line. */
                        *c = 11;
                        return 1;
                    case 'A': /* Ctrl+a, go to the start of the line */
                        *c = 1;
                        return 1;
                    case 'E': /* ctrl+e, go to the end of the line */
                        *c = 5;
                        return 1;
                }

                /* Other Ctrl+KEYs ignored */
            } else {

                switch (e.wVirtualKeyCode) {

                    case VK_ESCAPE: /* ignore - send ctrl-c, will return -1 */
                        *c = 3;
                        return 1;
                    case VK_RETURN:  /* enter */
                        *c = 13;
                        return 1;
                    case VK_LEFT:   /* left */
                        *c = 2;
                        return 1;
                    case VK_RIGHT: /* right */
                        *c = 6;
                        return 1;
                    case VK_UP:   /* up */
                        *c = 16;
                        return 1;
                    case VK_DOWN:  /* down */
                        *c = 14;
                        return 1;
                    case VK_HOME:
                        *c = 1;
                        return 1;
                    case VK_END:
                        *c = 5;
                        return 1;
                    case VK_BACK:
                        *c = 8;
                        return 1;
                    case VK_DELETE:
                        *c = 127;
                        return 1;
                    default:
                        if (*c) return 1;
                }
            }
        }
    }

    return -1; /* Makes compiler happy */
}

inline int win32_write(int fd, const void *buffer, unsigned int count) {
    if (fd == _fileno(stdout)) {
        DWORD bytesWritten = 0;
        if (FALSE != ansi::ParseAndPrintANSIString(GetStdHandle(STD_OUTPUT_HANDLE), buffer, (DWORD)count, &bytesWritten)) {
            return (int)bytesWritten;
        } else {
            errno = GetLastError();
            return 0;
        }
    } else if (fd == _fileno(stderr)) {
        DWORD bytesWritten = 0;
        if (FALSE != ansi::ParseAndPrintANSIString(GetStdHandle(STD_ERROR_HANDLE), buffer, (DWORD)count, &bytesWritten)) {
            return (int)bytesWritten;
        } else {
            errno = GetLastError();
            return 0;
        }
    } else {
        return _write(fd, buffer, count);
    }
}
#endif // _WIN32

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096
static const char *unsupported_term[] = {"dumb","cons25","emacs",NULL};
static CompletionCallback completionCallback;

#ifndef _WIN32
static struct termios orig_termios; /* In order to restore at exit.*/
#endif
static bool rawmode = false; /* For atexit() function to check if restore is needed*/
static bool mlmode = false;  /* Multi line mode. Default is single line. */
static bool atexit_registered = false; /* Register atexit just 1 time. */
static size_t history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static std::vector<std::string> history;

/* The linenoiseState structure represents the state during line editing.
 * We pass this state to functions implementing specific editing
 * functionalities. */
struct linenoiseState {
    int ifd;            /* Terminal stdin file descriptor. */
    int ofd;            /* Terminal stdout file descriptor. */
    char *buf;          /* Edited line buffer. */
    size_t buflen;      /* Edited line buffer size. */
    std::string prompt; /* Prompt to display. */
    size_t pos;         /* Current cursor position. */
    size_t oldpos;      /* Previous refresh cursor position. */
    size_t len;         /* Current edited line length. */
    size_t cols;        /* Number of columns in terminal. */
    size_t maxrows;     /* Maximum num of rows used so far (multiline mode) */
    int history_index;  /* The history index we are currently editing. */
};

enum KEY_ACTION {
    KEY_NULL = 0,       /* NULL */
    CTRL_A = 1,         /* Ctrl+a */
    CTRL_B = 2,         /* Ctrl-b */
    CTRL_C = 3,         /* Ctrl-c */
    CTRL_D = 4,         /* Ctrl-d */
    CTRL_E = 5,         /* Ctrl-e */
    CTRL_F = 6,         /* Ctrl-f */
    CTRL_H = 8,         /* Ctrl-h */
    TAB = 9,            /* Tab */
    CTRL_K = 11,        /* Ctrl+k */
    CTRL_L = 12,        /* Ctrl+l */
    ENTER = 13,         /* Enter */
    CTRL_N = 14,        /* Ctrl-n */
    CTRL_P = 16,        /* Ctrl-p */
    CTRL_T = 20,        /* Ctrl-t */
    CTRL_U = 21,        /* Ctrl+u */
    CTRL_W = 23,        /* Ctrl+w */
    ESC = 27,           /* Escape */
    BACKSPACE =  127    /* Backspace */
};

void linenoiseAtExit(void);
bool AddHistory(const char *line);
void refreshLine(struct linenoiseState *l);

/* ======================= Low level terminal handling ====================== */

/* Set if to use or not the multi line mode. */
inline void SetMultiLine(bool ml) {
    mlmode = ml;
}

/* Return true if the terminal name is in the list of terminals we know are
 * not able to understand basic escape sequences. */
inline bool isUnsupportedTerm(void) {
#ifndef _WIN32
    char *term = getenv("TERM");
    int j;

    if (term == NULL) return false;
    for (j = 0; unsupported_term[j]; j++)
        if (!strcasecmp(term,unsupported_term[j])) return true;
#endif
    return false;
}

/* Raw mode: 1960 magic shit. */
inline bool enableRawMode(int fd) {
#ifndef _WIN32
    struct termios raw;

    if (!isatty(STDIN_FILENO)) goto fatal;
    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = true;
    }
    if (tcgetattr(fd,&orig_termios) == -1) goto fatal;

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;
    rawmode = true;
#else
    if (!atexit_registered) {
        /* Init windows console handles only once */
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut==INVALID_HANDLE_VALUE) goto fatal;

        if (!GetConsoleMode(hOut, &consolemode)) {
            CloseHandle(hOut);
            errno = ENOTTY;
            return false;
        };

        hIn = GetStdHandle(STD_INPUT_HANDLE);
        if (hIn == INVALID_HANDLE_VALUE) {
            CloseHandle(hOut);
            errno = ENOTTY;
            return false;
        }

        GetConsoleMode(hIn, &consolemode);
        SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT);

        /* Cleanup them at exit */
        atexit(linenoiseAtExit);
        atexit_registered = true;
    }

    rawmode = true;
#endif
    return true;

fatal:
    errno = ENOTTY;
    return false;
}

inline void disableRawMode(int fd) {
#ifdef _WIN32
    rawmode = false;
#else
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(fd,TCSAFLUSH,&orig_termios) != -1)
        rawmode = false;
#endif
}

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor. */
inline int getCursorPosition(int ifd, int ofd) {
    char buf[32];
    int cols, rows;
    unsigned int i = 0;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf)-1) {
        if (read(ifd,buf+i,1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
#ifdef _WIN32
    if (sscanf_s(buf+2,"%d;%d",&rows,&cols) != 2) return -1;
#else
    if (sscanf(buf + 2, "%d;%d", &rows, &cols) != 2) return -1;
#endif
    return cols;
}

/* Try to get the number of columns in the current terminal, or assume 80
 * if it fails. */
inline int getColumns(int ifd, int ofd) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO b;

    if (!GetConsoleScreenBufferInfo(hOut, &b)) return 80;
    return b.srWindow.Right - b.srWindow.Left;
#else
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int start, cols;

        /* Get the initial position so we can restore it later. */
        start = getCursorPosition(ifd,ofd);
        if (start == -1) goto failed;

        /* Go to right margin and get position. */
        if (write(ofd,"\x1b[999C",6) != 6) goto failed;
        cols = getCursorPosition(ifd,ofd);
        if (cols == -1) goto failed;

        /* Restore position. */
        if (cols > start) {
            char seq[32];
            snprintf(seq,32,"\x1b[%dD",cols-start);
            if (write(ofd,seq,strlen(seq)) == -1) {
                /* Can't recover... */
            }
        }
        return cols;
    } else {
        return ws.ws_col;
    }

failed:
    return 80;
#endif
}

/* Clear the screen. Used to handle ctrl+l */
inline void linenoiseClearScreen(void) {
    if (write(STDOUT_FILENO,"\x1b[H\x1b[2J",7) <= 0) {
        /* nothing to do, just to avoid warning. */
    }
}

/* Beep, used for completion when there is nothing to complete or when all
 * the choices were already shown. */
inline void linenoiseBeep(void) {
    fprintf(stderr, "\x7");
    fflush(stderr);
}

/* ============================== Completion ================================ */

/* This is an helper function for linenoiseEdit() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 *
 * The state of the editing is encapsulated into the pointed linenoiseState
 * structure as described in the structure definition. */
inline int completeLine(struct linenoiseState *ls) {
    std::vector<std::string> lc;
    int nread, nwritten;
    char c = 0;

    completionCallback(ls->buf,lc);
    if (lc.empty()) {
        linenoiseBeep();
    } else {
        size_t stop = 0, i = 0;

        while(!stop) {
            /* Show completion or original buffer */
            if (i < lc.size()) {
                struct linenoiseState saved = *ls;

                ls->len = ls->pos = lc[i].size();
                ls->buf = &lc[i][0];
                refreshLine(ls);
                ls->len = saved.len;
                ls->pos = saved.pos;
                ls->buf = saved.buf;
            } else {
                refreshLine(ls);
            }

            nread = read(ls->ifd,&c,1);
            if (nread <= 0) {
                return -1;
            }

            switch(c) {
                case 9: /* tab */
                    i = (i+1) % (lc.size()+1);
                    if (i == lc.size()) linenoiseBeep();
                    break;
                case 27: /* escape */
                    /* Re-show original buffer */
                    if (i < lc.size()) refreshLine(ls);
                    stop = 1;
                    break;
                default:
                    /* Update buffer and return */
                    if (i < lc.size()) {
#ifdef _WIN32
                        nwritten = _snprintf_s(ls->buf, ls->buflen, _TRUNCATE,"%s", &lc[i][0]);
#else
                        nwritten = snprintf(ls->buf, ls->buflen, "%s", &lc[i][0]);
#endif
                        ls->len = ls->pos = nwritten;
                    }
                    stop = 1;
                    break;
            }
        }
    }

    return c; /* Return last read character */
}

/* Register a callback function to be called for tab-completion. */
void SetCompletionCallback(CompletionCallback fn) {
    completionCallback = fn;
}

/* =========================== Line editing ================================= */

/* Single line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
inline void refreshSingleLine(struct linenoiseState *l) {
    char seq[64];
    size_t plen = l->prompt.length();
    int fd = l->ofd;
    char *buf = l->buf;
    size_t len = l->len;
    size_t pos = l->pos;
    std::string ab;

    while((plen+pos) >= l->cols) {
        buf++;
        len--;
        pos--;
    }
    while (plen+len > l->cols) {
        len--;
    }

    /* Cursor to left edge */
#ifdef _WIN32
    _snprintf_s(seq, 64, _TRUNCATE, "\r");
#else
    snprintf(seq, 64, "\r");
#endif
    ab += seq;
    /* Write the prompt and the current buffer content */
    ab += l->prompt;
    ab.append(buf, len);
    /* Erase to right */
#ifdef _WIN32
    _snprintf_s(seq,64,_TRUNCATE,"\x1b[0K");
#else
    snprintf(seq, 64, "\x1b[0K");
#endif
    ab += seq;
    /* Move cursor to original position. */
#ifdef _WIN32
    _snprintf_s(seq, 64, _TRUNCATE, "\r\x1b[%dC", (int)(pos + plen));
#else
    snprintf(seq, 64, "\r\x1b[%dC", (int)(pos + plen));
#endif
    ab += seq;
    if (write(fd,ab.c_str(),ab.length()) == -1) {} /* Can't recover from write error. */
}

/* Multi line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
inline void refreshMultiLine(struct linenoiseState *l) {
    char seq[64];
    auto plen = l->prompt.length();
    int rows = (int)((plen+l->len+l->cols-1)/l->cols); /* rows used by current buf. */
    int rpos = (int)((plen+l->oldpos+l->cols)/l->cols); /* cursor relative row. */
    int rpos2; /* rpos after refresh. */
    int col; /* colum position, zero-based. */
    int old_rows = (int)l->maxrows;
    int fd = l->ofd, j;
    std::string ab;

    /* Update maxrows if needed. */
    if (rows > (int)l->maxrows) l->maxrows = rows;

    /* First step: clear all the lines used before. To do so start by
     * going to the last row. */
    if (old_rows-rpos > 0) {
#ifdef _WIN32
        _snprintf_s(seq,64,_TRUNCATE,"\x1b[%dB", old_rows-rpos);
#else
        snprintf(seq, 64, "\x1b[%dB", old_rows - rpos);
#endif
        ab += seq;
    }

    /* Now for every row clear it, go up. */
    for (j = 0; j < old_rows-1; j++) {
#ifdef _WIN32
       _snprintf_s(seq, 64, _TRUNCATE, "\r\x1b[0K\x1b[1A");
#else
       snprintf(seq, 64, "\r\x1b[0K\x1b[1A");
#endif
        ab += seq;
    }

    /* Clean the top line. */
#ifdef _WIN32
    _snprintf_s(seq, 64, _TRUNCATE, "\r\x1b[0K");
#else
    snprintf(seq, 64, "\r\x1b[0K");
#endif
    ab += seq;

    /* Write the prompt and the current buffer content */
    ab += l->prompt;
    ab.append(l->buf, l->len);

    /* If we are at the very end of the screen with our prompt, we need to
     * emit a newline and move the prompt to the first column. */
    if (l->pos &&
        l->pos == l->len &&
        (l->pos+plen) % l->cols == 0)
    {
        ab += "\n";
#ifdef _WIN32
        _snprintf_s(seq, 64, _TRUNCATE, "\r");
#else
        snprintf(seq, 64, "\r");
#endif
        ab += seq;
        rows++;
        if (rows > (int)l->maxrows) l->maxrows = rows;
    }

    /* Move cursor to right position. */
    rpos2 = (int)((plen+l->pos+l->cols)/l->cols); /* current cursor relative row. */

    /* Go up till we reach the expected positon. */
    if (rows-rpos2 > 0) {
#ifdef _WIN32
       _snprintf_s(seq, 64, _TRUNCATE, "\x1b[%dA", rows - rpos2);
#else
       snprintf(seq, 64, "\x1b[%dA", rows - rpos2);
#endif
        ab += seq;
    }

    /* Set column. */
    col = (plen+(int)l->pos) % (int)l->cols;
    if (col)
#ifdef _WIN32
       _snprintf_s(seq, 64, _TRUNCATE, "\r\x1b[%dC", col);
#else
       snprintf(seq, 64, "\r\x1b[%dC", col);
#endif
    else
#ifdef _WIN32
       _snprintf_s(seq, 64, _TRUNCATE, "\r");
#else
       snprintf(seq, 64, "\r");
#endif
    ab += seq;

    l->oldpos = l->pos;

    if (write(fd,ab.c_str(),ab.length()) == -1) {} /* Can't recover from write error. */
}

/* Calls the two low level functions refreshSingleLine() or
 * refreshMultiLine() according to the selected mode. */
inline void refreshLine(struct linenoiseState *l) {
    if (mlmode)
        refreshMultiLine(l);
    else
        refreshSingleLine(l);
}

/* Insert the character 'c' at cursor current position.
 *
 * On error writing to the terminal -1 is returned, otherwise 0. */
int linenoiseEditInsert(struct linenoiseState *l, char c) {
    if (l->len < l->buflen) {
        if (l->len == l->pos) {
            l->buf[l->pos] = c;
            l->pos++;
            l->len++;
            l->buf[l->len] = '\0';
            if ((!mlmode && l->prompt.length()+l->len < l->cols) /* || mlmode */) {
                /* Avoid a full update of the line in the
                 * trivial case. */
                if (write(l->ofd,&c,1) == -1) return -1;
            } else {
                refreshLine(l);
            }
        } else {
            memmove(l->buf+l->pos+1,l->buf+l->pos,l->len-l->pos);
            l->buf[l->pos] = c;
            l->len++;
            l->pos++;
            l->buf[l->len] = '\0';
            refreshLine(l);
        }
    }
    return 0;
}

/* Move cursor on the left. */
void linenoiseEditMoveLeft(struct linenoiseState *l) {
    if (l->pos > 0) {
        l->pos--;
        refreshLine(l);
    }
}

/* Move cursor on the right. */
void linenoiseEditMoveRight(struct linenoiseState *l) {
    if (l->pos != l->len) {
        l->pos++;
        refreshLine(l);
    }
}

/* Move cursor to the start of the line. */
inline void linenoiseEditMoveHome(struct linenoiseState *l) {
    if (l->pos != 0) {
        l->pos = 0;
        refreshLine(l);
    }
}

/* Move cursor to the end of the line. */
inline void linenoiseEditMoveEnd(struct linenoiseState *l) {
    if (l->pos != l->len) {
        l->pos = l->len;
        refreshLine(l);
    }
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'. */
#define LINENOISE_HISTORY_NEXT 0
#define LINENOISE_HISTORY_PREV 1
inline void linenoiseEditHistoryNext(struct linenoiseState *l, int dir) {
    if (history.size() > 1) {
        /* Update the current history entry before to
         * overwrite it with the next one. */
        history[history.size() - 1 - l->history_index] = l->buf;
        /* Show the new entry */
        l->history_index += (dir == LINENOISE_HISTORY_PREV) ? 1 : -1;
        if (l->history_index < 0) {
            l->history_index = 0;
            return;
        } else if (l->history_index >= (int)history.size()) {
            l->history_index = history.size()-1;
            return;
        }
        memset(l->buf, 0, l->buflen);
#ifdef _WIN32
        strcpy_s(l->buf, l->buflen, history[history.size() - 1 - l->history_index].c_str());
#else
        strcpy(l->buf, history[history.size() - 1 - l->history_index].c_str());
#endif
        l->len = l->pos = strlen(l->buf);
        refreshLine(l);
    }
}

/* Delete the character at the right of the cursor without altering the cursor
 * position. Basically this is what happens with the "Delete" keyboard key. */
inline void linenoiseEditDelete(struct linenoiseState *l) {
    if (l->len > 0 && l->pos < l->len) {
        memmove(l->buf+l->pos,l->buf+l->pos+1,l->len-l->pos-1);
        l->len--;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}

/* Backspace implementation. */
inline void linenoiseEditBackspace(struct linenoiseState *l) {
    if (l->pos > 0 && l->len > 0) {
        memmove(l->buf+l->pos-1,l->buf+l->pos,l->len-l->pos);
        l->pos--;
        l->len--;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}

/* Delete the previosu word, maintaining the cursor at the start of the
 * current word. */
inline void linenoiseEditDeletePrevWord(struct linenoiseState *l) {
    size_t old_pos = l->pos;
    size_t diff;

    while (l->pos > 0 && l->buf[l->pos-1] == ' ')
        l->pos--;
    while (l->pos > 0 && l->buf[l->pos-1] != ' ')
        l->pos--;
    diff = old_pos - l->pos;
    memmove(l->buf+l->pos,l->buf+old_pos,l->len-old_pos+1);
    l->len -= diff;
    refreshLine(l);
}

/* This function is the core of the line editing capability of linenoise.
 * It expects 'fd' to be already in "raw mode" so that every key pressed
 * will be returned ASAP to read().
 *
 * The resulting string is put into 'buf' when the user type enter, or
 * when ctrl+d is typed.
 *
 * The function returns the length of the current buffer. */
inline int linenoiseEdit(int stdin_fd, int stdout_fd, char *buf, size_t buflen, const char *prompt)
{
    struct linenoiseState l;

    /* Populate the linenoise state that we pass to functions implementing
     * specific editing functionalities. */
    l.ifd = stdin_fd;
    l.ofd = stdout_fd;
    l.buf = buf;
    l.buflen = buflen;
    l.prompt = prompt;
    l.oldpos = l.pos = 0;
    l.len = 0;
    l.cols = getColumns(stdin_fd, stdout_fd);
    l.maxrows = 0;
    l.history_index = 0;

    /* Buffer starts empty. */
    l.buf[0] = '\0';
    l.buflen--; /* Make sure there is always space for the nulterm */

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    AddHistory("");

    if (write(l.ofd,prompt,l.prompt.length()) == -1) return -1;
    while(1) {
        char c;
        int nread;
        char seq[3];

#ifdef _WIN32
        nread = win32read(&c);
#else
        nread = read(l.ifd,&c,1);
#endif
        if (nread <= 0) return (int)l.len;

        /* Only autocomplete when the callback is set. It returns < 0 when
         * there was an error reading from fd. Otherwise it will return the
         * character that should be handled next. */
        if (c == 9 && completionCallback != NULL) {
            c = completeLine(&l);
            /* Return on errors */
            if (c < 0) return (int)l.len;
            /* Read next character when 0 */
            if (c == 0) continue;
        }

        switch(c) {
        case ENTER:    /* enter */
            history.pop_back();
            if (mlmode) linenoiseEditMoveEnd(&l);
            return (int)l.len;
        case CTRL_C:     /* ctrl-c */
            errno = EAGAIN;
            return -1;
        case BACKSPACE:   /* backspace */
        case 8:     /* ctrl-h */
            linenoiseEditBackspace(&l);
            break;
        case CTRL_D:     /* ctrl-d, remove char at right of cursor, or if the
                            line is empty, act as end-of-file. */
            if (l.len > 0) {
                linenoiseEditDelete(&l);
            } else {
                history.pop_back();
                return -1;
            }
            break;
        case CTRL_T:    /* ctrl-t, swaps current character with previous. */
            if (l.pos > 0 && l.pos < l.len) {
                int aux = buf[l.pos-1];
                buf[l.pos-1] = buf[l.pos];
                buf[l.pos] = aux;
                if (l.pos != l.len-1) l.pos++;
                refreshLine(&l);
            }
            break;
        case CTRL_B:     /* ctrl-b */
            linenoiseEditMoveLeft(&l);
            break;
        case CTRL_F:     /* ctrl-f */
            linenoiseEditMoveRight(&l);
            break;
        case CTRL_P:    /* ctrl-p */
            linenoiseEditHistoryNext(&l, LINENOISE_HISTORY_PREV);
            break;
        case CTRL_N:    /* ctrl-n */
            linenoiseEditHistoryNext(&l, LINENOISE_HISTORY_NEXT);
            break;
        case ESC:    /* escape sequence */
            /* Read the next two bytes representing the escape sequence.
             * Use two calls to handle slow terminals returning the two
             * chars at different times. */
            if (read(l.ifd,seq,1) == -1) break;
            if (read(l.ifd,seq+1,1) == -1) break;

            /* ESC [ sequences. */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    if (read(l.ifd,seq+2,1) == -1) break;
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': /* Delete key. */
                            linenoiseEditDelete(&l);
                            break;
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': /* Up */
                        linenoiseEditHistoryNext(&l, LINENOISE_HISTORY_PREV);
                        break;
                    case 'B': /* Down */
                        linenoiseEditHistoryNext(&l, LINENOISE_HISTORY_NEXT);
                        break;
                    case 'C': /* Right */
                        linenoiseEditMoveRight(&l);
                        break;
                    case 'D': /* Left */
                        linenoiseEditMoveLeft(&l);
                        break;
                    case 'H': /* Home */
                        linenoiseEditMoveHome(&l);
                        break;
                    case 'F': /* End*/
                        linenoiseEditMoveEnd(&l);
                        break;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': /* Home */
                    linenoiseEditMoveHome(&l);
                    break;
                case 'F': /* End*/
                    linenoiseEditMoveEnd(&l);
                    break;
                }
            }
            break;
        default:
            if (linenoiseEditInsert(&l,c)) return -1;
            break;
        case CTRL_U: /* Ctrl+u, delete the whole line. */
            buf[0] = '\0';
            l.pos = l.len = 0;
            refreshLine(&l);
            break;
        case CTRL_K: /* Ctrl+k, delete from current to end of line. */
            buf[l.pos] = '\0';
            l.len = l.pos;
            refreshLine(&l);
            break;
        case CTRL_A: /* Ctrl+a, go to the start of the line */
            linenoiseEditMoveHome(&l);
            break;
        case CTRL_E: /* ctrl+e, go to the end of the line */
            linenoiseEditMoveEnd(&l);
            break;
        case CTRL_L: /* ctrl+l, clear screen */
            linenoiseClearScreen();
            refreshLine(&l);
            break;
        case CTRL_W: /* ctrl+w, delete previous word */
            linenoiseEditDeletePrevWord(&l);
            break;
        }
    }
    return (int)l.len;
}

/* This function calls the line editing function linenoiseEdit() using
 * the STDIN file descriptor set in raw mode. */
inline std::string linenoiseRaw(const char *prompt) {
    std::string line;

    if (!isatty(STDIN_FILENO)) {
        /* Not a tty: read from file / pipe. */
        std::getline(std::cin, line);
    } else {
        /* Interactive editing. */
        if (enableRawMode(STDIN_FILENO) == false) return line;

        char buf[LINENOISE_MAX_LINE];
        auto count = linenoiseEdit(STDIN_FILENO, STDOUT_FILENO, buf, LINENOISE_MAX_LINE, prompt);
        if (count != -1) {
            line.assign(buf, count);
        }

        disableRawMode(STDIN_FILENO);
        printf("\n");
    }
    return line;
}

/* The high level function that is the main API of the linenoise library.
 * This function checks if the terminal has basic capabilities, just checking
 * for a blacklist of stupid terminals, and later either calls the line
 * editing function or uses dummy fgets() so that you will be able to type
 * something even in the most desperate of the conditions. */
inline std::string Readline(const char *prompt) {
    if (isUnsupportedTerm()) {
        printf("%s",prompt);
        fflush(stdout);
        std::string line;
        std::getline(std::cin, line);
        return line;
    } else {
        return linenoiseRaw(prompt);
    }
}

/* ================================ History ================================= */

/* At exit we'll try to fix the terminal to the initial conditions. */
inline void linenoiseAtExit(void) {
    disableRawMode(STDIN_FILENO);
}

/* This is the API call to add a new entry in the linenoise history.
 * It uses a fixed array of char pointers that are shifted (memmoved)
 * when the history max length is reached in order to remove the older
 * entry and make room for the new one, so it is not exactly suitable for huge
 * histories, but will work well for a few hundred of entries.
 *
 * Using a circular buffer is smarter, but a bit more complex to handle. */
inline bool AddHistory(const char* line) {
    if (history_max_len == 0) return false;

    /* Don't add duplicated lines. */
    if (!history.empty() && history.back() == line) return false;

    /* If we reached the max length, remove the older line. */
    if (history.size() == history_max_len) {
        history.erase(history.begin());
    }
    history.push_back(line);

    return true;
}

/* Set the maximum length for the history. This function can be called even
 * if there is already some history, the function will make sure to retain
 * just the latest 'len' elements if the new history length value is smaller
 * than the amount of items already inside the history. */
inline bool SetHistoryMaxLen(size_t len) {
    if (len < 1) return false;
    history_max_len = len;
    if (len < history.size()) {
        history.resize(len);
    }
    return true;
}

/* Save the history in the specified file. On success *true* is returned
 * otherwise *false* is returned. */
inline bool SaveHistory(const char* path) {
    std::ofstream f(path); // TODO: need 'std::ios::binary'?
    if (!f) return false;
    for (const auto& h: history) {
        f << h << std::endl;
    }
    return true;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded *true* is returned, otherwise
 * on error *false* is returned. */
inline bool LoadHistory(const char* path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        AddHistory(line.c_str());
    }
    return true;
}

inline const std::vector<std::string>& GetHistory() {
    return history;
}

} // namespace linenoise

#ifdef _WIN32
#undef snprintf
#undef isatty
#undef write
#undef read
#endif

#endif /* __LINENOISE_HPP */
