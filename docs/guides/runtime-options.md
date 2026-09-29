# Runtime options

[Index](../README.md) · **English** · [简体中文](runtime-options.zh-CN.md)

The current option controls the backend's advisory unused-page release delay. It does not destroy live objects or impose an RSS deadline.

```cpp
using namespace unimem;
Backend backend = Backend::Mimalloc;
if (supports(backend, RuntimeOption::UnusedPageReleaseDelayMs)) {
    set_runtime_option(backend, RuntimeOption::UnusedPageReleaseDelayMs, 20);
}
```

| Value | Request |
| --- | --- |
| Positive integer | Delay, in milliseconds |
| `0` | Reclaim idle pages promptly |
| `-1` | Disable timed reclamation |
| Below `-1` or outside the supported integer range | `std::invalid_argument` |

Set options before creating workers and independent Heaps. They affect backend-wide policy/defaults, not one Memory. jemalloc applies the delay to defaults for newly created arenas; existing regions may be unaffected.

Unsupported capability returns `false`; a backend rejection throws `std::runtime_error`. Tuning trades throughput, retention and latency. Measure your workload before changing defaults.
