// Benchmark: Polymorphic Function Pointer Dispatch
// Compile (GCC 11):
//   ~/utils/mamba/envs/gcc11/bin/x86_64-conda-linux-gnu-g++ \
//     -std=c++17 -O2 -march=skylake-avx512 -fcf-protection \
//     -falign-functions=64 -falign-loops=64 \
//     -o bench_poly_fnptr bench_poly_fnptr.cpp
// Usage: ./bench_poly_fnptr [roundrobin|weighted|random]

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>

static constexpr int    HEAP_SIZE    = 64;
static constexpr long   ITERATIONS   = 100'000'000L;
static constexpr int    PATTERN_SIZE = 1'000'000;
static constexpr long   WARMUP       = 1'000'000L;
static constexpr int    RUNS         = 5;
static constexpr int    SEED         = 42;
static volatile int     sink;

void epsilon_store(int* addr, int value) { *addr = value; }

void serial_store(int* addr, int value) {
    *addr = value;
    sink = 1;
}

void g1_store(int* addr, int value) {
    sink = *addr;
    *addr = value;
    sink = 1;
}

using StoreFn = void(*)(int*, int);

static std::array<int, PATTERN_SIZE> make_pattern(const char* name) {
    std::array<int, PATTERN_SIZE> arr;
    if (std::strcmp(name, "roundrobin") == 0) {
        for (int i = 0; i < PATTERN_SIZE; ++i) arr[i] = i % 3;
    } else if (std::strcmp(name, "weighted") == 0) {
        std::mt19937 rng(SEED);
        std::uniform_int_distribution<> dist(1, 10);
        for (int i = 0; i < PATTERN_SIZE; ++i)
            arr[i] = (dist(rng) == 10) ? 1 : 2;
    } else if (std::strcmp(name, "random") == 0) {
        std::mt19937 rng(SEED);
        std::uniform_int_distribution<> dist(0, 2);
        for (int i = 0; i < PATTERN_SIZE; ++i)
            arr[i] = dist(rng);
    } else {
        throw std::runtime_error(std::string("unknown pattern: ") + name);
    }
    return arr;
}

int main(int argc, char* argv[]) {
    const char* pattern_name = (argc > 1) ? argv[1] : "roundrobin";

    StoreFn fns[3] = { &epsilon_store, &serial_store, &g1_store };

    auto pat  = make_pattern(pattern_name);
    int  heap[HEAP_SIZE] = {};

    for (long i = 0; i < WARMUP; ++i)
        fns[pat[i % PATTERN_SIZE]](heap + (i % HEAP_SIZE), static_cast<int>(i));

    double best = 1e18;
    for (int run = 0; run < RUNS; ++run) {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (long i = 0; i < ITERATIONS; ++i)
            fns[pat[i % PATTERN_SIZE]](heap + (i % HEAP_SIZE), static_cast<int>(i));
        auto t1 = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / ITERATIONS;
        if (ns < best) best = ns;
    }

    std::printf("FnPtr      [%-12s]: %.2f ns/call\n", pattern_name, best);
}
