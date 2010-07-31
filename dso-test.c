/* gcc -fPIC -shared -nostartfiles dso-test.c -o /tmp/i.so */
#include <stdio.h>


void _init()
{
	fprintf(stderr, "Yo from init()\n");
}


