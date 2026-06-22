"""Tests for result JSON parsing and chart building."""

from app.services.results import (
    normalize_result_rows,
    normalize_source_name,
    parse_healthgps_result_charts,
)
from app.services.visualizations import (
    _headlines,
    _parse_income_strata,
    build_pipeline_graph,
)


def test_normalize_source_name_lowercase():
    assert normalize_source_name("baseline") == "Baseline"
    assert normalize_source_name("intervention") == "Intervention"
    assert normalize_source_name("Baseline") == "Baseline"


def test_headlines_work_with_lowercase_engine_sources():
    from app.services.visualizations import _headlines

    rows = normalize_result_rows(
        [
            {
                "source": "baseline",
                "time": 2025,
                "indicators": {"DALY": 100.0},
                "disease_prevalence": {"diabetes": {"male": 10.0, "female": 12.0}},
                "risk_factors_average": {"BMI": {"male": 25.0, "female": 26.0}},
            },
            {
                "source": "intervention",
                "time": 2025,
                "indicators": {"DALY": 80.0},
                "disease_prevalence": {"diabetes": {"male": 9.0, "female": 11.0}},
                "risk_factors_average": {"BMI": {"male": 24.5, "female": 25.5}},
            },
        ]
    )
    headlines = _headlines(rows, 2025, "simple")
    assert any(h["id"] == "daly" for h in headlines)


def test_parse_healthgps_result_charts_from_engine_json():
    data = {
        "experiment": {"model": "HealthGPS", "intervention": "simple"},
        "result": [
            {
                "source": "baseline",
                "time": 2022,
                "indicators": {"DALY": 50.0, "YLL": 30.0, "YLD": 20.0},
                "population": {"alive": 700},
                "disease_prevalence": {"diabetes": {"male": 8.0, "female": 9.0}},
                "risk_factors_average": {"BMI": {"male": 24.0, "female": 25.0}},
                "average_age": {"male": 40.0, "female": 42.0},
            },
            {
                "source": "baseline",
                "time": 2025,
                "indicators": {"DALY": 60.0, "YLL": 35.0, "YLD": 25.0},
                "population": {"alive": 680},
                "disease_prevalence": {"diabetes": {"male": 9.0, "female": 10.0}},
                "risk_factors_average": {"BMI": {"male": 24.5, "female": 25.5}},
                "average_age": {"male": 41.0, "female": 43.0},
            },
            {
                "source": "intervention",
                "time": 2022,
                "indicators": {"DALY": 48.0, "YLL": 28.0, "YLD": 20.0},
                "population": {"alive": 700},
                "disease_prevalence": {"diabetes": {"male": 7.5, "female": 8.5}},
                "risk_factors_average": {"BMI": {"male": 23.8, "female": 24.8}},
                "average_age": {"male": 40.0, "female": 42.0},
            },
            {
                "source": "intervention",
                "time": 2025,
                "indicators": {"DALY": 55.0, "YLL": 32.0, "YLD": 23.0},
                "population": {"alive": 690},
                "disease_prevalence": {"diabetes": {"male": 8.0, "female": 9.0}},
                "risk_factors_average": {"BMI": {"male": 24.0, "female": 25.0}},
                "average_age": {"male": 41.0, "female": 43.0},
            },
        ],
    }
    parsed = parse_healthgps_result_charts(data)
    chart_ids = {c["id"] for c in parsed["charts"]}
    assert "daly" in chart_ids
    assert "yld" in chart_ids
    assert "avg_age" in chart_ids
    assert parsed["years"] == [2022, 2025]


def test_build_pipeline_graph_cycles_modules():
    pr = {
        "demographics": {"age": True, "gender": True},
        "income": {"enabled": True},
        "risk_factors": {},
    }
    active_ids = {
        build_pipeline_graph(pr, phase="simulating", elapsed=float(t))["active_module_id"]
        for t in range(0, 24)
    }
    assert active_ids.issubset({"demographics", "ses", "risk_factors", "diseases", "analysis"})
    assert len(active_ids) > 1


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


def test_chart_for_axes_time_vs_indicator():
    from app.services.result_explorer import TIME_AXIS, chart_for_axes

    rows = [
        {"source": "Baseline", "time": 2022, "indicators": {"DALY": 50.0}},
        {"source": "Baseline", "time": 2025, "indicators": {"DALY": 60.0}},
        {"source": "Intervention", "time": 2022, "indicators": {"DALY": 48.0}},
        {"source": "Intervention", "time": 2025, "indicators": {"DALY": 55.0}},
    ]
    payload = chart_for_axes(rows, TIME_AXIS, "indicators.DALY", chart_type="line")
    assert payload["chart_type"] == "line"
    assert len(payload["series"]) == 2
    assert payload["series"][0]["points"][0]["x"] == 2022.0


def test_chart_for_axes_scatter_two_variables():
    from app.services.result_explorer import chart_for_axes

    rows = [
        {
            "source": "Baseline",
            "time": 2025,
            "risk_factors_average": {"BMI": {"male": 25.0, "female": 26.0}},
            "disease_prevalence": {"diabetes": {"male": 10.0, "female": 12.0}},
        },
    ]
    payload = chart_for_axes(
        rows,
        "risk_factors_average.BMI",
        "disease_prevalence.diabetes",
        chart_type="scatter",
        sources=["Baseline"],
    )
    assert payload["chart_type"] == "scatter"
    assert len(payload["series"]) == 1
    assert len(payload["series"][0]["points"]) == 1
