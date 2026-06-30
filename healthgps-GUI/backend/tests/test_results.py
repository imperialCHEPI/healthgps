"""Tests for results file grouping."""

from pathlib import Path

from app.services.results import (
    engine_output_folder,
    expand_output_folder,
    latest_run_files,
    parse_healthgps_result_charts,
    run_stamp_from_name,
)


def test_run_stamp_from_name():
    assert run_stamp_from_name("HealthGPS_Result_2026-05-20_16-01-57.json") == (
        "2026-05-20_16-01-57"
    )
    assert (
        run_stamp_from_name("HealthGPS_Result_2026-05-20_16-01-57_LowIncome.csv")
        == "2026-05-20_16-01-57"
    )
    assert run_stamp_from_name("other.txt") is None


def test_engine_output_folder_windows_style():
    path = engine_output_folder("${HOME}/healthgps/results/finch")
    assert path is not None
    assert path.name == "finch"
    assert "healthgps" in str(path).lower()


def test_expand_output_folder_uses_home():
    path = expand_output_folder("${HOME}/healthgps/results/finch")
    assert path.name == "finch"
    assert str(path.home()) in str(path) or path.is_absolute()


def test_parse_healthgps_result_charts():
    data = {
        "experiment": {"intervention": "simple"},
        "result": [
            {
                "source": "Baseline",
                "time": 2022,
                "indicators": {"DALY": 100.0},
                "population": {"alive": 5000, "dead": 0},
            },
            {
                "source": "Baseline",
                "time": 2023,
                "indicators": {"DALY": 150.0},
                "population": {"alive": 4900, "dead": 100},
            },
            {
                "source": "Intervention",
                "time": 2022,
                "indicators": {"DALY": 100.0},
                "population": {"alive": 5000, "dead": 0},
            },
            {
                "source": "Intervention",
                "time": 2023,
                "indicators": {"DALY": 120.0},
                "population": {"alive": 4950, "dead": 50},
            },
        ],
    }
    parsed = parse_healthgps_result_charts(data)
    assert parsed["years"] == [2022, 2023]
    assert parsed["experiment"]["intervention"] == "simple"
    chart_ids = {c["id"] for c in parsed["charts"]}
    assert "daly" in chart_ids
    assert "population" in chart_ids
    daly = next(c for c in parsed["charts"] if c["id"] == "daly")
    assert len(daly["series"]) == 2
    assert daly["series"][0]["points"][0]["x"] == 2022.0


def test_latest_run_files_picks_newest_only(tmp_path: Path):
    old = tmp_path / "HealthGPS_Result_2026-05-20_09-36-00.json"
    new = tmp_path / "HealthGPS_Result_2026-05-20_16-01-57.json"
    new_csv = tmp_path / "HealthGPS_Result_2026-05-20_16-01-57.csv"
    old.write_text("{}", encoding="utf-8")
    new.write_text("{}", encoding="utf-8")
    new_csv.write_text("a,b", encoding="utf-8")

    stamp, files, found = latest_run_files([tmp_path])
    assert found == tmp_path
    assert stamp == "2026-05-20_16-01-57"
    assert len(files) == 2
    assert all("16-01-57" in f["name"] for f in files)
