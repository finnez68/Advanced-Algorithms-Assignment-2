# Advanced Algorithms Assignment 2: Primality Testing

Theme 2, deterministic vs randomised. Given an integer 1 ≤ n < 2⁶⁴, decide
whether n is **prime**, **composite**, or **neither** (n = 1).

| Algorithm | Kind | Primary author | Code |
|---|---|---|---|
| Miller-Rabin with fixed witness sets | deterministic for 64-bit n | atnm | `MillerRabinFixed/` |
| Miller-Rabin with random bases | randomised, error ≤ 4⁻ᵗ | UShadow03 | `MillerRabinRandom/` |
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
- **`AKS/aks.cpp`** is Finley's AKS implementation. `common/aks_adapter.cpp`
  includes it and wraps `aks_is_prime()` in the shared interface, plus
  `aksWithInfo()`, which also reports AKS's internal statistics (r, bound,
  congruences checked, deciding step) for the benchmark.

### The algorithms in one line each

- **Miller-Rabin, fixed witnesses:** tests the 12 bases 2, 3, 5, …, 37. That
  set is proven to catch every composite below about 3.18 × 10²³, which covers
  all 64-bit n, so a pass means *definitely* prime.
- **Miller-Rabin, random bases:** tests t random bases (t = 20 by default).
  A composite survives each round with probability at most ¼, so the answer
  is "probably prime" with error ≤ 4⁻ᵗ. The trivial cases n = 2 and 3 are
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
meaningless. Add `-DISPRIME_BUILD_GUI=OFF` to build only the algorithms and
tests, without the GUI dependencies.

### Targets

| Target | What it is |
|---|---|
| `primality` | static library with all three algorithms and trial division, shared by the GUI and the tests |
| `isPrime` | the GUI, including the benchmark |
| `tests` | gtest suite: correctness against trial division, shared hard cases, and random Miller-Rabin properties |

## Tests

```
cmake --build build --target tests
./build/tests                 # Linux
build\tests.exe               # Windows (MinGW / Ninja)
build\tests.exe --gtest_also_run_disabled_tests   # also run the slow AKS primes (several minutes)
```

Run a subset with a filter, e.g. `--gtest_filter=MillerRabinRandom.*` or
`--gtest_filter=Aks.*`.

**Correctness against trial division.** Both Miller-Rabin versions are checked
for every n up to 200,000, and AKS for every n up to 1,000 (it is too slow for
more).

**Shared hard cases.** All three algorithms are tested on the same two lists,
`kPrimes` and `kComposites` in `tests/test_algorithms.cpp`:
- Carmichael numbers (561, 1105, 1729, …, 825265)
- strong pseudoprimes to bases 2 / 2,3 / 2,3,5 / 2,3,5,7, which fool weaker
  witness sets, and 3825123056546413051, which fools every prime base from 2
  to 31
- 703 = 19 × 37, whose strong-liar rate (about 22.9%) is close to the ¼ bound
- prime powers, products of close primes, and Mersenne primes
- primes near 2⁶⁴, 2⁶⁴ − 1, 4294967291², and a product of the two largest
  32-bit primes, which catch 64-bit overflow in modular multiplication

AKS runs the composites below 2³² and the primes up to 127. Larger primes take
seconds to minutes each in AKS (524287 takes about 2 minutes), so primes from
128 to 2²⁰ are in `Aks.DISABLED_SlowPrimes`, which runs only with
`--gtest_also_run_disabled_tests`. The largest inputs are tested only on the
two Miller-Rabin versions.

**Random Miller-Rabin properties.** Extra tests check what is specific to
random bases:
- **one-sided error:** a prime is never reported composite, for 1,000 seeds
  with a single round
- **strong-liar bound:** with one round, 703 is fooled sometimes but less than
  ¼ of the time
- **rounds work:** with 20 rounds, every composite is caught on every run
- **reproducibility:** the same seed gives the same results
- **edge cases:** n = 0, 1, 4 and 5, and `rounds` ≤ 0 (clamped to 1)
- **large inputs:** agrees with the fixed version on 100,000 random odd 64-bit n

The fixed version is exact for 64-bit n, so it is the reference for inputs too
large for trial division.

## Benchmark

**Benchmark → Run benchmark suite…** in the GUI runs a fixed set of inputs
through all three algorithms on a background thread, then **Save CSV…**
writes the results. This is where the report's timings come from.

**Inputs.** For each bit length 8, 12, …, 64 (the step is adjustable): the
largest prime with that many bits, a semiprime of two primes near the square
root, and 3 × a large prime. Then 11 named hard cases: Carmichael numbers,
strong pseudoprimes, 703, and 4294967291².

**Runs.**
- Fixed Miller-Rabin once per input.
- Random Miller-Rabin at t = 1, 5, 10, 20 and 40, with a fixed seed
  (default 12345), so runs can be repeated.
- AKS only on primes up to 16 bits and composites up to 32 bits by default.
  Both limits are adjustable; 20-bit primes take minutes each.

Every algorithm is timed with the same `Measure` function as the GUI rows:
one run, or, if that took under 1 ms, the median of 7 batches of about 1 ms
each.

**CSV columns:** `n, bits, type, expected, algorithm, rounds, verdict,
correct, time_ns, calls`, plus `aks_r, aks_bound, aks_congruences_checked,
aks_deciding_step` on AKS rows. The file starts with `#` comment lines
recording the compiler, the `mulmod` path, the build type and the seed
(`pd.read_csv(path, comment='#')` in Python).

Don't press **Go!** on other rows while the benchmark runs: they compete for
the CPU, and the AKS statistics are shared between threads.

## Using the GUI

- Each row has an editable title, a **Number** box and **Go!** (Enter works too).
  The Number box accepts digits, with `,` `_` `'` and spaces as separators.
- **Go!** runs the three algorithms in turn on a background thread, so the
  window stays responsive. A running algorithm shows a live timer and a
  **Cancel** button.
- **AKS is slow on large primes.** It takes seconds for n ≈ 10⁴, about 2
  minutes for a 20-bit prime, and far longer beyond that, so expect to cancel
  it.
  Cancel marks the row cancelled straight away, but a call that's already
  running can't be interrupted from outside: it finishes in the background,
  using one CPU core, and its result is discarded. Closing the window stops
  it.
- **Timing.** Runs under 1 ms are repeated in 7 batches of about 1 ms each, and
  the median per-call time is shown, because a single sub-microsecond reading
  is mostly timer noise. Slower runs are timed once. Hover a runtime for the
  markdown details. These are demo timings; the report's numbers come from the
  benchmark (see above).
- If the algorithms **disagree**, the dissenting row turns red ("Prime" and
  "Probably prime" count as agreeing).
- **File → Load example numbers** loads a demo set: 1, 2, a Carmichael number,
  strong pseudoprimes, 2⁶¹−1, the largest 64-bit prime and more.
  **Save / Open** store rows as JSON.
- **Settings → Preferences** has the UI scale, the random Miller-Rabin rounds t,
  the RNG seed (0 = a new seed per run), and the repeat-timing toggle.
  Settings are saved to `isprime_settings.json` in the working directory.

## Layout

```
AKS/                 Finley: aks.cpp
MillerRabinFixed/    atnm:   miller_rabin_fixed.cpp
MillerRabinRandom/   UShadow03:   miller_rabin_random.cpp
common/              primality.h, modular_arithmetic.h, aks_adapter.cpp, trial_division.cpp
gui/                 main.cpp (window), app.* (UI), runner.* (background runs + timing),
                     benchmark.* (benchmark suite + CSV export),
                     number_input.* (parsing the Number box)
tests/               gtest suites
```

## Adding or replacing an algorithm

Each module has an `...IsPlaceholder()` function. The GUI labels any module
that returns `true` as "(placeholder)", which is how stand-in bodies are
marked while an implementation is in progress. Implement the function
declared in `common/primality.h`, return `false` from `...IsPlaceholder()`,
and run the tests.
