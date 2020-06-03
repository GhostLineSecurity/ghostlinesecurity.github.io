---
layout: post
title: "Heap Series: 2/5 Heap basics from a practical perspective"
date: 2020-06-03 09:20:00 +0200
tags: [Heap, C, Linux]
author: Mario
categories: research
---


In the [first part]({% post_url 2020-06-03-HeapSeries-1-5-Basics %}) of this series, we talked about some basic functionalities the heap manager provides, and by running some demos, we were able to learn simple `malloc/free` usage and its effects over the memory. 

I keep the intention to approach this from a practical point of view. However, there's some theory we can't avoid forever :)

---

### Chunks

The heap implementation states four categories for memory chunks: 

- Top chunk: Represents the remaining space (available) to store new memory allocations. It _stays_ at the top of the heap.
- Allocated chunk: A memory region that was allocated and **is** in use.
- Free chunk: A memory region that was allocated and is **not** in use, i.e: `free` was called over its pointer.
- Last remainder chunk: When a big chunk of memory is split in two, due to an allocation request, the part that **is not** returned is the so called _last remainder chunk_. This occurs sometimes when there's not chunks of the requested size but splitting a big one may satisfy the request.


As we've seen on the previous part's demos, the addresses provided by the heap manager grow as we request more and more memory. That, only happened when chunks were **created** and not **recycled** from a `freelist`.

But, what's **actually** a chunk? Well... when a memory allocation is requested, we don't get just the Bytes requested, but a structure that allows the heap manager to perform tracking of the memory allocations in a proper way.

In fact, when a certain amount of bytes is requested, the actual memory reservation made for us is 8Byte or 16Byte aligned, for 32bit or 64bit architectures respectively. 

So, if we happen to request 10Bytes on a 64bit architecture, the actual size of the memory reserved for us to play should be 32Bytes.

##### `p2_demo1.c`
{% highlight c linenos %}
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
{% endhighlight %}


The chunk structure differs for an in use chunk and a free chunk.

#### In use chunk

When memory is allocated and the heap manager returns those bytes for us to play with, this is how it looks like (taken from [GLIBC 2.28 Implementation](https://elixir.bootlin.com/glibc/glibc-2.28/source/malloc/malloc.c#L1059) and slightly modified for clarity):

```
  chunk_1-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		    |             Size of chunk_0, if unallocated (P clear)			|	<- SIZE_SZ
		    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		    |             Size of chunk_1, in bytes                   |A|M|P|	<- SIZE_SZ (16B or 32B aligned, remember?)
      mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		    |             User data starts here...                          .
		    .                                                               .	<- Your memory space!
		    .             (malloc_usable_size() bytes)                      .
		    .                                                               |
  chunk_2-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+	<- next_chunk in memory
		    |             (size of chunk_1, but used for application data)	|	<- SIZE_SZ
		    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		    |             Size of chunk_2, in bytes 				  |A|0|1|	<- SIZE_SZ; P bit set to 1
		    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- `chunk_x` Is the beginning of the **whole** area needed to track your memory allocation. It points to the size of the previous chunk. 
- Then goes the size of the currently returned chunk. Those `|A|M|P|` mean something as well :)
	- `A`bit: States if the chunk was retrieved from a _Non Main Arena_. On the previous part, we learned that there's an arena for each thread. If the `malloc` request was ran from a thread that is **not** the main thread, this bit will be set to 1.
	- `M`bit: States if the `malloc` request provided a pointer to a memory region obtained via `mmap`.
	- `P`bit: States if the **previous** chunk is in use. Check out the _chunk_2_ `P` value. The described structure belongs to an **in use** chunk, therefore, the `P`bit of _chunk_2_ is set to 1, due to _chunk_1_ being in use.
- `mem` points to the area allocated for us to fill it. This is the pointer returned from our `malloc` call.
- `chunk_2` Is the beginning of the next contiguous chunk


So, if we have the `minimal` allocation requested on _p2_demo1.c_ then, the location of the **real** size of the returned memory allocation is `SIZE_SZ`Bytes before our `minimal` pointer, right? What's the size of a `SIZE_SZ`? Architecture dependent.
At [malloc-internal.h](https://elixir.bootlin.com/glibc/glibc-2.28/source/malloc/malloc-internal.h#L59) we can see how `SIZE_SZ` is a macro for `sizeof(INTERNAL_SIZE_T)`, and that `INTERNAL_SIZE_T` is defined, a couple lines before, as `size_t`. 

So let's ask for it making use of the `sizeof(size_t)`

```c
unsigned long size_offset = sizeof(size_t);	
```

And that's the reason for that first weird looking line. The offset of the chunk size is located at our `minimal` pointer minus `sizeof(size_t)`.

Once we found that offset, what's the deal with the right and left-shifts? Well, if you take a look at the diagram, you'll note how the `|A|M|P|` bits seem to share the same space as the size. And that's because they are. 

If the alignment of every _mallocated_ should be either 8B or 16B, then in a binary representation of the size, the last 3 bits are always 0. These 3 last are then used to provide the `|A|M|P|` status. So, when checking the size for a, let's say... in use previous chunk, main-thread and non mmaped chunk on a 64bit architecture, the size returned from that memory address would look like 33Bytes. If we shift that value right 3 positions (it becomes 4) and then left (it becomes 32) it returns the real size due to the precision loss caused by right shift. 

```c
size_t minimal_real_size = *(minimal - size_offset) >> 3 << 3;
```


Hopefully those weird looking lines at previous demo now make sense. 

#### Free chunk

When `free` is called over a pointer, the heap manager references them in linked lists. This is how the chunks look like (taken from [GLIBC 2.28 Implementation](https://elixir.bootlin.com/glibc/glibc-2.28/source/malloc/malloc.c#L1098) and slightly modified for clarity):


```
 chunk_1 -> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+	
		    |             Size of previous chunk, if unallocated (P clear)  |	<- SIZE_SZ
		    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    `head:' |             Size of chunk, in bytes                     |A|0|P|	<- SIZE_SZ (16B or 32B aligned, remember?)
      mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+	<- Your memory space started here!
		    |             Forward pointer to next chunk in list (fd)        |	<- A pointer to a higher memory address
		    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+	
		    |             Back pointer to previous chunk in list (bk)       |	<- A pointer to a lower memory address
		    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		    |             Unused space (may be 0 bytes long)                .
		    .                                                               .
		    .                                                               |
 chunk_2 -> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+	<- next_chunk in memory
    `foot:' |             Size of chunk_1, in bytes                         |	<- SIZE_SZ
		    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		    |             Size of chunk_2, in bytes                   |A|0|0|	<- SIZE_SZ; P bit set to 0
		    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- The Forward Pointer (fd): Points to the next chunk.
- The Back Pointer (bk): Points to the previous chunk.

These two pointers are placed inside the memory allocated for the user.

Let's give it a try. This kind of behavior will come in use in future posts >)


