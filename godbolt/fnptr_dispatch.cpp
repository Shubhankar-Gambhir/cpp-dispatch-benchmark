// Function pointer dispatch — single indirect call through register
// Compiler Explorer: x86-64 gcc 11.4, -O2 -march=skylake-avx512

using StoreFn = void(*)(int*, int);

void call_store(StoreFn fn, int* addr, int value) {
    fn(addr, value);
}
