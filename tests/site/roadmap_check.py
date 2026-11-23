"""Site roadmap check (issue 138): Done shows only for issues closed as completed.
Issues closed as not planned stay off the list. Runs the roadmap filter from site/index.html in node."""
import json, re, shutil, subprocess, sys, pathlib

if not shutil.which("node"):
    sys.exit("FAIL: node is not on the PATH")

html = (pathlib.Path(__file__).resolve().parents[2] / "site" / "index.html").read_text()
m = re.search(r"// roadmap-filter-begin\n(.*?)// roadmap-filter-end", html, re.S)
if not m:
    sys.exit("FAIL: roadmap filter block not found in site/index.html")

issues = [
    {"number": 1, "title": "open", "state": "open", "state_reason": None},
    {"number": 2, "title": "reopened", "state": "open", "state_reason": "reopened"},
    {"number": 3, "title": "done", "state": "closed", "state_reason": "completed"},
    {"number": 4, "title": "not planned", "state": "closed", "state_reason": "not_planned"},
    {"number": 5, "title": "duplicate", "state": "closed", "state_reason": "duplicate"},
    {"number": 6, "title": "pr", "state": "closed", "state_reason": "completed", "pull_request": {}},
]
js = m.group(1) + "\nconsole.log(JSON.stringify(roadmapIssues(%s).map(function(i){return [i.number, roadmapDone(i)];})));" % json.dumps(issues)
out = subprocess.run(["node", "-e", js], capture_output=True, text=True)
if out.returncode:
    sys.exit("FAIL: " + out.stderr)
got = json.loads(out.stdout)
want = [[1, False], [2, False], [3, True]]
if got != want:
    sys.exit("FAIL: got %s want %s" % (got, want))
print("roadmap check ok")
