// Benchmark: Polymorphic Virtual Dispatch
// Compile (GCC 11):
//   ~/utils/mamba/envs/gcc11/bin/x86_64-conda-linux-gnu-g++ \
//     -std=c++17 -O2 -march=skylake-avx512 -fcf-protection \
//     -falign-functions=64 -falign-loops=64 \
//     -o bench_poly_virtual bench_poly_virtual.cpp
// Usage: ./bench_poly_virtual [roundrobin|weighted|random]

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
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

class BarrierSet {
public:
    virtual ~BarrierSet() = default;
    virtual void store(int* addr, int value) = 0;
};

class EpsilonBS final : public BarrierSet {
public:
    void store(int* addr, int value) override { *addr = value; }
};

class SerialBS final : public BarrierSet {
public:
    void store(int* addr, int value) override {
        *addr = value;
        sink = 1;
    }
};

class G1BS final : public BarrierSet {
public:
    void store(int* addr, int value) override {
        sink = *addr;
        *addr = value;
        sink = 1;
    }
};

static std::array<int, PATTERN_SIZE> make_pattern(const char* name) {
    std::array<int, PATTERN_SIZE> arr;
    if (std::strcmp(name, "roundrobin") == 0) {
        for (int i = 0; i < PATTERN_SIZE; ++i) arr[i] = i % 3;
    } else if (std::strcmp(name, "weighted") == 0) {
        // 90% G1 (index 2), 10% Serial (index 1), no Epsilon
        std::mt19937 rng(SEED);
        std::uniform_int_distribution<> dist(1, 10);
        for (int i = 0; i < PATTERN_SIZE; ++i)
            arr[i] = (dist(rng) == 10) ? 1 : 2;
    } else if (std::strcmp(name, "random") == 0) {
        // Equal probability of Epsilon (0), Serial (1), G1 (2)
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

    EpsilonBS eps;
    SerialBS  ser;
    G1BS      g1;
    BarrierSet* plugins[3] = { &eps, &ser, &g1 };

    auto pat  = make_pattern(pattern_name);
    int  heap[HEAP_SIZE] = {};

    // Warmup: not timed
    for (long i = 0; i < WARMUP; ++i)
        plugins[pat[i % PATTERN_SIZE]]->store(heap + (i % HEAP_SIZE), static_cast<int>(i));

    // Best of RUNS timed runs
    double best = 1e18;
    for (int run = 0; run < RUNS; ++run) {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (long i = 0; i < ITERATIONS; ++i)
            plugins[pat[i % PATTERN_SIZE]]->store(heap + (i % HEAP_SIZE), static_cast<int>(i));
        auto t1 = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / ITERATIONS;
        if (ns < best) best = ns;
    }

    std::printf("Virtual    [%-12s]: %.2f ns/call\n", pattern_name, best);
}
