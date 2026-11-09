#!/usr/bin/env python3
"""Assemble the editable chapters and validate local Markdown file links."""
import hashlib
import json
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
CHAPTERS = [
    "01-web-architecture.md", "02-gameplay.md", "03-competitions-and-seasons.md",
    "04-data-ui-and-assets.md", "05-evidence-and-verification.md",
    "06-agent-implementation-plan.md", "07-original-negotiation-evidence.md",
    "08-specification-completion-plan.md",
]


def main():
    intro = """# CM 01/02: Bosman Edition — technical specification and reverse-engineering dossier

Target: Angular frontend and Node.js authoritative backend. Research date: 28 October 2026.
Reference repository: `cf2e8fa5f4cc816f252611a0e9c81e66d088358e`.

**Status:** implementation blueprint and evidence-backed recovery dossier; not yet a complete semantic recovery of every original gameplay routine. Verified rules, documented product behavior, web design choices and unresolved algorithms are distinguished throughout. Full implementation readiness remains false in the coverage ledger until the required gameplay and end-to-end fidelity gates close.

The user selected Bosman Edition including its season selection and enhancements. The supplied official patch and matched original establish version 3.9.68; the installed Bosman host is older than the current source enhancements. The patch installer and live game were not run, and existing saves were not altered.

This is the consolidated reading copy. Edit the numbered source chapters and run `docs/web-port/build_book.py` to regenerate it. See [README](README.md) for artifact identities, reproduction commands and evidence navigation.

## Contents

"""
    body = [intro]
    metadata = []
    for i, name in enumerate(CHAPTERS, 1):
        raw = (HERE / name).read_text()
        title = raw.splitlines()[0].lstrip("# ")
        body.append(f"{i}. [{title}](#chapter-{i:02d})\n")
        metadata.append({"file": name, "sha256": hashlib.sha256(raw.encode()).hexdigest(),
                         "words": len(raw.split()), "lines": len(raw.splitlines())})
    for i, name in enumerate(CHAPTERS, 1):
        raw = (HERE / name).read_text()
        body.append(f'\n\n---\n\n<a id="chapter-{i:02d}"></a>\n\n')
        body.append(raw)
    target = HERE / "CM0102-WEB-TECHNICAL-SPEC.md"
    target.write_text("".join(body))
    errors = []
    for path in [HERE / "README.md", target] + [HERE / n for n in CHAPTERS]:
        for destination in re.findall(r"\]\(([^)]+)\)", path.read_text()):
            destination = destination.strip("<>")
            if destination.startswith(("http:", "https:", "#", "mailto:")):
                continue
            relative = destination.split("#", 1)[0]
            if not (path.parent / relative).exists():
                errors.append({"file": path.name, "missing_link": destination})
    for path in (HERE / "evidence").glob("*.json"):
        json.loads(path.read_text())
    probes = json.loads((HERE / "evidence/probed-functions.json").read_text())
    for row in probes["functions"]:
        if not (HERE / "evidence" / row["evidence"]).exists():
            errors.append({"missing_probe_evidence": row["evidence"]})
    report = {"chapters": metadata, "total_chapter_words": sum(x["words"] for x in metadata),
              "output": target.name, "output_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
              "local_link_errors": errors, "json_files_parse": True,
              "limits": "Structural artifact checks only; not a claim that every source citation or gameplay rule has passed runtime validation"}
    (HERE / "evidence/document-validation.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"words": report["total_chapter_words"], "chapters": len(metadata),
                      "link_errors": errors, "output": str(target)}, indent=2))
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
