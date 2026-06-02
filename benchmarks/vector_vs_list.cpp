// vector_vs_list.cpp — Benchmark vector vs list traversal
//
// Demonstrates cache-friendly sequential access vs pointer chasing.
//
// Build: g++ -std=c++20 -O2 vector_vs_list.cpp -o vector_vs_list
// Run:   ./vector_vs_list

#include <chrono>
#include <cstdio>
#include <list>
#include <numeric>
#include <vector>

static const int N = 1000000;
static const int TRIALS = 10;

template <typename Container>
long long bench_traversal(Container& c) {
    auto start = std::chrono::high_resolution_clock::now();
    volatile typename Container::value_type sum = 0;
    for (int t = 0; t < TRIALS; ++t) {
        for (const auto& val : c) {
            sum += val;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    (void)sum;
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main() {
    std::vector<int> vec(N);
    std::list<int> lst;

    std::iota(vec.begin(), vec.end(), 0);
    for (int i = 0; i < N; ++i) lst.push_back(i);

    auto vec_ms = bench_traversal(vec);
    auto lst_ms = bench_traversal(lst);

    std::printf("Container traversal benchmark (N=%d, %d trials)\n\n", N, TRIALS);
    std::printf("%-20s %8lld ms\n", "std::vector", (long long)vec_ms);
    std::printf("%-20s %8lld ms\n", "std::list", (long long)lst_ms);
    std::printf("\nvector/list ratio: %.2fx\n", (double)vec_ms / lst_ms);
    std::printf("Expected: vector ~3-10x faster due to cache locality\n");

    return 0;
}
