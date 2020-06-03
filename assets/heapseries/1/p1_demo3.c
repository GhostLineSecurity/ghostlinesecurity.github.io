#include <stdio.h>
#include <stdlib.h>

#define CHUNK_NUMBER 5
#define CHUNK_SIZE 10

int main(int argc, char **argv) {

	void *chunks[CHUNK_NUMBER];

	printf("Requesting a few chunks...\n");
	for (int i = 0; i < CHUNK_NUMBER; ++i) {
		chunks[i] = malloc(CHUNK_SIZE);
		printf("\t|...chunks[%i]@%p\n", i, chunks[i]);
	}

	printf("\nFreeing up chunks...\n");
	for (int i = 0; i < CHUNK_NUMBER; ++i)
		free(chunks[i]);

	printf("Requesting a few chunks...\n");
	for (int i = 0; i < CHUNK_NUMBER; ++i) {
		chunks[i] = malloc(CHUNK_SIZE);
		printf("\t|...chunks[%i]@%p\n", i, chunks[i]);
	}

	printf("\nFreeing up chunks...\n");
	for (int i = 0; i < CHUNK_NUMBER; ++i)
		free(chunks[i]);

	return 0;
}
