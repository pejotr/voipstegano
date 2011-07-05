/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4:  */
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

#if defined(__cplusplus) ||  defined(c_plusplus)
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define DO_NOTHING() ;

//! Calculate MD5 hash value of given string
void utils_md5hash(const char *pPlain, char *pDigest);

//! Convert MD5 sum to human readable form
/*!
    @param: message to calculate md5 sum
    @param: message length
    @return: NULL terminated digest
 */
void utils_char2hex(const char* pMsg, int len, char* pOut);

//! Calculates difference between two time moments
struct timeval utils_difftimeval(const struct timeval *pStart, const struct timeval *pStop);


#if defined(__cplusplus) ||  defined(c_plusplus)
}
#endif
