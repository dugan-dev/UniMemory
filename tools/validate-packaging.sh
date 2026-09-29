#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=${1:?Pass the native-validation work directory}
work=$(cd "$work" && pwd)
cmake -S "$work/mimalloc" -B "$work/mi-static" -DCMAKE_BUILD_TYPE=Release -DMI_OVERRIDE=OFF -DMI_BUILD_SHARED=OFF -DMI_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX="$work/static-prefix" > "$work/static-config.log"
cmake --build "$work/mi-static" --parallel 8 > "$work/static-build.log"
cmake --install "$work/mi-static" > "$work/static-install.log"
cmake -S "$root" -B "$work/static-unified" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$work/static-prefix" -DUNIMEMORY_WITH_MIMALLOC=ON > "$work/static-unified-config.log"
cmake --build "$work/static-unified" --parallel 8 > "$work/static-unified-build.log"
ctest --test-dir "$work/static-unified" --parallel 8 --output-on-failure > "$work/static-unified-test.log"
cmake --install "$work/static-unified" --prefix "$work/static-installed" > "$work/static-package.log"
cmake -S "$root/tests/consumer" -B "$work/static-consumer" -DCMAKE_PREFIX_PATH="$work/static-installed;$work/static-prefix" > "$work/static-consumer-config.log"
cmake --build "$work/static-consumer" --parallel 4 > "$work/static-consumer-build.log"
ctest --test-dir "$work/static-consumer" --output-on-failure > "$work/static-consumer-test.log"
cmake -S "$root" -B "$work/shared" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DUNIMEMORY_WITH_MIMALLOC=ON -DUNIMEMORY_WITH_JEMALLOC=ON -DCMAKE_PREFIX_PATH="$work/prefix" > "$work/shared-config.log"
cmake --build "$work/shared" --parallel 8 > "$work/shared-build.log"
ctest --test-dir "$work/shared" --parallel 8 --output-on-failure > "$work/shared-test.log"
cmake --install "$work/shared" --prefix "$work/shared-installed" > "$work/shared-install.log"
# Deploy optional native runtime libraries in the same private prefix.
cmake -E copy_directory "$work/prefix" "$work/shared-installed"
cmake -S "$root/tests/consumer" -B "$work/shared-consumer" -DCMAKE_PREFIX_PATH="$work/shared-installed;$work/prefix" > "$work/shared-consumer-config.log"
cmake --build "$work/shared-consumer" --parallel 4 > "$work/shared-consumer-build.log"
ctest --test-dir "$work/shared-consumer" --output-on-failure > "$work/shared-consumer-test.log"
tail -8 "$work/static-unified-test.log"
tail -8 "$work/shared-test.log"
tail -6 "$work/static-consumer-test.log"
tail -6 "$work/shared-consumer-test.log"
