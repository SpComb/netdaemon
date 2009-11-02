#include "helloworld.h"

// printf
#include <stdio.h>

int nd_helloworld (void)
{
    return printf("Hello World!\n") >= 0 ? 0 : -1;
}
