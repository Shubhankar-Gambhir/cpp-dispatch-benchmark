// Benchmark: std::variant + std::visit
// Compile: g++ -std=c++17 -O2 -march=native -o bench_variant bench_variant.cpp

#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <variant>

static constexpr int HEAP_SIZE = 64;
static constexpr long ITERATIONS = 100'000'000L;
static volatile int sink;

struct EpsilonBS {
    void store(int* addr, int value) const { *addr = value; }
};

struct SerialBS {
    void store(int* addr, int value) const {
        *addr = value;
        sink = 1;
    }
};

struct G1BS {
    void store(int* addr, int value) const {
        sink = *addr;
        *addr = value;
        sink = 1;
    }
};

using AnyBarrierSet = std::variant<EpsilonBS, SerialBS, G1BS>;

__attribute__((noinline))
AnyBarrierSet create(const char* gc) {
    if (std::strcmp(gc, "epsilon") == 0) return EpsilonBS{};
    if (std::strcmp(gc, "serial") == 0)  return SerialBS{};
    if (std::strcmp(gc, "g1") == 0)      return G1BS{};
    throw std::runtime_error(std::string("unknown GC: ") + gc);
}

int main(int argc, char* argv[]) {
    const char* gc = (argc > 1) ? argv[1] : "g1";
    auto bs = create(gc);
    int heap[HEAP_SIZE] = {};

    for (long i = 0; i < 1'000'000; ++i)
        std::visit([&](auto& b) { b.store(heap + (i % HEAP_SIZE), static_cast<int>(i)); }, bs);

    auto start = std::chrono::high_resolution_clock::now();
    for (long i = 0; i < ITERATIONS; ++i)
        std::visit([&](auto& b) { b.store(heap + (i % HEAP_SIZE), static_cast<int>(i)); }, bs);
    auto end = std::chrono::high_resolution_clock::now();

    double ns = std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
    std::printf("variant + visit:   %.2f ns/call\n", ns);
}
