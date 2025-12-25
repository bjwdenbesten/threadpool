# A Threadpool Library

## Basic usage + testing
To compile and run all tests in the test directory (including dependencies)
```
make test
```
To compile just the standard thread pool test (including dependencies)
```
make standard
```
To compile just the ws thread pool test (including dependencies)
```
make workstealing
```

## What is a threadpool, and why use it?
Creating and destroying threads on demand for concurrent tasks can be costly for the CPU.
Thus, if we know that we will be utilizing threads in our program, we can take up some of this overhead cost
upfront by allocating a fixed number of threads at the start of our program. 
These threads sit idly until there is work to be done. When work arrives, a thread awakens, does/computes the work, then goes back to being
idle.


## What does a work-stealing tp solve?
An issue arises when a task (work) spawns sub tasks, which then spawns more sub task, so on and so forth. This is an issue
because there is only one thread doing all the work, while all others remain idle. To fix this, we can give threads the ability
to steal work from other threads (if they are idle and aren't processing any work). This can significantly help balance workloads across all threads. 
This implementation is especially
useful for recursive tasks like Mergesort or Matrix Multiplication.

## Other details
* Requires at least **C11**
* (ws only) The Chase-Lev dequeues differ slightly from standard implementations. Most significantly, there is no dynamic resizing.
* (ws only) For simplicity, all work initially arrives in the global queue, instead of being distributed to indiviudal threads.
* (ws only) This implementation uses random work stealing, rather than deterministic.



