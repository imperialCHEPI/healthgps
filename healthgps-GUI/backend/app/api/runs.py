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


@router.get("/{workspace_id}/visualizations")
def workspace_visualizations(workspace_id: str) -> dict:
    try:
        from app.services.visualizations import load_visualization_bundle

        return load_visualization_bundle(workspace_id)
    except WorkspaceError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.get("/{workspace_id}/results/chart")
def result_chart(
    workspace_id: str,
    x: str = "__time__",
    y: str = "",
    chart_type: str = "line",
) -> dict:
    try:
        from app.services.result_explorer import (
            CHART_TYPES,
            chart_for_axes,
            extract_result_variables,
        )
        from app.services.run_analytics import _load_result_rows

        if not y:
            raise HTTPException(status_code=400, detail="Query parameter 'y' is required.")
        if chart_type not in CHART_TYPES:
            raise HTTPException(
                status_code=400,
                detail=f"chart_type must be one of: {', '.join(CHART_TYPES)}",
            )

        rows = _load_result_rows(workspace_id)
        if not rows:
            raise HTTPException(status_code=404, detail="No result JSON available yet.")
        want = [s.strip() for s in sources.split(",") if s.strip()]
        variables = extract_result_variables(rows)
        payload = chart_for_axes(
            rows,
            x,
            y,
            sources=want,
            chart_type=chart_type,
            variables=variables,
        )
        if not payload:
            raise HTTPException(
                status_code=404,
                detail=f"No chart data for x={x!r}, y={y!r}, type={chart_type!r}.",
            )
        return payload
    except WorkspaceError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.get("/{workspace_id}/results/series")
def result_series(
    workspace_id: str,
    variable: str,
    sources: str = "Baseline,Intervention",
) -> dict:
    try:
        from app.services.result_explorer import series_for_variable
        from app.services.run_analytics import _load_result_rows

        rows = _load_result_rows(workspace_id)
        if not rows:
            raise HTTPException(status_code=404, detail="No result JSON available yet.")
        want = [s.strip() for s in sources.split(",") if s.strip()]
        series = series_for_variable(rows, variable, sources=want)
        if not series:
            raise HTTPException(
                status_code=404,
                detail=f"No series for variable '{variable}' with sources {want}.",
            )
        return {"variable": variable, "sources": want, "series": series}
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
