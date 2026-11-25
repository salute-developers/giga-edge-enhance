"""Compact GitHub Actions product/coverage summary."""

import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
manifest = ROOT / "out/artifacts/python-release/manifest.json"
lines = ["### GigaEdge Enhance Python SDK", ""]
if manifest.is_file():
    state = json.loads(manifest.read_text())
    lines += [
        f"Platform: {state['platform']}; version: {state['version']}",
        "",
        "| Product | SHA-256 |",
        "| --- | --- |",
    ]
    lines += [f"| `{name}` | `{checksum}` |" for name, checksum in state["artifacts"].items()]
    lines += ["", "Both dependency bundles were validated by recipe/toolchain identity before use."]
else:
    lines.append("No validated product manifest was produced; see build/test diagnostics.")
content = "\n".join(lines) + "\n"
if destination := os.environ.get("GITHUB_STEP_SUMMARY"):
    with Path(destination).open("a") as stream:
        stream.write(content)
else:
    print(content)
