"""Preliminary same-host allocator inline experiment; never changes the checkout."""
import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[2]
HEADER = "include/unimem/detail/adapters.inl"
ORIGINAL = "UNIMEMORY_NOINLINE inline T* Allocator<T>::allocate(std::size_t count)"
VARIANTS = {"noinline": ORIGINAL,
            "inline": "inline T* Allocator<T>::allocate(std::size_t count)",
            "force_inline": "UNIMEMORY_FORCE_INLINE T* Allocator<T>::allocate(std::size_t count)"}
SCENARIOS = {"raw:" + str(size) for size in (16, 64, 256, 4096, 65536)} | {
    "zeroed", "reallocate", "reallocate_zeroed", "unique", "array", "shared", "vector", "pmr_vector", "owned"}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(command, log, cwd=ROOT):
    with log.open("w", encoding="utf-8") as stream:
        return subprocess.run([str(value) for value in command], cwd=cwd, stdout=stream,
                              stderr=subprocess.STDOUT, timeout=1800).returncode


def source_files(revision):
    archive = subprocess.check_output(["git", "archive", "--format=tar", revision], cwd=ROOT)
    files = {}
    with tarfile.open(fileobj=io.BytesIO(archive)) as package:
        for entry in package:
            if entry.isdir():
                continue
            if not entry.isfile() or Path(entry.name).is_absolute() or ".." in Path(entry.name).parts:
                raise ValueError("Source archive contains an unsupported entry")
            files[entry.name] = package.extractfile(entry).read()
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-ref", default="fe90b68")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    revision = subprocess.check_output(["git", "rev-parse", "--verify", args.source_ref + "^{commit}"], cwd=ROOT, text=True).strip()
    files = source_files(revision)
    original = files[HEADER].decode("utf-8")
    if original.count(ORIGINAL) != 1:
        raise ValueError("Expected exactly one original allocator NOINLINE boundary")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError("Use a fresh output directory; experiment evidence is never overwritten")
    checkout_headers = {str(path.relative_to(ROOT)): digest(path.read_bytes()) for path in (ROOT / "include").rglob("*") if path.is_file()}
    manifest = dict(schema=1, preliminary=True, correctness_acceptance=False, source_revision=revision,
                    workflow_revision=os.environ.get("GITHUB_SHA"), run_id=os.environ.get("GITHUB_RUN_ID"),
                    platform=platform.platform(), source_hashes={name: digest(data) for name, data in files.items()},
                    minimum_ratio=.90, trials=10, target_seconds=.25,
                    policy="All 14 unchanged Native/API scenarios; paired median and arithmetic mean gates; every failure retained",
                    profiles=[], completed=False)
    combined = []
    errors = []
    sdk_hashes = {}

    def save():
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        if combined:
            with (output / "summary.csv").open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=list(combined[0]))
                writer.writeheader()
                writer.writerows(combined)
        lines = ["# Preliminary allocator inline experiment", "",
                 "Same host and SDK per backend. This experiment does not replace correctness acceptance.", "",
                 "| Backend | Variant | Case | Paired median % | Mean % | >=90% both | API median vs baseline % |",
                 "| --- | --- | --- | ---: | ---: | --- | ---: |"]
        for row in combined:
            relative = row.get("api_median_performance_vs_noinline_percent", "")
            relative = f"{relative:.2f}" if isinstance(relative, float) else "pending"
            lines.append(f"| {row['backend']} | {row['variant']} | {row['factor']} | {float(row['performance_percent']):.2f} | {float(row['mean_performance_percent']):.2f} | {row['passed']} | {relative} |")
        lines += ["", "Infrastructure/profile errors: " + ("; ".join(errors) or "none"), "",
                  "All scenario failures are retained; no candidate is selected automatically."]
        (output / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    save()
    for variant, replacement in VARIANTS.items():
        source = output / "sources" / variant
        for name, data in files.items():
            destination = source / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
        changed = original.replace(ORIGINAL, replacement)
        (source / HEADER).write_bytes(changed.encode("utf-8"))
        expected_hashes = {name: digest(changed.encode("utf-8") if name == HEADER else data) for name, data in files.items()}
        for backend in ("standard", "mimalloc", "jemalloc"):
            build = output / "profiles" / (backend + "-" + variant)
            build.mkdir(parents=True)
            record = dict(backend=backend, variant=variant, header_sha256=digest(changed.encode("utf-8")),
                          modified_source_files=[] if variant == "noinline" else [HEADER], completed=False)
            manifest["profiles"].append(record)
            command = ["cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
                       "-DUNIMEMORY_BACKEND=" + backend, "-DUNIMEMORY_STATISTICS=OFF", "-DUNIMEMORY_CHECKS=AUTO",
                       "-DUNIMEMORY_IPO=ON", "-DUNIMEMORY_BUILD_BENCHMARKS=ON", "-DUNIMEMORY_PERFORMANCE_TESTS=ON",
                       "-DUNIMEMORY_PERFORMANCE_MIN_RATIO=0.90", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                       "-DCMAKE_PREFIX_PATH=" + (ROOT / "build/prefix").as_posix()]
            if os.name == "nt":
                command += ["-DCMAKE_TOOLCHAIN_FILE=" + (ROOT / "build/vcpkg/scripts/buildsystems/vcpkg.cmake").as_posix(),
                            "-DVCPKG_TARGET_TRIPLET=x64-windows-unimemory", "-DVCPKG_OVERLAY_TRIPLETS=" + (ROOT / "cmake/triplets").as_posix(),
                            "-DVCPKG_MANIFEST_MODE=OFF", "-DVCPKG_MANIFEST_INSTALL=OFF",
                            "-DVCPKG_INSTALLED_DIR=" + (ROOT / "build/sdk-installed").as_posix(),
                            "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL"]
            try:
                if run([sys.executable, ROOT / "tools/ci/configure.py", "--", *command], build / "configure.log"):
                    raise ValueError("configure failed")
                if run(["cmake", "--build", build, "--config", "Release", "--target", "UniMemoryDiagnostic", "--parallel", "4"], build / "build.log"):
                    raise ValueError("build failed")
                executable = build / ("Release/UniMemoryDiagnostic.exe" if os.name == "nt" else "UniMemoryDiagnostic")
                acceptance = build / "acceptance"
                status = run([sys.executable, source / "tools/ci/performance-acceptance.py", executable, acceptance,
                              "--trials", "10", "--seconds", ".25", "--minimum", ".90"], build / "acceptance.log")
                environment = json.loads((acceptance / "environment.json").read_text(encoding="utf-8"))
                if environment.get("completed") is not True:
                    raise ValueError("incomplete measurement")
                profile = environment["profile"]
                if profile["build_parameters"].get("ipo") != "ON":
                    raise ValueError("IPO was not enabled")
                binaries = {key: value["sha256"] for key, value in profile["sdk_binaries"].items()}
                if backend in sdk_hashes and binaries != sdk_hashes[backend]:
                    raise ValueError("SDK changed between variants")
                sdk_hashes[backend] = binaries
                with (acceptance / "summary.csv").open(newline="", encoding="utf-8") as stream:
                    rows = list(csv.DictReader(stream))
                if len(rows) != 14 or {row["factor"] for row in rows} != SCENARIOS:
                    raise ValueError("missing or duplicate scenario")
                if any(int(row["trials"]) != 10 or
                       (row["passed"] == "True") != (float(row["performance_percent"]) >= 90 and
                                                       float(row["mean_performance_percent"]) >= 90)
                       for row in rows):
                    raise ValueError("summary trial count or dual >=90% gate disagrees")
                if environment["passed"] != all(row["passed"] == "True" for row in rows):
                    raise ValueError("profile status disagrees with all scenario gates")
                if status not in (0, 1) or (status == 0) != environment["passed"]:
                    raise ValueError("acceptance status disagrees with retained evidence")
                if any(digest((source / name).read_bytes()) != expected for name, expected in expected_hashes.items()):
                    raise ValueError("experiment source changed during build/measurement")
                combined.extend(dict(row, backend=backend, variant=variant) for row in rows)
                record.update(completed=True, passed=environment["passed"], acceptance_exit=status,
                              executable_sha256=environment["executable_sha256"], sdk_binaries=binaries)
            except (ValueError, OSError, subprocess.SubprocessError, KeyError) as error:
                message = f"{backend}/{variant}: {error}"
                errors.append(message)
                record["error"] = str(error)
            save()
            print(f"{backend}/{variant}: completed={record['completed']} passed={record.get('passed')}", flush=True)
    current = {str(path.relative_to(ROOT)): digest(path.read_bytes()) for path in (ROOT / "include").rglob("*") if path.is_file()}
    if current != checkout_headers:
        errors.append("production checkout headers changed")
    baselines = {(row["backend"], row["factor"]): row for row in combined if row["variant"] == "noinline"}
    for row in combined:
        baseline = baselines.get((row["backend"], row["factor"]))
        row["api_median_performance_vs_noinline_percent"] = (
            100 * float(baseline["changed_median"]) / float(row["changed_median"]) if baseline else "")
    candidates = {variant: all(record.get("completed") and record.get("passed")
                               for record in manifest["profiles"] if record["variant"] == variant)
                  for variant in VARIANTS}
    manifest.update(completed=not errors, errors=errors, sdk_hashes=sdk_hashes,
                    all_scenario_gates_by_variant=candidates)
    save()
    raise SystemExit(1 if errors else 0)


if __name__ == "__main__":
    main()
