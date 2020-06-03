#include <stdio.h>
#include <stdlib.h>


int main(int argc, char **argv) {
    printf("Requesting to allocate 0Bytes\n");
    char *minimal = (char*)malloc(0);	// 0Bytes requested!
    printf("\t|...minimal@%p\n", minimal);

    // Offset to find size of the allocated area
    unsigned long size_offset = sizeof(size_t);

    // Just lost the last 3 bits, I know :) keep reading!
    size_t minimal_real_size = *(minimal - size_offset) >> 3 << 3;

    printf("Actual size of minimal:%zu\n", minimal_real_size);

    free(minimal);

    return 0;
}
