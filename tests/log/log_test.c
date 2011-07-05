#include "voipsteg/logger.h"


int main()
{

   vslog_set_level(E_ALL);


    SYS_LOG(E_DEBUG, "Hello World no. %d\n", 1);
    SYS_LOG(E_NOTICE, "Hello World no. %d\n", 1);
    SYS_LOG(E_WARNING, "Hello World no. %d\n", 1);
    SYS_LOG(E_ERROR, "Hello World no. %d\n", 1);
    SYS_LOG(E_EXCEPTION, "Hello World no. %d\n", 1);

    vslog_set_level(E_ERROR);
    SYS_LOG(E_NOTICE, "Hello World no. %d\n", 2);
    SYS_LOG(E_WARNING, "Hello World no. %d\n", 2);
    SYS_LOG(E_ERROR, "Hello World no. %d\n", 2);
    SYS_LOG(E_EXCEPTION, "Hello World no. %d\n", 2);

}
