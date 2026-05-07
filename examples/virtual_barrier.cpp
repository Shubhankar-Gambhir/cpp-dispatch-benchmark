// Approach 1: Virtual Dispatch
//
// Runtime plugin selection via abstract base + vtable.
// Clean and familiar, but every store() pays vtable indirection
// and the compiler cannot inline across the virtual call boundary.

#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

class BarrierSet {
public:
    virtual ~BarrierSet() = default;
    virtual void store(int* addr, int value) = 0;
    virtual void store_at(int* base, int offset, int value) = 0;

    static std::unique_ptr<BarrierSet> create(const char* gc_name);
};

// --- Epsilon: no barriers ---

class EpsilonBarrierSet : public BarrierSet {
public:
    void store(int* addr, int value) override {
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
    }
    void store_at(int* base, int offset, int value) override {
        store(base + offset, value);
    }
};

// --- Serial: post-barrier only (card marking) ---

class SerialBarrierSet : public BarrierSet {
public:
    void store(int* addr, int value) override {
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
        std::printf("  [post-barrier] card mark @ %p\n", static_cast<void*>(addr));
    }
    void store_at(int* base, int offset, int value) override {
        store(base + offset, value);
    }
};

// --- G1: pre-barrier (SATB) + post-barrier (card marking) ---

class G1BarrierSet : public BarrierSet {
public:
    void store(int* addr, int value) override {
        std::printf("  [pre-barrier] SATB snapshot: old=%d\n", *addr);
        std::printf("  [raw store] *addr = %d\n", value);
        *addr = value;
        std::printf("  [post-barrier] card mark @ %p\n", static_cast<void*>(addr));
    }
    void store_at(int* base, int offset, int value) override {
        store(base + offset, value);
    }
};

// --- Factory ---

std::unique_ptr<BarrierSet> BarrierSet::create(const char* gc_name) {
    if (std::strcmp(gc_name, "epsilon") == 0) return std::make_unique<EpsilonBarrierSet>();
    if (std::strcmp(gc_name, "serial") == 0)  return std::make_unique<SerialBarrierSet>();
    if (std::strcmp(gc_name, "g1") == 0)      return std::make_unique<G1BarrierSet>();
    throw std::runtime_error(std::string("unknown GC: ") + gc_name);
}

// --- Demo ---

int main(int argc, char* argv[]) {
    const char* gc = (argc > 1) ? argv[1] : "g1";
    auto bs = BarrierSet::create(gc);

    int heap[8] = {};
    std::printf("=== Virtual Dispatch — %s barriers ===\n", gc);

    for (int i = 0; i < 3; ++i) {
        std::printf("store_at(heap, %d, %d):\n", i, (i + 1) * 42);
        bs->store_at(heap, i, (i + 1) * 42);
    }

    std::printf("heap: [");
    for (int i = 0; i < 8; ++i) std::printf("%s%d", i ? ", " : "", heap[i]);
    std::printf("]\n");
}
