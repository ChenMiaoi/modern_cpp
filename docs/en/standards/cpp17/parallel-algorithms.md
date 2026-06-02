---
title: "C++17 Parallel Algorithms"
topic: unknown
feature: parallel-algorithms
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 Parallel Algorithms

## Overview

C++17 introduced **parallel execution policies** for most algorithms in `<algorithm>` and `<numeric>`, enabling parallelization by passing an execution policy parameter. This is the first time C++ has integrated parallel computing capabilities into the standard library. Execution policies control behavior — from strictly sequential to multi-threaded parallelism to SIMD vectorization.

## Syntax

```cpp
#include <algorithm>
#include <numeric>
#include <execution>

std::sort(std::execution::par, vec.begin(), vec.end());
std::for_each(std::execution::par_unseq, vec.begin(), vec.end(), func);
auto sum = std::reduce(std::execution::par, vec.begin(), vec.end(), 0.0);
```

## Execution Policies

```cpp
std::execution::seq        // sequential execution (C++17)
std::execution::par        // multi-threaded parallel, element access non-overlapping (C++17)
std::execution::par_unseq  // parallel + SIMD vectorization (C++17)
std::execution::unseq      // single-threaded + SIMD vectorization (C++20)
```

| Policy | Multi-threaded | Vectorized | Exception Behavior |
|--------|---------------|------------|-------------------|
| `seq` | No | No | Normal propagation |
| `par` | Yes | Possible | May `terminate` |
| `par_unseq` | Yes | Yes | May `terminate` |

## Algorithms Supporting Parallelism

### Sorting and Searching

```cpp
std::vector<int> data(1'000'000);
std::iota(data.begin(), data.end(), 0);
std::shuffle(data.begin(), data.end(), std::mt19937{42});

std::sort(std::execution::par, data.begin(), data.end());
auto it = std::find(std::execution::par, data.begin(), data.end(), 999'999);
bool found = std::binary_search(std::execution::par, data.begin(), data.end(), 500'000);
```

### Reduction Operations

```cpp
std::vector<double> values(10'000'000, 1.0);

double sum = std::reduce(std::execution::par, values.begin(), values.end(), 0.0);

double dot = std::transform_reduce(
    std::execution::par, a.begin(), a.end(), b.begin(), 0.0);

std::vector<int> prefix(n);
std::exclusive_scan(std::execution::par, data.begin(), data.end(), prefix.begin(), 0);
```

### Traversal and Transformation

```cpp
std::for_each(std::execution::par, items.begin(), items.end(),
    [](auto& item) { item.process(); });  // callback must be thread-safe

std::vector<double> output(input.size());
std::transform(std::execution::par, input.begin(), input.end(), output.begin(),
    [](double x) { return std::sin(x) * std::cos(x); });

auto n = std::count_if(std::execution::par, data.begin(), data.end(),
    [](int x) { return x % 2 == 0; });

bool has_neg = std::any_of(std::execution::par, v.begin(), v.end(),
    [](int x) { return x < 0; });
```

## Implementation Status

| Compiler | Backend | Status |
|----------|---------|--------|
| GCC 9+ | TBB or `std::thread` | Full support |
| MSVC 2019+ | Concurrency Runtime | Full support |
| Clang 13+ | libc++ | Partial support |

```bash
# GCC requires TBB
sudo apt install libtbb-dev
g++ -std=c++17 -ltbb main.cpp
# without TBB, the par policy silently falls back to sequential—looks normal but no speedup
```

## `reduce` vs `accumulate`

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};

int s1 = std::accumulate(v.begin(), v.end(), 0);     // strictly left to right
int s2 = std::reduce(std::execution::par, v.begin(), v.end(), 0); // any order

// floating-point caveat: parallel evaluation changes the order, results may differ
std::vector<double> fv = {1e10, 1.0, -1e10, 1.0};
// accumulate: 2.0, reduce: possibly 0.0
```

## Exception and Safety Requirements

```cpp
// exceptions should not be thrown in parallel algorithms
// under par / par_unseq, uncaught exceptions may cause std::terminate

// wrong: data race
int counter = 0;
std::for_each(std::execution::par, v.begin(), v.end(), [&](int) {
    counter++;  // undefined behavior
});

// correct: use atomics or reduce
std::atomic<int> counter{0};
std::for_each(std::execution::par, v.begin(), v.end(), [&](int) { counter++; });
```

Iterator requirements: parallel algorithms require random-access iterators. `vector`/`deque`/`array` are supported; `list`/`set`/`map` are not.

## Practical Application: Parallel Data Pipeline

```cpp
void process_dataset(std::vector<Record>& records) {
    std::sort(std::execution::par, records.begin(), records.end(),
        [](const auto& a, const auto& b) { return a.key < b.key; });

    std::vector<Result> results(records.size());
    std::transform(std::execution::par, records.begin(), records.end(),
        results.begin(), [](const Record& r) { return compute(r); });

    double total = std::reduce(std::execution::par,
        results.begin(), results.end(), 0.0,
        [](double acc, const Result& r) { return acc + r.value; });
}
```

## Best Practices

1. **Don't parallelize small data sets**: for fewer than 10,000 elements, sequential is often faster; benchmark to confirm.
2. **Use `reduce` instead of `accumulate` for parallel reduction**.
3. **Keep callbacks stateless and `noexcept`**: avoid shared state and exceptions.
4. **Floating-point reduction precision differences**: parallel `reduce` changes evaluation order.
5. **Prefer `par_unseq`**: gives the compiler more optimization freedom.
6. **Don't assume parallelism is always faster**: benchmark each scenario.

## Common Pitfalls

- **Data races**: modifying shared variables is the most common error.
- **Exceptions cause terminate**: throwing under `par`/`par_unseq` may terminate the program.
- **Linked lists/associative containers unsupported**: parallel algorithms require random-access iterators.
- **Silent fallback when TBB is not linked**: GCC without TBB falls back to sequential `par` with no warning.
- **`par_unseq` requires pure functional callbacks**: otherwise behavior is undefined.
- **Floating-point non-associativity**: `(a+b)+c ≠ a+(b+c)` holds for floating-point; parallel results may differ.
