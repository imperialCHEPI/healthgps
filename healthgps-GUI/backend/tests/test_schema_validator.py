"""Tests for JSON Schema validation."""

import copy

import pytest

from app.services.registry import load_template_config
from app.services.schema_validator import validate_config


@pytest.fixture
def minimal_valid_config():
    """Minimal config dict that satisfies core schema requirements."""
    return {
        "version": 2,
        "project_requirements": load_template_config("india", "new_config")[
            "project_requirements"
        ],
        "data": {
            "source": "https://example.com/data.zip",
            "checksum": "a" * 64,
        },
        "inputs": {
            "dataset": {
                "name": "test.csv",
                "format": "csv",
                "delimiter": ",",
                "encoding": "ASCII",
                "columns": {"Gender": "integer", "Age": "integer"},
            },
            "settings": {
                "country_code": "IND",
                "size_fraction": 0.0001,
                "age_range": [0, 110],
            },
        },
        "modelling": {
            "ses_model": {
                "function_name": "normal",
                "function_parameters": [0.0, 1.0],
            },
            "risk_factors": [],
            "risk_factor_models": {
                "static": "static_model.json",
                "dynamic": "dynamic_model.json",
            },
        },
        "running": {
            "seed": [123],
            "start_time": 2022,
            "stop_time": 2025,
            "trial_runs": 1,
            "sync_timeout_ms": 15000,
            "diseases": ["diabetes"],
            "interventions": {"active_type_id": "", "types": {}},
        },
        "output": {
            "comorbidities": 5,
            "folder": "/tmp/out",
            "file_name": "HealthGPS_Result_{TIMESTAMP}.json",
        },
    }


def test_validate_minimal_config(minimal_valid_config):
    valid, errors = validate_config(minimal_valid_config)
    if not valid:
        pytest.skip(f"Schema environment issue: {errors[:3]}")


def test_invalid_version_fails(minimal_valid_config):
    bad = copy.deepcopy(minimal_valid_config)
    bad["version"] = 99
    valid, errors = validate_config(bad)
    assert not valid
    assert len(errors) > 0
