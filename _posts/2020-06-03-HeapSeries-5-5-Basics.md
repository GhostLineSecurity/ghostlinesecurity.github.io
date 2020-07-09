---
layout: post
title: "Heap Series: 5/5 Heap basics from a practical perspective"
date: 2020-06-03 09:44:00 +0200
tags: [Heap, C, Linux]
author: MrI/O
categories: research
excerpt_separator: <!--more-->
---


In the [previous part]({% post_url 2020-06-03-HeapSeries-4-5-Basics %}), we talked about the TCache and FastBin freelists, by making use of a couple of code-samples that would make some memory allocations and allowed us to see how the heap management was taking care of the frees and mallocs performed. 


In this part, we will talk about the unsorted, small and large bins. We'll go through some samples to verify our assumptions and make sure that everything is understood :)

<!--more-->

Let's dig once again into heap's internals!


### Bins

#### UnsortedBin

Here's where all the rests from splitting chunks and returned chunks end up. Unless they're "stored" at Fastbin or TcacheBins. 
This structure acts like a queue where chunks are placed when `freed` and retrieved from when `malloc`_ated_ (is that even a word? o_Õ).

Weird and probably confusing description taken from the [GLIBC comments](https://elixir.bootlin.com/glibc/glibc-2.28/source/malloc/malloc.c#L1494), so let's take a look:

##### `p5_demo1.c`

{% highlight c linenos %}
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

        printf("Done! Check out the UnsortedBin for 5 * 176 size chunk :)\n");

        for(int i = 5; i < UNSHORTEDBIN_CHUNKS; ++i)
                free(chunks_at_unsortedbin[i]);

        return 0;
}
{% endhighlight %}


Good... fire up GDB  set a `b`reakpoint at line 31, and let it run.

![GDB init and break to L#31](/assets/heapseries/5/gdb1.png)

[...]

![GDB source section once breakpoint @L#31 is hit](/assets/heapseries/5/gdb2.png)

Noice, we hit the breakpoint as expected. Let's inspect the heap, focusing on the UnsortedBins: 

![UnsortedBins contents](/assets/heapseries/5/gdb3.png)


Right... so it seems to have one single chunk of size 0x371 (in-use bit set, so it's 0x370 in fact). If we follow the code, we have already freed up 5 chunks of size 160B, well at least that's the size we requested... However keep in mind that the heap is 16B aligned for 64bit arch and that we need some extra space for the metadata thing we talked about in previous posts.

Therefore we need another two 64bit pointer at the top of our memory allocation, which turn out to be 8B each for a 64bit arch, thus making 160B + 16B = 176B. 

Given that we already freed up 5 the total coalesced size of the UnsortedBin's chunk size should be 176B * 5 = 880B. Which in hex is 0x370. Yay!



#### SmallBins

There's [64](https://elixir.bootlin.com/glibc/glibc-2.28/source/malloc/malloc.c#L1445) of these. FIFO, and keeping same size chunks on each. These are represented as double-linked lists, where a chunk keeps a pointer to the next and previous chunks. 

> However, only 62 are usable, given that Bin 0 does not exist, and the Bin 1 is the unsorted list.


The insertions are made at the _head_ part of the list, and removals at the _tail_ thus the FIFO behavior. 


These keep chunks of size under 512Bytes on 32bit and under 1024Bytes on a 64bit architecture. [See some GLIBC code ;)](https://elixir.bootlin.com/glibc/glibc-2.28/source/malloc/malloc.c#L1450) where the macro `in_smallbin_range` is defined. 

As we can see, they will overlap with FastBins at some point. But that's because the Fastbins, TCache and UnsortedBins are regarded as an optimization layer present at the heap manager. That way a memory allocation that _should_ go to the SmallBin once freed, ends up at the TCache because the cache is not full yet. 

Instead of keeping the chunks allocated (that means the **P** bit set at the size field), the chunks that end up in the SmallBin may be coalesced if stacked together with some other chunks. That way, the heap manager prevents fragmentation of the heap, and makes use of that _coalesced_ space to serve further `malloc` requests, instead of asking the SO for it. 

We've seen how the coalescing works at the previous demo ;) where the 5 chunks are _added_ to conform a new bigger chunk. 


#### LargeBins

Small, fast and tcache had something in common: They all keep bins of an specific size for many different chunk sizes. 

This is pretty convenient, however is not practical when big allocations are requested. Unfortunately, the heap manager cannot have a bin for every `malloc` size in the devs mind. 

Once the 512Bytes (for 32bit) or 1024Bytes (for 64bit) limit is exceeded, the new allocations are served (if chunks available) and stored, once `free`, from the LargeBins. 

The LargeBins behave on a very different manner. They're not sorted by size. But by range of sizes. Therefore, different size chunks may end up in the same LargeBin if they're within the same size-range. 

There's 63 LargeBins, and given that the insertions are sorted by size, and removals require to traverse the whole list looking for a chunk of the desired size, makes them slower than the Small/Fast/TCache bins. 

As explained at [GLIBC source](https://elixir.bootlin.com/glibc/glibc-2.28/source/malloc/malloc.c#L1421), the LargeBin structure looks like this: 

```
32 bins of size      64
16 bins of size      512
8 bins of size       4096
4 bins of size       32768
2 bins of size       262144
1 bin  of size       what's left
```

Which means that there's 32 LargeBins separated by 64Bytes each, 16 with a 512Bytes separation, 8 with 4096Bytes of separation... etc. 

A **really good picture** of how the LargeBins structure is actually presented is the one available at [this article by Azeria Labs](https://azeria-labs.com/heap-exploitation-part-2-glibc-heap-free-bins/). These articles are part of the [references]({% post_url 2020-06-03-HeapSeries-References %}) so make sure to check 'em all!



---


This will be the last part of this series but not the last of heap related stuff, that's for sure :) This was just the beginning of a weird journey into the heap internals. 