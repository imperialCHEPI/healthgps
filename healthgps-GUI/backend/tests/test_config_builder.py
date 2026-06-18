"""Tests for config merge logic."""

from app.models.studio import RunSettings
from app.services.config_builder import apply_overrides, state_from_template
from app.services.registry import load_template_config


def test_state_from_template_matches_finch_new_config():
    template = load_template_config("finch", "new_config")
    state = state_from_template(template["project_requirements"])
    assert state.demographics.region is True
    assert state.income.type == "continuous"
    assert state.income.categories in ("4", "5")


def test_apply_overrides_updates_size_fraction():
    template = load_template_config("india", "config")
    pr = template.get("project_requirements") or {}
    if not pr:
        pr = load_template_config("india", "new_config")["project_requirements"]
    state = state_from_template(pr)
    run = RunSettings(size_fraction=0.0001, stop_time=2024)
    config = apply_overrides(template, state, run)
    assert config["inputs"]["settings"]["size_fraction"] == 0.0001
    assert config["running"]["stop_time"] == 2024


def test_apply_overrides_baseline_intervention():
    template = load_template_config("finch", "config")
    state = state_from_template({})
    run = RunSettings(active_intervention="")
    config = apply_overrides(template, state, run)
    assert config["running"]["interventions"]["active_type_id"] == ""
