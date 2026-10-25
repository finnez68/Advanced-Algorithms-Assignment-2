# Advanced Algorithms Assignment 2: Primality Testing

Theme 2, deterministic vs randomised. Given an integer 1 ≤ n < 2⁶⁴, decide
whether n is **prime**, **composite**, or **neither** (n = 1).

| Algorithm | Kind | Primary author | Code |
|---|---|---|---|
| Miller-Rabin with fixed witness sets | deterministic for 64-bit n | Atnm | `MillerRabinFixed/` |
| Miller-Rabin with random bases | randomised, error ≤ 4⁻ᵏ | Dino | `MillerRabinRandom/` |
| AKS | deterministic, polynomial time | Finley | `AKS/` |
| Trial division | reference oracle for testing | — | `common/trial_division.cpp` |

All of them are reached through one shared interface, `common/primality.h`.
Finley's `AKS/aks.cpp` is used unchanged: `common/aks_adapter.cpp` includes
it with `AKS_NO_MAIN` defined (as `AKS/aks_testing.cpp` does) and wraps
`aks_is_prime()`.

## Status

- **AKS:** real implementation (Finley).
- **Both Miller-Rabin modules:** still **placeholders** that call trial division,
  so the GUI and tests run end to end. The GUI labels them "(placeholder)". To
  plug yours in, replace the body in your module and change your
  `...IsPlaceholder()` function to `return false;`.

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
hard cases: Carmichael numbers, strong pseudoprimes to bases 2 / 2,3 / 2,3,5 /
2,3,5,7, primes near 2⁶⁴, and a product of two 32-bit primes. It takes about
40 s while the Miller-Rabin placeholders are in, because the big cases go
through trial division.

## Using the GUI

- Each row has an editable title, a **Number** box and **Go!** (Enter works too).
  The Number box accepts digits, with `,` `_` `'` and spaces as separators.
- **Go!** runs the three algorithms in turn on a background thread, so the
  window stays responsive. A running algorithm shows a live timer and a
  **Cancel** button. AKS takes about 1 s for n ≈ 10⁴ and minutes near 2³¹
  (see `AKS/aks.cpp`), so expect to cancel it on big numbers. Cancel can't
  stop a call that's already running: that call finishes in the background
  and its result is discarded.
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
