// Decoupled CRTP + lazy resolution — single indirect call through global
// Compiler Explorer: x86-64 gcc 11.4, -O2 -march=skylake-avx512

using StoreFn = void(*)(int*, int);

extern StoreFn _store_func;  // resolved at first call, cached thereafter

void call_store(int* addr, int value) {
    _store_func(addr, value);
}
