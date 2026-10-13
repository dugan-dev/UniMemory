# Native application results

Run date: 2026-10-13T18:03:01.634690+00:00 · [workflow run](https://github.com/dugan-dev/UniMemory/actions/runs/38415666741) · UniMemory workflow source `c917e81`.

[results.csv.gz](results.csv.gz) retains all 480 successful process runs: 16 programs, three native allocators and 10 trials. [environment.json](environment.json) records commands and the pinned mimalloc-bench revision.

These programs use native malloc/new through LD_PRELOAD; they do not measure the UniMemory API. Elapsed seconds include startup. Peak RSS is recorded in KiB. Larson and xmalloc-test run for a configured duration; elapsed time is not a speed ranking.
