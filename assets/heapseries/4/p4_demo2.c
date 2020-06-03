#include <stdio.h>
#include <stdlib.h>

#define MAX_TCACHE_SIZE 7
#define FASTBIN_CHUNKS 10
#define MY_CHUNK_SIZE 0

int main(int argc, char **argv) {

	void *chunks_at_tcache[MAX_TCACHE_SIZE];
	void *chunks_at_fastbin[FASTBIN_CHUNKS];
		for(int i = 0; i < MAX_TCACHE_SIZE; ++i)
			chunks_at_tcache[i] = malloc(MY_CHUNK_SIZE);

	printf("Requesting some more chunks to be stored at FastBin\n");
		for(int i = 0; i < FASTBIN_CHUNKS; ++i) {
			chunks_at_fastbin[i] = malloc(MY_CHUNK_SIZE);
			printf("\t|...chunk@%p\n", chunks_at_fastbin[i]);
		}

	printf("Filling up TCache bin for min. size...\n");
		for(int i = 0; i < MAX_TCACHE_SIZE; ++i)
			free(chunks_at_tcache[i]);

	printf("Calling free over FastBin chunks...\n");
		for(int i = 0; i < FASTBIN_CHUNKS; ++i)
			free(chunks_at_fastbin[i]);

	printf("Done! Chek out the FastBin :)\n");

	return 0;
}
