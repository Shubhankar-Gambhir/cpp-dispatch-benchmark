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
