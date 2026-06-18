"""Tests for project registry."""

import pytest

from app.services.registry import (
    RegistryError,
    get_project,
    list_projects,
    load_template_config,
    resolve_config_path,
)


def test_list_projects_has_three_entries():
    projects = list_projects()
    ids = {p["id"] for p in projects}
    assert ids == {"finch", "india", "pif", "hlm_france"}


def test_get_project_finch():
    project = get_project("finch")
    assert project["name"] == "FINCH (UK)"
    assert project["example_dir"] == "KevinHall_FINCH"


def test_unknown_project_raises():
    with pytest.raises(RegistryError):
        get_project("unknown")


def test_resolve_finch_config_json():
    path = resolve_config_path("finch", "config")
    assert path.name == "config.json"
    assert "KevinHall_FINCH" in str(path)
    assert path.is_file()


def test_load_template_config_india():
    config = load_template_config("india", "config")
    assert "inputs" in config
