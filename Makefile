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
