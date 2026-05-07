// std::variant + std::visit — generates _S_vtable + lambda capture spills
// Compiler Explorer: x86-64 gcc 11.4, -O2 -march=skylake-avx512

#include <variant>

struct EpsilonBarrierSet {
    void store(int* addr, int value) const { *addr = value; }
};

struct SerialBarrierSet {
    void store(int* addr, int value) const {
        *addr = value;
        volatile int card = *addr;  // simulate post-barrier
        (void)card;
    }
};

struct G1BarrierSet {
    void store(int* addr, int value) const {
        volatile int old = *addr;   // simulate pre-barrier
        (void)old;
        *addr = value;
        volatile int card = *addr;  // simulate post-barrier
        (void)card;
    }
};

using AnyBarrierSet = std::variant<EpsilonBarrierSet, SerialBarrierSet, G1BarrierSet>;

void call_store(AnyBarrierSet& bs, int* addr, int value) {
    std::visit([&](auto& b) { b.store(addr, value); }, bs);
}
