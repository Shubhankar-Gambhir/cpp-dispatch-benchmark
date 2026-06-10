CXX      = g++
CXXFLAGS = -std=c++17 -O2 -march=native -Wall -Wextra

EXAMPLES = virtual_barrier type_erased_barrier variant_barrier \
           decoupled_crtp_barrier lazy_resolution decorator_chain

BENCHMARKS = bench_direct bench_virtual bench_fnptr bench_variant \
             bench_crtp benchmark_barriers

.PHONY: all examples benchmarks clean

all: examples benchmarks

examples: $(addprefix build/,$(EXAMPLES))

benchmarks: $(addprefix build/,$(BENCHMARKS))

build/%: examples/%.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/%: benchmarks/%.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build

# Polymorphic benchmarks -- require pinned GCC versions
POLY_FLAGS   = -std=c++17 -O2 -march=skylake-avx512 -fcf-protection \
               -falign-functions=64 -falign-loops=64
GCC11        = $(HOME)/utils/mamba/envs/gcc11/bin/x86_64-conda-linux-gnu-g++
GCC15        = $(HOME)/utils/mamba/envs/gcc15/bin/x86_64-conda-linux-gnu-g++

POLY_BENCHES = bench_poly_virtual bench_poly_fnptr bench_poly_variant bench_poly_crtp

.PHONY: poly

poly: $(foreach b,$(POLY_BENCHES),build/$(b)_gcc11 build/$(b)_gcc15)

build/%_gcc11: benchmarks/%.cpp | build
	$(GCC11) $(POLY_FLAGS) $< -o $@

build/%_gcc15: benchmarks/%.cpp | build
	$(GCC15) $(POLY_FLAGS) $< -o $@

# Alignment artifact benchmarks -- Part 6
# Three alignment settings x three GCC versions x four mechanisms = 36 binaries
BASE_FLAGS   = -std=c++17 -O2 -march=skylake-avx512 -fcf-protection
ALIGN32      = -falign-functions=32 -falign-loops=32
ALIGN64      = -falign-functions=64 -falign-loops=64
GCC13        = $(HOME)/utils/mamba/envs/gcc13/bin/x86_64-conda-linux-gnu-g++

ALIGN_BENCHES = bench_virtual bench_fnptr bench_variant bench_crtp

.PHONY: align

align: $(foreach b,$(ALIGN_BENCHES),\
         $(foreach g,gcc11 gcc13 gcc15,\
           $(foreach a,default align32 align64,\
             build/$(b)_$(g)_$(a))))

# Default alignment (no -falign-* flags)
build/%_gcc11_default: benchmarks/%.cpp | build
	$(GCC11) $(BASE_FLAGS) $< -o $@

build/%_gcc13_default: benchmarks/%.cpp | build
	$(GCC13) $(BASE_FLAGS) $< -o $@

build/%_gcc15_default: benchmarks/%.cpp | build
	$(GCC15) $(BASE_FLAGS) -static $< -o $@

# 32-byte alignment
build/%_gcc11_align32: benchmarks/%.cpp | build
	$(GCC11) $(BASE_FLAGS) $(ALIGN32) $< -o $@

build/%_gcc13_align32: benchmarks/%.cpp | build
	$(GCC13) $(BASE_FLAGS) $(ALIGN32) $< -o $@

build/%_gcc15_align32: benchmarks/%.cpp | build
	$(GCC15) $(BASE_FLAGS) $(ALIGN32) -static $< -o $@

# 64-byte alignment
build/%_gcc11_align64: benchmarks/%.cpp | build
	$(GCC11) $(BASE_FLAGS) $(ALIGN64) $< -o $@

build/%_gcc13_align64: benchmarks/%.cpp | build
	$(GCC13) $(BASE_FLAGS) $(ALIGN64) $< -o $@

build/%_gcc15_align64: benchmarks/%.cpp | build
	$(GCC15) $(BASE_FLAGS) $(ALIGN64) -static $< -o $@
