// string_sso_boundary.cpp — Benchmark SSO boundary for std::string
//
// Measures allocation count and time for strings at different lengths
// to identify SSO threshold in the current stdlib implementation.
//
// Build: g++ -std=c++20 -O2 string_sso_boundary.cpp -o string_sso
// Run:   ./string_sso

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

static const int ITERATIONS = 1000000;

template <int N>
struct FixedString {
    char data[N];
    FixedString() { for (int i = 0; i < N - 1; ++i) data[i] = 'a'; data[N - 1] = '\0'; }
    const char* cstr() const { return data; }
};

template <int N>
long long bench_string_construct() {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        volatile std::string s(FixedString<N>().cstr());
        (void)s;
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

int main() {
    std::printf("std::string SSO boundary benchmark\n");
    std::printf("sizeof(std::string) = %zu\n\n", sizeof(std::string));
    std::printf("%6s  %12s  %8s\n", "Length", "Time(ns)", "ns/op");
    std::printf("%6s  %12s  %8s\n", "------", "--------", "------");

    // Test lengths around common SSO thresholds (15 for libstdc++, 22 for libc++)
    auto run = [](int len) {
        // Use a simple loop-based approach
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i) {
            std::string s(len, 'x');
            volatile auto p = s.data();
            (void)p;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        std::printf("%6d  %12lld  %8.1f\n", len, (long long)ns, (double)ns / ITERATIONS);
    };

    for (int len : {1, 5, 10, 15, 16, 20, 22, 23, 24, 30, 50, 100, 256}) {
        run(len);
    }

    std::printf("\nSSO threshold: length where time per op jumps significantly\n");
    std::printf("libstdc++ threshold: 15, libc++ threshold: 22\n");
    return 0;
}
