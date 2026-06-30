"""Build visualization payloads for HealthGPS Studio dashboards."""

from __future__ import annotations

import csv
import json
import re
from pathlib import Path
from typing import Any, Callable

from app.services.results import (
    SOURCE_COLORS,
    _avg_male_female,
    _chart,
    discover_results_dirs,
    latest_run_files,
    normalize_result_rows,
    parse_healthgps_result_charts,
)
from app.services.result_explorer import (
    TIME_AXIS,
    chart_for_axes,
    extract_result_variables,
    series_for_variable,
)
from app.services.workspace import active_config_path, get_workspace
from app.services.pipeline_progress import build_pipeline_modules as build_pipeline_graph
from app.services.run_analytics import get_run_telemetry

INCOME_STRATA_RE = re.compile(
    r"_(LowIncome|LowerMiddleIncome|MiddleIncome|UpperMiddleIncome|HighIncome)\.csv$",
    re.I,
)


def _rows_for_source(rows: list[dict], source: str) -> list[dict]:
    return [r for r in rows if isinstance(r, dict) and r.get("source") == source]


def _last_year_row(rows: list[dict], source: str) -> dict | None:
    matched = _rows_for_source(rows, source)
    if not matched:
        return None
    return max(matched, key=lambda r: int(r.get("time", 0)))


def _pct_delta(baseline: float | None, intervention: float | None) -> float | None:
    if baseline is None or intervention is None:
        return None
    if baseline == 0:
        return 0.0 if intervention == 0 else None
    return ((intervention - baseline) / baseline) * 100.0


def _headlines(rows: list[dict], target_year: int | None, intervention: str) -> list[dict]:
    base = _last_year_row(rows, "Baseline")
    inter = _last_year_row(rows, "Intervention")
    if not base or not inter:
        return []

    year = int(inter.get("time", target_year or 0))
    policy = intervention or "policy"

    items: list[tuple[str, str, Callable, str]] = [
        (
            "daly",
            "Δ DALYs",
            lambda r: (r.get("indicators") or {}).get("DALY"),
            "",
        ),
        (
            "diabetes",
            "Δ diabetes prevalence",
            lambda r: _avg_male_female((r.get("disease_prevalence") or {}).get("diabetes")),
            "pp",
        ),
        (
            "bmi",
            "Δ mean BMI",
            lambda r: _avg_male_female((r.get("risk_factors_average") or {}).get("BMI")),
            "",
        ),
        (
            "obesity",
            "Δ above healthy weight",
            lambda r: _avg_male_female((r.get("risk_factors_average") or {}).get("Weight")),
            "",
        ),
    ]

    out: list[dict] = []
    for hid, label, fn, unit in items:
        b = fn(base)
        i = fn(inter)
        if b is None or i is None:
            continue
        delta = float(i) - float(b)
        pct = _pct_delta(float(b), float(i))
        direction = "reduced" if delta < 0 else "increased" if delta > 0 else "unchanged"
        if hid == "daly":
            headline = (
                f"{policy} {direction} DALYs by {abs(delta):,.0f} at {year}"
                if delta != 0
                else f"DALYs unchanged at {year}"
            )
        elif hid == "diabetes":
            headline = (
                f"{policy} {direction} diabetes prevalence by {abs(delta):.2f} pp at {year}"
            )
        else:
            headline = f"{policy} {direction} {label.replace('Δ ', '')} by {abs(delta):.2f} at {year}"

        out.append(
            {
                "id": hid,
                "label": label,
                "baseline": float(b),
                "intervention": float(i),
                "delta": delta,
                "delta_pct": pct,
                "unit": unit,
                "year": year,
                "headline": headline,
            }
        )
    return out


def _burden_bars(rows: list[dict]) -> list[dict]:
    base = _last_year_row(rows, "Baseline")
    inter = _last_year_row(rows, "Intervention")
    if not base or not inter:
        return []

    bars = []
    for key, label in (("YLL", "YLL"), ("YLD", "YLD"), ("DALY", "DALY")):
        b = (base.get("indicators") or {}).get(key)
        i = (inter.get("indicators") or {}).get(key)
        if b is None or i is None:
            continue
        b_f, i_f = float(b), float(i)
        bars.append(
            {
                "id": key.lower(),
                "label": label,
                "baseline": b_f,
                "intervention": i_f,
                "delta": i_f - b_f,
            }
        )
    return bars


def _trajectory(
    chart_id: str,
    title: str,
    y_label: str,
    rows: list[dict],
    value_fn,
) -> dict | None:
    chart = _chart(chart_id, title, y_label, rows, value_fn)
    if chart is None:
        return None
    chart["chart_type"] = "trajectory"
    chart["uncertainty_available"] = False
    chart["bands"] = []
    return chart


def _comorbidity_matrix(rows: list[dict]) -> dict | None:
    inter = _last_year_row(rows, "Intervention")
    if not inter:
        return None
    comorb = inter.get("comorbidities")
    if not isinstance(comorb, dict):
        return None

    cells = []
    for level in sorted(comorb.keys(), key=lambda k: int(k) if str(k).isdigit() else k):
        block = comorb[level]
        if not isinstance(block, dict):
            continue
        cells.append(
            {
                "level": str(level),
                "label": f"{level} conditions",
                "male": float(block.get("male", 0)),
                "female": float(block.get("female", 0)),
                "average": _avg_male_female(block) or 0.0,
            }
        )
    if not cells:
        return None
    return {"title": "Comorbidity distribution (intervention, final year)", "cells": cells}


def _parse_income_strata(files: list[dict], outcome: str = "prevalence_diabetes") -> list[dict]:
    """Dumbbell data per income stratum from stratum CSV exports."""
    strata: dict[str, dict[str, float]] = {}
    col = outcome if outcome.startswith("prevalence_") else f"prevalence_{outcome}"

    for f in files:
        name = f.get("name", "")
        match = INCOME_STRATA_RE.search(name)
        if not match:
            continue
        path = Path(f["path"])
        if not path.is_file():
            continue
        stratum = match.group(1)
        by_source: dict[str, list[float]] = {"Baseline": [], "Intervention": []}
        try:
            with path.open(encoding="utf-8", newline="") as fh:
                reader = csv.DictReader(fh)
                if col not in (reader.fieldnames or []):
                    continue
                for row in reader:
                    src = row.get("source", "")
                    if src not in by_source:
                        continue
                    try:
                        by_source[src].append(float(row[col]))
                    except (TypeError, ValueError):
                        continue
        except OSError:
            continue

        entry: dict[str, float] = {}
        for src, vals in by_source.items():
            if vals:
                entry[src.lower()] = sum(vals) / len(vals)
        if entry:
            strata[stratum] = entry

    order = [
        "LowIncome",
        "LowerMiddleIncome",
        "MiddleIncome",
        "UpperMiddleIncome",
        "HighIncome",
    ]
    dumbbells = []
    for s in order:
        if s not in strata:
            continue
        d = strata[s]
        base = d.get("baseline")
        inter = d.get("intervention")
        if base is None or inter is None:
            continue
        dumbbells.append(
            {
                "stratum": s.replace("Income", " income").replace("Middle", " middle"),
                "baseline": base,
                "intervention": inter,
                "delta": inter - base,
            }
        )
    return dumbbells


def _population_pyramid(rows: list[dict]) -> dict | None:
    inter = _last_year_row(rows, "Intervention")
    if not inter:
        return None
    pop = inter.get("population") or {}
    male = pop.get("alive_male")
    female = pop.get("alive_female")
    if male is None or female is None:
        return None
    total = float(male) + float(female)
    return {
        "year": int(inter.get("time", 0)),
        "male": int(male),
        "female": int(female),
        "male_pct": round(100.0 * float(male) / total, 1) if total else 50.0,
        "female_pct": round(100.0 * float(female) / total, 1) if total else 50.0,
    }


def _reproducibility(experiment: dict, config: dict) -> dict:
    running = config.get("running", {})
    seed = experiment.get("custom_seed") or running.get("seed")
    return {
        "model": experiment.get("model"),
        "version": experiment.get("version"),
        "intervention": experiment.get("intervention"),
        "seed": seed,
        "job_id": experiment.get("job_id"),
        "output_filename": experiment.get("output_filename"),
        "time_of_day": experiment.get("time_of_day"),
        "message": (
            "Re-run with the same config and seed to reproduce outputs."
            if seed is not None
            else "No fixed seed in metadata — outputs may vary between runs."
        ),
    }


def _placeholder(id_: str, title: str, reason: str) -> dict:
    return {"id": id_, "title": title, "status": "needs_data", "message": reason}


def load_visualization_bundle(workspace_id: str) -> dict[str, Any]:
    meta = get_workspace(workspace_id)
    config_path = active_config_path(workspace_id)
    with config_path.open(encoding="utf-8") as f:
        config = json.load(f)

    pr = meta.get("project_requirements", {})
    run_settings = meta.get("run_settings", {})
    telemetry = get_run_telemetry(workspace_id)

    pipeline = telemetry.get("pipeline") or build_pipeline_graph(
        pr,
        phase=telemetry.get("phase", "idle"),
        elapsed=0.0,
        enabled_risk_factors=run_settings.get("enabled_risk_factors"),
    )

    dirs = discover_results_dirs(config_path, config)
    _stamp, files, found_dir = latest_run_files(dirs)

    result_data: dict | None = None
    json_files = [
        f
        for f in files
        if f["name"].endswith(".json")
        and "_HighIncome" not in f["name"]
        and "_Individual" not in f["name"]
        and "_LowIncome" not in f["name"]
    ]
    if json_files:
        main = Path(json_files[0]["path"])
        if main.is_file():
            try:
                with main.open(encoding="utf-8") as f:
                    result_data = json.load(f)
            except (json.JSONDecodeError, OSError):
                result_data = None

    parsed = (
        parse_healthgps_result_charts(result_data)
        if isinstance(result_data, dict)
        else {"charts": [], "experiment": {}, "years": []}
    )
    raw_rows = result_data.get("result", []) if isinstance(result_data, dict) else []
    rows = normalize_result_rows(raw_rows) if isinstance(raw_rows, list) else []
    experiment = parsed.get("experiment", {})
    years = parsed.get("years", [])
    target_year = years[-1] if years else run_settings.get("stop_time")
    intervention = experiment.get("intervention") or run_settings.get("active_intervention") or "policy"
    trial_runs = int(run_settings.get("trial_runs", 1))

    headlines = _headlines(rows, target_year, str(intervention)) if rows else []
    burden_bars = _burden_bars(rows) if rows else []
    comorbidity = _comorbidity_matrix(rows) if rows else None
    pyramid = _population_pyramid(rows) if rows else None
    dumbbells = _parse_income_strata(files, "prevalence_diabetes") if files else []

    trajectories = []
    for cid, title, ylab, fn in [
        ("diabetes", "Diabetes prevalence", "%", lambda r: _avg_male_female((r.get("disease_prevalence") or {}).get("diabetes"))),
        ("bmi", "Mean BMI", "BMI", lambda r: _avg_male_female((r.get("risk_factors_average") or {}).get("BMI"))),
        ("daly", "DALY", "DALY", lambda r: (r.get("indicators") or {}).get("DALY")),
    ]:
        t = _trajectory(cid, title, ylab, rows, fn)
        if t:
            trajectories.append(t)

    variables = extract_result_variables(rows) if rows else []

    return {
        "pipeline": pipeline,
        "chart_builder": {
            "variables": variables,
            "time_axis": {"id": TIME_AXIS, "label": "Year", "category": "Time"},
            "chart_types": [
                {"id": "line", "label": "Line"},
                {"id": "area", "label": "Area"},
                {"id": "smooth", "label": "Smooth line"},
                {"id": "step", "label": "Step line"},
                {"id": "bar", "label": "Bar"},
                {"id": "column", "label": "Column"},
                {"id": "stacked_bar", "label": "Stacked bar"},
                {"id": "scatter", "label": "Scatter"},
                {"id": "pie", "label": "Pie (latest year)"},
                {"id": "combo", "label": "Combo (bar + line)"},
            ],
            "result_file": json_files[0]["name"] if json_files else None,
        },
        "scenario1": {
            "pipeline": pipeline,
            "validation_hint": "Run Validate before compute to check schema and dry-run wiring.",
        },
        "scenario2": {
            "headlines": headlines,
            "burden_bars": burden_bars,
            "trajectories": trajectories,
            "charts": parsed.get("charts", []),
            "uncertainty_note": (
                f"Across-run 95% intervals require trial_runs > 1 (currently {trial_runs}). "
                "Paired baseline vs intervention lines use the latest aggregate JSON."
                if trial_runs <= 1
                else "Trial aggregation bands will appear when multiple run stamps are available."
            ),
        },
        "scenario3": {
            "dumbbells": dumbbells,
            "outcome": "diabetes prevalence",
            "strata_type": "income",
            "note": (
                "Regional and ethnicity strata need region/ethnicity columns enabled and exported."
                if not pr.get("demographics", {}).get("region")
                else "Income quintile dumbbells from stratum CSV exports."
            ),
        },
        "scenario4": {
            "reproducibility": _reproducibility(experiment, config),
            "individual_tracking": _placeholder(
                "life_history",
                "Individual life-history timeline",
                "Select a person ID from IndividualIDTracking.csv — UI picker coming next.",
            ),
        },
        "modelling": {
            "population_pyramid": pyramid,
            "comorbidity_matrix": comorbidity,
            "risk_factor_trends": [
                c for c in parsed.get("charts", []) if c["id"] in ("bmi", "physical_activity")
            ],
            "calibration": _placeholder(
                "calibration",
                "Calibration / validation panel",
                "Requires observed trend data wired to the validation period.",
            ),
            "convergence": _placeholder(
                "convergence",
                "Convergence diagnostics",
                f"Increase trial_runs above 1 (now {trial_runs}) to plot mean/interval stabilisation.",
            ),
            "tornado": _placeholder(
                "tornado",
                "Sensitivity tornado",
                "Requires sensitivity sweep outputs from the engine.",
            ),
            "sankey": _placeholder(
                "sankey",
                "Cohort flow / Sankey",
                "Requires health-state transition counts from aggregated outputs.",
            ),
            "live_progress": {
                "phase": telemetry.get("phase"),
                "phase_steps": telemetry.get("phase_steps", []),
                "state": telemetry.get("state"),
            },
        },
        "meta": {
            "results_dir": str(found_dir or ""),
            "result_file": json_files[0]["name"] if json_files else None,
            "years": years,
            "target_year": target_year,
            "intervention": intervention,
            "trial_runs": trial_runs,
        },
    }
