/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4:  */
/*
 * Copyright (C) 2011  Piotr Doniec, Warsaw University of Technology
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "voipsteg/algo/interface.hxx"
#include "voipsteg/algo/double.hxx"

algo_desc_t doublealgo;

void algo_double_init()
{
    doublealgo.encoder = algo_double_encoder;
    doublealgo.decoder = algo_double_decoder;
}

int algo_double_encoder(char *data, int byteNum, unsigned char **pszOut)
{
    int i, j, k;
    unsigned short m = 0, bit;
    unsigned short mask = 0x0080;
    unsigned char *pPtr;

    *pszOut = (unsigned char*)calloc(byteNum*2, sizeof(unsigned char));
    pPtr = *pszOut;

    for( i = 0; i < byteNum; ++i)
    {
        for(j = 0, k = 0; j < 8; ++j, k += 2)
        {
            bit = (data[i] & mask) << (j+8);
            m |= (bit >> (k + 0) );
            m |= (bit >> (k + 1) );

            mask = mask >> 1; 
        }

        *(pPtr++) |= (char)(m >> 8);
        *(pPtr++) |= (char)(m);
        
        mask = 0x80;
        m = 0x0000;
    }

    return (2*byteNum);
}


int algo_double_decoder(char *data, unsigned char **pszOut)
{
    int i, j;
    unsigned char mask = 0x80;
    unsigned char tmask = 0x40;
    unsigned char *pPtr, p1 = 0x00, p2 = 0x00, r;
    unsigned short v1, v2;
    unsigned char value = data[0];

    *pszOut = (unsigned char*)calloc(1, sizeof(unsigned char));
    pPtr = &p1;

    for( j = 0; j < 2; j++ )
    {
        for( i = 0; i < 4; i++ )
        {
            v1 = (value & mask) ;
            v2 = (value & tmask);

            *(pPtr) |= (value & mask)  << i;

            if( v1 == (v2  << 1) )
            {
                mask >>= 2;
                tmask >>= 2;
            }
            else
            {
                mask >>= 1;
                tmask >>= 1;
            } 
        }
    
        value = data[1];
        mask = 0x80;
        tmask = 0x40;
        pPtr = &p2;
    }

    r = p1 | (p2 >> 4);
    memcpy(*pszOut, &r, 1); 

    return 1;
}
