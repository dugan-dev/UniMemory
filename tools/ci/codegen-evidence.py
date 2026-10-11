"""Disassemble the measured Delivery executable without changing any inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from benchmark_profile import read_profile


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def raw_snapshot(results):
    return {path.relative_to(results).as_posix(): sha256(path)
            for path in sorted(results.rglob("*")) if path.is_file() and
            path.relative_to(results).parts[0] != "codegen"}


def input_snapshot(executable, results, profile):
    paths = {Path(item["path"]) for item in profile["sdk_binaries"].values()}
    paths.update(Path(path) for path in profile["sdk_runtime_files"])
    paths.update(executable.parent / name for name in profile["runtime_dlls"])
    return {"executable_sha256": sha256(executable),
            "sdk_files": {str(path): sha256(path) for path in sorted(paths)},
            "raw_files": raw_snapshot(results)}


def disassembler(executable, system):
    if system == "Windows":
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        found = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-find",
                                         "VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"], text=True)
        tools = [Path(line.strip()).resolve() for line in found.splitlines() if line.strip()]
        build = executable.parent.parent
        records = list(build.glob("CMakeFiles/*/CMakeCXXCompiler.cmake"))
        if len(records) != 1:
            raise RuntimeError("Actual Delivery compiler record missing")
        match = re.search(r'set\(CMAKE_CXX_COMPILER "([^"\n]+)"\)', records[0].read_text(encoding="utf-8"))
        if not match:
            raise RuntimeError("Actual Delivery compiler path missing")
        tool = (Path(match[1]).parent / "dumpbin.exe").resolve()
        if tool not in tools or not tool.is_file():
            raise RuntimeError("VSwhere could not confirm the actual compiler's dumpbin")
        return [str(tool), "/DISASM", str(executable)]
    if system == "Linux":
        tool = shutil.which("objdump")
        if not tool:
            raise RuntimeError("Existing objdump is required")
        return [tool, "-Cd", "--no-show-raw-insn", str(executable)]
    if system == "Darwin":
        xcrun = shutil.which("xcrun")
        if xcrun:
            found = subprocess.run([xcrun, "--find", "llvm-objdump"], text=True, capture_output=True)
            if found.returncode == 0 and Path(found.stdout.strip()).is_file():
                return [found.stdout.strip(), "-Cd", "--no-show-raw-insn", str(executable)]
        tool = shutil.which("otool")
        if not tool:
            raise RuntimeError("Existing Apple llvm-objdump or otool is required")
        return [tool, "-tvV", str(executable)]
    raise RuntimeError("Unsupported code generation evidence platform")


def collect(executable, results):
    executable, results = executable.resolve(), results.resolve()
    if executable.stem != "UniMemoryDeliveryBenchmark":
        raise ValueError("Evidence must use the measured Delivery executable")
    manifest = json.loads((results / "environment.json").read_text(encoding="utf-8"))
    profile = read_profile(executable)
    if profile != manifest.get("build_profile") or sha256(executable) != manifest.get("executable_sha256"):
        raise ValueError("Delivery executable or SDK profile changed after measurement")
    output = results / "codegen"
    if output.exists():
        raise ValueError("Code generation evidence requires a fresh output directory")
    before = input_snapshot(executable, results, profile)
    command = disassembler(executable, platform.system())
    output.mkdir()
    assembly, errors = output / "delivery-disassembly.txt", output / "delivery-disassembly.stderr.txt"
    with assembly.open("wb") as stdout, errors.open("wb") as stderr:
        process = subprocess.run(command, stdout=stdout, stderr=stderr)
    after = input_snapshot(executable, results, profile)
    evidence = {"schema": 1, "source_revision": manifest.get("source_revision"),
                "run_id": manifest.get("run_id"), "platform": platform.system(),
                "target": "UniMemoryDeliveryBenchmark", "command": command,
                "tool_sha256": sha256(Path(command[0])), "returncode": process.returncode,
                "sdk_profile": profile, "before": before, "after": after,
                "inputs_unchanged": before == after, "assembly_sha256": sha256(assembly),
                "completed": process.returncode == 0 and before == after and assembly.stat().st_size > 0}
    (output / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    if not evidence["completed"]:
        raise RuntimeError("Delivery disassembly failed or measured inputs changed; inspect codegen evidence")
    print("Captured actual Delivery disassembly; executable, SDK and all raw result hashes unchanged")


def self_test():
    with tempfile.TemporaryDirectory(prefix="unimemory-codegen-test-") as temporary:
        root = Path(temporary)
        executable = root / "UniMemoryDeliveryBenchmark"
        sdk = root / "native-sdk.bin"
        results = root / "results"
        results.mkdir()
        executable.write_bytes(b"test executable")
        sdk.write_bytes(b"test SDK")
        (results / "latency.csv").write_text("trial,value\n1,1\n", encoding="utf-8")
        profile = {"sdk_binaries": {"sdk_binary": {"path": str(sdk)}},
                   "sdk_runtime_files": {}, "runtime_dlls": {}}
        before = input_snapshot(executable, results, profile)
        (results / "codegen").mkdir()
        (results / "codegen/assembly.txt").write_text("test evidence", encoding="utf-8")
        assert before == input_snapshot(executable, results, profile)
        for path in (executable, sdk, results / "latency.csv"):
            original = path.read_bytes()
            path.write_bytes(original + b"changed")
            assert before != input_snapshot(executable, results, profile)
            path.write_bytes(original)
        assert before == input_snapshot(executable, results, profile)
    with tempfile.TemporaryDirectory(prefix="unimemory-codegen-failure-") as temporary:
        root = Path(temporary)
        executable = root / "UniMemoryDeliveryBenchmark"
        executable.write_bytes(b"test executable")
        results = root / "results"
        results.mkdir()
        profile = {"sdk_binaries": {}, "sdk_runtime_files": {}, "runtime_dlls": {}}
        (results / "environment.json").write_text(json.dumps({"build_profile": profile,
            "executable_sha256": sha256(executable)}), encoding="utf-8")
        module = sys.modules[__name__]
        with patch.object(module, "read_profile", return_value=profile), patch.object(module, "disassembler",
                return_value=[sys.executable, "-c", "raise SystemExit(7)"]):
            try:
                collect(executable, results)
                raise AssertionError("Disassembler failure was accepted")
            except RuntimeError:
                evidence = json.loads((results / "codegen/evidence.json").read_text())
                assert evidence["returncode"] == 7 and evidence["completed"] is False
                assert evidence["inputs_unchanged"] is True
    print("Codegen input snapshots detect executable, SDK and raw-result changes")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path, nargs="?")
    parser.add_argument("results", type=Path, nargs="?")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    elif args.executable is None or args.results is None:
        parser.error("executable and results are required")
    else:
        collect(args.executable, args.results)


if __name__ == "__main__":
    main()
