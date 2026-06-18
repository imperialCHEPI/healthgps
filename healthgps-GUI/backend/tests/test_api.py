"""FastAPI integration tests."""

from fastapi.testclient import TestClient

from app.main import app

client = TestClient(app)


def test_health():
    res = client.get("/api/health")
    assert res.status_code == 200
    assert res.json()["product"] == "HealthGPS Studio"


def test_list_projects():
    res = client.get("/api/projects")
    assert res.status_code == 200
    data = res.json()
    assert len(data) >= 3


def test_get_finch_project():
    res = client.get("/api/projects/finch")
    assert res.status_code == 200
    assert res.json()["id"] == "finch"
    assert "default_project_requirements" in res.json()
    assert "model_risk_factors" in res.json()


def test_custom_countries():
    res = client.get("/api/custom/countries")
    assert res.status_code == 200
    data = res.json()
    assert len(data) >= 3
    assert any(c["id"] == "fra" for c in data)


def test_get_hlm_france_legacy_defaults():
    res = client.get("/api/projects/hlm_france")
    assert res.status_code == 200
    data = res.json()
    assert data["id"] == "hlm_france"
    pr = data["default_project_requirements"]
    assert pr.get("demographics", {}).get("age") is True
    assert isinstance(data.get("model_risk_factors"), list)


def test_create_workspace():
    india = client.get("/api/projects/india").json()
    pr = india["default_project_requirements"]
    body = {
        "project_id": "india",
        "config_variant": "config",
        "project_requirements": pr if pr else {
            "demographics": {"age": True, "gender": True, "region": False, "ethnicity": False},
            "income": {
                "enabled": True,
                "type": "categorical",
                "categories": "3",
                "adjust_to_factors_mean": False,
                "trended": False,
                "income_based_csv_output": True,
            },
            "physical_activity": {
                "enabled": True,
                "type": "simple",
                "adjust_to_factors_mean": False,
                "trended": False,
            },
            "risk_factors": {"adjust_to_factors_mean": True, "trended": True},
            "trend": {"enabled": True, "type": "income_trend"},
            "two_stage": {"use_logistic": False},
        },
        "run_settings": {
            "size_fraction": 0.0001,
            "age_range_min": 0,
            "age_range_max": 110,
            "start_time": 2022,
            "stop_time": 2025,
            "trial_runs": 1,
            "active_intervention": "",
            "thread_count": 4,
        },
    }
    res = client.post("/api/workspaces", json=body)
    assert res.status_code == 200
    ws_id = res.json()["id"]
    assert ws_id

    res2 = client.get(f"/api/workspaces/{ws_id}")
    assert res2.status_code == 200

    res3 = client.post(f"/api/workspaces/{ws_id}/validate-schema")
    assert res3.status_code == 200
