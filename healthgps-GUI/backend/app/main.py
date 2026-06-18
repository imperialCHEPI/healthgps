"""HealthGPS Studio FastAPI application."""

from __future__ import annotations

from pathlib import Path

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from app.api import catalog, custom, projects, runs, workspaces
from app.config import get_settings
from app.models.studio import SettingsResponse

app = FastAPI(
    title="HealthGPS Studio",
    description="Local GUI backend for HealthGPS microsimulation",
    version="0.1.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:5173",
        "http://127.0.0.1:5173",
        "http://localhost:5174",
        "http://127.0.0.1:5174",
        "http://localhost:8000",
        "http://127.0.0.1:8000",
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(catalog.router)
app.include_router(custom.router)
app.include_router(projects.router)
app.include_router(workspaces.router)
app.include_router(runs.router)


@app.get("/api/settings", response_model=SettingsResponse)
def get_app_settings() -> SettingsResponse:
    s = get_settings()
    return SettingsResponse(
        healthgps_console=s["healthgps_console"],  # type: ignore[arg-type]
        healthgps_root=str(s["healthgps_root"]),
        examples_root=str(s["examples_root"]),
        workspaces_root=str(s["workspaces_root"]),
    )


@app.get("/api/health")
def health() -> dict[str, str]:
    return {"status": "ok", "product": "HealthGPS Studio"}


def _mount_frontend() -> None:
    """Serve built frontend from / when frontend/dist exists (SPA fallback)."""
    gui_root = Path(__file__).resolve().parents[2]
    dist = gui_root / "frontend" / "dist"
    if not dist.is_dir():
        return

    assets = dist / "assets"
    if assets.is_dir():
        app.mount("/assets", StaticFiles(directory=assets), name="studio-assets")

    @app.get("/healthgps-logo.png", include_in_schema=False)
    def studio_logo() -> FileResponse:
        return FileResponse(dist / "healthgps-logo.png")

    @app.get("/{spa_path:path}", include_in_schema=False)
    def spa_fallback(spa_path: str) -> FileResponse:
        if spa_path.startswith("api"):
            raise HTTPException(status_code=404, detail="Not found")
        candidate = dist / spa_path
        if candidate.is_file():
            return FileResponse(candidate)
        return FileResponse(dist / "index.html")


_mount_frontend()
