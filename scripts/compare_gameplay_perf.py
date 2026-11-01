#!/usr/bin/env python3
# SpeedBreaker. GPL-3.0-or-later (see COPYING).
"""Compare explicit race-only windows from two local session logs.

The existing [perf] rows each cover 120 rendered frames. These are aggregate
samples, not individual frame times; this report cannot calculate 1% lows.
No launch, input, save changes or network access is performed.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import statistics


PERF = re.compile(
    r"^\[\s*(?P<seconds>[\d.]+)\] \[perf\] (?P<fps>[\d.]+) fps "
    r"\(worst frame (?P<worst_ms>[\d.]+) ms\) \| CP busy (?P<cp_ms>[\d.]+) ms/frame"
)


def summarize(path, window):
    start, end = window
    if not (math.isfinite(start) and math.isfinite(end) and 0 <= start < end):
        raise ValueError("window must have 0 <= START < END, in session-log seconds")
    raw = path.read_bytes()
    samples = []
    for line in raw.decode("utf-8", errors="replace").splitlines():
        match = PERF.match(line)
        if not match:
            continue
        sample = {key: float(value) for key, value in match.groupdict().items()}
        if sample["fps"] <= 0:
            continue
        # Exclude a reporting window that straddles the chosen race boundary.
        duration = 120.0 / sample["fps"]
        if sample["seconds"] - duration < start or sample["seconds"] > end:
            continue
        sample["duration_s"] = duration
        thermal = re.search(r"\| thermal (\w+)", line)
        sample["thermal"] = thermal[1] if thermal else "unreported"
        samples.append(sample)
    if not samples:
        raise ValueError(f"{path}: no complete 120-frame reporting windows in {start}..{end}")
    duration = sum(s["duration_s"] for s in samples)
    return {
        "log": str(path.resolve()),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "requested_window_s": list(window),
        "included_sample_ends_s": [samples[0]["seconds"], samples[-1]["seconds"]],
        "samples_120_frames": len(samples),
        "estimated_rendered_seconds": duration,
        # Equal frame counts: total frames / total time, not an arithmetic
        # average of FPS (which overweights faster reporting windows).
        "estimated_fps": 120.0 * len(samples) / duration,
        "median_window_fps": statistics.median(s["fps"] for s in samples),
        "minimum_window_fps": min(s["fps"] for s in samples),
        "maximum_frame_ms": max(s["worst_ms"] for s in samples),
        "mean_cp_ms_per_frame": statistics.mean(s["cp_ms"] for s in samples),
        "windows_with_frame_over_50ms": sum(s["worst_ms"] > 50 for s in samples),
        "thermal_states": sorted({s["thermal"] for s in samples}),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("modified", type=Path)
    parser.add_argument("--baseline-window", nargs=2, type=float, required=True, metavar=("START", "END"))
    parser.add_argument("--modified-window", nargs=2, type=float, required=True, metavar=("START", "END"))
    parser.add_argument("--json", action="store_true", help="emit a local machine-readable report")
    args = parser.parse_args()
    try:
        base = summarize(args.baseline, args.baseline_window)
        mod = summarize(args.modified, args.modified_window)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    result = {
        "baseline": base,
        "modified": mod,
        "fps_change_percent": 100 * (mod["estimated_fps"] / base["estimated_fps"] - 1),
        "limitations": "120-frame aggregates; no 1% lows. Match race, car, graphics, warm caches, "
                       "power and thermal conditions; exclude loading, pauses, menus and inspection. "
                       "These logs alone cannot establish equivalent conditions or identify a cause.",
    }
    if args.json:
        print(json.dumps(result, indent=2))
        return
    for label, run in (("Baseline", base), ("Modified", mod)):
        print(f'{label}: {run["estimated_fps"]:.2f} estimated fps; '
              f'{run["maximum_frame_ms"]:.1f} ms maximum frame; '
              f'{run["mean_cp_ms_per_frame"]:.2f} ms/frame command processor; '
              f'{run["samples_120_frames"]} windows; thermal {", ".join(run["thermal_states"])}')
    print(f'FPS change: {result["fps_change_percent"]:+.2f}%')
    print(result["limitations"])


if __name__ == "__main__":
    main()
