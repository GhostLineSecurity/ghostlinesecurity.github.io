---
layout: post
title: "Heap Series: 3/5 Heap basics from a practical perspective"
date: 2020-06-03 09:30:00 +0200
tags: [Heap, C, Linux, GDB]
author: Mario
categories: research
excerpt_separator: <!--more-->
---


In the [second part]({% post_url 2020-06-03-HeapSeries-2-5-Basics %}) of this series, we had a look at the internals of GLIBC to learn how the heap management was implemented making use of two basic structures: 
- Allocated chunks (currently in use)
- Free chunks (`free` was called over the pointer returned from a `malloc`)


I know the title reads "... from a practical perspective" and the previous post was almost all theory, but that's only because this one was supposed to be part of the second. As I was writing it, became so long that it turned to be impractical to read.

So here it is, a whole practical post about heap insides
<!--more-->

---

But first, we'll need to install some stuff!

- gdb and gdbserver
	- Just `apt-get install gdb gdbserver`
- libc-6 debug symbols
	- Just `apt-get install libc6-dbg`
- pwndbg
	- Gotta clone and install it: `git clone https://github.com/pwndbg/pwndbg && pwndbg/setup.sh`
- heapInspect (optional, however it provides a nice view of the heap)
	- Gotta clone and install it: `git clone https://github.com/matrix1001/heapinspect` then `cd heapinspect && sed -i "1i source `pwd`/gdbscript.py" ~/.gdbinit`



---





### GDBugging

Now, fire up the `p2_demo1` binary (check-out the [first part]({% post_url 2020-06-03-HeapSeries-1-5-Basics %}) for compiling steps! And the [second part]({% post_url 2020-06-03-HeapSeries-2-5-Basics %}) for the binary code) on GDB: 


```bash
gdb p2_demo1 -q
```

It should show something like this: 

![GDB just started](/assets/heapseries/3/gdb1.png)

As we can see, GDB did not execute our program. It is waiting for us to ask to run it. But first let's add a breakpoint at `main` with `b main`:

![Setting a breakpoint at main](/assets/heapseries/3/gdb2.png)


Look at that! GDB is telling us the memory address, as well as where in the code is it setting the breakpoint. That's due to our use of the `-ggdb` flag during compilation.



#### Some info before we begin


Now, let it run by using `r` and let's see how it pauses the execution process once the breakpoint we sat is hit. Lots of info. will come up, so let's dig into it by parts.


1. An informative section explaining what is it doing, why it paused and where. Also, it presents a small legend regarding the coloring. 
	
	![GDB paused at our breakpoint](/assets/heapseries/3/gdb3.png)

2. The registers section. It shows the values on each register and information by coloring and displaying the data (when possible)
		
	![GDB Register information](/assets/heapseries/3/gdb4.png)

3. The disasm section. Provides some code instructions of the disassembly. (Fear not! That `-ggdb` flag used to compile is about to make its appearance)
	
	![GDB Disasm information](/assets/heapseries/3/gdb5.png)

4. The Source (Code) section. The source code the binary was compiled from. Shows where, in our code, the execution flow is.

	![GDB Source(code) information](/assets/heapseries/3/gdb6.png)

5. The Stack section. Showing information about the contents of the stack (based on the RSP register). 

	![GDB Stack and Backtrace information](/assets/heapseries/3/gdb7.png)

6. And last, but not least the Backtrace section. Showing function calling trace.

	![GDB Backtrace](/assets/heapseries/3/gdb8.png)



All right, let's do this... 


#### Debug 


Let's set a `b`reakpoint at line 18 in our code, and let it `c`ontinue ;)

![GDB Break and continue until line 18](/assets/heapseries/3/gdb9.png)

>>>We can achieve the, almost, same result by using `until 18` which will continue **until** the execution flow reaches line **18**. However, breakpoints are kept between executions on GDB, so they're useful.


Now that we let the program continue, the Source panel should look similar to this: 

![GDB Break and continue until line 18](/assets/heapseries/3/gdb10.png)


We can check the breakpoints in place making use of `info breakpoints` or its abv. `i b`

![GDB Breakpoints in place](/assets/heapseries/3/gdb11.png)


Let's find out where our var called _minimal_ is pointing to, by asking it nicely with `info locals` or its abv. `i locals`

![GDB Local Vars](/assets/heapseries/3/gdb12.png)


Nice and tidy. It will provide the info about the local vars and their contents. Such as _minimal_real_size_, remember? That's the one storing the **actual** size of our _minimal_ request. The one with the right/left shifts at line 14 of our code. To make sure, let's just `list` the code to verify. 


![GDB List code](/assets/heapseries/3/gdb13.png)


Now, we are going to hit `next`, or its abv `n`, in our GDB session, to allow that `free(minimal)` line to run: 


![GDB Next step](/assets/heapseries/3/gdb14.png)


See that? Our execution flow is now at the `return 0;` of our code (at line 20). So now that `minimal` was freed, we can inspect the heap bins to check if our assumptions were correct, regarding its size and all that. 


To achieve it, the two extensions we installed before (pwndb and HeapInspect), will come in handy, as they can interpret the heap bins structure for us to understand it very easily. 

Check it out by typing either `heap` (pwndbg command)

![GDB Next step](/assets/heapseries/3/gdb15.png)


or `hi heap all raw` (heapInspect command):

![GDB Next step](/assets/heapseries/3/gdb16.png)


And there it is! The size of our beloved chunk shown as 33 decimal or 0x21 hexadecimal!

- Represented as `mchunk_size = 33` at the pwndbg's `heap` command 
- Represented as `size=0x21` at the HeapInspect's `hi heap all raw` command.



--- 

>>>If you still have any doubts about why is it 33Bytes long instead of 32Bytes, I recommend reading the [second part]({% post_url 2020-06-03-HeapSeries-2-5-Basics %}) of this series. 


But, what are those _fastbins_, _unsortedbins_, _smallbins_, _largebins_ and _tcache_ things shown above?? Let's find out! Enters [the fourth part]({% post_url 2020-06-03-HeapSeries-4-5-Basics %}) of this series :)













































