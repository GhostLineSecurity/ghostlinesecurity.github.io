#include <stdio.h>
#include <stdlib.h>

#define MAX_TCACHE_SIZE 7
#define MY_CHUNK_SIZE 0

int main(int argc, char **argv) {

	void *chunks_at_tcache[MAX_TCACHE_SIZE];

	printf("Requesting some chunks!\n");
		for(int i = 0; i < MAX_TCACHE_SIZE; ++i) {
			chunks_at_tcache[i] = malloc(MY_CHUNK_SIZE);
			printf("\t|...chunk@%p\n", chunks_at_tcache[i]);
		}
	printf("All chunks allocated!\n");
	printf("Calling free over them...\n");
		for(int i = 0; i < MAX_TCACHE_SIZE; ++i)
			free(chunks_at_tcache[i]);

	printf("Chunks freed! Take a look at the TCache bin :)\n");


	return 0;
}
