#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {

	void *a = malloc(10);
	printf("Allocated a@%p\n", a);
	free(a);
	printf("\ta got freed\n");

	void *b = malloc(10);
	printf("Allocated b@%p\n", b);
	free(b);
	printf("\tb got freed\n");

}
