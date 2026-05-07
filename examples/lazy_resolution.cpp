// Lazy Resolution: resolve the GC barrier once at runtime, then direct dispatch forever.
//
// The function pointer starts at &store_init. The first call resolves
// the runtime GC choice, patches the pointer, and all subsequent calls
// go direct — no vtable, no switch, just a single indirect call.
//
// This is a simplified standalone version of the pattern. The full
// decoupled CRTP + lazy resolution combination is in decoupled_crtp_barrier.cpp.
//
// Compile: g++ -std=c++17 -O2 -march=native lazy_resolution.cpp -o lazy_resolution

#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

// --- GC barrier implementations (free functions) ---

void epsilon_store(int* addr, int value) {
    std::printf("  [raw store] *addr = %d\n", value);
    *addr = value;
}

void serial_store(int* addr, int value) {
    std::printf("  [raw store] *addr = %d\n", value);
    *addr = value;
    std::printf("  [post-barrier] card mark @ %p\n", (void*)addr);
}

void g1_store(int* addr, int value) {
    std::printf("  [pre-barrier] SATB snapshot: old=%d\n", *addr);
    std::printf("  [raw store] *addr = %d\n", value);
    *addr = value;
    std::printf("  [post-barrier] card mark @ %p\n", (void*)addr);
}

// --- Lazy resolver: runtime switch once, cached function pointer thereafter ---

using StoreFn = void(*)(int*, int);

struct LazyBarrier {
    const char* gc_name_;
    mutable StoreFn cached_ = nullptr;

    explicit LazyBarrier(const char* name) : gc_name_(name) {}

    void store(int* addr, int value) const {
        if (!cached_) {
            std::printf("  [resolver] first call -- resolving \"%s\"\n", gc_name_);
            if (std::strcmp(gc_name_, "epsilon") == 0) cached_ = &epsilon_store;
            else if (std::strcmp(gc_name_, "serial") == 0) cached_ = &serial_store;
            else if (std::strcmp(gc_name_, "g1") == 0) cached_ = &g1_store;
            else throw std::runtime_error(std::string("unknown GC: ") + gc_name_);
        }
        cached_(addr, value);  // direct call -- no vtable indirection
    }
};

// --- Virtual dispatch alternative for comparison ---

struct BarrierSet {
    virtual ~BarrierSet() = default;
    virtual void store(int* addr, int value) = 0;
};

struct VirtualEpsilon final : BarrierSet {
    void store(int* addr, int value) override {
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
    }
};

struct VirtualSerial final : BarrierSet {
    void store(int* addr, int value) override {
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
        std::printf("  [post-barrier] card mark @ %p\n", (void*)addr);
    }
};

struct VirtualG1 final : BarrierSet {
    void store(int* addr, int value) override {
        std::printf("  [pre-barrier] SATB snapshot: old=%d\n", *addr);
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
        std::printf("  [post-barrier] card mark @ %p\n", (void*)addr);
    }
};

std::unique_ptr<BarrierSet> make_virtual_barrier(const char* name) {
    if (std::strcmp(name, "epsilon") == 0) return std::make_unique<VirtualEpsilon>();
    if (std::strcmp(name, "serial") == 0) return std::make_unique<VirtualSerial>();
    if (std::strcmp(name, "g1") == 0) return std::make_unique<VirtualG1>();
    throw std::runtime_error("unsupported");
}

// --- Demo ---

int main(int argc, char* argv[]) {
    const char* gc = (argc > 1) ? argv[1] : "g1";
    int heap[4] = {100, 200, 300, 400};

    std::printf("=== Lazy Resolution (resolve once, direct dispatch forever) ===\n");
    LazyBarrier barrier(gc);

    for (int i = 0; i < 3; ++i) {
        std::printf("call %d:\n", i + 1);
        barrier.store(&heap[i], 42 + i);
    }

    std::printf("\n=== Virtual Dispatch (vtable lookup every call) ===\n");
    auto vbarrier = make_virtual_barrier(gc);
    for (int i = 0; i < 3; ++i) {
        std::printf("call %d: vtable indirect call\n", i + 1);
        vbarrier->store(&heap[i], 42 + i);
    }

    std::printf("\nKey difference: lazy resolution pays the runtime cost once,\n"
                "then every subsequent call is a direct function pointer -- no\n"
                "vtable indirection, fully inlineable by the compiler.\n");
}
