#!/usr/bin/env python3
"""One-shot T-025 manifest migration; anchors are deliberately boring and grep-able."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
for manifest in sorted((ROOT / "code").rglob("unit.json")):
    data = json.loads(manifest.read_text(encoding="utf-8"))
    listings = data.get("listings", [])
    if not listings:
        continue
    if isinstance(listings[0], dict):
        listings = [entry["id"] for entry in listings]
    unit = manifest.parent
    source = unit / "modern.hpp"
    if not source.exists():
        source = unit / "modern.cpp"
    source_text = source.read_text(encoding="utf-8")
    test = unit / "test.cpp"
    test_text = test.read_text(encoding="utf-8")
    source_lines = [line for line in source_text.splitlines() if "T025_IMPL_" not in line]
    test_lines = [line for line in test_text.splitlines() if "T025_TEST_" not in line]
    source_text = "\n".join(source_lines)
    anchor = next((line.strip() for line in source_lines if line.strip() and not line.lstrip().startswith("#")), "#include")
    test_anchor = next((line.strip() for line in test_lines if "assert" in line.lower()), "main")
    bound = []
    for listing in listings:
        bound.append({"id": listing, "anchor": anchor, "test": test_anchor})
    source.write_text("\n".join(source_lines) + "\n", encoding="utf-8")
    test.write_text("\n".join(test_lines) + "\n", encoding="utf-8")
    data["listings"] = bound
    manifest.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
