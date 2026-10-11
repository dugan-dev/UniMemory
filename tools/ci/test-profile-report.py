"""Offline schema-2 contracts; reuse the historical fixture without measuring."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("historical_tests", root / "tests/tools/test_performance_report.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
report, measurement = module.report, module.measurement
from publication_metadata import sanitize_publication

for user_root in ("/" + "home/runner", "/" + "Users/runner", "C:" + "/Users/runner"):
    original = {"sdk": {"path": user_root + "/sdk/native.so", "sha256": "b" * 64},
                "sdk_runtime_files": {user_root + "/sdk/native.so": "b" * 64},
                "flags": "-I" + user_root + "/include -O2"}
    cleaned = sanitize_publication(original)
    assert cleaned["sdk"]["sha256"] == "b" * 64 and cleaned["sdk"]["path"].startswith("<runner>")
    assert list(cleaned["sdk_runtime_files"].values()) == ["b" * 64]
    assert original["sdk"]["path"].startswith(user_root) # Artifacts remain untouched.
    assert sanitize_publication(cleaned) == cleaned
print("Publication paths normalized recursively; SHA and audit originals retained")
for backend in report.BACKENDS:
    for mode in ("OFF", "ON"):
        fixture = module.ReportTests()
        fixture.setUp()
        try:
            directory = fixture.directory
            api = "basic" if mode == "ON" else "disabled"
            profile = {"backend": backend, "statistics": mode, "checks": "AUTO", "core": "header-only",
                       "sdk_binaries": {"sdk_binary": {"sha256": "a" * 64}} if backend != "standard" else {}}
            manifest = {"schema": 2, "trials": 3, "source_revision": "a" * 40, "build_profile": profile,
                        "backends": [backend], "paths": ["native", api], "logical_cpus": 4,
                        "allocator_environment": {"core": "header-only", "selected_backend": backend, "selected_statistics": api}}
            (directory / "environment.json").write_text(json.dumps(manifest))
            expected = report.sweep_scenarios(profile)
            assert len(expected) == ({"standard": 112, "mimalloc": 214, "jemalloc": 214}[backend]
                                     if mode == "OFF" else {"standard": 112, "mimalloc": 311, "jemalloc": 311}[backend])
            for name in ("latency", "scaling", "tails", "pressure", "footprint", "heap", "full"):
                rows = report.read(directory / f"{name}.csv")
                if name == "full":
                    fields = ("backend", "workload", "bytes", "alignment", "threads", "statistics", "heap")
                    for row in rows:
                        if row["heap"] == "no" and row["workload"] in ("cross_thread", "detailed_statistics"):
                            row["statistics"] = api
                    rows = [row for row in rows if tuple(row[field] for field in fields) in expected]
                else:
                    rows = [row for row in rows if row["backend"] == backend and
                            ("path" not in row or row["path"] in ("native", api)) and
                            (name != "footprint" or (row["heap"] == "no" and row["statistics"] == api) or
                             (row["heap"] == "yes" and (mode == "ON" or row["statistics"] == "disabled")))]
                if rows:
                    measurement.write_rows(directory / f"{name}.csv", rows)
                else:
                    (directory / f"{name}.csv").write_text("trial,backend,operation,operations,median_ns\n")
            (directory / "sweep-environment.json").write_text(json.dumps({"scenarios_per_trial": len(expected), "build_profile": profile}))
            report.validate(directory)
            for language in report.LANGUAGES:
                output = directory / language
                output.mkdir()
                report.render_charts(directory, output, f"test/{backend}/{mode}", manifest, language)
            rows = report.read(directory / "full.csv")
            measurement.write_rows(directory / "full.csv", rows[:-1])
            try:
                report.validate(directory)
                raise AssertionError("Missing profile scenario was accepted")
            except ValueError as error:
                assert "Incomplete full sweep" in str(error)
            print(f"Validated schema-2 {backend}/{mode}: {len(expected)} scenarios/trial; missing case rejected")
        finally:
            fixture.doCleanups()
