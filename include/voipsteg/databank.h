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

#ifndef _VOIPSTEG_DATABANK_
#define _VOIPSTEG_DATABANK_


#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define DB_NO_DATA      0x81
#define DB_NOT_READY    0x82 

typedef int (*data_encode_fun)(char *data, int byteNum, char **pszOut);

//! Initialize databank
/*!
    Depending on configuration a file can read and stored in byte array
    or a message from conf file.
    This function has to be called after reading configuration file.
    @see vsconf_readconfig();:
 */
int db_init_data();

//! Shutdown databank and free allocated memory
int db_dispose();

//! Return bit to be send
/*!
    Depending on bit value two valid value can be returned 0x01 or 0x00.
    Function is thread safe.
    @return 0x01 bit of value 1 should be send
            0x00 bit of value 0 should be send
            negative - error
 */
unsigned char db_pick_bit();

void db_pop_bit();

//! Puts bit in buffer
/*!
    Puts least significant bit of parameter into the buffer.
 */
int db_put_bit(int bit);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif /* _VOIPSTEG_DATABANK_ */
