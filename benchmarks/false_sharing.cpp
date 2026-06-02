// false_sharing.cpp — Demonstrate false sharing between adjacent atomics
//
// Shows the performance impact of false sharing and how padding fixes it.
//
// Build: g++ -std=c++20 -O2 -pthread false_sharing.cpp -o false_sharing
// Run:   ./false_sharing

#include <atomic>
#include <chrono>
#include <cstdio>
#include <new>
#include <thread>
#include <vector>

static const int ITERATIONS = 10000000;
static const int NUM_THREADS = 4;

// Case 1: Adjacent atomics — suffer false sharing
struct PackedCounters {
    std::atomic<long long> counters[NUM_THREADS];
};

// Case 2: Padded atomics — no false sharing
struct alignas(std::hardware_destructive_interference_size) PaddedCounter {
    std::atomic<long long> value{0};
};
struct PaddedCounters {
    PaddedCounter counters[NUM_THREADS];
};

template <typename Counters>
long long run_bench(Counters& c) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&c, i]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                c.counters[i].value.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main() {
    std::printf("False sharing benchmark (%d threads, %d iters each)\n\n", NUM_THREADS, ITERATIONS);

    PackedCounters packed;
    auto packed_ms = run_bench(packed);
    std::printf("  Packed (false sharing):   %lld ms\n", (long long)packed_ms);

    PaddedCounters padded;
    auto padded_ms = run_bench(padded);
    std::printf("  Padded (no false sharing): %lld ms\n", (long long)padded_ms);

    std::printf("\n  Speedup from padding: %.2fx\n", (double)packed_ms / padded_ms);
    std::printf("  Expected: 2-8x improvement on multi-core systems\n");

    return 0;
}
