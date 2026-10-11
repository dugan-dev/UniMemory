# Native application results

Run date: 2026-10-11 · [workflow run](https://github.com/dugan-dev/UniMemory/actions/runs/38105585240) · UniMemory workflow source `81cfacd`.

[results.csv](results.csv) retains all 144 successful process runs: 16 programs, three native allocators and three trials. [environment.json](environment.json) records commands and the pinned mimalloc-bench revision.

These programs use native malloc/new through LD_PRELOAD; they do not measure the UniMemory API. Elapsed seconds include startup. Peak RSS is recorded in KiB. Larson and xmalloc-test run for a configured duration; elapsed time is not a speed ranking.
