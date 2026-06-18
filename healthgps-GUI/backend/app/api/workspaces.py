"""Workspace management endpoints."""

from __future__ import annotations

from fastapi import APIRouter, HTTPException

from app.models.studio import (
    ConsentRequest,
    SchemaValidationResult,
    WorkspaceCreateRequest,
    WorkspaceMeta,
)
from app.services.schema_validator import validate_config_file
from app.services.workspace import (
    WorkspaceError,
    create_workspace,
    get_workspace,
    update_workspace_config,
)
from app.services.terminal_runner import TerminalRunnerError, build_command, launch_run
from app.services.workspace import active_config_path

router = APIRouter(prefix="/api/workspaces", tags=["workspaces"])


def _require_consent(consent: ConsentRequest) -> None:
    if not consent.consent_acknowledged:
        raise HTTPException(
            status_code=403,
            detail="Terminal consent required. Acknowledge terminal access before Validate or Run.",
        )


@router.post("", response_model=WorkspaceMeta)
def post_workspace(request: WorkspaceCreateRequest) -> WorkspaceMeta:
    try:
        meta = create_workspace(request)
    except WorkspaceError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    return WorkspaceMeta(
        **{k: v for k, v in meta.items() if k not in ("thread_count", "run_pid", "pif_enabled", "last_exit_code")}
    )


@router.get("/{workspace_id}", response_model=WorkspaceMeta)
def get_workspace_detail(workspace_id: str) -> WorkspaceMeta:
    try:
        meta = get_workspace(workspace_id)
    except WorkspaceError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    return WorkspaceMeta(
        **{k: v for k, v in meta.items() if k not in ("thread_count", "run_pid", "pif_enabled", "last_exit_code")}
    )


@router.put("/{workspace_id}", response_model=WorkspaceMeta)
def put_workspace(workspace_id: str, request: WorkspaceCreateRequest) -> WorkspaceMeta:
    try:
        meta = update_workspace_config(
            workspace_id,
            request.project_requirements,
            request.run_settings,
            request.pif_enabled,
            request.config_variant,
        )
    except WorkspaceError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    return WorkspaceMeta(
        **{k: v for k, v in meta.items() if k not in ("thread_count", "run_pid", "pif_enabled", "last_exit_code")}
    )


@router.post("/{workspace_id}/validate-schema", response_model=SchemaValidationResult)
def validate_schema(workspace_id: str) -> SchemaValidationResult:
    try:
        config_path = active_config_path(workspace_id)
    except WorkspaceError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc

    valid, errors = validate_config_file(config_path)
    return SchemaValidationResult(valid=valid, errors=errors)


@router.post("/{workspace_id}/validate")
def validate_workspace(workspace_id: str, consent: ConsentRequest) -> dict:
    _require_consent(consent)
    try:
        valid, errors = validate_config_file(active_config_path(workspace_id))
        if not valid:
            return {
                "schema_valid": False,
                "schema_errors": errors,
                "terminal_launched": False,
            }
        from app.services.run_analytics import reset_telemetry

        reset_telemetry(workspace_id)
        result = launch_run(workspace_id, dry_run=True)
        return {"schema_valid": True, "schema_errors": [], **result}
    except (WorkspaceError, TerminalRunnerError) as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc


@router.get("/{workspace_id}/preview-command")
def preview_command(workspace_id: str, dry_run: bool = False) -> dict:
    try:
        meta = get_workspace(workspace_id)
        cmd = build_command(
            active_config_path(workspace_id),
            meta.get("thread_count", 4),
            dry_run=dry_run,
        )
        return {"command": cmd}
    except (WorkspaceError, TerminalRunnerError) as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
