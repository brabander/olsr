/* 
 * OLSR ad-hoc routing table management protocol
 * Copyright (C) 2004 Thomas Lopatic (thomas@lopatic.de)
 *
 * This file is part of the olsr.org OLSR daemon.
 *
 * olsr.org is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * olsr.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with olsr.org; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: misc.c,v 1.1 2004/11/20 15:40:52 tlopatic Exp $
 *
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef interface

void clear_console(void)
{
  HANDLE Hand;
  CONSOLE_SCREEN_BUFFER_INFO Info;
  unsigned long Written;
  static COORD Home = { 0, 0 };

  Hand = GetStdHandle(STD_OUTPUT_HANDLE);

  if (Hand == INVALID_HANDLE_VALUE)
    return;

  if(!GetConsoleScreenBufferInfo(Hand, &Info))
    return;

  if(!FillConsoleOutputCharacter(Hand, ' ',
                                 Info.dwSize.X * Info.dwSize.Y, Home,
                                 &Written))
    return;

  if(!FillConsoleOutputAttribute(Hand, Info.wAttributes,
                                 Info.dwSize.X * Info.dwSize.Y, Home,
                                 &Written))
    return;

  SetConsoleCursorPosition(Hand, Home);
}
