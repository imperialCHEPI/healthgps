"""Pipeline module progress for live dashboard."""

from __future__ import annotations

from typing import Any

PIPELINE_MODULES = [
    {"id": "demographics", "label": "Demographics", "description": "Age, gender, region, ethnicity"},
    {"id": "ses", "label": "Socioeconomic", "description": "Income / SES stratification"},
    {"id": "risk_factors", "label": "Risk factors", "description": "Diet, activity, BMI draws"},
    {"id": "diseases", "label": "Diseases", "description": "Incidence, prevalence, comorbidity"},
    {"id": "analysis", "label": "Analysis", "description": "Burden, outputs, policy comparison"},
]

PIPELINE_CYCLE = ["demographics", "ses", "risk_factors", "diseases", "analysis"]


def _enabled_module_ids(
    project_requirements: dict[str, Any],
    enabled_risk_factors: list[str] | None,
) -> list[str]:
    demo = project_requirements.get("demographics", {})
    income = project_requirements.get("income", {})
    rf = project_requirements.get("risk_factors", {})
    enabled: list[str] = []
    if any(demo.get(k) for k in ("age", "gender", "region", "ethnicity")):
        enabled.append("demographics")
    if income.get("enabled", True):
        enabled.append("ses")
    if enabled_risk_factors or rf:
        enabled.append("risk_factors")
    enabled.extend(["diseases", "analysis"])
    return enabled


def cycling_active_module(
    phase: str,
    elapsed: float,
    enabled_ids: list[str],
) -> str | None:
    if phase in ("idle", "failed"):
        return None
    if phase == "complete":
        return "analysis"
    cycle = [m for m in PIPELINE_CYCLE if m in enabled_ids]
    if not cycle:
        return None
    if phase in ("initializing", "baseline"):
        period = 2.5
        return cycle[int(elapsed / period) % len(cycle)]
    if phase in ("simulating", "policy"):
        period = 2.0
        offset = 5
        return cycle[int((elapsed + offset) / period) % len(cycle)]
    return cycle[0]


def build_pipeline_modules(
    project_requirements: dict[str, Any],
    *,
    phase: str,
    elapsed: float = 0.0,
    enabled_risk_factors: list[str] | None = None,
) -> dict[str, Any]:
    enabled_ids = _enabled_module_ids(project_requirements, enabled_risk_factors)
    active_id = cycling_active_module(phase, elapsed, enabled_ids)
    active_index = (
        PIPELINE_CYCLE.index(active_id) if active_id and active_id in PIPELINE_CYCLE else -1
    )

    modules = []
    for mod in PIPELINE_MODULES:
        mid = mod["id"]
        if mid not in enabled_ids:
            status = "disabled"
        elif active_id is None:
            status = "pending"
        elif mid == active_id:
            status = "active"
        elif active_id in PIPELINE_CYCLE and mid in PIPELINE_CYCLE:
            mod_idx = PIPELINE_CYCLE.index(mid)
            status = "done" if mod_idx < active_index else "pending"
        else:
            status = "pending"
        modules.append({**mod, "status": status, "enabled": mid in enabled_ids})

    return {"modules": modules, "active_module_id": active_id}
