"""Project catalog endpoints."""

from __future__ import annotations

from fastapi import APIRouter, HTTPException

from app.models.studio import ProjectDetail, ProjectSummary
from app.services.registry import RegistryError, list_projects, project_detail

router = APIRouter(prefix="/api/projects", tags=["projects"])


@router.get("", response_model=list[ProjectSummary])
def get_projects() -> list[ProjectSummary]:
    return [
        ProjectSummary(
            id=p["id"],
            name=p["name"],
            description=p["description"],
            has_pif=p.get("has_pif", False),
            locked_fields=p.get("locked_fields", []),
        )
        for p in list_projects()
    ]


@router.get("/{project_id}", response_model=ProjectDetail)
def get_project(project_id: str, config_variant: str = "config") -> ProjectDetail:
    try:
        detail = project_detail(project_id, config_variant)
    except RegistryError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc

    return ProjectDetail(
        id=detail["id"],
        name=detail["name"],
        description=detail["description"],
        has_pif=detail.get("has_pif", False),
        locked_fields=detail.get("locked_fields", []),
        examples_root=detail["examples_root"],
        example_dir_path=detail["example_dir_path"],
        default_config_variant=detail["default_config_variant"],
        default_config_path=detail["default_config_path"],
        config_options=detail["config_options"],
        default_project_requirements=detail["default_project_requirements"],
        model_risk_factors=detail.get("model_risk_factors", []),
        local_defaults=detail.get("local_defaults", {}),
        intervention_ids=detail.get("intervention_ids", []),
    )
