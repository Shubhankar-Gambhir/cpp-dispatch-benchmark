// Virtual dispatch — two dependent memory loads per call
// Compiler Explorer: x86-64 gcc 11.4, -O2 -march=skylake-avx512

class BarrierSet {
public:
    virtual ~BarrierSet() = default;
    virtual void store(int* addr, int value) = 0;
};

void call_store(BarrierSet* bs, int* addr, int value) {
    bs->store(addr, value);
}
