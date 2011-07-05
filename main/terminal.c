#include <stdio.h>
#include <string.h>
#include "voipsteg/terminal.h"


char *vsutils_term_printcolor(char *outbuf, const char* inbuf, int fgcolor, int bgcolor, int maxout)
{
    // TODO: check terminal background color !!!
    snprintf(outbuf, maxout, "%c[%d;%d;1m%s %c[0m", ESC, bgcolor, fgcolor, inbuf, ESC);   
    return outbuf;
}



