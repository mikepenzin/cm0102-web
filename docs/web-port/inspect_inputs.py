#!/usr/bin/env python3
"""Record local artifact identity and compare the official patch without running it."""
import argparse
import ast
import csv
import hashlib
import json
import pathlib
import re
import struct
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "docs/web-port/evidence"


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def identity(path):
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": digest(path)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--original", type=pathlib.Path, required=True)
    parser.add_argument("--download-original", type=pathlib.Path, required=True)
    parser.add_argument("--patch", type=pathlib.Path, required=True)
    parser.add_argument("--patch-payload", type=pathlib.Path, required=True)
    parser.add_argument("--manual", type=pathlib.Path, required=True)
    parser.add_argument("--generated", type=pathlib.Path, required=True)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    constants = {}
    tree = ast.parse((ROOT / "tools/clean_files/make_clean_files.py").read_text())
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id in {"PATCH_FILES", "PATCH_SIZE", "PATCH_SHA256"}:
                    constants[target.id] = ast.literal_eval(node.value)
    manifest = {}
    for line in (ROOT / "tools/clean_files/manifest.tsv").read_text().splitlines():
        if line and not line.startswith("#"):
            sha, size, path = line.split("\t")
            manifest[path] = {"sha256": sha, "bytes": int(size)}
    original, patch = identity(args.original), identity(args.patch)
    original["matches_clean_3968"] = all(original[k] == v for k, v in manifest["cm0102.exe"].items())
    patch["matches_official_3968"] = patch["sha256"] == constants["PATCH_SHA256"] and patch["bytes"] == constants["PATCH_SIZE"]
    if not patch["matches_official_3968"]:
        raise ValueError("Patch does not match repository identity; cannot apply known payload layout")
    payload_rows = []
    with args.patch_payload.open("rb") as stream:
        sizes = struct.unpack("<46I", stream.read(184))
        if sum(sizes) + 184 != args.patch_payload.stat().st_size:
            raise ValueError("Patch payload extent mismatch")
        for (folder, name), size in zip(constants["PATCH_FILES"], sizes):
            offset = stream.tell()
            raw = stream.read(size)
            path = str(pathlib.PurePosixPath(folder) / name)
            sha = hashlib.sha256(raw).hexdigest()
            expected = manifest.get(path)
            payload_rows.append({"path": path, "payload_offset": offset, "bytes": size,
                                 "sha256": sha, "matches_clean_manifest": expected == {"sha256": sha, "bytes": size}})
    with (ROOT / "docs/research/function-map/names.tsv").open() as stream:
        names = {int(row["address"], 16): row for row in csv.DictReader(stream, delimiter="\t")}
    function_rows, generated_files = [], []
    for path in sorted((args.generated / "gen").glob("fn_*.c")):
        generated_files.append(identity(path))
        current = None
        for line_num, line in enumerate(path.open(), 1):
            match = re.match(r"void ([fo])_([0-9a-f]{8})\(void\)", line)
            if match:
                if current:
                    current["end_line"] = line_num - 1
                    function_rows.append(current)
                address = int(match[2], 16)
                hint = names.get(address, {})
                current = {"address": hex(address), "symbol": match[0][5:-6],
                           "name_hint": hint.get("name", ""), "module_hint": hint.get("file", ""),
                           "path": str(path), "start_line": line_num, "end_line": 0,
                           "semantic_review": "not_established_by_translation"}
        if current:
            current["end_line"] = line_num
            function_rows.append(current)
    with (OUT / "generated-function-index.tsv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, list(function_rows[0]), delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(function_rows)
    report = json.loads((args.generated / "report.json").read_text())
    (OUT / "recompiler-report.json").write_text(json.dumps(report, indent=2) + "\n")
    result = {"original": original, "download_original": identity(args.download_original),
              "patch": patch, "manual": identity(args.manual),
              "patch_payload": identity(args.patch_payload), "patch_members": payload_rows,
              "patch_members_all_match": all(row["matches_clean_manifest"] for row in payload_rows),
              "generated_functions_indexed": len(function_rows), "generated_files": generated_files,
              "limits": ["Static file inspection; patch installer and game were not executed",
                         "Generated C is mechanical translation, not proof of semantic correctness",
                         "Runtime enhancements require the separate Bosman source wrappers",
                         "Original addresses apply only to the matched executable"]}
    (OUT / "artifact-audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"original_matches": original["matches_clean_3968"],
                      "patch_matches": patch["matches_official_3968"],
                      "payload_members": len(payload_rows), "all_members_match": result["patch_members_all_match"],
                      "generated_functions": len(function_rows)}, indent=2))


if __name__ == "__main__":
    main()
