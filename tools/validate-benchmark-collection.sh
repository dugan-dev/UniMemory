#!/usr/bin/env bash
# Linux-only, test builds. These override libraries are never linked into UniMemory.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=${1:?Pass the native-validation work directory}
work=$(cd "$work" && pwd)
if [ ! -d "$work/benchmark-collection/.git" ]; then
  git clone https://github.com/daanx/mimalloc-bench.git "$work/benchmark-collection"
fi
git -C "$work/benchmark-collection" checkout ce2df0bcf27ddcc0a690ae777788d1dfcb5fae86
python3 - "$work/benchmark-collection/bench/CMakeLists.txt" <<'PY'
import pathlib, sys
path = pathlib.Path(sys.argv[1])
text = path.read_text().replace('if(NOT APPLE)', 'if(FALSE)')
text = text.replace('add_subdirectory(security)', '# Security probes intentionally execute undefined behavior.')
path.write_text(text)
PY
cmake -S "$work/benchmark-collection/bench" -B "$work/collection-build" -DCMAKE_BUILD_TYPE=Release > "$work/collection-config.log"
cmake --build "$work/collection-build" --parallel 8 > "$work/collection-build.log" 2>&1
cmake -S "$work/mimalloc" -B "$work/mi-override" -DCMAKE_BUILD_TYPE=Release -DMI_OVERRIDE=ON -DMI_BUILD_TESTS=OFF > "$work/mi-override-config.log"
cmake --build "$work/mi-override" --parallel 8 > "$work/mi-override-build.log"
mkdir -p "$work/je-override"
(
  cd "$work/je-override"
  "$work/jemalloc-5.3.1/configure" > "$work/je-override-config.log"
  make -j8 > "$work/je-override-build.log"
)
python3 "$root/tools/run-upstream-benchmarks.py" "$work/collection-build" "$work/benchmark-collection/bench" "$work/collection-results" \
  --mimalloc "$work/mi-override/libmimalloc.so" --jemalloc "$work/je-override/lib/libjemalloc.so"
