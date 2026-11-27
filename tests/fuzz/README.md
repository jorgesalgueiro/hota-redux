# Fuzz targets

The LZSS backdrop decoder parses data from the game ISO. These targets
feed arbitrary bytes through it under AddressSanitizer/UBSan.

## Build with CMake

```
cmake -B build-fuzz -DHOTA_BUILD_FUZZERS=ON -DCMAKE_C_COMPILER=clang
cmake --build build-fuzz --target fuzz_unlzss
./build-fuzz/fuzz_unlzss -max_total_time=60
```

## Build directly

```
clang -g -O1 -fsanitize=fuzzer,address,undefined -I src \
    tests/fuzz/fuzz_unlzss.c src/lzss.c src/vm.c src/debug.c \
    -o fuzz_unlzss
./fuzz_unlzss -max_total_time=60
```

The CI workflow runs a short 30-second fuzzing pass. Use a longer
`-max_total_time` and a corpus directory for deeper runs.
