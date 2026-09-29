#!/usr/bin/env bash
# Approved test-only source; not a UniMemory allocation backend or dependency.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=${1:-"$root/build/rpmalloc-validation"}
mkdir -p "$work"
work=$(cd "$work" && pwd)
if [ ! -d "$work/source/.git" ]; then
  git clone --depth 1 --branch 1.4.5 https://github.com/mjansson/rpmalloc.git "$work/source"
fi
test "$(git -C "$work/source" rev-parse HEAD)" = e4393ff85585d91400bcbad2e7266c011075b673
src="$work/source"
gcc -std=c11 -D_GNU_SOURCE -O2 -DENABLE_ASSERTS=1 -DENABLE_STATISTICS=1 -DRPMALLOC_FIRST_CLASS_HEAPS=1 -DRPMALLOC_CONFIGURABLE=1 \
  -I"$src/rpmalloc" -I"$src/test" "$src/rpmalloc/rpmalloc.c" "$src/test/thread.c" "$src/test/main.c" -pthread -lm -o "$work/native-test"
# Match configure.py: the override library keeps its default diagnostic flags.
gcc -std=c11 -D_GNU_SOURCE -O2 -DENABLE_PRELOAD=1 -DENABLE_OVERRIDE=1 \
  -I"$src/rpmalloc" -c "$src/rpmalloc/rpmalloc.c" -o "$work/override.o"
gcc -std=c11 -D_GNU_SOURCE -O2 -I"$src/test" -c "$src/test/thread.c" -o "$work/thread.o"
g++ -std=c++17 -D_GNU_SOURCE -O2 -DENABLE_ASSERTS=1 -DENABLE_STATISTICS=1 -I"$src/rpmalloc" -I"$src/test" \
  "$src/test/main-override.cc" "$work/override.o" "$work/thread.o" -pthread -lm -o "$work/override-test"
python3 - "$work" <<'PY'
import json, os, pathlib, subprocess, sys
work = pathlib.Path(sys.argv[1])
cpus = sorted(os.sched_getaffinity(0))[:4]
for name in ('native-test', 'override-test'):
    with (work / (name + '.log')).open('w') as log:
        subprocess.run([str(work / name)], stdout=log, stderr=subprocess.STDOUT,
                       preexec_fn=lambda: os.sched_setaffinity(0, cpus),
                       check=True, timeout=600)
    print((work / (name + '.log')).read_text())
manifest = {'revision': 'e4393ff85585d91400bcbad2e7266c011075b673',
    'affinity': cpus, 'native_assertions': True, 'native_statistics': True,
    'first_class_heaps': True, 'override_library_flags': ['ENABLE_PRELOAD=1', 'ENABLE_OVERRIDE=1']}
with (work / 'environment.json').open('w', encoding='utf-8', newline='\n') as file:
    file.write(json.dumps(manifest, indent=2) + '\n')
PY
