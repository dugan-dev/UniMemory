"""Record exact-source baseline evidence without claiming unrun current tests passed."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess


def evidence_only(path):
    return (path.startswith(("tests/tools/", "tools/", ".github/workflows/", "docs/"))
            or path in {"README.md", "README.zh-CN.md", "AGENTS.md", "tests/allocation_failure.cpp",
                        "tests/headers/factory_statistics.cpp", "tests/headers/inheritance.cpp",
                        "tests/headers/memory.cpp", "tests/headers/owned_block.cpp"})


def correctness_job(name):
    return not (name in {"changes", "overview", "publish", "performance acceptance"} or
                name.endswith(("acceptance", "/ correctness")) or
                any(part in name for part in ("measure-", "diagnosis-", "/ measure", "/ diagnose")))


def baseline_evidence(run, jobs, repository, expected_sha, changed):
    if (run["head_repository"]["full_name"] != repository or
            run["path"].split("@", 1)[0] != ".github/workflows/performance.yml" or
            run["event"] not in {"push", "workflow_dispatch"} or run["status"] != "completed" or
            (run["id"] == 38262808305 and run["event"] != "workflow_dispatch") or
            not re.fullmatch(r"[0-9a-f]{7,40}", expected_sha) or
            not run["head_sha"].startswith(expected_sha)):
        raise RuntimeError("Baseline repository, workflow, event, completion or SHA mismatch")
    passed = [{"id": job["id"], "name": job["name"], "source_revision": run["head_sha"]}
              for job in jobs if job["status"] == "completed" and job["conclusion"] == "success"
              and correctness_job(job["name"])]
    if not passed:
        raise RuntimeError("Baseline has no completed passing jobs")
    source_changes = [path for path in changed if not evidence_only(path)]
    return {"baseline_run_id": run["id"], "baseline_source_revision": run["head_sha"],
            "baseline_url": run["html_url"], "passed_baseline_jobs": passed,
            "changed_paths": changed, "source_changes": source_changes,
            "requires_regression": bool(source_changes),
            "baseline_reuse_eligible": not source_changes,
            "targeted_fixtures": [path for path in changed if path in
                                  {"tests/allocation_failure.cpp", "tests/headers/factory_statistics.cpp",
                                   "tests/headers/inheritance.cpp", "tests/headers/memory.cpp",
                                   "tests/headers/owned_block.cpp"}],
            "performance": "not requested"}


def retry_evidence(run, jobs, repository, changed):
    if (run["head_repository"]["full_name"] != repository or run["status"] != "completed" or
            run["event"] != "workflow_dispatch" or
            run["path"].split("@", 1)[0] != ".github/workflows/performance.yml"):
        raise RuntimeError("Prior recovery identity or completion mismatch")
    # Skipped tests are safe only when no fixture they exercise changed again.
    other_fixtures = {"tests/allocation_failure.cpp", "tests/headers/factory_statistics.cpp",
                      "tests/headers/inheritance.cpp"}
    if any(not evidence_only(path) or path in other_fixtures for path in changed):
        raise RuntimeError("Analysis-only retry cannot inherit checks after other source/fixture changes")
    suffixes = ["report-tests",
                "missing standard Release OFF / ubuntu-24.04 / checks=AUTO",
                "missing standard Release OFF / ubuntu-24.04 / checks=ON",
                "missing standard Release OFF / ubuntu-24.04 / checks=OFF",
                "missing standard Release OFF / ubuntu-24.04-arm / checks=AUTO"]
    inherited = []
    for suffix in suffixes:
        matches = [job for job in jobs if (job["name"] == suffix or job["name"].endswith(" / " + suffix))
                   and job["conclusion"] != "skipped"]
        if len(matches) != 1 or matches[0]["status"] != "completed" or matches[0]["conclusion"] != "success":
            raise RuntimeError("Required prior recovery target did not pass: " + suffix)
        inherited.append({"id": matches[0]["id"], "name": matches[0]["name"], "source_revision": run["head_sha"]})
    coverage = []
    for statistics in ("OFF", "ON"):
        suffix = "missing standard analysis and coverage / stats=" + statistics
        matches = [job for job in jobs if job["name"] == suffix or job["name"].endswith(" / " + suffix)]
        if len(matches) != 1:
            raise RuntimeError("Prior coverage profile missing: " + statistics)
        steps = [step for step in matches[0].get("steps", [])
                 if step["name"] == "Complete project line and branch coverage"]
        if len(steps) != 1 or steps[0]["status"] != "completed" or steps[0]["conclusion"] != "success":
            raise RuntimeError("Prior complete coverage did not pass: " + statistics)
        coverage.append({"job_id": matches[0]["id"], "statistics": statistics,
                         "step": steps[0]["name"], "source_revision": run["head_sha"]})
    return {"run_id": run["id"], "source_revision": run["head_sha"], "url": run["html_url"],
            "inherited_jobs": inherited, "inherited_coverage_steps": coverage,
            "changed_paths": changed}


def command(*args):
    return subprocess.check_output(args, text=True).strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-run", type=int)
    parser.add_argument("--verify-fixtures-run", type=int)
    parser.add_argument("--verify-adapters-run", type=int)
    parser.add_argument("--baseline-sha")
    parser.add_argument("--allow-source-changes", action="store_true")
    parser.add_argument("--analysis-retry", action="store_true")
    parser.add_argument("--previous-recovery-run", type=int, default=38332523441)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    repository = os.environ["GITHUB_REPOSITORY"]
    if args.verify_fixtures_run or args.verify_adapters_run:
        selected_run = args.verify_fixtures_run or args.verify_adapters_run
        run = json.loads(command("gh", "api", f"repos/{repository}/actions/runs/{selected_run}"))
        pages = json.loads(command("gh", "api", "--paginate", "--slurp",
                                  f"repos/{repository}/actions/runs/{selected_run}/jobs?filter=latest&per_page=100"))
        jobs = [job for page in pages for job in page["jobs"]]
        if (run["head_repository"]["full_name"] != repository or run["status"] != "completed" or
                run["event"] != "workflow_dispatch" or run["path"].split("@", 1)[0] != ".github/workflows/performance.yml"):
            raise RuntimeError("Untrusted fixture evidence")
        head = command("git", "rev-parse", "HEAD")
        subprocess.run(["git", "merge-base", "--is-ancestor", run["head_sha"], head], check=True)
        changes = command("git", "diff", "--name-only", run["head_sha"], head).splitlines()
        if any(not evidence_only(path) or path.startswith("tests/headers/") for path in changes):
            raise RuntimeError("Library or header fixtures changed since their verified run")
        prefixes = (("recover-correctness / jemalloc ",) if args.verify_adapters_run else
                    ("recover-correctness / changed header fixtures / ",
                     "recover-correctness / missing standard analysis and coverage / stats="))
        matches = [job for job in jobs if job["name"].startswith(prefixes)]
        expected_count = 2 if args.verify_adapters_run else 6
        if len(matches) != expected_count or any(job["status"] != "completed" or job["conclusion"] != "success" for job in matches):
            raise RuntimeError("Required inherited jobs did not all pass")
        evidence = {"run_id": run["id"], "source_revision": run["head_sha"], "jobs":
                    [{"id": job["id"], "name": job["name"]} for job in matches]}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
            output.write("verified=true\n")
        print(json.dumps(evidence, indent=2))
        return
    if not args.baseline_run or not args.baseline_sha:
        parser.error("--baseline-run and --baseline-sha are required for baseline verification")
    run = json.loads(command("gh", "api", f"repos/{repository}/actions/runs/{args.baseline_run}"))
    pages = json.loads(command("gh", "api", "--paginate", "--slurp",
                              f"repos/{repository}/actions/runs/{args.baseline_run}/jobs?filter=latest&per_page=100"))
    jobs = [job for page in pages for job in page["jobs"]]
    counts = {status: sum(job["conclusion"] == status for job in jobs)
              for status in ("success", "failure", "cancelled", "skipped")}
    if args.baseline_run == 38262808305 and counts != {
            "success": 108, "failure": 15, "cancelled": 1, "skipped": 8}:
        raise RuntimeError("Baseline manifest does not match audited 132-job evidence")
    head = command("git", "rev-parse", "HEAD")
    if head != os.environ["GITHUB_SHA"]:
        raise RuntimeError("Checkout is not the dispatched source revision")
    subprocess.run(["git", "merge-base", "--is-ancestor", run["head_sha"], head], check=True)
    changed = command("git", "diff", "--name-only", run["head_sha"], head).splitlines()
    evidence = baseline_evidence(run, jobs, repository, args.baseline_sha, changed)
    evidence["sdk_header_tree_hash"] = {
        "baseline": command("git", "rev-parse", run["head_sha"] + ":include"),
        "current": command("git", "rev-parse", head + ":include")}
    if evidence["sdk_header_tree_hash"]["baseline"] != evidence["sdk_header_tree_hash"]["current"]:
        evidence["requires_regression"] = True
        evidence["baseline_reuse_eligible"] = False
    if args.analysis_retry:
        previous = json.loads(command("gh", "api", f"repos/{repository}/actions/runs/{args.previous_recovery_run}"))
        pages = json.loads(command("gh", "api", "--paginate", "--slurp",
                                  f"repos/{repository}/actions/runs/{args.previous_recovery_run}/jobs?filter=latest&per_page=100"))
        subprocess.run(["git", "merge-base", "--is-ancestor", previous["head_sha"], head], check=True)
        previous_changes = command("git", "diff", "--name-only", previous["head_sha"], head).splitlines()
        if command("git", "rev-parse", previous["head_sha"] + ":include") != evidence["sdk_header_tree_hash"]["current"]:
            raise RuntimeError("SDK headers changed since prior recovery")
        evidence["previous_recovery"] = retry_evidence(previous, [job for page in pages for job in page["jobs"]],
                                                       repository, previous_changes)
    evidence.update(source_revision=head, baseline_job_counts=counts, recovery_run_id=os.environ["GITHUB_RUN_ID"],
                    recovery_url=f"https://github.com/{repository}/actions/runs/{os.environ['GITHUB_RUN_ID']}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    if evidence["requires_regression"] and not args.allow_source_changes:
        raise RuntimeError("Source changed: explicitly enable targeted recovery; full additional regression remains required")
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        output.write("requires_regression=" + str(evidence["requires_regression"]).lower() + "\n")
        output.write("inherited_verified=" + str(args.analysis_retry).lower() + "\n")
    print(json.dumps(evidence, indent=2))


if __name__ == "__main__":
    main()
