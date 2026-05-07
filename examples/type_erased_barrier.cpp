// Approach 2: Type-Erased Function Pointers
//
// Runtime plugin selection via function pointers assigned at startup.
// No class hierarchy needed, but the compiler cannot see through the
// function pointer — every call is an indirect call. Barrier logic
// is duplicated across each function (no composition via inheritance).

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

// --- Free functions: each GC's barrier logic ---

void epsilon_store(int* addr, int value) {
    std::printf("  [raw store] *addr = %d\n", value);
    *addr = value;
}

void serial_store(int* addr, int value) {
    std::printf("  [raw store] *addr = %d\n", value);
    *addr = value;
    std::printf("  [post-barrier] card mark @ %p\n", static_cast<void*>(addr));
}

void g1_store(int* addr, int value) {
    std::printf("  [pre-barrier] SATB snapshot: old=%d\n", *addr);
    std::printf("  [raw store] *addr = %d\n", value);
    *addr = value;
    std::printf("  [post-barrier] card mark @ %p\n", static_cast<void*>(addr));
}

// --- Type-erased dispatch ---

using StoreFn = void(*)(int*, int);

StoreFn resolve(const char* gc_name) {
    if (std::strcmp(gc_name, "epsilon") == 0) return &epsilon_store;
    if (std::strcmp(gc_name, "serial") == 0)  return &serial_store;
    if (std::strcmp(gc_name, "g1") == 0)      return &g1_store;
    throw std::runtime_error(std::string("unknown GC: ") + gc_name);
}

void store_at(StoreFn fn, int* base, int offset, int value) {
    fn(base + offset, value);
}

// --- Demo ---

int main(int argc, char* argv[]) {
    const char* gc = (argc > 1) ? argv[1] : "g1";
    StoreFn store_fn = resolve(gc);

    int heap[8] = {};
    std::printf("=== Type-Erased Function Pointer — %s barriers ===\n", gc);

    for (int i = 0; i < 3; ++i) {
        std::printf("store_at(heap, %d, %d):\n", i, (i + 1) * 42);
        store_at(store_fn, heap, i, (i + 1) * 42);
    }

    std::printf("heap: [");
    for (int i = 0; i < 8; ++i) std::printf("%s%d", i ? ", " : "", heap[i]);
    std::printf("]\n");
}
