"""Program and country catalog for HealthGPS Studio."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from app.config import get_settings
from app.services.registry import RegistryError, example_dir_path, get_project


class CatalogError(Exception):
    pass


def load_catalog() -> dict[str, Any]:
    gui_root: Path = get_settings()["gui_root"]  # type: ignore[assignment]
    path = gui_root / "projects" / "catalog.json"
    if not path.is_file():
        raise CatalogError(f"Catalog not found: {path}")
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def list_catalog() -> list[dict[str, Any]]:
    data = load_catalog()
    programs = []
    for program in data.get("programs", []):
        countries = []
        for country in program.get("countries", []):
            entry = {**country}
            if program.get("status") == "active" and country.get("project_id"):
                try:
                    proj = get_project(country["project_id"])
                    ex_dir = example_dir_path(proj)
                    entry["example_path"] = str(ex_dir)
                    entry["available"] = ex_dir.is_dir()
                except RegistryError:
                    entry["available"] = False
            else:
                entry["available"] = False
            countries.append(entry)
        programs.append({**program, "countries": countries})
    return programs
