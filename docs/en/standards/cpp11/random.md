---
title: "std::random Number Library"
topic: unknown
feature: random
standard: N/A
status_checked_at: 2026-06-02
---
# std::random Number Library

## Overview

C++11 introduced the `<random>` header, splitting random number generation into two independent components: **engines** (producing raw random bits) and **distributions** (mapping bits to specific distributions). This design is more flexible, more controllable, and of higher quality than C-style `rand()`.

Problems with `rand()`: global state makes it thread-unsafe, implementation-defined quality and range, modulo bias. C++11's `<random>` completely solves these problems.

## API Overview

| Component | Description |
|-----------|-------------|
| `std::mt19937` | 32-bit Mersenne Twister engine (period 2^19937-1) |
| `std::mt19937_64` | 64-bit Mersenne Twister engine |
| `std::random_device` | Hardware/OS entropy source |
| `std::uniform_int_distribution<T>` | Uniform integer distribution |
| `std::uniform_real_distribution<T>` | Uniform real distribution |
| `std::normal_distribution<T>` | Normal distribution |
| `std::bernoulli_distribution` | Bernoulli distribution (probabilistic boolean) |

## Separation of Engine and Distribution

```cpp
#include <random>
#include <iostream>

int main() {
    std::mt19937 engine(42);                       // fixed seed, reproducible
    std::uniform_int_distribution<int> dist(1, 6); // maps to [1, 6]
    for (int i = 0; i < 10; ++i) {
        std::cout << dist(engine) << ' ';  // same output every run
    }
}
```

## Why rand() Is Bad

```cpp
#include <cstdlib>

// 1. Range too small: RAND_MAX is usually only 32767
// 2. Modulo bias: rand() % 6 is non-uniform when RAND_MAX is not a multiple of 6
int biased = std::rand() % 6;
// 3. Global state, multithreaded calls are data races
// 4. Low bits have very short period in some implementations

// The C++11 way:
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> dist(0, 5);  // precisely uniform
```

## std::mt19937: Recommended Default Engine

```cpp
std::mt19937 gen32;      // 32-bit output, suitable for most scenarios
std::mt19937_64 gen64;   // 64-bit output, suitable for large-range random numbers

static_assert(std::mt19937::min() == 0);
static_assert(std::mt19937::max() == 4294967295);  // 2^32 - 1
```

## Seeding Best Practices

```cpp
// Method 1: std::random_device as seed (recommended)
std::random_device rd;
std::mt19937 gen(rd());

// Method 2: seed_seq mixing multiple entropy values (more robust)
std::seed_seq seed{rd(), rd(), rd(), rd()};
std::mt19937 gen2(seed);

// Method 3: fixed seed for debugging and testing
std::mt19937 gen3(42);  // produces the same sequence every run
```

> **`std::random_device` note:** The standard does not guarantee it is non-deterministic. Older versions of MinGW used a deterministic implementation. Verify your toolchain for production.

## Common Distributions

```cpp
std::mt19937 gen(std::random_device{}());

// Uniform integer distribution: [1, 6] closed interval
std::uniform_int_distribution<int> dice(1, 6);
int roll = dice(gen);

// Uniform real distribution: [0.0, 1.0) closed-open interval
std::uniform_real_distribution<double> unit(0.0, 1.0);
double p = unit(gen);  // never generates 1.0

// Normal distribution: mean 100, standard deviation 15
std::normal_distribution<double> iq(100.0, 15.0);
double score = iq(gen);

// Bernoulli distribution: 70% probability of returning true
std::bernoulli_distribution coin(0.7);
if (coin(gen)) { /* executes 70% of the time */ }

// Discrete distribution: weighted selection
std::discrete_distribution<int> weighted({60, 20, 15, 5});
int category = weighted(gen);  // returns 0, 1, 2, 3
```

## Thread-Local Generators

Global engines have contention issues in multithreaded environments. Each thread should have its own engine:

```cpp
#include <thread>
#include <vector>

thread_local std::mt19937 tl_gen(std::random_device{}());

int generate_random() {
    std::uniform_int_distribution<int> dist(1, 100);
    return dist(tl_gen);  // thread-safe, no contention
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) threads.emplace_back([] {
        for (int j = 0; j < 1000; ++j) generate_random();
    });
    for (auto& t : threads) t.join();
}
```

## Generating Random Container Data

```cpp
#include <vector>
#include <algorithm>
#include <numeric>

std::mt19937 gen(std::random_device{}());

// Generate a random vector
std::vector<int> v(100);
std::uniform_int_distribution<int> dist(1, 1000);
std::generate(v.begin(), v.end(), [&] { return dist(gen); });

// Random shuffle (Fisher-Yates)
std::vector<int> deck(52);
std::iota(deck.begin(), deck.end(), 0);
std::shuffle(deck.begin(), deck.end(), gen);
```

## constexpr Random (Compile-Time)

The C++ standard library's random engines do not support `constexpr`. For compile-time use, a simple LCG can be used:

```cpp
constexpr unsigned lcg(unsigned seed) {
    return seed * 1664525u + 1013904223u;  // Numerical Recipes parameters
}

constexpr unsigned r1 = lcg(42);
constexpr unsigned r2 = lcg(r1);  // compile-time computation complete
// Not cryptographically secure, only for compile-time scenarios like template metaprogramming
```

## Best Practices

1. **Use `std::mt19937` as the default engine**: Good enough quality, fast enough speed.
2. **Seed with `std::random_device`**: Unless you need reproducible sequences.
3. **Use engine and distribution separately**: Don't fall back to `rand()`.
4. **Use a separate engine per thread**: Via `thread_local` or parameter passing.
5. **Fix the seed when reproducible results are needed**: A critical requirement in testing and simulation.
6. **Use `std::seed_seq` to increase seed entropy**: When a single 32-bit seed is insufficient.

## Common Pitfalls

- **Modulo bias**: `gen() % n` still has bias. **Must use `std::uniform_int_distribution`**.
- **`std::random_device` may be deterministic**: MinGW's older implementation degraded to pseudorandom.
- **Distributions are not thread-safe**: Distribution objects maintain internal state caches; do not share across threads.
- **`uniform_real_distribution` interval is closed-open**: `[a, b)`, never generates `b`.
- **Engine's `seed()` resets all state**: Don't accidentally reset the engine mid-generation.
- **Don't use `std::time(nullptr)` as seed**: Low resolution, predictable, rapid consecutive creation yields the same seed.
