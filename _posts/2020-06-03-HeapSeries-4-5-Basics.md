---
layout: post
title: "Heap Series: 4/5 Heap basics from a practical perspective"
date: 2020-06-03 09:40:00 +0200
tags: [Heap, C, Linux, GDB]
author: Mario
categories: research
excerpt_separator: <!--more-->
---



In the [previous part]({% post_url 2020-06-03-HeapSeries-3-5-Basics %}) of this series we had a look to he structures used by the heap manager to keep track of the allocated/freed chunks. As we learned from [the second part]({% post_url 2020-06-03-HeapSeries-2-5-Basics %}), the chunks could be in two states (whether they're in use or not), and we _peekaboo_ just the tip of that iceberg making use of GDB and a couple of plugins. 

Let's dig into those structures we mentioned before, the Bins or FreeLists used by the heap manager.
<!--more-->

---

### Bins or Freelists

The main idea behind the `bins` implementations is the following: EFFICIENCY. 

Programmers tend to request the same size allocations over and over. Usually because their structures are the same size and because humanz tend to program in a way that everything looks organized, thus requesting allocations of 10, 50, 100, 110 or so... round numbers, grouping the frees and mallocs... That kind of stuff.  

Therefore, the developers of this implementation decided that keeping lists with the used chunks would be of benefit, as those chunks sizes were likely to be requested over and over during the thread execution. 


There are 5 different `bins`: 


#### TCache bins or Per-Thread Cache bins

The TCache bins are an optimization layer added to the heap manager in order to prevent race-conditions when requesting heap operations. As by their name, these are bins provided **for each thread** in a program. 

Picture this: A program with multiple threads attempts to request a memory allocation on every thread at the same time. How would the heap manager do this? Setting up a lock, making the reservation, returning the pointer and then releasing the lock seems like a reasonable approach. 

However, a lock is expensive. It sequentializes access to a resource, and thus _slows_ down the execution of the program. 

Instead of that, we could provide a memory arena for each thread to play with. Also, instead of firing up locks every time a memory allocation is requested, how about having a cache that contains the last requested/freed allocations to quickly retrieve memory from? That's exactly what the TCache bins try to do.

- There's 64 TCache bins per thread. These are **single**-linked lists. Each one, can contain up to 7 chunks of the same size. 
- The chunks at the TCache bins range from 16 to 516 Bytes on a 32bit implementation, and 24 to 1032 on a 64bit implementation. 
- These are intended to be fast. As fast as possible, therefore they're never marked as free. I.e: Remember the _Previous in use_ flag at the `A|M|P` section of the chunk? The next chunk's `P` is not set to 0.

Let's take a look at these with GDB. 

##### `p4_demo1.c`
{% highlight c linenos %}
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

{% endhighlight %}

All right! Fire up GDB with the `p4_demo1` in it, set a breakpoint to last line of code (24 if you followed the p4_demo1.c), aaaand let it run. 

![GDB init and break to L#24](/assets/heapseries/4/gdb1.png)

You should see something like this: 

![GDB about to finish](/assets/heapseries/4/gdb2.png)


A quick peek into the heap, making use of HeapInspect with `hi heap`, provides some useful information about how are the things under the hood: 

![HeapInspect hi heap cmd](/assets/heapseries/4/gdb3.png)


That's cool, however we want to go to the bone with this. Let's ask HeapInspect to give us a more insight into the TCache bin making use of `hi tcache raw all`

![HeapInspect tell me more](/assets/heapseries/4/gdb4.png)

That's better. 

- As we can see in the last pic, the chunk address starts 0x10Bytes before our actual playground memory area. (due to the two SIZE_SZ making part of the metadata added to our memory allocation, remember [part 2]({% post_url 2020-06-03-HeapSeries-2-5-Basics %})?)
- The `fd` points to the next chunk's `fd`, or memory user area... Wait, what? I thought `fd` should point to the next chunk. Not to the next chunk's `fd`... Well, if we check [GLIBC's code](https://elixir.bootlin.com/glibc/glibc-2.28/source/malloc/malloc.c#L4151), we can see how the `free` procedure follows up to [`tcache_put`](https://elixir.bootlin.com/glibc/glibc-2.28/source/malloc/malloc.c#L4159), where the chunk is "converted" back from its chunkiness to a _mem_ via the [`chunk2mem`](https://elixir.bootlin.com/glibc/glibc-2.28/source/malloc/malloc.c#L1171) macro. Which turns out to return a pointer to the `beginning of the chunk + 2 * SIZE_SZ`,  which is in fact the address returned by a `malloc` call. (WTFoo!). 
- The `BK` points to the heap_base +0x10B (Single-linked list remember?)


#### FastBins

There's 10 of these. LIFO (as said before). Each fastBin keeps track of chunks of the same size:

- 16, 24, 32, 40, 48, 56, 64, 72, 80 and 88 Bytes on a 32bit implementation. On 8 by 8 Bytes steps.
- 32, 48, 64, 80, 96, 112, 128, 142 and 160 Bytes on a 64bit implementation. On 16 by 16 Bytes steps.

The maximum size of an allocation to be regarded as an item for a FastBin is `80 * SIZE_SZ / 4`. Where `SIZE_SZ` is the same size as the size of a pointer, 4Bytes for 32bit and 8Bytes for a 64bit architecture. This is defined as a macro at GLIBC implementation code.

So, if we request an allocation of memory under 160Bytes on a 64bit architecture, once freed it will be referenced at the FastBin if the `TCACHE` entry bin for that size is already filled up... 

##### `p4_demo2.c`

{% highlight c linenos %}
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

{% endhighlight %}



Once again, run GDB and with the `p4_demo2` in it, set a breakpoint to last line of code (31 if you followed the p4_demo2.c), aaaand let it run. 


![GDB init and break to L#31](/assets/heapseries/4/gdb5.png)


You should see something like this: 

![GDB about to finish](/assets/heapseries/4/gdb6.png)


A quick peek into the heap, making use of HeapInspect with `hi heap`, provides some useful information about how are the things under the hood: 

![HeapInspect hi heap cmd](/assets/heapseries/4/gdb7.png)

Indeed there are our `tcache 0x20 size`'s chunks freed and, right above them, the `fastbins 0x20` chunks we `free`d at the last loop (L#26-27). 

That's cool, however we want to go to the bone with this. Let's ask HeapInspect to give us a more insight into the TCache bin making use of `hi fastbins raw all`

![HeapInspect tell me more](/assets/heapseries/4/gdb8.png)

That's better. 

As explained before, the `fd` pointer of the elements on a FastBin points to the **next chunk** not to the next chunk's `fd`. Let's take a look to this. 

By inspecting the memory at, let's say the 4th, `chunks_at_fastbin[3]` we can see how `malloc` allocated some memory for us at `0x5555555597b0`, and that the chunk begins 0x10 Bytes before, thus at `0x5555555597a0`.

![HeapInspect 4th fastbin chunk](/assets/heapseries/4/gdb9.png)

The `fd` pointer for this chunk, points to `0x555555559780`, which is indeed the beginning of the next chunk for the pointer returned by `malloc` at `0x555555559790`.


![HeapInspect 3th fastbin chunk](/assets/heapseries/4/gdb10.png)



---


I hope this all makes sense. Play with the examples provided until it sticks :)

Also, feel free to contact us via our Twitter handle [@GhostLineS3c](https://twitter.com/GhostLineS3c).

Or open an issue if there's some mistake on our [@GhostLineSecurity](https://github.com/GhostLineSecurity).


See you on the next chapter of this series! 






































