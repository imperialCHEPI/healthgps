"""Merge UI state into workspace config.json."""

from __future__ import annotations

import copy
from typing import Any

from app.models.studio import ProjectRequirementsState, RunSettings
from app.services.results import normalize_output_folder


def project_requirements_to_dict(state: ProjectRequirementsState) -> dict[str, Any]:
    demo = state.demographics.model_dump(exclude_none=True)
    if state.demographics.max_age_for_linear_models is None:
        demo.pop("max_age_for_linear_models", None)
    return {
        "demographics": demo,
        "income": state.income.model_dump(),
        "physical_activity": state.physical_activity.model_dump(),
        "risk_factors": state.risk_factors.model_dump(),
        "trend": state.trend.model_dump(),
        "two_stage": state.two_stage.model_dump(),
    }


def apply_overrides(
    template_config: dict[str, Any],
    project_requirements: ProjectRequirementsState,
    run_settings: RunSettings,
    output_folder: str | None = None,
    pif_enabled: bool | None = None,
) -> dict[str, Any]:
    config = copy.deepcopy(template_config)
    config["project_requirements"] = project_requirements_to_dict(project_requirements)

    config.setdefault("inputs", {}).setdefault("settings", {})
    config["inputs"]["settings"]["size_fraction"] = run_settings.size_fraction
    config["inputs"]["settings"]["age_range"] = [
        run_settings.age_range_min,
        run_settings.age_range_max,
    ]

    config.setdefault("running", {})
    config["running"]["start_time"] = run_settings.start_time
    config["running"]["stop_time"] = run_settings.stop_time
    config["running"]["trial_runs"] = run_settings.trial_runs

    interventions = config["running"].get("interventions", {})
    active = run_settings.active_intervention or ""
    interventions["active_type_id"] = active if active else ""
    config["running"]["interventions"] = interventions

    if output_folder is not None:
        config.setdefault("output", {})
        config["output"]["folder"] = output_folder

    out = config.get("output", {}).get("folder")
    if out:
        config.setdefault("output", {})
        config["output"]["folder"] = normalize_output_folder(str(out))

    if pif_enabled is not None and "population_impact_fraction" in config:
        config["population_impact_fraction"]["enabled"] = pif_enabled

    if run_settings.enabled_risk_factors:
        enabled = set(run_settings.enabled_risk_factors)
        keep_demo = {"Gender", "Age", "Age2", "Age3"}
        modelling = config.get("modelling", {})
        factors = modelling.get("risk_factors", [])
        if factors:
            modelling["risk_factors"] = [
                rf
                for rf in factors
                if rf.get("name") in keep_demo or rf.get("name") in enabled
            ]
            config["modelling"] = modelling

    return config


def state_from_template(template_pr: dict[str, Any]) -> ProjectRequirementsState:
    from app.models.studio import (
        DemographicsRequirements,
        IncomeRequirements,
        PhysicalActivityRequirements,
        ProjectRequirementsState,
        RiskFactorsRequirements,
        TrendRequirements,
        TwoStageRequirements,
    )

    demo = template_pr.get("demographics", {})
    return ProjectRequirementsState(
        demographics=DemographicsRequirements(
            age=demo.get("age", True),
            gender=demo.get("gender", True),
            region=demo.get("region", False),
            ethnicity=demo.get("ethnicity", False),
            max_age_for_linear_models=demo.get("max_age_for_linear_models"),
        ),
        income=IncomeRequirements(**template_pr.get("income", {})),
        physical_activity=PhysicalActivityRequirements(
            **template_pr.get("physical_activity", {})
        ),
        risk_factors=RiskFactorsRequirements(**template_pr.get("risk_factors", {})),
        trend=TrendRequirements(**template_pr.get("trend", {})),
        two_stage=TwoStageRequirements(**template_pr.get("two_stage", {})),
    )
