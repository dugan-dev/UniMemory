#!/usr/bin/env bash
# Requires Bazel 8.4.2; native dependencies come from the pinned upstream MODULE.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=${1:?Pass a persistent validation work directory}
mkdir -p "$work"
work=$(cd "$work" && pwd)
cmake -S "$root" -B "$work/library" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF > "$work/library-config.log"
cmake --build "$work/library" --parallel 4 > "$work/library-build.log"
if [ ! -d "$work/source/.git" ]; then
  git clone --filter=blob:none https://github.com/google/tcmalloc.git "$work/source"
fi
git -C "$work/source" checkout 1c6a831d649134efac38663f5a269a43f0d02702
mkdir -p "$work/source/unimemory_check/include"
cp "$work/library/libUniMemory.a" "$work/source/unimemory_check/"
cp -R "$root/include/unimem" "$work/source/unimemory_check/include/"
cp "$root/tests/tcmalloc_linux/BUILD.bazel" "$root/tests/tcmalloc_linux/smoke.cpp" \
  "$root/tests/production_tests.cpp" "$root/tests/conformance_tests.cpp" \
  "$root/tests/memory_tests.cpp" "$root/tests/unified_memory_tests.cpp" \
  "$root/tests/interface_usage_tests.cpp" \
  "$root/benchmarks/delivery_bench.cpp" "$work/source/unimemory_check/"
cd "$work/source"
flags=(-c opt)
if [ "${UNIMEMORY_VALIDATION_PORTABLE_CLOCK:-0}" = 1 ]; then
  # Abseil supports this fallback when a VM's hardware clock is inconsistent.
  flags+=(--copt=-DABSL_USE_UNSCALED_CYCLECLOCK=0)
fi
if ! grep -q 'single_version_override(module_name = "protobuf"' MODULE.bazel; then
  printf '\nsingle_version_override(module_name = "protobuf", version = "34.0")\nsingle_version_override(module_name = "abseil-cpp", version = "20260526.0")\n' >> MODULE.bazel
fi
test_result=0
bazel test "${flags[@]}" //tcmalloc/... //unimemory_check:all --build_tests_only --jobs="${UNIMEMORY_VALIDATION_JOBS:-8}" \
  --local_ram_resources=HOST_RAM*.65 --test_timeout=600 --test_output=errors > "$work/tests.log" 2>&1 || test_result=$?
bazel run "${flags[@]}" //unimemory_check:smoke > "$work/smoke.log" 2>&1
bazel run "${flags[@]}" //tcmalloc/testing:tcmalloc_benchmark -- --benchmark_min_time=0.01s --benchmark_repetitions=3 \
  --benchmark_out="$work/benchmark.json" --benchmark_out_format=json > "$work/benchmark.log" 2>&1
bazel build "${flags[@]}" //unimemory_check:delivery_benchmark
binary_dir=$(bazel info "${flags[@]}" bazel-bin)
python3 "$root/tools/run-benchmarks.py" "$binary_dir/unimemory_check/delivery_benchmark" "$work/performance" --backends standard
python3 - "$work/performance/environment.json" "${UNIMEMORY_VALIDATION_PORTABLE_CLOCK:-0}" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
data = json.loads(path.read_text())
data.update(tcmalloc_revision='1c6a831d649134efac38663f5a269a43f0d02702',
            abseil_version='20260526.0', protobuf_version='34.0',
            portable_cycle_clock=sys.argv[2] == '1', bazel_compilation_mode='opt')
with path.open('w', encoding='utf-8', newline='\n') as file:
    file.write(json.dumps(data, indent=2) + '\n')
PY
tail -15 "$work/tests.log"
exit "$test_result"
