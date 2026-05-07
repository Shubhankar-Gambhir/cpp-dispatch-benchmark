// Benchmark: Direct call (baseline)
// Compile: g++ -std=c++17 -O2 -march=native -o bench_direct bench_direct.cpp

#include <chrono>
#include <cstdio>

static constexpr int HEAP_SIZE = 64;
static constexpr long ITERATIONS = 100'000'000L;
static volatile int sink;

int main() {
    int heap[HEAP_SIZE] = {};

    for (long i = 0; i < 1'000'000; ++i) {
        int* addr = heap + (i % HEAP_SIZE);
        sink = *addr;
        *addr = static_cast<int>(i);
        sink = 1;
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (long i = 0; i < ITERATIONS; ++i) {
        int* addr = heap + (i % HEAP_SIZE);
        sink = *addr;
        *addr = static_cast<int>(i);
        sink = 1;
    }
    auto end = std::chrono::high_resolution_clock::now();

    double ns = std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
    std::printf("Direct call:       %.2f ns/call\n", ns);
}
