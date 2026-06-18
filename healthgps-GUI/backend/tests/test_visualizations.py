"""Tests for visualization bundle builder."""

from app.services.visualizations import (
    build_pipeline_graph,
    load_visualization_bundle,
    _headlines,
    _parse_income_strata,
)


def test_build_pipeline_graph_active_module():
    pr = {
        "demographics": {"age": True, "gender": True},
        "income": {"enabled": True},
        "risk_factors": {},
    }
    graph = build_pipeline_graph(pr, active_phase="simulating")
    assert graph["active_module_id"] == "diseases"
    statuses = {m["id"]: m["status"] for m in graph["modules"]}
    assert statuses["demographics"] == "done"
    assert statuses["diseases"] == "active"


def test_headlines_from_result_rows():
    rows = [
        {
            "source": "Baseline",
            "time": 2025,
            "indicators": {"DALY": 100.0},
            "disease_prevalence": {"diabetes": {"male": 10.0, "female": 12.0}},
            "risk_factors_average": {"BMI": {"male": 25.0, "female": 26.0}},
        },
        {
            "source": "Intervention",
            "time": 2025,
            "indicators": {"DALY": 80.0},
            "disease_prevalence": {"diabetes": {"male": 9.0, "female": 11.0}},
            "risk_factors_average": {"BMI": {"male": 24.5, "female": 25.5}},
        },
    ]
    headlines = _headlines(rows, 2025, "simple")
    assert any(h["id"] == "daly" for h in headlines)
    daly = next(h for h in headlines if h["id"] == "daly")
    assert daly["delta"] == -20.0


def test_parse_income_strata_empty_without_files():
    assert _parse_income_strata([]) == []
