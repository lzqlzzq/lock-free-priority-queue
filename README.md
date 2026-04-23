# Lock-Free Priority Queue

A small C++17 bounded multi-producer, multi-consumer priority queue.

The queue is implemented as 64 per-priority MPMC queues backed by
[rigtorp/MPMCQueue](https://github.com/rigtorp/MPMCQueue), plus an atomic bitmask
that tracks which priorities currently have items available. Lower priority
numbers are popped first.

## Features

- Thread-safe multi-producer, multi-consumer `try_push`, `try_emplace`, and
  `try_pop` operations.
- Fixed priority range: `0` to `63`.
- Bounded capacity per priority bucket.
- Header-only public API exposed through `include/pq/PriorityQueue.hpp`.
- CMake interface target: `lock_free_priority_queue`.

## Requirements

- C++17 compiler
- CMake 3.10 or newer
- Git submodules initialized for `3rdparty/MPMCQueue`

## Build

Clone with submodules, or initialize them after cloning:

```sh
git submodule update --init --recursive
```

Configure and build the example:

```sh
cmake -S . -B build
cmake --build build --target example
```

Run it:

```sh
./build/example
```

## Usage

```cpp
#include "pq/PriorityQueue.hpp"

#include <iostream>

int main() {
    lock_free::PriorityQueue<int> queue(8);

    queue.try_push(3, 30);
    queue.try_push(1, 10);
    queue.try_push(2, 20);
    queue.try_push(0, 0);

    while (auto value = queue.try_pop()) {
        std::cout << *value << '\n';
    }
}
```

Output:

```text
0
10
20
30
```

## API Notes

- `PriorityQueue<T>(capacity)` creates 64 internal buckets, each with the given
  bounded capacity.
- `try_push(priority, item)` returns `false` when the selected priority bucket is
  full.
- `try_emplace(priority, args...)` constructs an item in place when capacity is
  available.
- `try_pop()` returns `std::optional<T>` and pops the lowest non-empty priority.
- Priorities greater than `63` are clamped to `63`.
- `T` must be nothrow default constructible and satisfy the nothrow construction,
  assignment, and destruction requirements of `rigtorp::MPMCQueue<T>`.

## License

This project is licensed under the terms in [LICENSE](LICENSE).
