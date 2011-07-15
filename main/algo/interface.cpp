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

#include "voipsteg/config.h"
#include "voipsteg/algo/interface.hpp"

IAlgorithm::IAlgorithm()
{
    #ifdef SEND
        const vsconf_value_t *filename = vsconf_get("filename");
        int fileByteNum;
        char *fileData;
        int i = 0;
    
        pthread_mutex_init(&mSync, NULL);
        if(filename->isvalid) 
        {
            if( (fileByteNum = _db_read_file(filename->value.strval, &fileData)) != -1)
            {
                SYS_LOG(E_NOTICE, "<IAlgorithm> Successfully read %d bytes", fileByteNum);
                mByteNumMax = db_double_encode(fileData, fileByteNum, &gsData);
                mbReady  = 1;
                mByteNum = 0;
                mBitNum  = 7;
                free(fileData);
            }
            else
            {
                SYS_LOG(E_ERROR, "<IAlgorithm> Something terrible happen :)");
                return -1;
            }
        }
        else
        {
            SYS_LOG(E_ERROR, "<IAlgorithm> Incomplete configurationi file");
            assert(0);
            return -1;
        }
        return 1;
    #else
        const vsconf_value_t *filename = vsconf_get("dstfile");
        int len = strlen(filename->value.strval);

        SYS_LOG(E_NOTICE, "Initializing databank for receiver")
        if( ( filename->isvalid ) && ( len < DB_MAX_FILENAME ) )
        {
            SYS_LOG(E_DEBUG, "Correct file name");
            strncpy(_szFilename, filename->value.strval, len);
            gsData = (unsigned char*)calloc(DB_CHUNK_SIZE, sizeof(unsigned char));
            _bReady = 1;
            _bitNum = 7;
            _byteNum = 0;
            return 1;
        }
        else
        {
            SYS_LOG(E_WARNING, "Cannot create destination file - filename is too long");
            assert(0);
            return -1;
        }
    #endif
}

int IAlgorithm::pick_bit()
{
    pthread_mutex_lock(&mSync);
    int ret = 0x00;

    if(mByteNum > mByteNumMax)
    {
        SYS_LOG(E_DEBUG, "<db_pick_bit> Nothing to send");
        pthread_mutex_unlock(&mSync);
        return DB_NO_DATA;
    }

    ret = ((mpBuffer[_byteNum] & IAlgorithm::bitMasks[mBitNum]) >> (mBitNum));

    pthread_mutex_unlock(&mSync);
    return ret;
}

void IAlgorithm::pop_bit()
{
    pthread_mutex_lock(&mSync);

    --mBitNum;
    if(mBitNum < 0)
    {
        SYS_LOG(E_NOTICE, "Byte sent");
        mBitNum = 7;
        mByteNum++;
    }

    pthread_mutex_unlock(&mSync);
}

void IAlgorithm::put_bit(int bitval)
{
    pthread_mutex_lock(&mSync);

    if( ( bit & 0x01 ) == 1 )
    {
        mpBuffer[mByteNum] |= mBitMasks[mBitNum];
    }
    --mBitNum;

    if(mBitNum < 0)
    {
        mBitNum = 7;
        mByteNum++;

        if(mByteNum == DB_CHUNK_SIZE)
        {
            _db_write_chunk(gsData, DB_CHUNK_SIZE);
            memset(mpBuffer, 0x00, DB_CHUNK_SIZE);
            mByteNum = 0;
        }
    }

    pthread_mutex_unlock(&mSync);
}

int IAlgorithm::write_chunk()
{
    int result;
    FILE *pFile;

    pFile = fopen(mszFilename, "ab");
    if( pFile == NULL )
    {
        SYS_LOG(E_ERROR, "Cannot open file %s for writing", mszFilename);
        return -1;
    }
    
    result = fwrite(mChunk, sizeof(char), 1, pFile);
    if( result != size )
    {
        SYS_LOG(E_ERROR, "Error writing file %s", mszFilename);
        return -1;
    }

    SYS_LOG(E_DEBUG, "Chunk written");
    fclose(pFile);
    return 1;
}
