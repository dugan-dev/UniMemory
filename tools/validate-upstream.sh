#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=${1:-"$root/build/native-validation"}
mkdir -p "$work"
work=$(cd "$work" && pwd)
if [ ! -d "$work/mimalloc/.git" ]; then
  git clone --depth 1 --branch v3.4.3 https://github.com/microsoft/mimalloc.git "$work/mimalloc"
fi
test "$(git -C "$work/mimalloc" rev-parse HEAD)" = 152fbf2634aeafca3774df791b0a77683035f076
cmake -S "$work/mimalloc" -B "$work/mi-build" -DCMAKE_BUILD_TYPE=Release -DMI_OVERRIDE=OFF -DCMAKE_INSTALL_PREFIX="$work/prefix" > "$work/mi-config.log"
cmake --build "$work/mi-build" --parallel 8 > "$work/mi-build.log"
ctest --test-dir "$work/mi-build" --output-on-failure > "$work/mi-test.log"
cmake --install "$work/mi-build" > "$work/mi-install.log"
if [ ! -d "$work/jemalloc-5.3.1" ]; then
  curl -L --fail https://github.com/jemalloc/jemalloc/releases/download/5.3.1/jemalloc-5.3.1.tar.bz2 -o "$work/jemalloc.tar.bz2"
  python3 - "$work" <<'PY'
import pathlib, sys, tarfile
root = pathlib.Path(sys.argv[1])
with tarfile.open(root / 'jemalloc.tar.bz2') as archive:
    archive.extractall(root, filter='data')
PY
fi
(
  cd "$work/jemalloc-5.3.1"
  ./configure --with-jemalloc-prefix=je_ --disable-cxx --enable-prof --prefix="$work/prefix" > "$work/je-config.log"
  make -j8 > "$work/je-build.log"
  make check -j4 > "$work/je-check.log" 2>&1
  make stress > "$work/je-stress.log" 2>&1
  make analyze > "$work/je-analyze.log" 2>&1
  make install > "$work/je-install.log"
)
cmake -S "$root" -B "$work/unified" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$work/prefix" -DUNIMEMORY_WITH_MIMALLOC=ON -DUNIMEMORY_WITH_JEMALLOC=ON -DUNIMEMORY_BUILD_BENCHMARKS=ON > "$work/unified-config.log"
cmake --build "$work/unified" --parallel 8 > "$work/unified-build.log"
ctest --test-dir "$work/unified" --parallel 8 --output-on-failure > "$work/unified-tests.log"
tail -10 "$work/unified-tests.log"
