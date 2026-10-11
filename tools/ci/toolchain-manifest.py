"""Assert the selected compiler and produced architecture; retain actual versions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import struct


def architecture(data):
    if data[:4] == b"\x7fELF":
        endian = "<" if data[5] == 1 else ">"
        return {62: "x64", 183: "arm64"}.get(struct.unpack_from(endian + "H", data, 18)[0])
    if data[:2] == b"MZ":
        offset = struct.unpack_from("<I", data, 60)[0]
        if data[offset:offset+4] != b"PE\0\0":
            raise ValueError("Malformed PE image")
        return {0x8664: "x64", 0xaa64: "arm64"}.get(struct.unpack_from("<H", data, offset+4)[0])
    if data[:4] in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe"):
        return {0x1000007: "x64", 0x100000c: "arm64"}.get(struct.unpack_from("<I", data, 4)[0])
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build", type=Path)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--arch", choices=("x64", "arm64"), required=True)
    args = parser.parse_args()
    files = list((args.build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    if len(files) != 1:
        raise ValueError("Expected one compiler configuration")
    text = files[0].read_text()
    values = dict(re.findall(r'set\((CMAKE_CXX_[A-Z_]+) "([^"\n]*)"\)', text))
    expected = {"msvc": "MSVC", "clang-cl": "Clang", "mingw": "GNU", "appleclang": "AppleClang"}.get(
        args.compiler, "GNU" if args.compiler.startswith("gcc-") else "Clang")
    if values.get("CMAKE_CXX_COMPILER_ID") != expected:
        raise ValueError("Selected compiler family was substituted")
    version = values["CMAKE_CXX_COMPILER_VERSION"]
    if "-" in args.compiler and args.compiler.rsplit("-", 1)[-1].isdigit():
        if version.split(".")[0] != args.compiler.rsplit("-", 1)[-1]:
            raise ValueError("Selected compiler major version was substituted")
    candidates = [file for file in files[0].parent.rglob("*") if file.is_file()
                  and "CompilerIdCXX" in file.parts and file.name in ("a.out", "a.exe", "CMakeCXXCompilerId.exe")]
    detected = {architecture(file.read_bytes()) for file in candidates}
    if args.arch not in detected or any(item and item != args.arch for item in detected):
        raise ValueError(f"Compiler target architecture mismatch: {detected}")
    manifest = {"compiler": args.compiler, "family": expected, "version": version,
                "architecture": args.arch, "host": platform.machine(),
                "simulation": values.get("CMAKE_CXX_SIMULATE_ID", ""),
                "frontend": values.get("CMAKE_CXX_COMPILER_FRONTEND_VARIANT", ""),
                "compiler_config_sha256": hashlib.sha256(text.encode()).hexdigest()}
    cache = (args.build / "CMakeCache.txt").read_text()
    sdk = re.search(r"CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION:[^=]*=([^\n]*)", cache)
    if sdk:
        manifest["windows_sdk"] = sdk[1]
    elif os.environ.get("WindowsSDKVersion"):
        manifest["windows_sdk"] = os.environ["WindowsSDKVersion"].rstrip("\\/")
    (args.build / "toolchain.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
