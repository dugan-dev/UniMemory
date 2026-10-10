"""Publish trusted complete results using a scoped bot branch and reviewed PR."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def run(*command):
    return subprocess.check_output(command, text=True).strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("artifacts", type=Path)
    parser.add_argument("base", choices=("dev", "main"))
    args = parser.parse_args()
    revision = os.environ["GITHUB_SHA"]
    root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location("report", root / "tools/ci/performance-report.py")
    report = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(report)
    run("git", "fetch", "origin", args.base)
    if run("git", "rev-parse", f"origin/{args.base}") != revision:
        print("A newer source revision exists; do not publish stale results")
        return
    branch = f"automation/performance-{args.base}"
    previous = run("git", "ls-remote", "--heads", "origin", branch)
    lease = previous.split()[0] if previous else ""
    run("git", "switch", "-c", branch)
    for label in ("linux-x64", "macos-arm64"):
        directory = args.artifacts / f"performance-{label}"
        data = directory / "results"
        manifest = report.validate(data)
        if manifest["source_revision"] != revision or str(manifest["run_id"]) != os.environ["GITHUB_RUN_ID"]:
            raise RuntimeError("Artifact provenance does not match the trusted workflow")
        charts = root / "docs/images/performance" / label
        subprocess.run(["python3", str(root / "tools/ci/performance-report.py"), str(data), str(charts), "--label", label], check=True)
        destination = root / "docs/results/current" / label
        destination.mkdir(parents=True, exist_ok=True)
        for name in ("environment.json", "scaling.csv", "tails.csv", "latency.csv", "pressure.csv", "footprint.csv", "heap.csv", "full.csv"):
            shutil.copyfile(data / name, destination / name)
    for name in ("README.md", "README.zh-CN.md"):
        path = root / name
        text = path.read_text(encoding="utf-8")
        old = '![Windows and Linux workload comparison](docs/images/workload-comparison.png)' if name == 'README.md' else '![Windows 与 Linux 耗时对比](docs/images/workload-comparison.png)'
        images = '![Cross-thread throughput](docs/images/performance/linux-x64/throughput.svg)\n\n![Peak resident memory](docs/images/performance/linux-x64/memory.svg)'
        if old in text:
            path.write_text(text.replace(old, images), encoding="utf-8")
    allowed = ("docs/images/performance/", "docs/results/current/", "README.md", "README.zh-CN.md")
    changed = run("git", "status", "--porcelain").splitlines()
    if any(not line[3:].startswith(allowed) for line in changed):
        raise RuntimeError("Publication attempted changes outside generated paths")
    run("git", "config", "user.name", "github-actions[bot]")
    run("git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com")
    run("git", "add", "--", *allowed)
    run("git", "commit", "-m", f"Update performance results for {revision[:12]}")
    run("git", "push", f"--force-with-lease=refs/heads/{branch}:{lease}", "origin", f"HEAD:refs/heads/{branch}")
    existing = json.loads(run("gh", "pr", "list", "--head", branch, "--base", args.base, "--json", "number"))
    if existing:
        number = str(existing[0]["number"])
    else:
        body = f"Generated from trusted source {revision}. All measurements and report validation passed. Raw individual samples remain in workflow artifacts.\n\nRun: https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}\n"
        body_file = root / "build/publication-body.txt"
        body_file.write_text(body, encoding="utf-8")
        url = run("gh", "pr", "create", "--base", args.base, "--head", branch, "--title", "Update measured performance charts", "--body-file", str(body_file))
        number = url.rsplit("/", 1)[-1]
    # Token-created PRs do not trigger ordinary Actions events; explicitly dispatch
    # the existing required workflows, and wait for their checks before merging.
    validations = []
    for workflow in ("ci.yml", "release-validation.yml"):
        run("gh", "workflow", "run", workflow, "--ref", branch)
        for attempt in range(30):
            candidates = json.loads(run("gh", "run", "list", "--workflow", workflow,
                                       "--branch", branch, "--event", "workflow_dispatch",
                                       "--json", "databaseId,headSha", "--limit", "5"))
            selected = next((item for item in candidates if item["headSha"] == run("git", "rev-parse", "HEAD")), None)
            if selected:
                validations.append(str(selected["databaseId"]))
                break
            time.sleep(10)
        else:
            raise RuntimeError("Required publication checks did not start")
    for validation in validations:
        subprocess.run(["gh", "run", "watch", validation, "--exit-status", "--interval", "20"], check=True)
    run("git", "fetch", "origin", args.base)
    if run("git", "rev-parse", f"origin/{args.base}") != revision:
        print("Source changed during publication checks; leave results PR unmerged")
        return
    if run("gh", "pr", "view", number, "--json", "headRefOid", "--jq", ".headRefOid") != run("git", "rev-parse", "HEAD"):
        raise RuntimeError("Results PR changed during validation")
    run("gh", "pr", "merge", number, "--squash", "--delete-branch")
    print(f"Performance PR #{number} merged after its required checks passed")


if __name__ == "__main__":
    main()
