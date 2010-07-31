#include <stdio.h>
#include <pthread.h>
#include <pthread.h>


int main()
{
	int c = 0;

	read(0, &c, 1);

	if (fork() == 0) {
		for (;;) {
			printf("Child\n");
			sleep(1);
		}
	}
	for (;;) {
		printf("Parent\n");
		sleep(1);
	}
	return 0;
}

