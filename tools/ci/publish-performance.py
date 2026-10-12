"""Publish trusted complete results using a scoped bot branch and reviewed PR."""
import argparse
import gzip
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

WORKFLOWS = {"performance.yml"}
REQUIRED_CHECKS = {"performance acceptance"}
GENERATED_DIRECTORIES = ("docs/images/performance/", "docs/results/current/")
GENERATED_FILES = {"README.md", "README.zh-CN.md", "docs/images/workload-comparison.png",
                   "docs/images/memory-retention.png",
                   *[f"docs/performance/{name}{suffix}" for name in ("latency", "memory", "applications")
                     for suffix in (".md", ".zh-CN.md")]}
GENERATED_PATHS = (*GENERATED_DIRECTORIES, *sorted(GENERATED_FILES))


def generated_path(path):
    return path in GENERATED_FILES or path.startswith(GENERATED_DIRECTORIES)


def narrative(content, filename):
    """Ignore generated numbers/source lines while preserving all user prose."""
    if filename in ("README.md", "README.zh-CN.md"):
        heading = "## Performance" if filename == "README.md" else "## \u6027\u80fd"
        begin = content.index(heading + "\n")
        end = content.index("\n## ", begin + len(heading))
        lines = content[begin:end].splitlines()
        for index, line in enumerate(lines):
            if line.startswith("Windows x64 ·"):
                lines[index] = "<measurement environment>"
            elif line.startswith("|"):
                label = line.split("|")[1].strip()
                lines[index] = f"| {label} | <measurements> |"
        return content[:begin] + "\n".join(lines) + content[end:]
    return re.sub(r"(<!-- performance-(?:table:[^>]+|source) -->)\n.*?\n(<!-- /performance-(?:table|source) -->)",
                  r"\1\n<measurements>\n\2", content, flags=re.DOTALL)


def validate_narrative(before, after, filename):
    if narrative(before, filename) != narrative(after, filename):
        raise RuntimeError(f"Publication changed user prose outside performance data: {filename}")


def validate_results_pr(pr, files, repository, branch, base, revision, head):
    if (pr["state"] != "open" or pr["user"]["login"] != "github-actions[bot]" or
            pr["head"]["repo"]["full_name"] != repository or pr["base"]["repo"]["full_name"] != repository or
            pr["head"]["ref"] != branch or pr["head"]["sha"] != head or
            pr["base"]["ref"] != base or pr["base"]["sha"] != revision):
        raise RuntimeError("Results PR identity or source revision changed")
    if len(files) != pr["changed_files"] or not files or any(not generated_path(item["filename"])
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


def compress_results(directory):
    """Keep every raw observation in deterministic, lossless gzip files."""
    for path in sorted(directory.rglob("*")):
        if path.is_file() and (path.suffix == ".csv" or path.name in (
                "summary.json", "controlled-suite.json", "tail-traces.json",
                "calibration-pilots.json", "full-calibration.json", "measurement-failures.json")):
            original = path.read_bytes()
            compressed = gzip.compress(original, compresslevel=9, mtime=0)
            if gzip.decompress(compressed) != original:
                raise RuntimeError("Result compression failed round-trip verification")
            path.with_name(path.name + ".gz").write_bytes(compressed)
            path.unlink()


def run(*command):
    return subprocess.check_output(command, text=True).strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("artifacts", type=Path)
    parser.add_argument("base", choices=("dev", "main"))
    parser.add_argument("--native-input", type=Path)
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
    def load_refresh(name):
        spec = importlib.util.spec_from_file_location(name.replace("-", "_"), root / "tools/ci" / (name + ".py"))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    overview = load_refresh("refresh-performance-overview")
    overview.stage_artifacts(root, args.artifacts, revision, expected_run_id=os.environ["GITHUB_RUN_ID"])
    load_refresh("refresh-performance-tables").refresh_tables(root, args.native_input)
    compress_results(root / "docs/results/current")
    report.refresh_reports(root / "docs/results/current", root / "docs/images/performance", revision)
    expected = overview.refresh_overview(root, root / "build/publication-overview.json", revision)
    rendered = args.artifacts / "performance-overview"
    actual = json.loads((rendered / "comparison.json").read_text(encoding="utf-8"))
    if actual != expected:
        raise RuntimeError("Rendered PNG figures use different measurements")
    for name in ("workload-comparison.png", "memory-retention.png"):
        source = rendered / name
        if not source.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"):
            raise RuntimeError("Invalid rendered PNG figure")
        shutil.copyfile(source, root / "docs/images" / name)
    allowed = GENERATED_PATHS
    changed = subprocess.check_output(["git", "status", "--porcelain=v1", "-z"], text=True).split("\0")
    if any(line and not generated_path(line[3:]) for line in changed):
        raise RuntimeError("Publication attempted changes outside generated paths")
    for item in changed:
        if item and item[3:] in GENERATED_FILES and item[3:].endswith(".md"):
            filename = item[3:]
            before = run("git", "show", f"{revision}:{filename}")
            after = (root / filename).read_text(encoding="utf-8").strip()
            validate_narrative(before, after, filename)
    run("git", "config", "user.name", "github-actions[bot]")
    run("git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com")
    run("git", "add", "--", *allowed)
    run("git", "commit", "-m", f"Update performance results for {revision[:12]}")
    run("git", "push", f"--force-with-lease=refs/heads/{branch}:{lease}", "origin", f"HEAD:refs/heads/{branch}")
    existing = json.loads(run("gh", "pr", "list", "--head", branch, "--base", args.base, "--json", "number"))
    body = f"Generated from trusted source {revision}. All measurements and report validation passed. Every process-trial observation and statistical summary is retained losslessly in docs/results/current. Individual operation traces and build evidence remain in workflow artifacts for 90 days; traces were validated before publication.\n\nRun: https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}\n"
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
