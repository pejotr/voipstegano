
#ifndef _VOIPSTEG_LOGGER_H_
#define _VOIPSTEG_LOGGER_H_


#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>
#include "voipsteg/terminal.h"

#define SYS_LOG(LEVEL, FORMAT, ...) vslog_log((LEVEL), __LINE__, __FILE__, FORMAT, ##__VA_ARGS__);
#define NEVER_GET_HERE() vslog_log(E_ERROR, __LINE__, __FILE__, "Undefined behaviour")

enum LOG_LEVEL { 
    E_ALL       ,

    E_DEBUG     = 0x01, 
    E_NOTICE    /* = 0x02 */, 
    E_WARNING   /* = 0x04 */, 
    E_ERROR     /* = 0x08 */, 
    E_EXCEPTION /* = 0x10 */,
 
    E_NONE      /* = 0x12 */
};

typedef struct {
    short value;
    int   color;
    char  symbol;
} vslog_levelnfo_t ;

//! Set minimal message level to be logged
void vslog_set_level(int l);

void vslog_log(LOG_LEVEL lev, int lineno, const char *file, const char *format, ...);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif /* _VOIPSTEG_LOGGER_H_  */
