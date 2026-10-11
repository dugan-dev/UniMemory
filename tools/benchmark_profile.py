"""Read the actual benchmark's selected profile and native SDK binary evidence."""
import hashlib
import os
from pathlib import Path

BACKENDS = ("standard", "mimalloc", "jemalloc")


def read_profile(executable):
    executable = Path(executable).resolve()
    candidates = [directory for directory in (executable.parent, executable.parent.parent)
                  if (directory / "CMakeCache.txt").is_file()]
    if len(candidates) != 1:
        raise ValueError("Missing or ambiguous benchmark CMakeCache.txt")
    build = candidates[0]
    cache = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line and ":" in line.split("=", 1)[0]:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    backend = cache.get("UNIMEMORY_BACKEND")
    statistics = cache.get("UNIMEMORY_STATISTICS")
    checks = cache.get("UNIMEMORY_CHECKS")
    if backend not in BACKENDS or statistics not in ("ON", "OFF") or checks not in ("AUTO", "ON", "OFF"):
        raise ValueError("Benchmark is missing its explicit backend/statistics/checks profile")
    if cache.get("CMAKE_CONFIGURATION_TYPES"):
        configuration = executable.parent.name
        if configuration not in cache["CMAKE_CONFIGURATION_TYPES"].split(";"):
            raise ValueError("Cannot identify the actual multi-configuration executable")
    else:
        configuration = cache.get("CMAKE_BUILD_TYPE")
    if not configuration:
        raise ValueError("Missing actual benchmark configuration")
    parameters = build / f"benchmark-build-{configuration}.txt"
    if not parameters.is_file():
        raise ValueError("Missing generated benchmark SDK/target provenance")
    record = dict(line.split("=", 1) for line in parameters.read_text(encoding="utf-8").splitlines() if "=" in line)
    for key, expected in (("core", "header-only"), ("backend", backend), ("statistics", statistics), ("checks", checks)):
        if record.get(key) != expected:
            raise ValueError(f"Generated benchmark {key} disagrees with its CMake cache")
    record["target_parameters"] = "UniMemoryDeliveryBenchmark"
    if executable.stem == "UniMemoryDiagnostic":
        diagnostic = build / f"diagnostic-build-{configuration}.txt"
        if not diagnostic.is_file():
            raise ValueError("Missing actual diagnostic target flags")
        actual = dict(line.split("=", 1) for line in diagnostic.read_text(encoding="utf-8").splitlines() if "=" in line)
        record.update(actual)
        record.pop("compile_options", None)
        record.pop("compile_definitions", None)
        record["target_parameters"] = "UniMemoryDiagnostic; SDK paths from the same build's benchmark record"
    binaries = {}
    for key in ("sdk_binary", "sdk_linker_file"):
        if record.get(key):
            path = Path(record[key])
            if not path.is_file():
                raise ValueError(f"Missing actual SDK {key}: {path}")
            binaries[key] = {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    if backend != "standard" and not binaries:
        raise ValueError("SDK provenance must contain actual binaries, not just headers")
    # DLL copies are the files the Windows benchmark can actually load.
    dlls = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in executable.parent.glob("*.dll")}
    runtime_files = {}
    if os.name == "nt":
        for item in binaries.values():
            path = Path(item["path"])
            directory = path.parent if path.suffix.lower() == ".dll" else path.parent.parent / "bin"
            for runtime in directory.glob("*.dll"):
                runtime_files[str(runtime)] = hashlib.sha256(runtime.read_bytes()).hexdigest()
    return {"backend": backend, "statistics": statistics, "checks": checks,
            "api_path": "basic" if statistics == "ON" else "disabled",
            "core": "header-only", "configuration": configuration,
            "build_parameters": record, "sdk_binaries": binaries, "runtime_dlls": dlls,
            "sdk_runtime_files": runtime_files,
            "cmake_cache_sha256": hashlib.sha256((build / "CMakeCache.txt").read_bytes()).hexdigest(),
            "native_api_same_executable": True, "native_api_same_sdk_target": True}


def environment_for(profile):
    environment = os.environ.copy()
    environment["MIMALLOC_DISABLE_REDIRECT"] = "1"
    directories = sorted({str(Path(path).parent) for path in profile["sdk_runtime_files"]})
    if directories:
        environment["PATH"] = os.pathsep.join([*directories, environment.get("PATH", "")])
    return environment
