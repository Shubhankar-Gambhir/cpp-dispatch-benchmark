# Runtime Plugin Dispatch in C++: Four Approaches Compared

Four ways to dispatch a runtime-selected strategy in C++, using a GC barrier system as the running example. The user chooses a garbage collector at startup (Epsilon, Serial, G1), and every heap write goes through the selected barrier — millions of times per second.

Same domain, same API, same three GCs. Four different dispatch mechanisms.

## The Four Approaches

| # | Approach | File | Key idea |
|---|----------|------|----------|
| 1 | Virtual dispatch | [`examples/virtual_barrier.cpp`](examples/virtual_barrier.cpp) | Abstract base class + vtable |
| 2 | Function pointer | [`examples/type_erased_barrier.cpp`](examples/type_erased_barrier.cpp) | Type-erased, resolved at startup |
| 3 | std::variant + std::visit | [`examples/variant_barrier.cpp`](examples/variant_barrier.cpp) | Closed type set, compiler-generated dispatch |
| 4 | Decoupled CRTP + lazy resolution | [`examples/decoupled_crtp_barrier.cpp`](examples/decoupled_crtp_barrier.cpp) | Template-based, OpenJDK-inspired |

### Additional Pattern Examples

| File | Pattern |
|------|---------|
| [`examples/lazy_resolution.cpp`](examples/lazy_resolution.cpp) | Lazy resolution standalone — resolve once, direct dispatch forever |
| [`examples/decorator_chain.cpp`](examples/decorator_chain.cpp) | Compile-time vs runtime decorator chains for barrier composition |

## Benchmark Results

Intel Xeon Gold 6130 @ 2.10 GHz, 100M iterations, G1 barriers, GCC 11, libstdc++, `-O2 -march=skylake-avx512`.

| Approach | ns/call | Overhead vs direct |
|---|---|---|
| Direct call (baseline) | 1.48 ns | — |
| Decoupled CRTP | 2.42 ns | +0.94 ns |
| Function pointer | 2.43 ns | +0.95 ns |
| Virtual dispatch | 2.90 ns | +1.42 ns |
| std::variant + std::visit | 3.71 ns | +2.23 ns |

### Binary Size

| Approach | Text section | Relative |
|---|---|---|
| Function pointer | 4,727 bytes | baseline |
| Virtual dispatch | 6,776 bytes | +43% |
| Decoupled CRTP | 7,885 bytes | +67% |

## Build

```bash
make              # build everything
make examples     # build examples only
make benchmarks   # build benchmarks only
```

Or compile individually:

```bash
g++ -std=c++17 -O2 -march=native examples/virtual_barrier.cpp -o virtual_barrier
```

## Run

Each example takes the GC name as a command-line argument:

```bash
./build/virtual_barrier g1       # or: epsilon, serial
./build/benchmark_barriers       # runs all four approaches head-to-head
```

## Full Comparison

| | Virtual | FnPtr | variant | Decoupled CRTP |
|---|---|---|---|---|
| **Dispatch overhead** | +1.4 ns | +0.9 ns | +2.2 ns | +0.9 ns |
| **Binary size (text)** | 6.8 KB | 4.7 KB | — | 7.9 KB |
| **Compile time** | 0.38s | 0.25s | 0.32s | 0.29s |
| **Lines of code** | 90 | 65 | 76 | 346 |
| **Extensibility** | Open | Open | Closed | Open |
| **Decoupling** | Partial | Poor | Poor | Excellent |
| **Composition** | Duplicated | Duplicated | Duplicated | Layered |
| **Debuggability** | Excellent | Good | Moderate | Moderate |

## Blog Post

For the full analysis with assembly walkthroughs and a decision framework, see:
[Four Ways to Dispatch a Runtime-Selected Strategy in C++](https://shubhankar-gambhir.github.io/posts/four-ways-to-dispatch-a-runtime-selected-strategy-in-cpp/)

## OpenJDK Source References

The decoupled CRTP approach is inspired by OpenJDK's GC barrier system:

- `src/hotspot/share/gc/shared/barrierSet.hpp` — decoupled CRTP base & AccessBarrier template
- `src/hotspot/share/oops/accessBackend.hpp` — RuntimeDispatch / lazy resolution
- `src/hotspot/share/gc/shared/barrierSetConfig.hpp` — `FOR_EACH_BARRIER_SET_DO` macro

## License

MIT
