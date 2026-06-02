// Benchmark: Polymorphic Decoupled CRTP + Lazy Resolution
// Compile (GCC 11):
//   ~/utils/mamba/envs/gcc11/bin/x86_64-conda-linux-gnu-g++ \
//     -std=c++17 -O2 -march=skylake-avx512 -fcf-protection \
//     -falign-functions=64 -falign-loops=64 \
//     -o bench_poly_crtp bench_poly_crtp.cpp
// Usage: ./bench_poly_crtp [roundrobin|weighted|random]
//
// Note: under polymorphic workloads, lazy resolution reduces to a function
// pointer array (one resolved pointer per plugin type). This should perform
// identically to bench_poly_fnptr. Divergence indicates a codegen artifact.

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

// BarrierSet hierarchy with decoupled CRTP
class BarrierSet {
public:
    enum Kind { Epsilon, Serial, G1 };
    Kind kind_;
    static BarrierSet* the_;

    template <typename BST>
    struct AccessBarrier {
        static void store(int* addr, int value) { *addr = value; }
    };
};
BarrierSet* BarrierSet::the_ = nullptr;

struct EpsilonBS : BarrierSet {
    EpsilonBS() { kind_ = Epsilon; }
    template <typename BST = EpsilonBS>
    struct AccessBarrier : BarrierSet::AccessBarrier<BST> {};
};

struct SerialBS : BarrierSet {
    SerialBS() { kind_ = Serial; }
    template <typename BST = SerialBS>
    struct AccessBarrier : BarrierSet::AccessBarrier<BST> {
        static void store(int* addr, int value) { *addr = value; sink = 1; }
    };
};

struct G1BS : BarrierSet {
    G1BS() { kind_ = G1; }
    template <typename BST = G1BS>
    struct AccessBarrier : BarrierSet::AccessBarrier<BST> {
        static void store(int* addr, int value) { sink = *addr; *addr = value; sink = 1; }
    };
};

// Pre-resolve one function pointer per plugin type (no lazy init needed)
using StoreFn = void(*)(int*, int);
static const StoreFn resolved[3] = {
    &EpsilonBS::AccessBarrier<>::store,
    &SerialBS::AccessBarrier<>::store,
    &G1BS::AccessBarrier<>::store,
};

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

    auto pat  = make_pattern(pattern_name);
    int  heap[HEAP_SIZE] = {};

    for (long i = 0; i < WARMUP; ++i)
        resolved[pat[i % PATTERN_SIZE]](heap + (i % HEAP_SIZE), static_cast<int>(i));

    double best = 1e18;
    for (int run = 0; run < RUNS; ++run) {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (long i = 0; i < ITERATIONS; ++i)
            resolved[pat[i % PATTERN_SIZE]](heap + (i % HEAP_SIZE), static_cast<int>(i));
        auto t1 = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / ITERATIONS;
        if (ns < best) best = ns;
    }

    std::printf("CRTP       [%-12s]: %.2f ns/call\n", pattern_name, best);
}
