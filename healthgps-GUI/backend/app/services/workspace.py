"""Workspace metadata and config generation beside healthgps-examples."""

from __future__ import annotations

import json
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from app.config import get_settings
from app.models.studio import ProjectRequirementsState, RunSettings, WorkspaceCreateRequest
from app.services.config_builder import apply_overrides, state_from_template
from app.services.registry import (
    load_config_at_path,
    load_template_config,
    resolve_config_path,
)


class WorkspaceError(Exception):
    pass

STUDIO_CONFIG_PREFIX = "HealthGPS_Studio_"


def _run_settings_differ(base: dict[str, Any], run_settings: RunSettings) -> bool:
    settings = base.get("inputs", {}).get("settings", {})
    running = base.get("running", {})
    if settings.get("size_fraction") != run_settings.size_fraction:
        return True
    if running.get("start_time") != run_settings.start_time:
        return True
    if running.get("stop_time") != run_settings.stop_time:
        return True
    if running.get("trial_runs") != run_settings.trial_runs:
        return True
    active = running.get("interventions", {}).get("active_type_id", "") or ""
    if active != (run_settings.active_intervention or ""):
        return True
    return False


def _meta_path(workspace_id: str) -> Path:
    root: Path = get_settings()["workspaces_root"]  # type: ignore[assignment]
    return root / workspace_id / "studio-meta.json"


def _studio_config_path(example_dir: Path, workspace_id: str) -> Path:
    short = workspace_id.split("-")[0]
    return example_dir / f"{STUDIO_CONFIG_PREFIX}{short}.json"


def _write_active_config(
    base_config_path: Path,
    workspace_id: str,
    project_requirements: ProjectRequirementsState,
    run_settings: RunSettings,
    pif_enabled: bool | None,
) -> Path:
    base = load_config_at_path(base_config_path)
    config = apply_overrides(
        base,
        project_requirements,
        run_settings,
        output_folder=None,
        pif_enabled=pif_enabled,
    )
    out_path = _studio_config_path(base_config_path.parent, workspace_id)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(config, f, indent=4)
    return out_path.resolve()


def create_workspace(request: WorkspaceCreateRequest) -> dict[str, Any]:
    variant = request.config_variant or "config"
    source_config = resolve_config_path(request.project_id, variant)

    workspace_id = str(uuid.uuid4())
    meta_dir = _meta_path(workspace_id).parent
    meta_dir.mkdir(parents=True, exist_ok=True)

    base = load_config_at_path(source_config)
    has_pr = bool(base.get("project_requirements"))
    run_settings_changed = _run_settings_differ(base, request.run_settings)
    if has_pr or run_settings_changed:
        active_config = _write_active_config(
            source_config,
            workspace_id,
            request.project_requirements,
            request.run_settings,
            request.pif_enabled,
        )
        run_config_path = active_config
    else:
        active_config = source_config.resolve()
        run_config_path = active_config

    meta = {
        "id": workspace_id,
        "project_id": request.project_id,
        "config_variant": variant,
        "source_config_path": str(source_config.resolve()),
        "active_config_path": str(active_config),
        "run_config_path": str(run_config_path),
        "path": str(meta_dir.resolve()),
        "created_at": datetime.now(timezone.utc).isoformat(),
        "project_requirements": request.project_requirements.model_dump(),
        "run_settings": request.run_settings.model_dump(),
        "pif_enabled": request.pif_enabled,
        "last_run_status": None,
        "thread_count": request.run_settings.thread_count,
        "run_pid": None,
    }
    with _meta_path(workspace_id).open("w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)

    return meta


def get_workspace(workspace_id: str) -> dict[str, Any]:
    meta_file = _meta_path(workspace_id)
    if not meta_file.is_file():
        raise WorkspaceError(f"Workspace not found: {workspace_id}")
    with meta_file.open(encoding="utf-8") as f:
        return json.load(f)


def workspace_dir(workspace_id: str) -> Path:
    meta = get_workspace(workspace_id)
    return Path(meta["path"])


def active_config_path(workspace_id: str) -> Path:
    meta = get_workspace(workspace_id)
    return Path(meta.get("run_config_path") or meta["active_config_path"])


def update_workspace_config(
    workspace_id: str,
    project_requirements: ProjectRequirementsState,
    run_settings: RunSettings,
    pif_enabled: bool | None = None,
    config_variant: str | None = None,
) -> dict[str, Any]:
    meta = get_workspace(workspace_id)
    if meta.get("project_id") == "expert":
        meta["run_settings"] = run_settings.model_dump()
        meta["thread_count"] = run_settings.thread_count
        if project_requirements.model_dump():
            meta["project_requirements"] = project_requirements.model_dump()
        with _meta_path(workspace_id).open("w", encoding="utf-8") as f:
            json.dump(meta, f, indent=2)
        return meta

    variant = config_variant or meta.get("config_variant", "config")
    source = Path(meta["source_config_path"])
    if config_variant and config_variant != meta.get("config_variant"):
        source = resolve_config_path(meta["project_id"], variant)

    active = _write_active_config(
        source,
        workspace_id,
        project_requirements,
        run_settings,
        pif_enabled if pif_enabled is not None else meta.get("pif_enabled"),
    )

    meta["config_variant"] = variant
    meta["source_config_path"] = str(source.resolve())
    meta["active_config_path"] = str(active)
    meta["run_config_path"] = str(active)
    meta["project_requirements"] = project_requirements.model_dump()
    meta["run_settings"] = run_settings.model_dump()
    meta["thread_count"] = run_settings.thread_count
    if pif_enabled is not None:
        meta["pif_enabled"] = pif_enabled

    with _meta_path(workspace_id).open("w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)

    return meta


def set_run_status(
    workspace_id: str,
    status: str,
    exit_code: int | None = None,
    run_pid: int | None = None,
) -> None:
    meta = get_workspace(workspace_id)
    meta["last_run_status"] = status
    if exit_code is not None:
        meta["last_exit_code"] = exit_code
    if run_pid is not None:
        meta["run_pid"] = run_pid
    elif status in ("succeeded", "failed", "idle"):
        meta["run_pid"] = None
    with _meta_path(workspace_id).open("w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)


def default_state_for_project(
    project_id: str, config_variant: str = "config"
) -> tuple[ProjectRequirementsState, RunSettings]:
    from app.models.studio import RunSettings

    template = load_template_config(project_id, config_variant)
    pr = state_from_template(template.get("project_requirements", {}))
    project = __import__("app.services.registry", fromlist=["get_project"]).get_project(
        project_id
    )
    defaults = project.get("local_defaults", {})
    rs = RunSettings(
        size_fraction=defaults.get("size_fraction", 0.0001),
        start_time=defaults.get("start_time", 2022),
        stop_time=defaults.get("stop_time", 2025),
        trial_runs=defaults.get("trial_runs", 1),
        active_intervention=defaults.get("active_intervention", ""),
        thread_count=defaults.get("thread_count", 4),
    )
    return pr, rs
