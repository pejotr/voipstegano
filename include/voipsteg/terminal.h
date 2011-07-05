/* -*- Mode: C++; shiftwidth = 4; softtabstop=4; expandtab; -*- */
/*
 * Copyright (C) 2010  Piotr Doniec, Warsaw University of Technology
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _VOIPSTEG_TERM_H_
#define _VOIPSTEG_TERM_H_

#if defined(__cplusplus) ||  defined(c_plusplus)
extern "C" {
#endif

#define ESC 0x1b

/*! \name Terminal Attributes
*/
/*@{*/
#define ATTR_RESET 0
#define ATTR_BOLD 1
#define ATTR_UNDER 4
#define ATTR_BLINK 5
/*@}*/

/*! \name Terminal Colors
*/
/*@{*/
#define COLOR_BLACK 30
#define COLOR_RED 31
#define COLOR_GREEN 32
#define COLOR_BLUE 34
#define COLOR_CYAN 36
#define COLOR_WHITE 37
#define COLOR_YELLOW 33
/*@}*/


char *vsutils_term_printcolor(char *outbuf, const char* inbuf, int fgcolor, int bgcolor, int maxout);

#if defined(__cplusplus) ||  defined(c_plusplus)
}
#endif

#endif /* _VOIPSTEG_TERM_H_ */
