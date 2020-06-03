#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	void *a = malloc(10);
	printf("We requested 10B allocated at %p\n", a);

	free(a);
	printf("The memory just got freed\n");

	return 0;
}
