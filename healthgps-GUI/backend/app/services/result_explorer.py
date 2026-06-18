"""Extract plottable variables and series from HealthGPS result JSON."""

from __future__ import annotations

from typing import Any

from app.services.results import SOURCE_COLORS, _avg_male_female

SKIP_KEYS = {"metrics", "comorbidities", "average_age"}


def _value_from_row(row: dict, var_id: str) -> float | None:
    cur: Any = row
    for part in var_id.split("."):
        if not isinstance(cur, dict) or part not in cur:
            return None
        cur = cur[part]
    if isinstance(cur, dict) and ("male" in cur or "female" in cur):
        return _avg_male_female(cur)
    if isinstance(cur, (int, float)):
        return float(cur)
    return None


def extract_result_variables(rows: list[dict]) -> list[dict[str, str]]:
    """List numeric variables available across result rows."""
    if not rows:
        return []

    variables: list[dict[str, str]] = []
    seen: set[str] = set()

    def add_var(var_id: str, category: str, label: str, unit: str = "") -> None:
        if var_id in seen or var_id.split(".")[0] in SKIP_KEYS:
            return
        # Must have at least one numeric value
        if not any(_value_from_row(r, var_id) is not None for r in rows if isinstance(r, dict)):
            return
        seen.add(var_id)
        variables.append(
            {
                "id": var_id,
                "label": label,
                "category": category,
                "unit": unit,
            }
        )

    for row in rows:
        if not isinstance(row, dict):
            continue
        indicators = row.get("indicators")
        if isinstance(indicators, dict):
            for key in indicators:
                add_var(f"indicators.{key}", "Burden", key, "")
        pop = row.get("population")
        if isinstance(pop, dict):
            for key in ("alive", "dead", "size", "alive_male", "alive_female"):
                if key in pop:
                    add_var(f"population.{key}", "Population", key.replace("_", " "), "")
        diseases = row.get("disease_prevalence")
        if isinstance(diseases, dict):
            for name in diseases:
                add_var(
                    f"disease_prevalence.{name}",
                    "Disease prevalence",
                    name,
                    "%",
                )
        risks = row.get("risk_factors_average")
        if isinstance(risks, dict):
            for name in risks:
                add_var(
                    f"risk_factors_average.{name}",
                    "Risk factors",
                    name,
                    "",
                )

    variables.sort(key=lambda v: (v["category"], v["label"]))
    return variables


TIME_AXIS = "__time__"
CHART_TYPES = (
    "line",
    "area",
    "bar",
    "column",
    "scatter",
    "step",
    "smooth",
    "pie",
    "stacked_bar",
    "combo",
)


def _label_for_var(var_id: str, variables: list[dict[str, str]] | None = None) -> str:
    if var_id == TIME_AXIS:
        return "Year"
    if variables:
        for v in variables:
            if v.get("id") == var_id:
                return str(v.get("label", var_id))
    return var_id.split(".")[-1].replace("_", " ")


def chart_for_axes(
    rows: list[dict],
    x_var: str,
    y_var: str,
    *,
    sources: list[str] | None = None,
    chart_type: str = "line",
    variables: list[dict[str, str]] | None = None,
) -> dict[str, Any]:
    """Build chart payload for arbitrary X/Y variable pairing."""
    if chart_type not in CHART_TYPES:
        chart_type = "line"
    want = sources or ["Baseline", "Intervention"]
    by_source: dict[str, list[dict[str, float]]] = {}

    for row in rows:
        if not isinstance(row, dict):
            continue
        source = str(row.get("source", ""))
        if source not in want:
            continue

        if x_var == TIME_AXIS:
            x_val = row.get("time")
        else:
            x_val = _value_from_row(row, x_var)

        y_val = _value_from_row(row, y_var)
        if not isinstance(x_val, (int, float)) or y_val is None:
            continue

        by_source.setdefault(source, []).append({"x": float(x_val), "y": float(y_val)})

    min_points = 1 if chart_type in ("scatter", "pie") else 2
    out: list[dict[str, Any]] = []
    for source in sorted(by_source.keys()):
        points = sorted(by_source[source], key=lambda p: (p["x"], p["y"]))
        if len(points) < min_points:
            continue
        out.append(
            {
                "name": source,
                "color": SOURCE_COLORS.get(source, "#334155"),
                "points": points,
            }
        )

    if not out:
        return {}

    return {
        "x_var": x_var,
        "y_var": y_var,
        "chart_type": chart_type,
        "x_label": _label_for_var(x_var, variables),
        "y_label": _label_for_var(y_var, variables),
        "title": f"{_label_for_var(y_var, variables)} vs {_label_for_var(x_var, variables)}",
        "series": out,
    }


def series_for_variable(
    rows: list[dict],
    var_id: str,
    *,
    sources: list[str] | None = None,
) -> list[dict[str, Any]]:
    """Build chart series for a variable, optionally filtered by source."""
    want = sources or ["Baseline", "Intervention"]
    by_source: dict[str, list[dict[str, float]]] = {}

    for row in rows:
        if not isinstance(row, dict):
            continue
        source = str(row.get("source", ""))
        if source not in want:
            continue
        year = row.get("time")
        val = _value_from_row(row, var_id)
        if not isinstance(year, (int, float)) or val is None:
            continue
        by_source.setdefault(source, []).append({"x": float(year), "y": val})

    out: list[dict[str, Any]] = []
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


def scenario_timelines(
    rows: list[dict],
    start_year: int,
    stop_year: int,
) -> dict[str, dict[str, Any]]:
    span = max(1, stop_year - start_year)

    def timeline_for(source: str) -> dict[str, Any]:
        matched = [r for r in rows if isinstance(r, dict) and r.get("source") == source]
        if not matched:
            return {
                "current_year": start_year,
                "progress_pct": 0.0,
                "active": False,
                "status": "waiting",
            }
        year = max(int(r.get("time", start_year)) for r in matched)
        done = year >= stop_year
        pct = 100.0 if done else min(100.0, ((year - start_year) / span) * 100.0)
        return {
            "current_year": stop_year if done else year,
            "progress_pct": round(pct, 1),
            "active": not done,
            "status": "complete" if done else "running",
        }

    baseline = timeline_for("Baseline")
    intervention = timeline_for("Intervention")

    if baseline["status"] != "complete":
        intervention["active"] = False
    elif intervention["status"] == "running":
        baseline["active"] = False

    return {"baseline": baseline, "intervention": intervention}
