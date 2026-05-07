// Approach 4: std::variant + std::visit
//
// Runtime plugin selection via a closed set of types in a variant.
// std::visit generates a jump table at compile time — no vtable,
// no function pointer indirection. But the type set is closed:
// adding a new GC means modifying the variant typedef.

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <variant>

// --- Barrier sets (plain structs, no base class, no virtual) ---

struct EpsilonBarrierSet {
    void store(int* addr, int value) const {
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
    }
};

struct SerialBarrierSet {
    void store(int* addr, int value) const {
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
        std::printf("  [post-barrier] card mark @ %p\n", static_cast<void*>(addr));
    }
};

struct G1BarrierSet {
    void store(int* addr, int value) const {
        std::printf("  [pre-barrier] SATB snapshot: old=%d\n", *addr);
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
        std::printf("  [post-barrier] card mark @ %p\n", static_cast<void*>(addr));
    }
};

// Closed set — every GC must be listed here
using AnyBarrierSet = std::variant<EpsilonBarrierSet, SerialBarrierSet, G1BarrierSet>;

AnyBarrierSet create(const char* gc_name) {
    if (std::strcmp(gc_name, "epsilon") == 0) return EpsilonBarrierSet{};
    if (std::strcmp(gc_name, "serial") == 0)  return SerialBarrierSet{};
    if (std::strcmp(gc_name, "g1") == 0)      return G1BarrierSet{};
    throw std::runtime_error(std::string("unknown GC: ") + gc_name);
}

void store(AnyBarrierSet& bs, int* addr, int value) {
    std::visit([&](auto& concrete) { concrete.store(addr, value); }, bs);
}

void store_at(AnyBarrierSet& bs, int* base, int offset, int value) {
    store(bs, base + offset, value);
}

// --- Demo ---

int main(int argc, char* argv[]) {
    const char* gc = (argc > 1) ? argv[1] : "g1";
    auto bs = create(gc);

    int heap[8] = {};
    std::printf("=== std::variant + std::visit — %s barriers ===\n", gc);
    std::printf("variant index = %zu\n\n", bs.index());

    for (int i = 0; i < 3; ++i) {
        std::printf("store_at(heap, %d, %d):\n", i, (i + 1) * 42);
        store_at(bs, heap, i, (i + 1) * 42);
    }

    std::printf("heap: [");
    for (int i = 0; i < 8; ++i) std::printf("%s%d", i ? ", " : "", heap[i]);
    std::printf("]\n");
}
