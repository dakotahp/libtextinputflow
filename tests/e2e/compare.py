#!/usr/bin/env python3
import json
import sys


def load_expected(path):
    with open(path) as f:
        return [json.loads(line) for line in f if line.strip()]


def load_actual(path):
    rows = []
    with open(path) as f:
        for line in f:
            if not line.startswith("{"):
                continue
            row = json.loads(line)
            result = row["result"]
            if "error" in result:
                rows.append({"toolkit": row["toolkit"], "widget": row["widget"], "step": row["step"],
                             "error": result["error"]})
                continue
            rows.append({
                "toolkit": row["toolkit"],
                "widget": row["widget"],
                "step": row["step"],
                "caret": result["caret"],
                "selection": [list(r) for r in result["selection"]],
                "text": result["text"],
            })
    return rows


def describe(row):
    if row is None:
        return "nothing"
    if "error" in row:
        return f"error: {row['error']}"
    selection = ",".join(f"{start}-{end}" for start, end in row["selection"]) or "-"
    return f"step {row['step']!r} caret {row['caret']} sel {selection} text {row['text']!r}"


def main():
    expected = load_expected(sys.argv[1])
    try:
        actual = load_actual(sys.argv[2])
    except FileNotFoundError:
        print(f"FAIL: no results file at {sys.argv[2]}; the harness did not start")
        sys.exit(1)
    mismatches = 0
    counts = {}
    for i in range(max(len(expected), len(actual))):
        want = expected[i] if i < len(expected) else None
        got = actual[i] if i < len(actual) else None
        where = want or got
        case = f"{where['toolkit']}-{where['widget']}"
        counts[case] = counts.get(case, 0) + 1
        if want != got:
            mismatches += 1
            print(f"MISMATCH {case} step {counts[case]}:")
            print(f"  expected {describe(want)}")
            print(f"  actual   {describe(got)}")
    if len(actual) < len(expected):
        print(f"results end after {len(actual)} of {len(expected)} rows; the harness may have timed out")
    if mismatches:
        print(f"FAIL: {mismatches} of {len(expected)} results differ")
        sys.exit(1)
    print(f"ok: all {len(expected)} results match")


if __name__ == "__main__":
    main()
