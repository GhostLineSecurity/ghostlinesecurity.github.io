---
layout: post
title: "Heap Series: 1/5 Heap basics from a practical perspective"
date: 2020-06-03 09:10:00 +0200
tags: [Heap, C, Linux]
author: Mario
categories: research
excerpt_separator: <!--more-->
---

I've tried a few times to learn some _heap_ stuff, however I usually ended up reading about something else in my to-do list or something brand new... 

For sure I'm not the only one :) 

This time I wanted to make a change in the way I approach this particular dragon. I promised to myself I would write about the learnt and findings I would make during this, rather long, process. 


So let's give it a try.

<!--more-->

---
All the code samples can be compiled with `gcc`: 

```bash
gcc p1_demoX.c -Wall -ggdb -o p1_demoX
```

All the code samples were compiled under the last Debian 10 distro, and the GLIBC implementation used was 2.28, check yours with:

```bash
ldd --version
```

---

### Basics
>heap is that malloc-free thing, right?

The _Heap_ is a memory region that an operative system provides to allocate memory on the fly to every program in execution that actually needs it.

That region grows as more and more memory is requested. 

There's many _Heap_ implementations around (such as dlmalloc, ptmalloc2, tcmalloc...). I focused on the GLIBC one, commonly used on modern Linux OS. 

The GLIBC implementation allows to have separated _heaps_ and _freelists_ for each thread. This provides concurrency safety, making possible for two threads to call heap-related functions at the very same time without racing-consequences. 

>>>These private heaps are called `per thread Arenas`. 

During the course of this post, we will allocate memory making use of `malloc` and will release it with `free`. 

On a properly structured program, every call to `malloc` should be accompanied with one, and only one, `free` of the allocated buffer.

##### `p1_demo1.c`
{% highlight c linenos %}
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	void *a = malloc(10);
	printf("We requested 10B allocated at %p\n", a);

	free(a);
	printf("The memory just got freed\n");

	return 0;
}
{% endhighlight %}


### The heap is weird and we like it 
>at least for now...


There's some particular behaviors to keep on mind when dealing with heap allocations. 

The first one I'd like to mention is: When memory is requested a pointer is returned. If we call `free` over that pointer, and then request the same amount of memory as before, then the same pointer is returned. Weird uh?

##### `p1_demo2.c`
{% highlight c linenos %}
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {

	void *a = malloc(10);
	printf("Allocated a@%p\n", a);
	free(a);
	printf("\ta got freed\n");

	void *b = malloc(10);
	printf("Allocated b@%p\n", b);
	free(b);
	printf("\tb got freed\n");

}
{% endhighlight %}


But how is `malloc` returning twice the same pointer? Coincidence? Witchcraft? Just like "Poker" is "Joker" but swapping a _P_ for a _J_?

Nope. `Freelists` or `bins`. Once `free` is executed over the pointer, the heap manager takes care of keeping these memory blocks, or `chunks`, on linked lists.

A `freelist` is a structure were memory allocations are referenced once freed. And there's many.

>>>Every element in a double-linked list keeps track of which element goes next, and which element goes before. Every element in a single-linked list keeps track of which element goes next.


### mmmmmmmmalloc...


Basically `malloc` returned the same pointer as before because the heap manager checked the correspondent freelist and found a chunk in it, that satisfied the requested memory allocation.

So, as usual, it behaves like a LIFO queue. Last object that enters the list is the first one leaving it. 

Check it out:

##### `p1_demo3.c`
{% highlight c linenos %}
#include <stdio.h>
#include <stdlib.h>

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
{% endhighlight %}

As we can see, the first allocation for-loop makes the heap manager allocate newly created chunks that are retrieved to us. Once freed, these chunks behave like elements on a linked-list. 

As allocations are requested again, the same memory addresses that were returned in the first place, are provided in **inverse** order to us by the heap manager. This is due to the LIFO behavior the heap has. 


To complicate the things even more, there's a few types of freelists: fastbins, unsorted bins, smallbins, largebins and per-thread tcache. 

>>>Chunks are placed on them based on their size and in some optimization procedures. We'll get our explanation as well, don't worry... for now >)


