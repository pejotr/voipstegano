#include "voipsteg/utils.h"
#include "md5/md5.h"

void utils_md5hash(const char *pPlain, char *pDigest)
{ 
    md5_state_t     state;
    md5_byte_t      digest[16];

    md5_init(&state);
    md5_append(&state, (const md5_byte_t*)pPlain, strlen(pPlain));
    md5_finish(&state, digest);

    memcpy(pDigest, digest, 16);

}

//! Convert MD5 sum to human readable form
void utils_char2hex(const char* pMsg, int len, char* pOut)
{
    unsigned char byte;

    for(int i = 0; i < len; ++i) {
        byte = pMsg[i];
        sprintf(pOut+2*i, "%02x", byte);
    }
}

//! Calculates difference between two time moments
struct timeval utils_difftimeval(const struct timeval *pStart, const struct timeval *pStop)
{
    struct timeval diff;

    diff.tv_sec  = pStop->tv_sec  - pStart->tv_sec;
    diff.tv_usec = pStop->tv_usec - pStart->tv_usec;
    if( diff.tv_usec < 0 )
    {
        diff.tv_usec += 1000000;
        --diff.tv_sec;
    }

    return diff;
}
