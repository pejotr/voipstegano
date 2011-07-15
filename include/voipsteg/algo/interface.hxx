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

#include <pthread.h>


#ifndef _ALGO_INTERFACE_
#define _ALGO_INTERFACE_

const int DB_NO_DATA      0x81
const int DB_NOT_READY    0x82 

class IAlgorithm
{
public:
    IAlgorithm();
    virtual ~IAlgorithm();

    static char bitMasks[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

protected:
    virtual bool    encode(char*, int) = 0;

    virtual int     pick_bit();
    virtual void    pop_bit();
    virtual void    put_bit(int);

    virtual int     write_chunk();

    const std::string     mszFilename;
    char            mChunk;
    char*           mpBuffer;
    pthread_mutex_t mSync;
    int             mByteNum;
    int             mByteNumMax;
    int             mBitNum;
    int             mbReady;
};

#endif
