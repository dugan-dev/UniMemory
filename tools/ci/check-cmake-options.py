"""Reject incomplete or invalid public profiles without building a target."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
cases = (
    ("backend-missing", ["-DUNIMEMORY_STATISTICS=OFF"], "UNIMEMORY_BACKEND"),
    ("statistics-missing", ["-DUNIMEMORY_BACKEND=standard"], "UNIMEMORY_STATISTICS"),
    ("backend-invalid", ["-DUNIMEMORY_BACKEND=invalid", "-DUNIMEMORY_STATISTICS=OFF"], "UNIMEMORY_BACKEND"),
    ("statistics-invalid", ["-DUNIMEMORY_BACKEND=standard", "-DUNIMEMORY_STATISTICS=invalid"], "UNIMEMORY_STATISTICS"),
    ("checks-invalid", ["-DUNIMEMORY_BACKEND=standard", "-DUNIMEMORY_STATISTICS=OFF", "-DUNIMEMORY_CHECKS=invalid"], "UNIMEMORY_CHECKS"),
)
for name, flags, reason in cases:
    directory = root / "build/option-validation" / name
    if (directory / "CMakeCache.txt").exists():
        raise SystemExit(f"Use a fresh option-validation directory: {directory}")
    result = subprocess.run(["cmake", "-S", str(root), "-B", str(directory),
                             "-DUNIMEMORY_BUILD_TESTS=OFF", *flags],
                            text=True, capture_output=True)
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "configure.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode == 0 or reason not in result.stdout + result.stderr:
        raise SystemExit(f"{name} was not rejected for {reason}")
    print(f"Rejected {name}: {reason}")
