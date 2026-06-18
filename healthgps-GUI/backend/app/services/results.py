"""Group HealthGPS output files by run timestamp."""

from __future__ import annotations

import json
import os
import re
from pathlib import Path

RUN_STAMP_RE = re.compile(
    r"HealthGPS_Result_(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})(?:\.\d+)?"
)


def expand_output_folder(folder: str) -> Path:
    expanded = folder.replace("${HOME}", str(Path.home()))
    return Path(os.path.expandvars(expanded)).resolve()


def engine_output_folder(folder: str) -> Path | None:
    """Path HealthGPS uses when ${HOME} is unset (common on Windows)."""
    if "${HOME}" not in folder:
        return None
    suffix = folder.split("${HOME}", 1)[1].lstrip("/\\")
    if not suffix:
        return None
    # C++ expand leaves a leading slash -> /healthgps/... -> C:\healthgps\... on Windows
    return Path(f"/{suffix}").resolve()


def normalize_output_folder(folder: str) -> str:
    """Rewrite config output.folder to an absolute path the engine can write reliably."""
    if not folder:
        return folder
    if "${HOME}" in folder:
        return str(expand_output_folder(folder))
    return str(Path(os.path.expandvars(folder)).resolve())


def discover_results_dirs(config_path: Path, config: dict) -> list[Path]:
    """Candidate directories where HealthGPS may write output files."""
    candidates: list[Path] = []
    folder = config.get("output", {}).get("folder", "")
    if folder:
        if not os.path.isabs(folder.replace("${HOME}", "")):
            candidates.append((config_path.parent / folder).resolve())
        candidates.append(expand_output_folder(folder))
        engine_path = engine_output_folder(folder)
        if engine_path is not None:
            candidates.append(engine_path)

    parent = config_path.parent.resolve()
    candidates.extend(
        [
            parent,
            parent / "results",
            parent / "output",
        ]
    )

    root_path = config.get("config", {}).get("root_path") or config.get("root_path")
    if root_path:
        rp = Path(str(root_path)).resolve()
        candidates.extend([rp, rp / "results", rp / "output"])
        if folder:
            candidates.append((rp / Path(folder.replace("${HOME}", "").lstrip("/\\"))).resolve())

    seen: set[Path] = set()
    unique: list[Path] = []
    for path in candidates:
        try:
            resolved = path.resolve()
        except OSError:
            continue
        if resolved not in seen:
            seen.add(resolved)
            unique.append(resolved)
    return unique


def run_stamp_from_name(filename: str) -> str | None:
    match = RUN_STAMP_RE.search(filename)
    return match.group(1) if match else None


def latest_run_files(results_dirs: list[Path]) -> tuple[str | None, list[dict], Path | None]:
    """Return newest run timestamp, existing files only, and the dir they live in."""
    by_stamp: dict[str, list[Path]] = {}

    for results_dir in results_dirs:
        if not results_dir.is_dir():
            continue
        for path in results_dir.iterdir():
            if not path.is_file():
                continue
            stamp = run_stamp_from_name(path.name)
            if stamp is None:
                continue
            by_stamp.setdefault(stamp, []).append(path)

    if not by_stamp:
        primary = results_dirs[0] if results_dirs else None
        return None, [], primary

    latest_stamp = max(by_stamp.keys())
    files = sorted(by_stamp[latest_stamp], key=lambda p: p.name)
    primary_dir = files[0].parent if files else None
    return latest_stamp, [
        {
            "name": path.name,
            "path": str(path.resolve()),
            "size_bytes": path.stat().st_size,
            "exists": path.is_file(),
        }
        for path in files
        if path.is_file()
    ], primary_dir


SOURCE_COLORS = {
    "Baseline": "#64748b",
    "Intervention": "#b91c3c",
}


def _avg_male_female(block: dict | None) -> float | None:
    if not isinstance(block, dict):
        return None
    vals = [
        float(block[k])
        for k in ("male", "female")
        if isinstance(block.get(k), (int, float))
    ]
    return sum(vals) / len(vals) if vals else None


def _series_by_source(
    rows: list[dict],
    *,
    year_key: str = "time",
    value_fn,
) -> list[dict]:
    by_source: dict[str, list[dict]] = {}
    for row in rows:
        if not isinstance(row, dict):
            continue
        source = str(row.get("source", "Unknown"))
        year = row.get(year_key)
        if not isinstance(year, (int, float)):
            continue
        value = value_fn(row)
        if value is None:
            continue
        by_source.setdefault(source, []).append({"x": float(year), "y": float(value)})

    out: list[dict] = []
    for source in sorted(by_source.keys()):
        points = sorted(by_source[source], key=lambda p: p["x"])
        if len(points) < 2:
            continue
        out.append(
            {
                "name": source,
                "color": SOURCE_COLORS.get(source, "#334155"),
                "points": points,
            }
        )
    return out


def _chart(
    chart_id: str,
    title: str,
    y_label: str,
    rows: list[dict],
    value_fn,
) -> dict | None:
    series = _series_by_source(rows, value_fn=value_fn)
    if not series:
        return None
    return {
        "id": chart_id,
        "title": title,
        "x_label": "Year",
        "y_label": y_label,
        "series": series,
    }


def parse_healthgps_result_charts(data: dict) -> dict:
    """Build dashboard charts from HealthGPS aggregate result JSON."""
    rows = data.get("result")
    if not isinstance(rows, list) or not rows:
        return {"charts": [], "experiment": {}, "years": []}

    experiment = data.get("experiment") if isinstance(data.get("experiment"), dict) else {}
    years = sorted(
        {
            int(r["time"])
            for r in rows
            if isinstance(r, dict) and isinstance(r.get("time"), (int, float))
        }
    )

    defs = [
        (
            "daly",
            "DALY",
            "DALY",
            lambda r: (r.get("indicators") or {}).get("DALY"),
        ),
        (
            "yll",
            "Years of life lost (YLL)",
            "YLL",
            lambda r: (r.get("indicators") or {}).get("YLL"),
        ),
        (
            "population",
            "Population alive",
            "People",
            lambda r: (r.get("population") or {}).get("alive"),
        ),
        (
            "deaths",
            "Cumulative deaths",
            "Deaths",
            lambda r: (r.get("population") or {}).get("dead"),
        ),
        (
            "diabetes",
            "Diabetes prevalence",
            "%",
            lambda r: _avg_male_female((r.get("disease_prevalence") or {}).get("diabetes")),
        ),
        (
            "bmi",
            "Average BMI",
            "BMI",
            lambda r: _avg_male_female((r.get("risk_factors_average") or {}).get("BMI")),
        ),
        (
            "physical_activity",
            "Physical activity",
            "Index",
            lambda r: _avg_male_female(
                (r.get("risk_factors_average") or {}).get("PhysicalActivity")
            ),
        ),
    ]

    charts: list[dict] = []
    for chart_id, title, y_label, fn in defs:
        chart = _chart(chart_id, title, y_label, rows, fn)
        if chart is not None:
            charts.append(chart)

    return {"charts": charts, "experiment": experiment, "years": years}


def load_result_chart_series(config_path: Path, config: dict) -> dict:
    """Extract aggregate time-series charts from the latest result JSON."""
    dirs = discover_results_dirs(config_path, config)
    _stamp, files, found_dir = latest_run_files(dirs)
    json_files = [
        f
        for f in files
        if f["name"].endswith(".json")
        and "_HighIncome" not in f["name"]
        and "_LowIncome" not in f["name"]
        and "_Individual" not in f["name"]
    ]
    if not json_files:
        return {
            "charts": [],
            "experiment": {},
            "years": [],
            "results_dir": str(found_dir) if found_dir else "",
            "message": "No result JSON found",
        }

    main = Path(json_files[0]["path"])
    if not main.is_file():
        return {
            "charts": [],
            "experiment": {},
            "years": [],
            "results_dir": str(found_dir or ""),
            "message": "Result file missing on disk",
        }

    try:
        with main.open(encoding="utf-8") as f:
            data = json.load(f)
    except (json.JSONDecodeError, OSError) as exc:
        return {
            "charts": [],
            "experiment": {},
            "years": [],
            "results_dir": str(found_dir or ""),
            "message": str(exc),
        }

    if not isinstance(data, dict):
        return {
            "charts": [],
            "experiment": {},
            "years": [],
            "results_dir": str(found_dir or main.parent),
            "message": "Unexpected result JSON shape",
        }

    parsed = parse_healthgps_result_charts(data)
    charts = parsed["charts"]
    return {
        **parsed,
        "results_dir": str(found_dir or main.parent),
        "result_file": main.name,
        "message": None if charts else "Result file has no plottable aggregate series",
    }
