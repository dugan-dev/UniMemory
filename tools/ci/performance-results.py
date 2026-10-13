"""Reassemble measured artifacts without changing their source or acceptance gate."""
import argparse
import csv
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import subprocess

PLATFORMS = {"windows-x64": "windows-2022", "linux-x64": "ubuntu-24.04", "macos-arm64": "macos-15"}
BACKENDS = ("standard", "mimalloc", "jemalloc")
SOURCE_PREFIXES = ("include/", "benchmarks/", "cmake/")
SOURCE_FILES = {"CMakeLists.txt", "CMakePresets.json", "vcpkg.json"}


def command(*args):
    return subprocess.check_output(args, text=True, encoding="utf-8").strip()


def api(path, pages=False):
    flags = ("--paginate", "--slurp") if pages else ()
    return json.loads(command("gh", "api", *flags, path))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def successful_step(job, name):
    steps = [step for step in job.get("steps", []) if step["name"] == name]
    return len(steps) == 1 and steps[0]["status"] == "completed" and steps[0]["conclusion"] == "success"


def trusted_run(repository, run_id):
    run = api(f"repos/{repository}/actions/runs/{run_id}")
    require(run["head_repository"]["full_name"] == repository and run["event"] == "workflow_dispatch"
            and run["status"] == "completed" and run["path"].split("@", 1)[0] == ".github/workflows/performance.yml",
            f"Run {run_id} is not a completed trusted performance dispatch")
    pages = api(f"repos/{repository}/actions/runs/{run_id}/jobs?per_page=100", pages=True)
    return run, [job for page in pages for job in page["jobs"]]


def unchanged(left, right):
    subprocess.run(["git", "merge-base", "--is-ancestor", left, right], check=True)
    changed = command("git", "diff", "--name-only", left, right).splitlines()
    require(not any(path.startswith(SOURCE_PREFIXES) or path in SOURCE_FILES for path in changed),
            f"Production sources changed between {left} and {right}")
    return changed


def leaf_diagnoses(jobs):
    return [job for job in jobs if job["name"].startswith("verified-performance / diagnosis")
            and " / diagnose (" in job["name"]]


def audit(args):
    repository = os.environ["GITHUB_REPOSITORY"]
    run, jobs = trusted_run(repository, args.measured_run)
    previous, old_jobs = trusted_run(repository, args.baseline_diagnosis_run)
    revision = command("git", "rev-parse", args.source_revision + "^{commit}")
    require(run["head_sha"] == revision, "Measured run source differs from requested source")
    checkout = command("git", "rev-parse", "HEAD")
    require(checkout == os.environ["GITHUB_SHA"], "Checkout differs from workflow source")
    changed = unchanged(revision, checkout)
    unchanged(previous["head_sha"], revision)
    measured = {}
    for platform, runner in PLATFORMS.items():
        suffix = f" / measure ({runner}, {platform})"
        selected = [job for job in jobs if job["name"].startswith("verified-performance / measure (") and job["name"].endswith(suffix)]
        require(len(selected) == 1, f"Missing or duplicate main measurement: {platform}")
        job = selected[0]
        require(job["status"] == "completed" and successful_step(job, "Measure matched Native and UniMemory workloads"),
                f"Incomplete matched measurement: {platform}")
        if platform == "linux-x64":
            require(successful_step(job, "Existing16 native application benchmarks on the same Linux host"), "Incomplete native application measurements")
        measured[platform] = job
    expected = {(platform, backend) for platform in PLATFORMS for backend in BACKENDS}
    inherited = [job for job in leaf_diagnoses(old_jobs) if not job["name"].endswith("windows-x64, standard)")]
    repaired = [job for job in leaf_diagnoses(jobs) if job["name"].endswith("windows-x64, standard)")]
    selected = inherited + repaired
    require(len(inherited) == 8 and len(repaired) == 1, "Require eight original diagnosis leaves and repaired Windows Standard")
    found = set()
    for job in selected:
        match = re.search(r" / diagnose \([^,]+, ([^,]+), ([^)]+)\)$", job["name"])
        require(match is not None, "Unexpected diagnosis job name")
        found.add(tuple(match.groups()))
        require(job["status"] == "completed" and job["conclusion"] == "success", "Diagnosis failed: " + job["name"])
    require(found == expected, "Diagnosis matrix is incomplete or duplicated")
    fixtures = [job for job in jobs if job["name"].startswith("verified-performance / fault-fixtures (")]
    evidence = [job for job in jobs if job["name"] == "verified-performance / correctness-evidence"]
    require(len(fixtures) == 6 and len(evidence) == 1 and all(job["conclusion"] == "success" for job in fixtures + evidence),
            "Missing successful correctness provenance or fault fixtures")
    output = dict(repository=repository, measured_run=run["id"], measured_sha=revision,
                  measured_run_conclusion=run["conclusion"], checkout_sha=checkout, changed_paths=changed,
                  baseline_diagnosis_run=previous["id"], baseline_diagnosis_sha=previous["head_sha"],
                  measurement_jobs=measured, diagnosis_jobs=selected, all_measured_run_jobs=jobs,
                  artifact_sources={name: run["id"] for name in [*("performance-" + p for p in PLATFORMS), "native-application-comparison"]})
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "source-audit.json").write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(f"Verified complete matched measurements at {revision}; diagnosis leaves 8 + 1")


def load_tool(root, name):
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), root / "tools/ci" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def performance_section(content, heading):
    begin = content.index(heading + "\n")
    finish = content.index("\n## ", begin + len(heading))
    return begin, finish


def restore_overview_framework(root):
    """Use the main four-row framework; preserve all prose outside this section."""
    prepared = []
    for name, heading in (("README.md", "## Performance"), ("README.zh-CN.md", "## \u6027\u80fd")):
        path = root / name
        content = path.read_text(encoding="utf-8")
        template = command("git", "show", "origin/main:" + name)
        begin, finish = performance_section(content, heading)
        template_begin, template_finish = performance_section(template, heading)
        section = template[template_begin:template_finish]
        require(len([line for line in section.splitlines() if line.startswith("|")]) == 6,
                "Main README performance framework must contain four operation rows")
        require("performance-overview-source:" not in section, "Main template contains a frozen local source")
        prepared.append((path, content[:begin] + section + content[finish:]))
    for path, content in prepared:
        path.write_text(content, encoding="utf-8", newline="\n")


def refresh_provenance_pages(root, audit_data):
    revision = audit_data["measured_sha"]
    url = f"https://github.com/{audit_data['repository']}/actions/runs/{audit_data['measured_run']}"
    for name, text in (("performance.md", f"Measured source `{revision}` · [original workflow run]({url}). All three platforms, all three backends and both statistics settings retain their original provenance."),
                       ("performance.zh-CN.md", f"\u6d4b\u91cf\u6e90\u7801 `{revision}` · [\u539f\u59cb\u5de5\u4f5c\u6d41]({url})\u3002\u4e09\u4e2a\u5e73\u53f0\u3001\u4e09\u79cd\u540e\u7aef\u53ca\u7edf\u8ba1\u5f00\u5173\u7684\u7ed3\u679c\u4fdd\u7559\u539f\u59cb\u6765\u6e90\u3002")):
        path = root / "docs" / name
        content = path.read_text(encoding="utf-8")
        paragraphs = content.split("\n\n", 2)
        require(len(paragraphs) == 3, "Missing performance report introduction")
        path.write_text(paragraphs[0] + "\n\n" + text + "\n\n" + paragraphs[2], encoding="utf-8", newline="\n")
    index = root / "docs/results/current/README.md"
    index.write_text(f"# Current measurements\n\nMeasured source `{revision}` · [original workflow run]({url}).\n\n"
                     "All 18 platform/backend/statistics profiles were validated against the original run and SDK manifests. "
                     "Raw process observations, summary statistics and individual profile provenance are retained in their profile directories.\n\n"
                     "[Performance](../../performance.md) · [\u7b80\u4f53\u4e2d\u6587](../../performance.zh-CN.md) · "
                     "[Native applications](native-applications/README.md)\n", encoding="utf-8", newline="\n")


def gates(args, audit_data):
    rows, failures = [], []
    for platform in PLATFORMS:
        artifact = args.artifacts / ("performance-" + platform)
        statuses = list(artifact.rglob("acceptance-status.txt"))
        failed_files = list(artifact.rglob("acceptance-failures.txt"))
        require(len(statuses) == len(failed_files) == 1, "Missing retained gate status for " + platform)
        status = statuses[0].read_text().splitlines()
        require(len(status) == 3 and {line.split(":", 1)[0] for line in status} == set(BACKENDS), "Incomplete backend gate statuses")
        if failed_files[0].read_text().strip() or any(line != backend + ": passed >=0.90" for backend in BACKENDS for line in status if line.startswith(backend + ":")):
            failures.append(platform + ": original recorded gate failed")
        for backend in BACKENDS:
            manifests = [path for path in artifact.rglob("environment.json") if path.parent.name == "performance-acceptance-Release"
                         and json.loads(path.read_text())["profile"]["backend"] == backend]
            require(len(manifests) == 1, f"Missing or duplicate raw gate: {platform}/{backend}")
            manifest = json.loads(manifests[0].read_text())
            require(manifest["completed"] is True and manifest["trials"] >= 10 and manifest["minimum_ratio"] == .90,
                    "Incomplete gate or changed threshold")
            with (manifests[0].parent / "summary.csv").open(newline="") as stream:
                cases = list(csv.DictReader(stream))
            expected_cases = {"raw:" + str(size) for size in (16, 64, 256, 4096, 65536)} | {"zeroed", "reallocate", "reallocate_zeroed", "unique", "array", "shared", "vector", "pmr_vector", "owned"}
            require(len(cases) == len(expected_cases) and {row["factor"] for row in cases} == expected_cases,
                    "Incomplete acceptance scenario matrix")
            for row in cases:
                median, mean = float(row["performance_percent"]), float(row["mean_performance_percent"])
                passed = math.isfinite(median) and math.isfinite(mean) and median >= 90 and mean >= 90 and row["passed"].lower() == "true"
                rows.append(dict(platform=platform, backend=backend, scenario=row["factor"], median_percent=median, mean_percent=mean, passed=passed))
                if not passed:
                    failures.append(f"{platform}/{backend}/{row['factor']}: median={median:.3f}%, mean={mean:.3f}%")
            if manifest.get("passed") is not True:
                failures.append(platform + "/" + backend + ": original gate manifest failed")
        if audit_data["measurement_jobs"][platform]["conclusion"] != "success":
            failures.append(platform + ": original main measurement job failed")
    result = dict(minimum_percent=90, cases=rows, failures=failures, passed=not failures)
    (args.output / "gate-report.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    lines = ["# Recorded Native/API acceptance", "", "Threshold: median and arithmetic mean both >=90%. No tolerance changes.", "", "| Platform | Backend | Scenario | Median % | Mean % | Passed |", "| --- | --- | --- | ---: | ---: | --- |"]
    lines += [f"| {row['platform']} | {row['backend']} | {row['scenario']} | {row['median_percent']:.3f} | {row['mean_percent']:.3f} | {row['passed']} |" for row in rows]
    lines += ["", *failures]
    (args.output / "gate-report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return result


def assemble(args):
    root = Path(__file__).resolve().parents[2]
    audit_data = json.loads((args.output / "source-audit.json").read_text())
    gates(args, audit_data)
    revision = audit_data["measured_sha"]
    overview = load_tool(root, "refresh-performance-overview")
    overview.stage_artifacts(root, args.artifacts, revision, audit_data["measured_run"])
    load_tool(root, "refresh-performance-tables").refresh_tables(root, args.native_input)
    report = load_tool(root, "performance-report")
    report.refresh_reports(root / "docs/results/current", root / "docs/images/performance", revision)
    restore_overview_framework(root)
    overview.refresh_overview(root, args.output / "comparison.json", revision)
    refresh_provenance_pages(root, audit_data)
    print("Prepared measured results and 36 bilingual SVGs; final acceptance remains separate")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("audit", "assemble", "accept"))
    parser.add_argument("--measured-run", type=int, default=38340749502)
    parser.add_argument("--baseline-diagnosis-run", type=int, default=38340149739)
    parser.add_argument("--source-revision", default="fe90b68953b3af88d7ebd3d64131e76673c234ca")
    parser.add_argument("--artifacts", type=Path, default=Path("build/downloads"))
    parser.add_argument("--native-input", type=Path, default=Path("build/native-applications"))
    parser.add_argument("--output", type=Path, default=Path("build/performance-results"))
    args = parser.parse_args()
    if args.phase == "audit":
        audit(args)
    elif args.phase == "assemble":
        assemble(args)
    else:
        result = json.loads((args.output / "gate-report.json").read_text())
        require(result["passed"] is True and not result["failures"], "Recorded >=90% acceptance failed: " + "; ".join(result["failures"]))
        print("All original main measurements and recorded >=90% cases passed")


if __name__ == "__main__":
    main()
