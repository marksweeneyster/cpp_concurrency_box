# cpp_concurrency_box
Sandbox for C++ concurrency stuff.

Mostly inspired by [C++ Concurrency in Action](https://www.manning.com/books/c-plus-plus-concurrency-in-action-second-edition) (a must have).


## Queue stuff
Lock based queues defined in `include/apricot/queue.hpp`.

(The namespace name "apricot" was a random choice.)
### demo apps
These exercise the queues in various ways
1. **demo0** - Queue0 with `std::future`
2. **demo1** - Queue1 with `std::future`
3. **demo2** - Queue1 with `std::packaged_task`
4. **tdemo** - Interruptible thread (_in progress_)
5. **thread_local_app** - demo of `thread_local` storage
