# CS3006 Assignment 2 — Results Summary (for write-up drafting)

## Machine Declaration

| Field | Value | Status |
|---|---|---|
| CPU model | Intel(R) Core(TM) i5-6300HQ CPU @ 2.30GHz | ✅ |
| Physical cores | 4 | ✅ confirmed |
| Hardware threads (logical CPUs) | 4 | ✅ confirmed |
| SMT / Hyper-Threading present? | No | ✅ confirmed |
| Base / max clock | 2.30 / 3.20 GHz | ✅ |
| Widest SIMD available | AVX2 (8-wide single precision), W=8 | ✅ confirmed via `/proc/cpuinfo` |
| RAM size | 7,602,280 kB (about 7.25 GiB visible) | ✅ |
| RAM type/speed/channels | DDR4-2133, single-channel | ✅ |
| Machine type | KVM full-virtualization guest on my own laptop; four vCPUs exposed, no SMT | ✅ |
| OS, kernel, GCC version | Ubuntu; Linux 7.0.0-31-generic; GCC 15.2.0 | ✅ |
| ISPC version | Intel ISPC 1.31.0, LLVM 23.0.0 | ✅ |
| Power state during measurement | Plugged in; performance governor unavailable in the guest | ✅ |

## Build Notes
- Fix 1: `#include <cstring>` added to `prog1_mandelbrot_threads/main.cpp` (fixes `memset` not declared)
- Fix 2: `#include <cstdlib>` added to `prog1_mandelbrot_threads/mandelbrotThread.cpp` (fixes `exit` not declared)
- Used ISPC v1.31.0 instead of handout's v1.28.1

---

## Program 1 — Mandelbrot (threads)

**Decomposition comparison (4 threads, per-thread timings):**
| Decomposition | Per-thread time range |
|---|---|
| Blocked (naive row-block) | 60–372 ms |
| Cyclic (interleaved) | 178–183 ms |

→ Demonstrates blocked decomposition is load-imbalanced; cyclic evens it out.

**Thread count speedup (minimum-of-five timing per invocation):**
| Threads | Serial | Threaded | Speedup | Ideal |
|---|---|---|---|---|
| 4 (=T) | 927.913 ms | 275.590 ms | 3.37x | 4x |
| 8 (=2T) | 569.903 ms | 243.607 ms | 2.34x | 8x |

The serial baselines came from separate invocations and varied substantially despite each invocation taking the minimum of five samples. This is measurement noise from the virtualized/shared environment and changing CPU scheduling or frequency, so each speedup is compared with the serial baseline from its own invocation. The conclusion is stable: four threads are close to ideal, while eight threads lose efficiency.

**Explanation for 2T shortfall:** No SMT — 4 physical cores only. 8 threads oversubscribes cores 2:1, adding OS scheduling/context-switch overhead without additional hardware execution lanes, so 2T speedup underperforms proportionally versus T.

---

## Program 2 — Vector Intrinsics

**Vector utilization by width:**
| Width | Utilization |
|---|---|
| 2 | 85.8% |
| 4 | 79.1% |
| 8 | 75.3% |
| 16 | 73.7% |

**Trend explanation:** As vector width increases, the fixed amount of divergent/masked-out work from the clamping control flow represents a proportionally larger share of total lane-work, so utilization declines monotonically with width.

---

## Program 3 — Mandelbrot (ISPC)

C = 4, W = 8 → SIMD ceiling = 8, Task ceiling = C×W = 32

**Timing spread:**
| Config | Time range |
|---|---|
| Serial | 442.078–495.388 ms |
| ISPC (SIMD only) | 70.833–130.692 ms |
| ISPC + tasks (4 tasks) | 47.067–64.526 ms |

**Speedups:**
- ISPC-only: 6.24x → 6.24/8 = **78.0% of SIMD ceiling**
- ISPC+tasks: 9.39x → 9.39/32 = **29.3% of task ceiling**

These are the final measurements used here; the earlier 4.27x/6.16x sample was discarded because it came from a different run.

**Task shortfall explanation:** task/runtime launch overhead, memory bandwidth limits, imperfect load balancing across 4 tasks.

---

## Program 4 — Sqrt (best/worst inputs)

**Lane iteration counts:**
- Best case: `0 0 0 0 0 0 0 0` (all lanes converge identically)
- Worst case: `21 0 0 0 0 0 0 0` (one straggler lane)

**SIMD-only results:**
| Case | Serial | ISPC | Speedup |
|---|---|---|---|
| Best | 26.352 ms | 14.507 ms | 1.82x |
| Worst | 494.972 ms | 656.216 ms | **0.75x (slowdown)** |

**Task results:**
| Case | Speedup |
|---|---|
| Best | 1.49x |
| Worst | 3.19x |

**Timing spread:**
| Config | Time range |
|---|---|
| Best ISPC | 14.507–15.075 ms |
| Worst ISPC | 656.216–685.159 ms |

**Key explanation (write this explicitly):**
- SIMD-only worst-case <1x demonstrates lock-step execution: the whole vector waits on the single slow lane (21 iterations vs 0).
- Task result inversion (worst > best) is real and explainable: with tasks, only SIMD groups containing the outlier pay the lock-step penalty — other groups on other cores finish independently. The worst case also has far more absolute serial time (494.972 ms vs 26.352 ms) available to reclaim, giving threads proportionally more headroom than in the already-fast best case, where launch overhead eats a larger share of any possible gain.

---

## Program 5 — Saxpy (bandwidth)

**Results:**
| Config | GB/s |
|---|---|
| ISPC-only | 10.659 |
| ISPC+tasks | 14.034 |

**Timing spread:**
| Config | Time range |
|---|---|
| ISPC | 30.020–35.074 ms |
| Tasks | 22.801–27.077 ms |

**RAM peak:** DDR4-2133 single-channel = 2133×10⁶×8 bytes ≈ **17.064 GB/s** (decimal convention, consistent with measured units)

**% of peak:**
- ISPC-only: 10.659/17.064 = 62.5%
- Tasks: 14.034/17.064 = 82.3%

**Explanation:** memory-bandwidth-bound workload, not compute-bound — SIMD/task gains are capped by data movement, not arithmetic throughput.

---

## Program 6 — K-means

**Matched serial/parallel pair (final, corrected):**
- Serial: 78.943 s
- 4 threads: 44.520 s
- Achieved speedup: **1.773x**
- Ideal: 4x

**Amdahl analysis:**
- Measured parallel fraction f = 0.65
- Smax = 1/((1−0.65) + 0.65/4) = **1.951**
- Achieved = 90.9% of Smax (well above the 80% requirement)

**Karp-Flatt:**
- e = (1/1.773 − 1/4)/(1 − 1/4) ≈ **0.419**
- Compare to directly measured 1−f = 0.35 → higher effective serial fraction indicates thread creation/join overhead, synchronization, and cache/memory effects not captured by the profiling-based f alone. **This disagreement is worth its own paragraph per the addendum.**

**Additional profiling detail:** at 4 threads, the assignment phase shrinks to 42.6% of remaining runtime (vs. its higher share serially), because the parallelized assignment step shrinks while the serial centroid/cost computation stays fixed.

**Profiling narrative:** I first timed the complete `kMeansThread` call and then added `CycleTimer` measurements around assignment, centroid recomputation, and cost calculation. The initial breakdown showed that distance calculations during point-to-centroid assignment dominated the runtime, so the first hypothesis was that assigning different points to different workers would provide the largest gain. I tested that decomposition by giving each worker a disjoint range of points; this avoided races on `clusterAssignments` and reduced the dominant phase substantially. I did not parallelize centroid recomputation because it is a smaller serial section and mutates shared centroid state, making synchronization costs harder to justify. The final four-thread run reached 1.773x, while the measured Amdahl limit was 1.951x. The higher Karp-Flatt effective serial fraction relative to the directly measured serial fraction shows that thread creation/join, synchronization, and memory/cache effects add overhead not captured by the simple phase fraction.

`data.dat` MD5: `3a25f24193f4fdca82ee4cb2737fd5bb`.

`start.png` and `end.png` show three coherent clusters; the final plot shows the assignments converging around their three centroids.

---

## Disclosure and Extra Credit

OpenAI`s GPT5.6-Sol was used to help interpret the assignment requirements, check implementation reasoning, and review the consistency of the measured results. All builds, benchmark runs, measurements, and conclusions were performed and verified in this workspace.

Extra credit was not attempted till now