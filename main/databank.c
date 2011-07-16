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

#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "voipsteg/databank.h"
#include "voipsteg/config.h"

#define DB_MAX_FILENAME 128
#define DB_CHUNK_SIZE   1


static pthread_mutex_t _dbsync;
static unsigned char *gsData;
static int  _byteNum,
            _byteNumMax,
            _bitNum,
            _bReady;
static char _bitMasks[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
static char _szFilename[DB_MAX_FILENAME];

static int _db_read_file(const char *filename, char **pszOut);
static int _db_write_chunk(const unsigned char *chunk, int size);

static int db_double_encode(char *data, int byteNum, unsigned char **pszOut);

int db_init_data()
{
    #ifdef SEND
        const vsconf_value_t *filename = vsconf_get("filename");
        int fileByteNum;
        char *fileData;
        int i = 0;
    
        pthread_mutex_init(&_dbsync, NULL);
        if(filename->isvalid) 
        {
            if( (fileByteNum = _db_read_file(filename->value.strval, &fileData)) != -1)
            {
                SYS_LOG(E_NOTICE, "<db_init_data> Successfully read %d bytes", fileByteNum);
                _byteNumMax = db_double_encode(fileData, fileByteNum, &gsData);
                _bReady  = 1;
                _byteNum = 0;
                _bitNum  = 7;
                free(fileData);
            }
            else
            {
                SYS_LOG(E_ERROR, "<db_init_data> Something terrible happen :)");
                return -1;
            }
        }
        else
        {
            SYS_LOG(E_ERROR, "<db_init_data> Incomplete configurationi file");
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

int db_dispose()
{
    SYS_LOG(E_DEBUG, "<db_dispose> Closing Databank...");
    free(gsData);
}

unsigned char db_pick_bit()
{
    pthread_mutex_lock(&_dbsync);
    int ret = 0x00;

    if( !_bReady )
    {
        SYS_LOG(E_ERROR, "Data is not ready");
        pthread_mutex_unlock(&_dbsync);
        return DB_NOT_READY;
    }

    if(_byteNum > _byteNumMax)
    {
        SYS_LOG(E_DEBUG, "<db_pick_bit> Nothing to send");
        pthread_mutex_unlock(&_dbsync);
        return DB_NO_DATA;
    }

    ret = ((gsData[_byteNum] & _bitMasks[_bitNum]) >> (_bitNum));
    //SYS_LOG(E_DEBUG, "<db_pick_bit> RET: %d, %x, %d", ret, gsData[_byteNum], _bitMasks[_bitNum] );

    pthread_mutex_unlock(&_dbsync);
    return ret;
}

void db_pop_bit()
{
    pthread_mutex_lock(&_dbsync);

    --_bitNum;
    if(_bitNum < 0)
    {
        SYS_LOG(E_NOTICE, "Byte sent");
        _bitNum = 7;
        _byteNum++;
    }

    pthread_mutex_unlock(&_dbsync);
}

int db_put_bit(int bit)
{
    if( !_bReady )
    {
        SYS_LOG(E_ERROR, "Data is not ready");
        pthread_mutex_unlock(&_dbsync);
        return -1;
    }

    pthread_mutex_lock(&_dbsync);

    if( ( bit & 0x01 ) == 1 )
    {
        gsData[_byteNum] |= _bitMasks[_bitNum];
    }
    --_bitNum;

    if(_bitNum < 0)
    {
        _bitNum = 7;
        _byteNum++;

        if(_byteNum == DB_CHUNK_SIZE)
        {
            _db_write_chunk(gsData, DB_CHUNK_SIZE);
            memset(gsData, 0x00, DB_CHUNK_SIZE);
            _byteNum = 0;
        }
    }

    pthread_mutex_unlock(&_dbsync);
}

int db_double_encode(char *data, int byteNum, unsigned char **pszOut)
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

        //SYS_LOG(E_DEBUG, "Encoded byte %x", m);
        *(pPtr++) |= (char)(m >> 8);
        *(pPtr++) |= (char)(m);
        
        mask = 0x80;
        m = 0x0000;
    }

    return (2*byteNum);
}

int _db_read_file(const char *filename, char **pszOut)
{
    int result;
    struct stat st;
    FILE *pFile;

    stat(filename, &st);
    *pszOut = (char*)calloc(st.st_size, sizeof(char));


    pFile = fopen(filename, "rb");
    if( pFile == NULL )
    {
        SYS_LOG(E_ERROR, "Error opening file %s", filename);
        return -1;
    }

    result = fread(*pszOut, sizeof(char), st.st_size, pFile);
    if( result != st.st_size )
    {
        SYS_LOG(E_ERROR, "Error reading file %s", filename);
        return -1;
    }

    fclose(pFile);
    return st.st_size;
}

static int _db_write_chunk(const unsigned char *chunk, int size)
{
    int result;
    FILE *pFile;

    pFile = fopen(_szFilename, "ab");
    if( pFile == NULL )
    {
        SYS_LOG(E_ERROR, "Cannot open file %s for writing", _szFilename);
        return -1;
    }
    
    result = fwrite(chunk, sizeof(char), size, pFile);
    if( result != size )
    {
        SYS_LOG(E_ERROR, "Error writing file %s", _szFilename);
        return -1;
    }

    SYS_LOG(E_DEBUG, "Chunk written");
    fclose(pFile);
    return 1;
}
