// Benchmark: Function Pointer (type-erased)
// Compile: g++ -std=c++17 -O2 -march=native -o bench_fnptr bench_fnptr.cpp

#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

static constexpr int HEAP_SIZE = 64;
static constexpr long ITERATIONS = 100'000'000L;
static volatile int sink;

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

__attribute__((noinline))
StoreFn resolve(const char* gc) {
    if (std::strcmp(gc, "epsilon") == 0) return &epsilon_store;
    if (std::strcmp(gc, "serial") == 0)  return &serial_store;
    if (std::strcmp(gc, "g1") == 0)      return &g1_store;
    throw std::runtime_error(std::string("unknown GC: ") + gc);
}

int main(int argc, char* argv[]) {
    const char* gc = (argc > 1) ? argv[1] : "g1";
    StoreFn store = resolve(gc);
    int heap[HEAP_SIZE] = {};

    for (long i = 0; i < 1'000'000; ++i)
        store(heap + (i % HEAP_SIZE), static_cast<int>(i));

    auto start = std::chrono::high_resolution_clock::now();
    for (long i = 0; i < ITERATIONS; ++i)
        store(heap + (i % HEAP_SIZE), static_cast<int>(i));
    auto end = std::chrono::high_resolution_clock::now();

    double ns = std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
    std::printf("Function pointer:  %.2f ns/call\n", ns);
}
