// Compile-time decorator chains vs runtime decorator chains for GC barriers.
//
// Each barrier concern (SATB pre-barrier, raw store, card marking post-barrier)
// is a separate decorator layer. The compile-time version uses template
// inheritance — the compiler flattens the chain to straight-line code.
// The runtime version uses virtual dispatch with a pointer chain.
//
// Compile: g++ -std=c++17 -O2 -march=native decorator_chain_middleware.cpp -o decorator_chain

#include <cstdio>
#include <memory>

// --- Compile-time decorators (template chain) ---

struct RawStore {
    static void store(int* addr, int value) {
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
    }
};

template <typename Next>
struct CardMarkDecorator : Next {
    static void store(int* addr, int value) {
        Next::store(addr, value);
        std::printf("  [post-barrier] card mark @ %p\n", (void*)addr);
    }
};

template <typename Next>
struct SATBDecorator : Next {
    static void store(int* addr, int value) {
        std::printf("  [pre-barrier] SATB snapshot: old=%d\n", *addr);
        Next::store(addr, value);
    }
};

// Compose barrier chains as type aliases — adding/removing a concern
// is a one-line change, and the compiler flattens everything.
using EpsilonBarrier = RawStore;                                   // raw only
using SerialBarrier  = CardMarkDecorator<RawStore>;                // raw + post
using G1Barrier      = SATBDecorator<CardMarkDecorator<RawStore>>; // pre + raw + post

// --- Runtime decorators (virtual dispatch + pointer chain) ---

struct BarrierBase {
    virtual ~BarrierBase() = default;
    virtual void store(int* addr, int value) = 0;
};

struct RuntimeRawStore : BarrierBase {
    void store(int* addr, int value) override {
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
    }
};

struct RuntimeCardMark : BarrierBase {
    std::unique_ptr<BarrierBase> next;
    explicit RuntimeCardMark(std::unique_ptr<BarrierBase> n) : next(std::move(n)) {}
    void store(int* addr, int value) override {
        next->store(addr, value);
        std::printf("  [post-barrier] card mark @ %p\n", (void*)addr);
    }
};

struct RuntimeSATB : BarrierBase {
    std::unique_ptr<BarrierBase> next;
    explicit RuntimeSATB(std::unique_ptr<BarrierBase> n) : next(std::move(n)) {}
    void store(int* addr, int value) override {
        std::printf("  [pre-barrier] SATB snapshot: old=%d\n", *addr);
        next->store(addr, value);
    }
};

// --- Demo ---

int main() {
    int heap[4] = {100, 200, 300, 400};

    std::printf("=== Compile-time decorator chains ===\n\n");

    std::printf("Epsilon (raw only):\n");
    EpsilonBarrier::store(&heap[0], 42);

    std::printf("\nSerial (raw + post):\n");
    SerialBarrier::store(&heap[1], 43);

    std::printf("\nG1 (pre + raw + post):\n");
    G1Barrier::store(&heap[2], 44);

    std::printf("\n=== Runtime decorator chains ===\n\n");

    std::printf("Epsilon (raw only):\n");
    auto rt_epsilon = std::make_unique<RuntimeRawStore>();
    rt_epsilon->store(&heap[0], 42);

    std::printf("\nSerial (raw + post):\n");
    auto rt_serial = std::make_unique<RuntimeCardMark>(
        std::make_unique<RuntimeRawStore>());
    rt_serial->store(&heap[1], 43);

    std::printf("\nG1 (pre + raw + post):\n");
    auto rt_g1 = std::make_unique<RuntimeSATB>(
        std::make_unique<RuntimeCardMark>(
            std::make_unique<RuntimeRawStore>()));
    rt_g1->store(&heap[2], 44);

    std::printf("\nKey difference: the compile-time chain is flattened to\n"
                "straight-line code (zero virtual calls). The runtime chain\n"
                "traverses a pointer chain with a virtual call at each layer.\n");
}
