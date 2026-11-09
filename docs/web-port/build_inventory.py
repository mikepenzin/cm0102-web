#!/usr/bin/env python3
"""Build a bounded source inventory, not a claim that gameplay is understood."""
import collections
import csv
import hashlib
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "docs/web-port/evidence"


def rows(path):
    with (ROOT / path).open(newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def write_tsv(path, records, columns):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, columns, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(records)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    names = rows("docs/research/function-map/names.tsv")
    placements = rows("docs/research/function-map/function-files.tsv")
    globals_ = rows("docs/research/data-map/globals.tsv")
    probe_file = OUT / "probed-functions.json"
    probed = {r["address"]: r for r in json.loads(probe_file.read_text())["functions"]} if probe_file.exists() else {}
    by_address = {r["address"]: r for r in names}
    catalog = []
    groups = collections.defaultdict(collections.Counter)
    for row in placements:
        name = by_address.get(row["address"], {})
        catalog.append({
            "address": row["address"], "category": row["category"],
            "module": row["file"], "name_hint": name.get("name", ""),
            "inherited_confidence": name.get("confidence", "unnamed"),
            "placement_method": row["method"], "instructions": row["instructions"],
            "inherited_references": name.get("cited_in", ""),
            "bounded_original_probe": probed.get(row["address"], {}).get("evidence", "none"),
        })
        owner = name.get("file") or ("[" + row["category"] + ": owner unresolved]")
        group = groups[owner]
        group["functions"] += 1
        group[row["category"]] += 1
        group[name.get("confidence", "unnamed")] += 1
    columns = list(catalog[0])
    write_tsv(OUT / "original-functions.tsv", catalog, columns)
    module_rows = [{"module": module, "functions": c["functions"],
                    "high_name_confidence": c["high"], "medium_name_confidence": c["medium"],
                    "low_name_confidence": c["low"], "unnamed": c["unnamed"],
                    "implementation_status": "original-body-review-required"}
                   for module, c in sorted(groups.items())]
    write_tsv(OUT / "original-modules.tsv", module_rows, list(module_rows[0]))
    tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
    manifest = []
    for relative in sorted(filter(None, tracked)):
        if relative.startswith("docs/web-port/"):
            continue
        path = ROOT / relative
        if not path.is_file():
            continue
        raw = path.read_bytes()
        manifest.append({"path": relative, "bytes": len(raw),
                         "sha256": hashlib.sha256(raw).hexdigest()})
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip()
    summary = {
        "repository_commit": commit,
        "scope": "Repository inventory plus bounded probe ledger; artifact/data audits record separately inspected local inputs",
        "function_placements": len(placements), "named_functions": len(names),
        "named_source_modules": len({r["file"] for r in names}),
        "catalog_groups_including_unresolved": len(groups), "global_rows": len(globals_),
        "function_categories": dict(collections.Counter(r["category"] for r in placements)),
        "name_confidence": dict(collections.Counter(r["confidence"] for r in names)),
        "tracked_files_hashed": len(manifest),
        "direct_original_entries_with_bounded_probes": len(probed),
        "full_game_implementation_ready": False,
        "blocking_evidence": ["Complete match/quick-result, growth, finance, AI and negotiation semantic contracts",
                              "All required original caller/field/scheduling dependencies",
                              "Runtime career scenarios and full subsystem differential vectors",
                              "Complete legacy-save member decoding and season-pack coverage"],
    }
    (OUT / "source-manifest.json").write_text(json.dumps({"commit": commit, "files": manifest}, indent=2) + "\n")
    (OUT / "coverage-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
