#include <stdio.h>
#include <stdlib.h>

#define MAX_TCACHE_SIZE 7
#define UNSHORTEDBIN_CHUNKS 10
#define UNSORTED_CHUNK_SIZE 160

int main(int argc, char **argv) {

	void *chunks_at_tcache[MAX_TCACHE_SIZE];
	void *chunks_at_unsortedbin[UNSHORTEDBIN_CHUNKS];
	for(int i = 0; i < MAX_TCACHE_SIZE; ++i)
		chunks_at_tcache[i] = malloc(UNSORTED_CHUNK_SIZE);

	printf("Requesting some more chunks to be stored at UnsortedBin\n");
	for(int i = 0; i < UNSHORTEDBIN_CHUNKS; ++i) {
		chunks_at_unsortedbin[i] = malloc(UNSORTED_CHUNK_SIZE);
		printf("\t|...chunk@%p\n", chunks_at_unsortedbin[i]);
	}

	printf("Filling up TCache bin for 160B request in size...\n");
	for(int i = 0; i < MAX_TCACHE_SIZE; ++i)
		free(chunks_at_tcache[i]);


	for(int i = 0; i < UNSHORTEDBIN_CHUNKS - 5; ++i)
		free(chunks_at_unsortedbin[i]);

	printf("Done! Chek out the UnsortedBin for 5 * 176 size chunk :)\n");

	for(int i = 5; i < UNSHORTEDBIN_CHUNKS; ++i)
		free(chunks_at_unsortedbin[i]);

	return 0;
}
