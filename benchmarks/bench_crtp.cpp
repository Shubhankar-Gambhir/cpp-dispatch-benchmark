// Benchmark: Decoupled CRTP + Lazy Resolution
// Compile: g++ -std=c++17 -O2 -march=native -o bench_crtp bench_crtp.cpp

#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

static constexpr int HEAP_SIZE = 64;
static constexpr long ITERATIONS = 100'000'000L;
static volatile int sink;

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
        static void store(int* addr, int value) {
            *addr = value;
            sink = 1;
        }
    };
};

struct G1BS : BarrierSet {
    G1BS() { kind_ = G1; }
    template <typename BST = G1BS>
    struct AccessBarrier : BarrierSet::AccessBarrier<BST> {
        static void store(int* addr, int value) {
            sink = *addr;
            *addr = value;
            sink = 1;
        }
    };
};

struct RuntimeDispatch {
    using Fn = void(*)(int*, int);
    static Fn _store_func;

    static void store_init(int* addr, int value) {
        Fn func;
        switch (BarrierSet::the_->kind_) {
            case BarrierSet::G1:      func = &G1BS::AccessBarrier<>::store; break;
            case BarrierSet::Serial:  func = &SerialBS::AccessBarrier<>::store; break;
            default:                  func = &EpsilonBS::AccessBarrier<>::store; break;
        }
        _store_func = func;
        func(addr, value);
    }

    static void store(int* addr, int value) { _store_func(addr, value); }
};
RuntimeDispatch::Fn RuntimeDispatch::_store_func = &RuntimeDispatch::store_init;

__attribute__((noinline))
BarrierSet* create(const char* gc) {
    if (std::strcmp(gc, "epsilon") == 0) { static EpsilonBS bs; return &bs; }
    if (std::strcmp(gc, "serial") == 0)  { static SerialBS bs;  return &bs; }
    if (std::strcmp(gc, "g1") == 0)      { static G1BS bs;     return &bs; }
    throw std::runtime_error(std::string("unknown GC: ") + gc);
}

int main(int argc, char* argv[]) {
    const char* gc = (argc > 1) ? argv[1] : "g1";
    BarrierSet::the_ = create(gc);
    int heap[HEAP_SIZE] = {};

    RuntimeDispatch::store(heap, 0); // trigger lazy resolution

    for (long i = 0; i < 1'000'000; ++i)
        RuntimeDispatch::store(heap + (i % HEAP_SIZE), static_cast<int>(i));

    auto start = std::chrono::high_resolution_clock::now();
    for (long i = 0; i < ITERATIONS; ++i)
        RuntimeDispatch::store(heap + (i % HEAP_SIZE), static_cast<int>(i));
    auto end = std::chrono::high_resolution_clock::now();

    double ns = std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
    std::printf("Decoupled CRTP:    %.2f ns/call\n", ns);
}
