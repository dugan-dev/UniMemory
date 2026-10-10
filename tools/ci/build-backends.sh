#!/usr/bin/env bash
# Fixed existing dependencies. No global malloc/new replacement.
set -euo pipefail
prefix=$(pwd)/build/prefix
mkdir -p build
git clone --depth 1 --branch v3.4.3 https://github.com/microsoft/mimalloc.git build/mimalloc
test "$(git -C build/mimalloc rev-parse HEAD)" = 152fbf2634aeafca3774df791b0a77683035f076
cmake -S build/mimalloc -B build/mi -DCMAKE_BUILD_TYPE=Release -DMI_OVERRIDE=OFF -DMI_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build build/mi --parallel 4
cmake --install build/mi
curl --fail --location https://github.com/jemalloc/jemalloc/releases/download/5.3.1/jemalloc-5.3.1.tar.bz2 -o build/jemalloc.tar.bz2
tar -xf build/jemalloc.tar.bz2 -C build
(
    cd build/jemalloc-5.3.1
    ./configure --with-jemalloc-prefix=je_ --disable-cxx --prefix="$prefix"
    make -j4
    make install
)
