#include <assert.h>
#include "voipsteg/config.h"

int main(int argc, char** argv)
{
    const vsconf_value_t* val;

    //printf("%s", argv[1]);

    vsconf_readconfig("config.xml");

    val = vsconf_get("sip-queuenum-a");
    assert(val->isvalid == true);
    assert(val->isstr == true);
    assert( strcmp(val->value.strval, "1") == 0);
    
    printf("Test 1 passed...\n");

    
    val = vsconf_get("sender");
    assert(val->isvalid == true);
    assert(val->isstr == true);
    assert( strcmp(val->value.strval ,"sip:pdoniec@sip.elka.pw.edu.pl") == 0 );
    
    printf("Test 2 passed...\n");

    val = vsconf_get("undefined");
    assert(val == NULL);

    printf("Test 3 passed...\n");

}

