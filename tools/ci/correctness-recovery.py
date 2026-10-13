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
                        "tests/headers/factory_statistics.cpp", "tests/headers/inheritance.cpp"})


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
                                   "tests/headers/inheritance.cpp"}],
            "performance": "not requested"}


def command(*args):
    return subprocess.check_output(args, text=True).strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-run", required=True, type=int)
    parser.add_argument("--baseline-sha", required=True)
    parser.add_argument("--allow-source-changes", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    repository = os.environ["GITHUB_REPOSITORY"]
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
    evidence.update(source_revision=head, baseline_job_counts=counts, recovery_run_id=os.environ["GITHUB_RUN_ID"],
                    recovery_url=f"https://github.com/{repository}/actions/runs/{os.environ['GITHUB_RUN_ID']}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    if evidence["requires_regression"] and not args.allow_source_changes:
        raise RuntimeError("Source changed: explicitly enable targeted recovery; full additional regression remains required")
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        output.write("requires_regression=" + str(evidence["requires_regression"]).lower() + "\n")
    print(json.dumps(evidence, indent=2))


if __name__ == "__main__":
    main()
