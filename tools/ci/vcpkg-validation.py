"""Validate the pinned release port and installed consumers without allocator SDKs."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vcpkg-root", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--triplet", required=True)
    parser.add_argument("--statistics", required=True, choices=("ON", "OFF"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    vcpkg_root = args.vcpkg_root.resolve()
    executable = vcpkg_root / ("vcpkg.exe" if os.name == "nt" else "vcpkg")
    installed = work / "installed"
    results = []

    def run(label, command):
        print(f"::group::{label}", flush=True)
        result = subprocess.run([str(item) for item in command], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                encoding="utf-8", errors="replace")
        (work / f"{label}.log").write_text(result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        print("::endgroup::", flush=True)
        if result.returncode:
            raise RuntimeError(f"{label} failed with exit code {result.returncode}")
        results.append(label)

    port = "unimemory[statistics]" if args.statistics == "ON" else "unimemory"
    run("install", [executable, "install", f"{port}:{args.triplet}", "--classic", f"--overlay-ports={root / 'ports'}",
                    f"--x-install-root={installed}", f"--x-buildtrees-root={work / 'buildtrees'}",
                    f"--x-packages-root={work / 'packages'}", "--no-binarycaching", "--enforce-port-checks"])
    prefix = installed / args.triplet
    config = prefix / "share" / "unimemory"
    if not (config / "copyright").is_file() or not (config / "UniMemoryConfig.cmake").is_file():
        raise RuntimeError("Missing installed license or CMake package")
    profile = (config / "UniMemoryConfig.cmake").read_text(encoding="utf-8")
    if f'set(UniMemory_STATISTICS "{args.statistics}")' not in profile or 'set(UniMemory_BACKEND "standard")' not in profile:
        raise RuntimeError("Installed profile differs from requested port features")

    relocated = work / "relocated"
    shutil.copytree(prefix / "include", relocated / "include")
    shutil.copytree(config, relocated / "share" / "unimemory")
    generator = ["-G", "Visual Studio 17 2022", "-A", "x64"] if os.name == "nt" else ["-G", "Ninja"]
    runtime = []
    if os.name == "nt":
        runtime = ["-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>" + ("" if args.triplet.endswith("-static") else "DLL")]
    for mode in ("Debug", "Release"):
        for name, source, options in (
            ("consumer", root / "tests" / "consumer", [f"-DCMAKE_TOOLCHAIN_FILE={vcpkg_root / 'scripts/buildsystems/vcpkg.cmake'}",
                                                       f"-DVCPKG_INSTALLED_DIR={installed}", f"-DVCPKG_TARGET_TRIPLET={args.triplet}",
                                                       "-DVCPKG_MANIFEST_MODE=OFF"]),
            ("relocated", root / "tests" / "vcpkg-consumer", [f"-DUniMemory_DIR={relocated / 'share/unimemory'}"]),
        ):
            build = work / f"{name}-{mode}"
            run(f"configure-{name}-{mode}", ["cmake", "-S", source, "-B", build, *generator, f"-DCMAKE_BUILD_TYPE={mode}", *runtime, *options])
            run(f"build-{name}-{mode}", ["cmake", "--build", build, "--config", mode, "--parallel", "3"])
            run(f"test-{name}-{mode}", ["ctest", "--test-dir", build, "-C", mode, "--output-on-failure", "--no-tests=error"])
    summary = {"triplet": args.triplet, "statistics": args.statistics, "backend": "standard", "passed": True, "steps": results}
    (work / "result.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        print(f"vcpkg validation failed: {error}", file=sys.stderr)
        sys.exit(1)
