// Benchmark: compare dispatch overhead across four approaches.
// Measures only the dispatch mechanism — barrier side-effects (printf) disabled.
//
// IMPORTANT: all four approaches resolve from argv (runtime input) so the
// compiler cannot devirtualize or resolve function pointers at compile time.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>

static constexpr int HEAP_SIZE = 64;
static constexpr long ITERATIONS = 100'000'000L;

static volatile int sink;

// ============================================================
// Approach 1: Virtual Dispatch
// ============================================================

class VBarrierSet {
public:
    virtual ~VBarrierSet() = default;
    virtual void store(int* addr, int value) = 0;
};

class VEpsilon final : public VBarrierSet {
public:
    void store(int* addr, int value) override { *addr = value; }
};

class VSerial final : public VBarrierSet {
public:
    void store(int* addr, int value) override {
        *addr = value;
        sink = 1;
    }
};

class VG1 final : public VBarrierSet {
public:
    void store(int* addr, int value) override {
        sink = *addr;
        *addr = value;
        sink = 1;
    }
};

__attribute__((noinline))
std::unique_ptr<VBarrierSet> create_virtual(const char* gc) {
    if (std::strcmp(gc, "epsilon") == 0) return std::make_unique<VEpsilon>();
    if (std::strcmp(gc, "serial") == 0)  return std::make_unique<VSerial>();
    if (std::strcmp(gc, "g1") == 0)      return std::make_unique<VG1>();
    throw std::runtime_error(std::string("unknown GC: ") + gc);
}

// ============================================================
// Approach 2: Function Pointer (type-erased)
// ============================================================

void fp_epsilon_store(int* addr, int value) { *addr = value; }

void fp_serial_store(int* addr, int value) {
    *addr = value;
    sink = 1;
}

void fp_g1_store(int* addr, int value) {
    sink = *addr;
    *addr = value;
    sink = 1;
}

using StoreFn = void(*)(int*, int);

__attribute__((noinline))
StoreFn resolve_fp(const char* gc) {
    if (std::strcmp(gc, "epsilon") == 0) return &fp_epsilon_store;
    if (std::strcmp(gc, "serial") == 0)  return &fp_serial_store;
    if (std::strcmp(gc, "g1") == 0)      return &fp_g1_store;
    throw std::runtime_error(std::string("unknown GC: ") + gc);
}

// ============================================================
// Approach 3: Decoupled CRTP + Lazy Resolution
// ============================================================

class DCBarrierSet {
public:
    enum Kind { Epsilon, Serial, G1 };
    Kind kind_;
    static DCBarrierSet* the_;

    template <typename BST>
    struct AccessBarrier {
        static void store(int* addr, int value) { *addr = value; }
    };
};
DCBarrierSet* DCBarrierSet::the_ = nullptr;

struct DCEpsilon : DCBarrierSet {
    DCEpsilon() { kind_ = Epsilon; }
    template <typename BST = DCEpsilon>
    struct AccessBarrier : DCBarrierSet::AccessBarrier<BST> {};
};

struct DCSerial : DCBarrierSet {
    DCSerial() { kind_ = Serial; }
    template <typename BST = DCSerial>
    struct AccessBarrier : DCBarrierSet::AccessBarrier<BST> {
        static void store(int* addr, int value) {
            *addr = value;
            sink = 1;
        }
    };
};

struct DCG1 : DCBarrierSet {
    DCG1() { kind_ = G1; }
    template <typename BST = DCG1>
    struct AccessBarrier : DCBarrierSet::AccessBarrier<BST> {
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
        switch (DCBarrierSet::the_->kind_) {
            case DCBarrierSet::G1:      func = &DCG1::AccessBarrier<>::store; break;
            case DCBarrierSet::Serial:  func = &DCSerial::AccessBarrier<>::store; break;
            default:                    func = &DCEpsilon::AccessBarrier<>::store; break;
        }
        _store_func = func;
        func(addr, value);
    }

    static void store(int* addr, int value) { _store_func(addr, value); }
};
RuntimeDispatch::Fn RuntimeDispatch::_store_func = &RuntimeDispatch::store_init;

__attribute__((noinline))
DCBarrierSet* create_dc(const char* gc) {
    if (std::strcmp(gc, "epsilon") == 0) { static DCEpsilon bs; return &bs; }
    if (std::strcmp(gc, "serial") == 0)  { static DCSerial bs;  return &bs; }
    if (std::strcmp(gc, "g1") == 0)      { static DCG1 bs;     return &bs; }
    throw std::runtime_error(std::string("unknown GC: ") + gc);
}

// ============================================================
// Approach 4: std::variant + std::visit
// ============================================================

struct VarEpsilon {
    void store(int* addr, int value) const { *addr = value; }
};

struct VarSerial {
    void store(int* addr, int value) const {
        *addr = value;
        sink = 1;
    }
};

struct VarG1 {
    void store(int* addr, int value) const {
        sink = *addr;
        *addr = value;
        sink = 1;
    }
};

using AnyBarrierSet = std::variant<VarEpsilon, VarSerial, VarG1>;

__attribute__((noinline))
AnyBarrierSet create_variant(const char* gc) {
    if (std::strcmp(gc, "epsilon") == 0) return VarEpsilon{};
    if (std::strcmp(gc, "serial") == 0)  return VarSerial{};
    if (std::strcmp(gc, "g1") == 0)      return VarG1{};
    throw std::runtime_error(std::string("unknown GC: ") + gc);
}

// ============================================================
// Benchmark harness
// ============================================================

using Clock = std::chrono::high_resolution_clock;

struct Result {
    double ns_per_call;
};

template <typename F>
Result bench(const char* label, int* heap, F&& fn) {
    for (long i = 0; i < 1'000'000; ++i)
        fn(heap + (i % HEAP_SIZE), static_cast<int>(i));

    auto start = Clock::now();
    for (long i = 0; i < ITERATIONS; ++i)
        fn(heap + (i % HEAP_SIZE), static_cast<int>(i));
    auto end = Clock::now();

    double total_ns = std::chrono::duration<double, std::nano>(end - start).count();
    double ns_per = total_ns / ITERATIONS;

    std::printf("  %-30s %6.2f ns/call\n", label, ns_per);
    return {ns_per};
}

int main(int argc, char* argv[]) {
    const char* gc = (argc > 1) ? argv[1] : "g1";
    int heap[HEAP_SIZE] = {};

    std::printf("GC: %s, Iterations: %ldM\n\n", gc, ITERATIONS / 1'000'000);

    // All four resolve from argv — compiler cannot see the concrete type
    auto vbs = create_virtual(gc);
    StoreFn fp = resolve_fp(gc);
    DCBarrierSet::the_ = create_dc(gc);
    RuntimeDispatch::store(heap, 0);
    auto var_bs = create_variant(gc);

    auto r_virtual = bench("Virtual dispatch", heap,
        [&](int* addr, int val) { vbs->store(addr, val); });

    auto r_fnptr = bench("Function pointer", heap,
        [&](int* addr, int val) { fp(addr, val); });

    auto r_crtp = bench("Decoupled CRTP (resolved)", heap,
        [&](int* addr, int val) { RuntimeDispatch::store(addr, val); });

    auto r_variant = bench("std::variant + std::visit", heap,
        [&](int* addr, int val) {
            std::visit([&](auto& bs) { bs.store(addr, val); }, var_bs);
        });

    auto r_direct = bench("Direct call (baseline)", heap,
        [](int* addr, int val) {
            sink = *addr;
            *addr = val;
            sink = 1;
        });

    std::printf("\n=== Overhead vs direct call ===\n");
    std::printf("  Virtual:        +%.2f ns\n",
                r_virtual.ns_per_call - r_direct.ns_per_call);
    std::printf("  Function ptr:   +%.2f ns\n",
                r_fnptr.ns_per_call - r_direct.ns_per_call);
    std::printf("  Decoupled CRTP: +%.2f ns\n",
                r_crtp.ns_per_call - r_direct.ns_per_call);
    std::printf("  Variant+visit:  +%.2f ns\n",
                r_variant.ns_per_call - r_direct.ns_per_call);
}
