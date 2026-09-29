# Runtime options

[Index](../README.md) · **English** · [简体中文](runtime-options.zh-CN.md)

Examples: [Quick Start](../../README.md#runtime-options).

`UnusedPageReleaseDelayMs` adjusts how long unused memory may wait before returning to the OS. Allocators may cache freed storage for reuse. Check `supports()` first.

| Value | Request |
| --- | --- |
| Positive integer | Delay, in milliseconds |
| `0` | Reclaim idle pages promptly |
| `-1` | Disable timed reclamation |
| Below `-1` or outside the supported integer range | `std::invalid_argument` |

Set options before creating workers and independent Heaps. They affect backend-wide policy/defaults, not one Memory. jemalloc applies the delay to defaults for newly created arenas; existing regions may be unaffected.

With a valid value, an unsupported capability returns `false`; a backend rejection throws `std::runtime_error`. This never frees live objects or guarantees a deadline for reducing RSS. More frequent reclamation may reduce retained memory and increase allocation time; measure your workload.
