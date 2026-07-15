#include "syshead.h"
#include "utils.h"

extern int debug;//外部声明

int run_cmd(char *cmd, ...)
{
    va_list ap;
    char buf[CMDBUFLEN] = {0};
    va_start(ap, cmd);
    vsnprintf(buf, CMDBUFLEN, cmd, ap);

    va_end(ap);

    if (debug)
    {
        printf("EXEC:%s\n", buf);    
    }

    return system(buf);    
}

