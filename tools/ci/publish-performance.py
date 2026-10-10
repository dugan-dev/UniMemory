"""Publish trusted complete results using a scoped bot branch and reviewed PR."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

WORKFLOWS = {"ci.yml", "release-validation.yml", "portability.yml", "diagnostics.yml", "performance.yml"}
REQUIRED_CHECKS = {"build acceptance", "release acceptance", "portability acceptance",
                   "diagnostics acceptance", "performance acceptance"}


def validate_results_pr(pr, files, repository, branch, base, revision, head):
    if (pr["state"] != "open" or pr["user"]["login"] != "github-actions[bot]" or
            pr["head"]["repo"]["full_name"] != repository or pr["base"]["repo"]["full_name"] != repository or
            pr["head"]["ref"] != branch or pr["head"]["sha"] != head or
            pr["base"]["ref"] != base or pr["base"]["sha"] != revision):
        raise RuntimeError("Results PR identity or source revision changed")
    if len(files) != pr["changed_files"] or not files or any(item["filename"] not in ("README.md", "README.zh-CN.md") and
                        not item["filename"].startswith(("docs/images/performance/", "docs/results/current/"))
                        for item in files):
        raise RuntimeError("Results PR contains changes outside generated paths")


def select_pr_runs(runs, number, head, repository):
    selected = {}
    for item in runs:
        path = item["path"].split("@", 1)[0]
        workflow = path.rsplit("/", 1)[-1]
        if (path == f".github/workflows/{workflow}" and workflow in WORKFLOWS and
                item["head_repository"]["full_name"] == repository and
                item["event"] == "pull_request" and item["head_sha"] == head and
                any(pr["number"] == number for pr in item["pull_requests"])):
            if workflow not in selected or item["id"] > selected[workflow]["id"]:
                selected[workflow] = item
    return selected


def read_pr_files(repository, number):
    pages = json.loads(run("gh", "api", "--paginate", "--slurp", f"repos/{repository}/pulls/{number}/files"))
    return [item for page in pages for item in page]


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
    for label in ("linux-x64", "windows-x64", "macos-arm64"):
        directory = args.artifacts / f"performance-{label}"
        data = directory / "results"
        manifest = report.validate(data)
        if manifest["source_revision"] != revision or str(manifest["run_id"]) != os.environ["GITHUB_RUN_ID"]:
            raise RuntimeError("Artifact provenance does not match the trusted workflow")
        charts = root / "docs/images/performance" / label
        subprocess.run(["python3", str(root / "tools/ci/performance-report.py"), str(data), str(charts), "--label", label], check=True)
        destination = root / "docs/results/current" / label
        destination.mkdir(parents=True, exist_ok=True)
        for name in ("environment.json", "scaling.csv", "tails.csv", "latency.csv", "pressure.csv", "footprint.csv", "heap.csv", "full.csv", "sweep-environment.json"):
            shutil.copyfile(data / name, destination / name)
    for name in ("README.md", "README.zh-CN.md"):
        path = root / name
        text = path.read_text(encoding="utf-8")
        old = re.compile(r'!\[[^\]]*\]\(' + re.escape('docs/images/workload-comparison.png') + r'\)')
        images = '![Cross-thread throughput](docs/images/performance/linux-x64/throughput.svg)\n\n![Peak resident memory](docs/images/performance/linux-x64/memory.svg)'
        if old.search(text):
            path.write_text(old.sub(lambda match: images, text), encoding="utf-8")
    allowed = ("docs/images/performance/", "docs/results/current/", "README.md", "README.zh-CN.md")
    changed = subprocess.check_output(["git", "status", "--porcelain=v1", "-z"], text=True).split("\0")
    if any(line and not line[3:].startswith(allowed) for line in changed):
        raise RuntimeError("Publication attempted changes outside generated paths")
    run("git", "config", "user.name", "github-actions[bot]")
    run("git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com")
    run("git", "add", "--", *allowed)
    run("git", "commit", "-m", f"Update performance results for {revision[:12]}")
    run("git", "push", f"--force-with-lease=refs/heads/{branch}:{lease}", "origin", f"HEAD:refs/heads/{branch}")
    existing = json.loads(run("gh", "pr", "list", "--head", branch, "--base", args.base, "--json", "number"))
    body = f"Generated from trusted source {revision}. All measurements and report validation passed. Raw individual samples remain in workflow artifacts for 14 days.\n\nRun: https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}\n"
    body_file = root / "build/publication-body.txt"
    body_file.write_text(body, encoding="utf-8")
    if existing:
        number = str(existing[0]["number"])
        run("gh", "pr", "edit", number, "--body-file", str(body_file))
    else:
        url = run("gh", "pr", "create", "--base", args.base, "--head", branch, "--title", "Update measured performance charts", "--body-file", str(body_file))
        number = url.rsplit("/", 1)[-1]
    # Dispatch checks do not satisfy PR protection. Approve only real PR runs
    # from this exact, generated-only bot commit, then require their checks.
    repository = os.environ["GITHUB_REPOSITORY"]
    head = run("git", "rev-parse", "HEAD")
    def verify_pr():
        pr = json.loads(run("gh", "api", f"repos/{repository}/pulls/{number}"))
        files = read_pr_files(repository, number)
        validate_results_pr(pr, files, repository, branch, args.base, revision, head)
    for attempt in range(30):
        verify_pr()
        candidates = json.loads(run("gh", "api", f"repos/{repository}/actions/runs?event=pull_request&head_sha={head}&per_page=100"))
        selected = select_pr_runs(candidates["workflow_runs"], int(number), head, repository)
        if set(selected) == WORKFLOWS:
            break
        time.sleep(10)
    else:
        raise RuntimeError("Real PR validation workflows did not start")
    for item in selected.values():
        verify_pr()
        if item["conclusion"] == "action_required":
            run("gh", "api", "--method", "POST", f"repos/{repository}/actions/runs/{item['id']}/approve")
            for attempt in range(30):
                current = json.loads(run("gh", "api", f"repos/{repository}/actions/runs/{item['id']}"))
                if current["conclusion"] != "action_required":
                    break
                time.sleep(2)
            else:
                raise RuntimeError("PR workflow approval was not acknowledged")
    for validation in (str(item["id"]) for item in selected.values()):
        subprocess.run(["gh", "run", "watch", validation, "--exit-status", "--interval", "20"], check=True)
    verify_pr()
    for attempt in range(30):
        checks = json.loads(run("gh", "pr", "view", number, "--json", "statusCheckRollup", "--jq", ".statusCheckRollup"))
        passed = {item.get("name") for item in checks if (item.get("conclusion") or "").upper() == "SUCCESS"}
        if REQUIRED_CHECKS <= passed:
            break
        time.sleep(10)
    else:
        raise RuntimeError("Successful required checks are missing from the PR")
    run("git", "fetch", "origin", args.base)
    if run("git", "rev-parse", f"origin/{args.base}") != revision:
        print("Source changed during publication checks; leave results PR unmerged")
        return
    if run("gh", "pr", "view", number, "--json", "headRefOid", "--jq", ".headRefOid") != run("git", "rev-parse", "HEAD"):
        raise RuntimeError("Results PR changed during validation")
    run("gh", "pr", "merge", number, "--squash", "--delete-branch", "--match-head-commit", run("git", "rev-parse", "HEAD"))
    print(f"Performance PR #{number} merged after its required checks passed")


if __name__ == "__main__":
    main()
