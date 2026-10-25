# Advanced Algorithms Assignment 2: Primality Testing

Theme 2, deterministic vs randomised. Given an integer 1 ≤ n < 2⁶⁴, decide
whether n is **prime**, **composite**, or **neither** (n = 1).

| Algorithm | Kind | Primary author | Code |
|---|---|---|---|
| Miller-Rabin with fixed witness sets | deterministic for 64-bit n | atnm | `MillerRabinFixed/` |
| Miller-Rabin with random bases | randomised, error ≤ 4⁻ᵏ | UShadow03 | `MillerRabinRandom/` |
| AKS | deterministic, polynomial time | Finley | `AKS/` |
| Trial division | reference oracle for testing | — | `common/trial_division.cpp` |

## How the code fits together

- **`common/primality.h`** is the shared interface every algorithm implements.
  Each one returns a `Verdict`: `Prime`, `Composite`, `Neither`, or (random
  Miller-Rabin only) `ProbablyPrime`.
- **`common/modular_arithmetic.h`** is shared by all three algorithms:
  `addmod`, `mulmod` and `powmod` in namespace `mod_arith`. It is
  header-only, so including it is enough. `mulmod` uses `unsigned __int128`
  where the compiler supports it (GCC, Clang, MinGW), and otherwise falls back
  to double-and-add (MSVC). Because the arithmetic is shared, timing
  differences between the algorithms come from the algorithms, not from
  different multiplication code.
- **`AKS/aks.cpp`** is a standalone program with its own `main()`.
  `common/aks_adapter.cpp` includes it with `AKS_NO_MAIN` defined (as
  `AKS/aks_testing.cpp` does) and wraps `aks_is_prime()` in the shared
  interface.

### The algorithms in one line each

- **Miller-Rabin, fixed witnesses:** tests the 12 bases 2, 3, 5, …, 37. That
  set is proven to catch every composite below about 3.18 × 10²³, which covers
  all 64-bit n, so a pass means *definitely* prime.
- **Miller-Rabin, random bases:** tests k random bases (k = 20 by default).
  A composite survives each round with probability at most ¼, so the answer
  is "probably prime" with error ≤ 4⁻ᵏ. The trivial cases n = 2 and 3 are
  answered exactly before any rounds run.
- **AKS:** the deterministic polynomial-time test, with polynomials mod
  (Xʳ − 1, n) multiplied by schoolbook convolution. Its cost is set by r,
  which is just above 4096 for 64-bit n.

## Building

You need CMake ≥ 3.16, a C++20 compiler, and Git. On the first configure, CMake
uses Git to download GLFW, Dear ImGui, nlohmann/json, tinyfiledialogs and
googletest.

**Linux**

```
sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/isPrime
```

**Windows (MinGW, e.g. MSYS2 or CLion's bundled toolchain)**

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\isPrime.exe
```

A Release build on MinGW is statically linked and has no console window.

**Use GCC, Clang or MinGW for any timings.** With MSVC everything still
compiles, but `mulmod` takes the slower double-and-add fallback, so all three
algorithms run several times slower. Seeded runs of random Miller-Rabin also
pick different bases under MSVC, because `std::uniform_int_distribution`
differs between standard libraries.

The build type defaults to Release, because unoptimised timings are
meaningless. Add `-DISPRIME_BUILD_GUI=OFF` to build only the algorithms,
tests and AKS programs, without the GUI dependencies.

### Targets

| Target | What it is |
|---|---|
| `isPrime` | the GUI |
| `tests` | gtest suite: every algorithm against trial division |
| `aks` | Finley's standalone AKS program (`./aks 1000003`, or no args for its self-test) |
| `aks_testing` | Finley's AKS data-collection harness (writes `aks_results.csv`) |

## Tests

```
cmake --build build --target tests
./build/tests
```

The suite checks every algorithm against trial division for each n up to
200,000 (1,000 for AKS, matching `aks.cpp`'s own self-test). It also covers
hard cases:
- Carmichael numbers (561, 1105, 1729, …)
- strong pseudoprimes to bases 2 / 2,3 / 2,3,5 / 2,3,5,7, which fool weaker
  witness sets
- primes near 2⁶⁴, which catch 64-bit overflow in modular multiplication
- 2⁶⁴ − 1, and a product of the two largest 32-bit primes

All 14 tests pass, and the suite takes about 15 s. Most of that is trial
division and AKS; the Miller-Rabin tests take milliseconds.

## Using the GUI

- Each row has an editable title, a **Number** box and **Go!** (Enter works too).
  The Number box accepts digits, with `,` `_` `'` and spaces as separators.
- **Go!** runs the three algorithms in turn on a background thread, so the
  window stays responsive. A running algorithm shows a live timer and a
  **Cancel** button.
- **AKS is slow on large primes.** It takes about 1 s for n ≈ 10⁴, minutes for
  a 32-bit prime, and hours for a prime near 2⁶⁴, so expect to cancel it.
  Cancel marks the row cancelled straight away, but a call that's already
  running can't be interrupted from outside: it finishes in the background,
  using one CPU core, and its result is discarded. Closing the window stops
  it.
- **Timing.** Runs under 1 ms are repeated in 7 batches of about 1 ms each, and
  the median per-call time is shown, because a single sub-microsecond reading
  is mostly timer noise. Slower runs are timed once. Hover a runtime for the
  details. These are demo timings; the report's numbers should come from
  repeated benchmark runs.
- If the algorithms **disagree**, the dissenting row turns red ("Prime" and
  "Probably prime" count as agreeing).
- **File → Load example numbers** loads a demo set: 1, 2, a Carmichael number,
  strong pseudoprimes, 2⁶¹−1, the largest 64-bit prime and more.
  **Save / Open** store rows as JSON.
- **Settings → Preferences** has the UI scale, the random Miller-Rabin rounds k,
  the RNG seed (0 = a new seed per run), and the repeat-timing toggle.
  Settings are saved to `isprime_settings.json` in the working directory.

## Layout

```
AKS/                 Finley: aks.cpp, aks_testing.cpp, aks_results.csv
MillerRabinFixed/    Atnm:   miller_rabin_fixed.cpp
MillerRabinRandom/   Dino:   miller_rabin_random.cpp
common/              primality.h, modular_arithmetic.h, aks_adapter.cpp, trial_division.cpp
gui/                 main.cpp (window), app.* (UI), runner.* (background runs + timing),
                     number_input.* (parsing the Number box)
tests/               gtest suites
```

## Adding or replacing an algorithm

Each module has an `...IsPlaceholder()` function. The GUI labels any module
that returns `true` as "(placeholder)", which is how stand-in bodies are
marked while an implementation is in progress. Implement the function
declared in `common/primality.h`, return `false` from `...IsPlaceholder()`,
and run the tests.
