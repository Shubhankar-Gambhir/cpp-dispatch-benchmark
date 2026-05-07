// Benchmark: Virtual Dispatch
// Compile: g++ -std=c++17 -O2 -march=native -o bench_virtual bench_virtual.cpp

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

static constexpr int HEAP_SIZE = 64;
static constexpr long ITERATIONS = 100'000'000L;
static volatile int sink;

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

__attribute__((noinline))
std::unique_ptr<BarrierSet> create(const char* gc) {
    if (std::strcmp(gc, "epsilon") == 0) return std::make_unique<EpsilonBS>();
    if (std::strcmp(gc, "serial") == 0)  return std::make_unique<SerialBS>();
    if (std::strcmp(gc, "g1") == 0)      return std::make_unique<G1BS>();
    throw std::runtime_error(std::string("unknown GC: ") + gc);
}

int main(int argc, char* argv[]) {
    const char* gc = (argc > 1) ? argv[1] : "g1";
    auto bs = create(gc);
    int heap[HEAP_SIZE] = {};

    for (long i = 0; i < 1'000'000; ++i)
        bs->store(heap + (i % HEAP_SIZE), static_cast<int>(i));

    auto start = std::chrono::high_resolution_clock::now();
    for (long i = 0; i < ITERATIONS; ++i)
        bs->store(heap + (i % HEAP_SIZE), static_cast<int>(i));
    auto end = std::chrono::high_resolution_clock::now();

    double ns = std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
    std::printf("Virtual dispatch:  %.2f ns/call\n", ns);
}
