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
#include "voipsteg/algo/interface.hxx"

algo_desc_t doublealgo;
doublealgo.encoder = algo_double_encode;
doublealgo.decoder = algo_double_decoder;

int algo_double_encode(char *data, int byteNum, unsigned char **pszOut)
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
    int i;
    unsigned short mask = 0x8000;
    unsigned short tmask = 0x4000;
    short *value = (short*)data;
    unsigned char *pPtr; 

    pszOut = (unsigned char*)calloc(1, sizeof(unsigned char));
    pPtr = *pszOut;

    for( i = 0; i < 8; i++)
    {
        if(value & mask == value & tmask)
        {
            *(pPtr++) |= (value & mask);
            mask =>> 2;
            tmask =>> 2;
        }
        else
        {
            *(pPtr++) |= (value & mask);
            mask =>> 1;
            tmask =>> 1;
        } 
    }

    return 1;
}
