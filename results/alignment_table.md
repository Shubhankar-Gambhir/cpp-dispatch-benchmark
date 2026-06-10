Building 36 binaries...
make: Nothing to be done for 'align'.

Running benchmarks (best of 3 runs each)...

| Mechanism | GCC | Default | align=32 | align=64 |
|-----------|-----|---------|----------|----------|
| virtual | 11 | 2.87 | 2.42 | 2.40 |
| virtual | 13 | 2.39 | 2.39 | 2.39 |
| virtual | 15 | 2.87 | 2.39 | 2.39 |
| fnptr | 11 | 2.39 | 2.39 | 2.39 |
| fnptr | 13 | 3.35 | 2.39 | 2.39 |
| fnptr | 15 | 3.35 | 2.39 | 2.39 |
| variant | 11 | 3.62 | 3.63 | 3.59 |
| variant | 13 | 1.44 | 1.44 | 1.44 |
| variant | 15 | 1.44 | 1.44 | 1.44 |
| crtp | 11 | 2.87 | 2.39 | 2.39 |
| crtp | 13 | 3.35 | 2.39 | 2.39 |
| crtp | 15 | 2.39 | 2.39 | 2.39 |

Raw data saved to /home/sgambhir/tmp/cpp-dispatch-benchmark/results/alignment_raw.csv
