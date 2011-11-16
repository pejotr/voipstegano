#include "voipsteg/logger.h"

int log_level__;

const vslog_levelnfo_t LOG_LEVEL_INFO[] = {
    { 0x00, COLOR_BLACK,  ' ' },
    { 0x01, COLOR_GREEN,  'D' },
    { 0x02, COLOR_WHITE,  '$' },
    { 0x04, COLOR_YELLOW, '%' },
    { 0x08, COLOR_RED,    '*' },
    { 0x10, COLOR_RED,    '!' }
};

void vslog_set_level(int l)
{
    // Check if level is proper value
    if( l > 0x1F || ( l % 2 != 0 && l != 0x01 )  ) {
        return;
    }

    log_level__ = l;
}


void vslog_log(LOG_LEVEL lev, int lineno, const char *file, double milis, const char *format, ...) 
{
    
    vslog_levelnfo_t levelnfo;
    char msg[256]; 
    char temp[512], colorized[512], loc[128];

    levelnfo = LOG_LEVEL_INFO[lev];

    if( ( lev != E_ALL ) && ( lev != E_NONE ) && (!(lev >= log_level__)) ) {
        return;
    }

    va_list args;
    va_start (args, format);
    vsprintf(msg, format, args);
  
    snprintf(loc, 128, "%s:%d", file, lineno);
    snprintf(temp, 512, "%f,%c,%-30s,%s", milis, levelnfo.symbol, loc, msg); 

    vsutils_term_printcolor(colorized, (const char*)temp, levelnfo.color, COLOR_BLACK, sizeof(colorized)/sizeof(char));

    printf("%s\n", colorized);
}
