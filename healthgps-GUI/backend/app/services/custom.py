"""Custom session flows: new user wizard and expert uploads."""

from __future__ import annotations

import json
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from app.config import get_settings
from app.models.studio import (
    NewUserSessionRequest,
    ProjectRequirementsState,
    RunSettings,
    WorkspaceCreateRequest,
)
from app.services.catalog import list_catalog
from app.services.registry import (
    get_project,
    model_risk_factors_from_config,
    project_detail,
    resolve_config_path,
)
from app.services.workspace import _meta_path, create_workspace, get_workspace


class CustomSessionError(Exception):
    pass


DEFAULT_PROJECT_BY_COUNTRY: dict[str, str] = {
    "fra": "hlm_france",
    "ind": "india",
    "gbr": "finch",
}


def list_countries() -> list[dict[str, Any]]:
    """Unique countries from catalog with example-data availability."""
    seen: dict[str, dict[str, Any]] = {}
    for program in list_catalog():
        for country in program.get("countries", []):
            cid = country["id"]
            if cid not in seen:
                seen[cid] = {
                    "id": cid,
                    "name": country["name"],
                    "has_example_data": bool(country.get("available")),
                    "project_id": country.get("project_id"),
                }
            else:
                if country.get("available"):
                    seen[cid]["has_example_data"] = True
                if country.get("project_id"):
                    seen[cid]["project_id"] = country["project_id"]
    return sorted(seen.values(), key=lambda c: c["name"])


def resolve_project_for_country(country_id: str) -> str:
    if country_id in DEFAULT_PROJECT_BY_COUNTRY:
        return DEFAULT_PROJECT_BY_COUNTRY[country_id]
    for country in list_countries():
        if country["id"] == country_id and country.get("project_id"):
            return country["project_id"]
    return "finch"


def new_user_defaults(country_id: str) -> dict[str, Any]:
    project_id = resolve_project_for_country(country_id)
    detail = project_detail(get_project(project_id))
    pr = detail["default_project_requirements"]
    model_rf = detail.get("model_risk_factors", [])
    defaults = detail.get("local_defaults", {})
    return {
        "country_id": country_id,
        "project_id": project_id,
        "project_name": detail["name"],
        "default_project_requirements": pr,
        "model_risk_factors": model_rf,
        "local_defaults": defaults,
    }


def create_new_user_session(request: NewUserSessionRequest) -> dict[str, Any]:
    project_id = resolve_project_for_country(request.country_id)
    ws_request = WorkspaceCreateRequest(
        project_id=project_id,
        config_variant="config",
        project_requirements=request.project_requirements,
        run_settings=request.run_settings,
        pif_enabled=None,
    )
    meta = create_workspace(ws_request)
    meta["session_type"] = "new_user"
    meta["country_id"] = request.country_id
    meta["country_name"] = request.country_name
    meta["population_label"] = request.population_label or request.country_name
    with _meta_path(meta["id"]).open("w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
    return meta


def create_expert_session(
    country_id: str,
    country_name: str,
    session_label: str,
    config_bytes: bytes,
    config_filename: str,
    extra_files: list[tuple[str, bytes]],
) -> dict[str, Any]:
    workspace_id = str(uuid.uuid4())
    root: Path = get_settings()["workspaces_root"]  # type: ignore[assignment]
    session_dir = root / workspace_id
    upload_dir = session_dir / "uploads"
    upload_dir.mkdir(parents=True, exist_ok=True)

    config_path = upload_dir / (config_filename or "config.json")
    config_path.write_bytes(config_bytes)

    saved_files = [str(config_path.resolve())]
    for name, data in extra_files:
        safe = Path(name).name
        if not safe or safe == config_path.name:
            continue
        dest = upload_dir / safe
        dest.write_bytes(data)
        saved_files.append(str(dest.resolve()))

    try:
        with config_path.open(encoding="utf-8") as f:
            config_data = json.load(f)
    except (json.JSONDecodeError, OSError) as exc:
        raise CustomSessionError(f"Invalid config file: {exc}") from exc

    model_rf = model_risk_factors_from_config(config_data)
    now = datetime.now(timezone.utc).isoformat()
    meta = {
        "id": workspace_id,
        "project_id": "expert",
        "session_type": "expert",
        "country_id": country_id,
        "country_name": country_name,
        "session_label": session_label or country_name,
        "config_variant": "uploaded",
        "source_config_path": str(config_path.resolve()),
        "active_config_path": str(config_path.resolve()),
        "run_config_path": str(config_path.resolve()),
        "path": str(session_dir.resolve()),
        "created_at": now,
        "uploaded_files": saved_files,
        "model_risk_factors": model_rf,
        "project_requirements": {},
        "run_settings": RunSettings().model_dump(),
        "pif_enabled": None,
        "last_run_status": None,
        "thread_count": 4,
        "run_pid": None,
    }
    with _meta_path(workspace_id).open("w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
    return meta


def get_session_meta(workspace_id: str) -> dict[str, Any]:
    return get_workspace(workspace_id)
