# Header-only performance controls

[Report](../performance.md) · **English** · [简体中文](header-only.zh-CN.md)

Measured **2026-10-11** on Windows x64 / MSVC 19.44.35208, mimalloc 3.4.3 shared SDK, Release `/O2 /EHsc /MD`, statistics OFF and checks OFF. Core source includes the full Heap/Stack interface, Global PMR and the private stateless Global shared allocator. The three builds use the same source, SDK binary and CRT; each build applies the same compiler flags to SDK and API benchmark translation units. SDK producer binaries are unchanged.

The V5 protocol pins workers using compact CPU placement and verifies their CPU before/after the workload. Five paired runs alternate path order, calibrate equal iterations for both paths, and validate legal payload/checksum work. All **23 workload/scale groups** are retained, including raw and typed operations, PMR, 1/2/4/8/16-thread allocation, highest stateless shared and a separate stateful shared reference. These are bounded microbenchmarks, not a guarantee for every application or per-operation latency.

## Vector: absolute time and paired ratio

The workload reserves 256 uint64 elements, constructs the same 256 values, reads its checksum and destroys the vector. ns/iteration is the complete container workload; it is not SDK malloc latency. Ratios are the median of paired `100 * SDK/API`; separately computed time medians need not divide to that same ratio.

| Control | SDK median ns | API median ns | Paired ratio median [min,max] | Group medians >=90% | Groups with every pair >=90% |
| --- | ---: | ---: | --- | --- | --- |
| Default `/Ob2`, IPO OFF | 221.59 | 250.05 | 89.79% [84.30,94.31] | 22/23 | 20/23 |
| `/Ob2`, IPO ON | 214.45 | 492.48 | 43.02% [42.90,44.44] | 22/23 | 19/23 |
| `/Ob3`, IPO OFF | 252.45 | 210.34 | 119.02% [115.90,129.51] | 23/23 | 23/23 |

`/Ob3` improves API vector time by about **16%** versus default, while the SDK vector workload becomes about **14% slower**. Its 119.02% ratio includes both changes; it does not mean the SDK allocator accelerated. API 210.34 ns is also about 5% below the default SDK 221.59 ns reference. IPO instead increases API time from 250.05 to 492.48 ns, a real regression; it is not the recommended default.

The `/Ob3` build passes the >=90% criterion for all 23 medians and every recorded pair: worst median 98.299%, lowest pair 92.574%, highest median 121.726%. That threshold is this protocol's acceptance rule, not proof of identical native code or a universal speed guarantee.

## What the machine code confirms

Default emits API cold growth inline and SDK growth out of line. IPO moves API `reserve` to an out-of-line helper: call `0x140008f34 -> reserve 0x140009fe0 -> AllocateAtLeast 0x14000a1c0`. The allocation helper reloads generic context/count, tests zero/overflow and compares Global identity at`0x14000a229` before calling the same `mi_malloc` SDK entry. These normal reserve checks are additional work; growth remains cold after successful reserve 256.

The IPO API successful push path has 11 decoded instructions per element versus default 15; the slowdown therefore cannot be explained by counting entire functions or assuming every cold instruction executes per push. The observed helper and memory-access changes do not quantitatively explain the full timing gap; cycle-level attribution remains unknown. Compiler decisions can affect SDK and API container loops differently.

## Shared allocation and semantics

On this MSVC scalar 64-byte payload, actual primary shared allocation requests are **SDK 80 bytes / API 80 bytes**, alignment 8. Global `make_shared()` uses a private empty allocator whose binding is the immortal Global; Heap/Stack retain the public allocator's explicit Memory binding. Statistics ON still records exact successful allocation/free counts, and constructor failure uses standard allocator cleanup.

The separately retained group named `shape_matched_shared` is the earlier stateful SDK reference: its allocator has the same 8-byte binding footprint as the public `Allocator`, but the final Global shared path now uses an empty private allocator. Its 88-byte SDK control block is therefore not a same-byte reference for the final 80-byte API path. It is reported alongside the primary highest stateless reference, not substituted for it.

## Build preference and evidence limits

`UNIMEMORY_AGGRESSIVE_INLINING=ON` now selects MSVC Release `/Ob3` through the exported interface target. It affects the entire consumer C++ translation unit; source and installed consumers can set it OFF. GCC/Clang options are unchanged and global IPO is not forced. Microsoft documents `/Ob3` as more aggressive inlining, available from Visual Studio 2019, while still treating inline keywords and flags as suggestions. [Official `/Ob` reference](https://learn.microsoft.com/en-us/cpp/build/reference/ob-inline-function-expansion?view=msvc-170).

Earlier cloud source 35 data contained 207 cells: 197 medians met 90%, ten failed. Final source 39 is different; this Windows/mimalloc/OFF control does not establish that those cloud failures are fixed. Other SDKs, statistics ON, compilers, link profiles, applications and every future pair need their own validation. The original README chart remains historical and unchanged.

## Standard backend: throwing allocation control

A separate **2026-10-11** control uses GCC 13.3 Release on a local Xeon / WSL2 host, pinned to CPU 0. Both variants keep the same benchmark source, compiler flags, C++ runtime and direct throwing aligned `operator new` reference. Five process pairs measure 200,000 operations per round, nine rounds, alignment 16, statistics OFF and checks OFF.

The only allocation factor removes the private Global Standard `std::nothrow` wrapper and lets the matching throwing allocation propagate `std::bad_alloc`. Conditional `noexcept` preserves the mimalloc/jemalloc helper contracts. Zero handling, failed reallocation ownership, constructor rollback and exact Basic statistics remain required; Heap/Stack paths are unchanged.

| Bytes | Previous API median ns | Throwing API median ns | Previous paired ratio median | Throwing paired ratio median [min,max] |
| --- | ---: | ---: | ---: | --- |
| 16 | 13.1275 | 10.8337 | 77.11% | 102.53% [79.97,110.35] |
| 64 | 12.7759 | 10.7696 | 78.39% | 99.07% [89.62,108.61] |
| 256 | 12.8916 | 11.2464 | 85.00% | 99.41% [91.33,108.75] |
| 4096 | 22.4839 | 23.8641 | 85.50% | 98.02% [84.91,108.85] |
| 65536 | 24.0164 | 22.8747 | 86.39% | 97.90% [94.79,120.54] |

All five paired medians exceed 90%; some individual pairs remain below it. Ratios do not establish an absolute improvement at every size: the separately computed 4096-byte API time median increases in this sample. The strengthened Standard allocation/OOM suite passes all four prototype configurations (Release ON 673, Release OFF 671, Debug ON 676, Debug OFF 674 cases). This local host and two-build control is separate from the cloud delivery results; it cannot erase their failures or establish other backend/platform performance.

[All paired measurements](../results/header-only/linux-standard-throw/measurements.csv) · [Environment](../results/header-only/linux-standard-throw/environment.json)

## Raw measurements

| Profile | Complete 23-group summary | All paired path measurements |
| --- | --- | --- |
| Default | [summary](../results/header-only/windows-mimalloc-default/summary.csv) | [measurements](../results/header-only/windows-mimalloc-default/measurements.csv) |
| IPO | [summary](../results/header-only/windows-mimalloc-ipo/summary.csv) | [measurements](../results/header-only/windows-mimalloc-ipo/measurements.csv) |
| Ob3 | [summary](../results/header-only/windows-mimalloc-ob3/summary.csv) | [measurements](../results/header-only/windows-mimalloc-ob3/measurements.csv) |

[Build options](../getting-started.md#build-options) · [Lifetime](../compatibility.md)
