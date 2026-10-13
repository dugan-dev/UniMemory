"""Render matched native/wrapper charts from retained measurements, without dependencies."""
import argparse
import html
import json
import math
from pathlib import Path
import statistics

ROOT = Path(__file__).resolve().parents[2]
BACKENDS = ("standard", "mimalloc", "jemalloc")
SCENARIOS = ("unique", "vector", "owned")
COLORS = ("#94a3b8", "#334155", "#71cdb6", "#087f68", "#b9a3ed", "#7047b9")
SERIES = ("std", "std-unim", "mi", "mi-unim", "je", "je-unim")
ENGLISH = {
    "title": "Native and UniMemory",
    "subtitle": "Native std = 1; lower is faster. Labels show median ns/operation.",
    "unique": "Object, 64 B",
    "vector": "Vector, 256 uint64_t values",
    "owned": "Block, 64 B",
    "footer": "10 paired trials; statistics off; baseline before the Global interface update",
}


def validate(data):
    if data.get("schema") != 1 or data.get("aggregation") != "median" or data.get("statistics") != "OFF" or data.get("trials") != 10:
        raise ValueError("Expected ten-trial median measurements with statistics disabled")
    if set(data["platforms"]) != {"windows", "linux", "macos"}:
        raise ValueError("All three platforms are required")
    for platform in data["platforms"].values():
        if set(platform["backends"]) != set(BACKENDS):
            raise ValueError("All three backends are required")
        for backend in platform["backends"].values():
            for scenario in SCENARIOS:
                row = backend["scenarios"][scenario]
                for side in ("native", "unimemory"):
                    samples = row["samples_ns"][side]
                    if len(samples) != 10 or any(not math.isfinite(value) or value <= 0 for value in samples):
                        raise ValueError("Invalid retained timing samples")
                    if not math.isclose(statistics.median(samples), row[side + "_ns"], rel_tol=1e-10):
                        raise ValueError("Chart median differs from retained samples")


def render(data, platform, labels):
    selected = data["platforms"][platform]
    name = {"windows": "Windows", "linux": "Linux", "macos": "macOS"}[platform]
    elements = ['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="470" viewBox="0 0 1200 470" role="img">']
    elements.append(f'<title>{name}: {html.escape(labels["title"])}</title>')
    elements.append(f'<desc>{html.escape(labels["subtitle"])} Source: {data["source_revision"]}; {data["measured_at"]}.</desc>')
    elements.append('<rect width="1200" height="470" fill="#ffffff"/>')

    def text(x, y, value, size=15, color="#475569", anchor="start", weight="normal"):
        elements.append(f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" text-anchor="{anchor}" font-weight="{weight}" font-family="Segoe UI,Microsoft YaHei,Arial,sans-serif">{html.escape(str(value))}</text>')

    text(36, 43, f'{name}  |  {labels["title"]}', 27, "#0f172a", weight="600")
    text(36, 73, labels["subtitle"], 16)
    for index, label in enumerate(SERIES):
        x = 42 + index * 188
        elements.append(f'<rect x="{x}" y="99" width="19" height="13" rx="3" fill="{COLORS[index]}"/>')
        text(x + 27, 112, label, 16, "#334155")
    values = {}
    for scenario in SCENARIOS:
        values[scenario] = [selected["backends"][backend]["scenarios"][scenario][side + "_ns"]
                            for backend in BACKENDS for side in ("native", "unimemory")]
    upper = math.ceil(max(value / group[0] for group in values.values() for value in group) * 2) / 2
    upper = max(1.5, upper)
    bottom, height = 344, 180
    for panel, scenario in enumerate(SCENARIOS):
        left = 70 + panel * 382
        group = values[scenario]
        for tick in range(int(upper * 2) + 1):
            value = tick / 2
            y = bottom - height * value / upper
            color = "#94a3b8" if value == 1 else "#e2e8f0"
            dash = ' stroke-dasharray="4 4"' if value == 1 else ""
            elements.append(f'<line x1="{left - 9}" y1="{y:.2f}" x2="{left + 284}" y2="{y:.2f}" stroke="{color}"{dash}/>')
            if panel == 0:
                text(left - 17, y + 5, f"{value:g}", 13, anchor="end")
        for index, value in enumerate(group):
            bar_height = height * value / group[0] / upper
            x = left + index * 46
            y = bottom - bar_height
            elements.append(f'<rect x="{x}" y="{y:.2f}" width="32" height="{bar_height:.2f}" rx="3" fill="{COLORS[index]}"><title>{SERIES[index]}: {value:.2f} ns</title></rect>')
            text(x + 16, y - 8, f"{value:.1f}", 13, "#334155", "middle")
        text(left + 132, 379, labels[scenario], 17, "#0f172a", "middle", "600")
    text(36, 423, f'{selected["compiler"]}  |  {data["measured_at"]}  |  {data["source_revision"][:7]}', 14)
    text(36, 449, labels["footer"], 14)
    elements.append("</svg>")
    return "\n".join(elements) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT / "docs/results/native-comparison.json")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/images/native-comparison")
    args = parser.parse_args()
    data = json.loads(args.input.read_text(encoding="utf-8"))
    validate(data)
    catalog = json.loads((ROOT / "tools/ci/locales/zh-CN.json").read_text(encoding="utf-8"))
    chinese = {key: catalog[value] for key, value in ENGLISH.items()}
    args.output.mkdir(parents=True, exist_ok=True)
    for platform in ("windows", "linux", "macos"):
        for suffix, labels in (("", ENGLISH), (".zh-CN", chinese)):
            (args.output / f"{platform}{suffix}.svg").write_bytes(render(data, platform, labels).encode("utf-8"))
    print("Rendered six bilingual figures from 540 retained measurements")


if __name__ == "__main__":
    main()
