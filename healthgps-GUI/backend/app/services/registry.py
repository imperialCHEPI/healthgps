"""Load project registry and config files from healthgps-examples."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from app.config import get_settings


class RegistryError(Exception):
    pass


def load_registry() -> dict[str, Any]:
    settings = get_settings()
    path: Path = settings["registry_path"]  # type: ignore[assignment]
    if not path.is_file():
        raise RegistryError(f"Registry not found: {path}")
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def list_projects() -> list[dict[str, Any]]:
    data = load_registry()
    return data.get("projects", [])


def get_project(project_id: str) -> dict[str, Any]:
    for project in list_projects():
        if project["id"] == project_id:
            return project
    raise RegistryError(f"Unknown project: {project_id}")


def examples_root() -> Path:
    root: Path = get_settings()["examples_root"]  # type: ignore[assignment]
    if not root.is_dir():
        raise RegistryError(
            f"healthgps-examples not found at {root}. "
            "Set HEALTHGPS_EXAMPLES_ROOT to your examples checkout."
        )
    return root


def example_dir_path(project: dict[str, Any]) -> Path:
    return (examples_root() / project["example_dir"]).resolve()


def resolve_config_path(project_id: str, config_variant: str) -> Path:
    project = get_project(project_id)
    options = project.get("config_options", [])
    match = next((o for o in options if o["id"] == config_variant), None)
    if match is None:
        raise RegistryError(
            f"Unknown config variant '{config_variant}' for project '{project_id}'"
        )
    config_path = example_dir_path(project) / match["file"]
    if not config_path.is_file():
        raise RegistryError(f"Config file not found: {config_path}")
    return config_path


def load_config_at_path(config_path: Path) -> dict[str, Any]:
    with config_path.open(encoding="utf-8") as f:
        return json.load(f)


def load_template_config(project_id: str, config_variant: str = "config") -> dict[str, Any]:
    return load_config_at_path(resolve_config_path(project_id, config_variant))


def intervention_ids_from_config(config: dict[str, Any]) -> list[str]:
    types = config.get("running", {}).get("interventions", {}).get("types", {})
    ids = [""] + list(types.keys())
    return ids


DEMOGRAPHIC_RF_NAMES = frozenset({"Gender", "Age", "Age2", "Age3"})


def model_risk_factors_from_config(config: dict[str, Any]) -> list[str]:
    factors = config.get("modelling", {}).get("risk_factors", [])
    return [
        str(item["name"])
        for item in factors
        if isinstance(item, dict)
        and item.get("name")
        and item["name"] not in DEMOGRAPHIC_RF_NAMES
    ]


def default_project_requirements() -> dict[str, Any]:
    """Defaults for legacy configs without project_requirements (e.g. HLM_France)."""
    return {
        "demographics": {
            "age": True,
            "gender": True,
            "region": False,
            "ethnicity": False,
        },
        "income": {
            "enabled": True,
            "type": "categorical",
            "categories": "3",
            "adjust_to_factors_mean": False,
            "trended": False,
            "income_based_csv_output": True,
        },
        "physical_activity": {
            "enabled": True,
            "type": "simple",
            "adjust_to_factors_mean": False,
            "trended": False,
        },
        "risk_factors": {
            "adjust_to_factors_mean": True,
            "trended": True,
        },
        "trend": {"enabled": False, "type": "null"},
        "two_stage": {"use_logistic": False},
    }


def project_detail(project_id: str, config_variant: str = "config") -> dict[str, Any]:
    project = get_project(project_id)
    config_path = resolve_config_path(project_id, config_variant)
    template = load_config_at_path(config_path)
    pr = template.get("project_requirements", {})
    if not pr:
        fallback = example_dir_path(project) / "new_config.json"
        if fallback.is_file() and fallback != config_path:
            pr = load_config_at_path(fallback).get("project_requirements", {})
    if not pr:
        pr = default_project_requirements()
    model_risk_factors = model_risk_factors_from_config(template)
    config_options = []
    for opt in project.get("config_options", []):
        path = example_dir_path(project) / opt["file"]
        config_options.append(
            {
                **opt,
                "path": str(path),
                "exists": path.is_file(),
            }
        )
    return {
        **project,
        "examples_root": str(examples_root()),
        "example_dir_path": str(example_dir_path(project)),
        "default_config_variant": config_variant,
        "default_config_path": str(config_path),
        "config_options": config_options,
        "default_project_requirements": pr,
        "model_risk_factors": model_risk_factors,
        "intervention_ids": intervention_ids_from_config(template),
    }
