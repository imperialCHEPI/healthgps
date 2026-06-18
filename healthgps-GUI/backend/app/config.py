"""Application paths and environment configuration."""

from __future__ import annotations

import os
from functools import lru_cache
from pathlib import Path


def _detect_healthgps_root() -> Path:
    env = os.environ.get("HEALTHGPS_ROOT")
    if env:
        return Path(env).resolve()
    # healthgps-GUI/backend/app/config.py -> repo root is parents[3]
    return Path(__file__).resolve().parents[3]


def _detect_examples_root(healthgps_root: Path) -> Path:
    env = os.environ.get("HEALTHGPS_EXAMPLES_ROOT")
    if env:
        return Path(env).resolve()
    sibling = healthgps_root.parent / "healthgps-examples"
    if sibling.is_dir():
        return sibling.resolve()
    return Path("C:/healthgps-examples").resolve()


@lru_cache
def get_settings() -> dict[str, Path | str | None]:
    gui_root = Path(__file__).resolve().parents[2]
    healthgps_root = _detect_healthgps_root()
    examples_root = _detect_examples_root(healthgps_root)
    workspaces_root = Path(
        os.environ.get(
            "HEALTHGPS_WORKSPACES_ROOT",
            Path.home() / "healthgps-workspaces",
        )
    ).resolve()
    console = os.environ.get("HEALTHGPS_CONSOLE")
    registry_path = gui_root / "projects" / "registry.json"
    schemas_dir = healthgps_root / "schemas" / "v1"
    scripts_dir = gui_root / "backend" / "scripts"
    return {
        "gui_root": gui_root,
        "healthgps_root": healthgps_root,
        "examples_root": examples_root,
        "workspaces_root": workspaces_root,
        "healthgps_console": console,
        "registry_path": registry_path,
        "schemas_dir": schemas_dir,
        "scripts_dir": scripts_dir,
    }
