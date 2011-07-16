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

#ifndef _VOIPSTEG_ALGO_
#define _VOIPSTEG_ALGO_


#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef int (*algo_encode_func)(char *data, int byteNum, unsigned char **pszOut);
typedef int (*algo_decode_func)(char *data, unsigned char **pszOut);

typedef struct algorithm_description {
    algo_encode_func encoder;
    algo_decode_func decoder;
    int bytes_per_char;
} algo_desc_t;

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif /* _VOIPSTEG_ALGO_ */
