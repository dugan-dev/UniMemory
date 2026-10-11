# Documentation

[UniMemory](../README.md) · **English** · [简体中文](README.zh-CN.md)

The [Quick Start](../README.md#quick-start) shows common APIs. Choose a topic below for setup, usage and requirements.

```mermaid
flowchart LR
    A[Add to a project] --> B[Allocation and objects]
    B --> C[Containers and ownership]
    C --> D[Heap / Stack / Statistics]
```

## 1 · Get started

| Topic | Content |
| --- | --- |
| [Build and install](getting-started.md) | Header-only CMake integration and deployment |
| [Configuration migration](migration.md) | Fixed backend/statistics/check policy |
| [Platforms and backends](guides/backends.md) | Platform support and optional backends |
| [Runnable examples](../examples/README.md) | Complete programs |

## 2 · Everyday use

| Topic | Content |
| --- | --- |
| [Raw memory](guides/raw-memory.md) | Allocate, free, align and resize |
| [Objects and arrays](guides/objects.md) | Construction, destruction and smart pointers |
| [Blocks](guides/raw-memory.md#resize-and-ownership) | Automatic release and resizing |
| [Containers](guides/containers.md) | Allocator, PMR and nested containers |

## 3 · Memory management

| Topic | Content |
| --- | --- |
| [Heap](guides/heap.md) | Independent groups and bulk release |
| [Stack](guides/stack.md) | Fixed buffers, marks and rewind |
| [Statistics](guides/statistics.md) | Usage, peaks and query scopes |
| [Runtime options](guides/runtime-options.md) | Unused-memory release delay |

## 4 · Reference

| Topic | Content |
| --- | --- |
| [API](api-reference.md) | Functions, parameters and return values |
| [Lifetime and threads](compatibility.md) | Ownership, concurrency and shared libraries |
| [Performance](performance.md) | Time and memory comparisons |
| [Test results](testing.md) | Verified platforms and coverage |
| [Remote validation](remote-validation.md) | Native compiler matrix, detection and chart automation |

[Backend capabilities](allocator-capabilities.md) · [Measurement method](benchmarking.md) · [Raw data](results/0.0.1/README.md) · [Upstream validation](upstream-validation.md)

## 5 · Validation and maintenance

Historical repair verification from **2026-10-08 to 2026-10-09**; these earlier library-build counts do not describe the current header-only profiles.

| Validation | Recorded result |
| --- | --- |
| Windows: three backends + examples | **1666/1666** in Release and Debug |
| Linux: three backends + examples | **1667/1667**, installed consumers **7/7** |
| macOS: three backends + examples | [GitHub CI passed](https://github.com/dugan-dev/UniMemory/actions/runs/37871859097) |
| ASan / UBSan, Standard and Stack | **715/715**, leak detection enabled |
| ThreadSanitizer, Standard and Stack | [Release validation passed](https://github.com/dugan-dev/UniMemory/actions/runs/37871859066) |

Shared builds passed **1664/1664** on Windows and **1666/1666** on Linux. All six local configurations passed **7/7** installed consumers. Coverage includes everyday use, typed storage, independent public headers, package discovery, boundaries, exceptions and concurrency. Each CI link verifies its recorded revision. [Full test report](testing.md)

The header-only fixed-profile source described here is on `dev`; the earlier stable implementation is on `main`.
The [dev source ZIP](https://github.com/dugan-dev/UniMemory/archive/refs/heads/dev.zip)
is distributed without tags or Releases. The library version remains **0.0.1**;
use matching headers, generated configuration and SDK libraries and identify deliveries by source revision.
[Current performance reports](performance.md#current-remote-reports) record their
measured revision; September measurements remain as historical data.

[Contributing](../CONTRIBUTING.md) · [Security policy](../SECURITY.md) · [Repair review](review-2026-10-08.md)

Before publishing documentation, run:

```sh
python tools/check-docs.py
```
