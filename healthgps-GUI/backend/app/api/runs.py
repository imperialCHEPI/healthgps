"""Run and results endpoints."""

from __future__ import annotations

from pathlib import Path

from fastapi import APIRouter, HTTPException

from app.models.studio import ConsentRequest, RunStatus, RunTelemetry
from app.services.run_analytics import get_run_telemetry, reset_telemetry
from app.services.terminal_runner import TerminalRunnerError, launch_run, read_run_status
from app.services.workspace import WorkspaceError, workspace_dir

router = APIRouter(prefix="/api/workspaces", tags=["runs"])


def _require_consent(consent: ConsentRequest) -> None:
    if not consent.consent_acknowledged:
        raise HTTPException(
            status_code=403,
            detail="Terminal consent required.",
        )


@router.post("/{workspace_id}/run", response_model=RunStatus)
def run_workspace(workspace_id: str, consent: ConsentRequest) -> RunStatus:
    _require_consent(consent)
    try:
        reset_telemetry(workspace_id)
        result = launch_run(workspace_id, dry_run=False)
        return RunStatus(**result)
    except (WorkspaceError, TerminalRunnerError) as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc


@router.get("/{workspace_id}/run/status", response_model=RunStatus)
def run_status(workspace_id: str) -> RunStatus:
    try:
        result = read_run_status(workspace_id)
        return RunStatus(**result)
    except WorkspaceError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.get("/{workspace_id}/run/telemetry", response_model=RunTelemetry)
def run_telemetry(workspace_id: str) -> RunTelemetry:
    try:
        return RunTelemetry(**get_run_telemetry(workspace_id))
    except WorkspaceError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.get("/{workspace_id}/results")
def list_results(workspace_id: str) -> dict:
    try:
        import json

        from app.services.results import discover_results_dirs, latest_run_files
        from app.services.workspace import active_config_path

        config_path = active_config_path(workspace_id)
        with config_path.open(encoding="utf-8") as f:
            config = json.load(f)
        results_dirs = discover_results_dirs(config_path, config)
        run_timestamp, files, found_dir = latest_run_files(results_dirs)
        configured = config.get("output", {}).get("folder", "")
        return {
            "results_dir": str(found_dir or (results_dirs[0] if results_dirs else "")),
            "configured_folder": configured,
            "searched_dirs": [str(d) for d in results_dirs[:6]],
            "run_timestamp": run_timestamp,
            "files": files,
        }
    except WorkspaceError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.get("/{workspace_id}/results/charts")
def result_charts(workspace_id: str) -> dict:
    try:
        import json

        from app.services.results import load_result_chart_series
        from app.services.workspace import active_config_path

        config_path = active_config_path(workspace_id)
        with config_path.open(encoding="utf-8") as f:
            config = json.load(f)
        return load_result_chart_series(config_path, config)
    except WorkspaceError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
