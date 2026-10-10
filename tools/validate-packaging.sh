#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=${1:?Pass the native-validation work directory}
work=$(cd "$work" && pwd)
cmake -S "$work/mimalloc" -B "$work/mi-static" -DCMAKE_BUILD_TYPE=Release -DMI_OVERRIDE=OFF -DMI_BUILD_SHARED=OFF -DMI_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX="$work/static-prefix" > "$work/static-config.log"
cmake --build "$work/mi-static" --parallel 8 > "$work/static-build.log"
cmake --install "$work/mi-static" > "$work/static-install.log"
# UniMemory is INTERFACE in every profile. Shared/static below refer only to SDKs.
for profile in static-mimalloc shared-mimalloc shared-jemalloc; do
  backend=${profile#*-}
  prefix="$work/prefix"
  if [ "$profile" = static-mimalloc ]; then prefix="$work/static-prefix"; fi
  for statistics in OFF ON; do
    directory="$work/$profile-$statistics"
    installed="$work/$profile-$statistics-installed"
    consumer="$work/$profile-$statistics-consumer"
    cmake -S "$root" -B "$directory" -DCMAKE_BUILD_TYPE=Release -DUNIMEMORY_BACKEND="$backend" -DUNIMEMORY_STATISTICS="$statistics" -DUNIMEMORY_CHECKS=AUTO -DCMAKE_PREFIX_PATH="$prefix" > "$work/$profile-$statistics-config.log"
    cmake --build "$directory" --parallel 4 > "$work/$profile-$statistics-build.log"
    ctest --test-dir "$directory" --parallel 4 --output-on-failure --no-tests=error > "$work/$profile-$statistics-test.log"
    cmake --install "$directory" --prefix "$installed" > "$work/$profile-$statistics-install.log"
    cmake -S "$root/tests/consumer" -B "$consumer" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$installed;$prefix" -DUNIMEMORY_BACKEND="$backend" -DUNIMEMORY_STATISTICS="$statistics" -DUNIMEMORY_CHECKS=AUTO > "$work/$profile-$statistics-consumer-config.log"
    cmake --build "$consumer" --parallel 4 > "$work/$profile-$statistics-consumer-build.log"
    ctest --test-dir "$consumer" --output-on-failure --no-tests=error > "$work/$profile-$statistics-consumer-test.log"
    tail -8 "$work/$profile-$statistics-test.log"
    tail -6 "$work/$profile-$statistics-consumer-test.log"
  done
 done
