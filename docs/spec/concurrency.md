# Nevaarize Formal Language Specification: Concurrency & Async Architecture

## 1. Concurrency Model

Nevaarize provides high-throughput native multithreading using asynchronous function declarations (`async func`) and task synchronization primitives (`await`). Under the hood, tasks execute across an OS-level worker thread pool using hardware-parallel Linux threads (`pthread` / `std::jthread`).

## 2. Syntax & Semantics

### 2.1 Declaring Async Functions
An asynchronous function is prefixed with the `async` keyword:

```nva
async func computeBatch(batchId, size) {
    total = 0
    for (i in Range(0, size)) {
        total = total + (batchId * size + i)
    }
    return total
}
```

### 2.2 Invoking and Awaiting Tasks
Calling an `async func` does not block the caller. Instead, it dispatches the task to the background thread pool and immediately returns a lightweight task handle (Type Tag 8: `ASYNC_HANDLE`):

```nva
handleA = computeBatch(1, 1000000)
handleB = computeBatch(2, 1000000)

// Tasks execute concurrently on distinct CPU cores

resultA = await handleA
resultB = await handleB
print("Total:", resultA + resultB)
```

## 3. Memory Safety & Thread Isolation

### 3.1 Thread-Local Garbage Collection
To eliminate lock contention and data races, each worker thread runs its own isolated `thread_local nevaarize::GarbageCollector jitGC` instance:
- Allocations triggered inside worker threads draw exclusively from the worker's private thread-local GC arena.
- Heap collections scan only the calling thread's stack space.
- Mutex contention on memory allocation is completely avoided.

### 3.2 Cross-Thread Exception Propagation
When an asynchronous worker encounters a runtime error or user exception (via `throw` or hardware signal):
1. The error does not terminate the host parent process or other concurrent workers.
2. The worker thread intercepts the exception and saves the error payload (message, value, type tag) directly into the `TaskHandle`.
3. When the parent thread executes `await handle`:
   - If the task completed successfully, the return value is unpacked into the caller's register/stack.
   - If the task failed with an exception, the exception is automatically re-thrown into the parent thread's JIT context, allowing standard `try / catch` handling:

```nva
async func riskyWorker(id) {
    if (id < 0) {
        throw "Invalid negative task ID"
    }
    return id * 100
}

task = riskyWorker(-1)

try {
    val = await task
} catch (e) {
    print("Caught async worker failure:", e)
}
```

### 3.3 Hardware Signal & Stack Bounds Protection
Hardware signals (such as SIGSEGV from stack exhaustion or out-of-bounds array access) in worker threads are trapped by per-thread recovery jump buffers (`current_async_task_env`). A trapped worker gracefully records the fault and transitions into the failed state rather than crashing the runtime.
