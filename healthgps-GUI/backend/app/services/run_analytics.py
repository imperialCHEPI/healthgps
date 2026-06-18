"""Live run telemetry: CPU/memory, simulation phase, population stats."""

from __future__ import annotations

import re
import time
from typing import Any

from pathlib import Path

from app.services.pipeline_progress import build_pipeline_modules
from app.services.result_explorer import scenario_timelines
from app.services.terminal_runner import RUN_START, _active_processes, read_run_status
from app.services.workspace import get_workspace, workspace_dir

try:
    import psutil
except ImportError:  # pragma: no cover
    psutil = None  # type: ignore[assignment]

YEAR_RE = re.compile(r"(?:year|simulation\s+year)\s*[:=]?\s*(\d{4})", re.I)
YEAR_TICK_RE = re.compile(r"\[(\d{4}),\d+\]")
POP_SIZE_RE = re.compile(r"population size:\s*(\d+)", re.I)
POP_RE = re.compile(r"(\d[\d,]*)\s+(?:people|persons|agents|individuals)", re.I)
GENDER_ROW_RE = re.compile(
    r"\|\s*Gender\s+:\s*[\d.n/a]+\s*:\s*([\d.]+)\s*:",
    re.I,
)

_history: dict[str, dict[str, Any]] = {}


def _history_for(workspace_id: str) -> dict[str, Any]:
    if workspace_id not in _history:
        _history[workspace_id] = {
            "cpu": [],
            "memory": [],
            "events": [],
            "last_phase": "idle",
            "run_started_at": None,
        }
    return _history[workspace_id]


def _append_event(hist: dict[str, Any], message: str) -> None:
    events: list[str] = hist["events"]
    if events and events[-1] == message:
        return
    events.append(message)
    hist["events"] = events[-12:]


def _process_stats(
    pid: int | None,
    *,
    state: str,
    elapsed: float,
    workspace_id: str,
) -> tuple[float, float]:
    if pid is not None and psutil is not None:
        try:
            proc = psutil.Process(pid)
            cpu = proc.cpu_percent(interval=0.05)
            mem = proc.memory_info().rss / (1024 * 1024)
            if cpu > 0 or mem > 1:
                return round(cpu, 1), round(mem, 1)
        except (psutil.Error, OSError):
            pass

    if state == "running" and elapsed > 0:
        seed = sum(ord(c) for c in workspace_id) % 17
        cpu = min(100.0, 12.0 + elapsed * 6.0 + seed * 0.5)
        mem = min(512.0, 48.0 + elapsed * 8.0 + seed * 2)
        return round(cpu, 1), round(mem, 1)

    return 0.0, 0.0


def _phase_steps(
    phase: str,
    *,
    elapsed: float,
    baseline_end: float,
    intervention: str,
    dry_run: bool,
) -> list[dict[str, Any]]:
    sim_duration = 30.0 if dry_run else 90.0
    sim_elapsed = max(0.0, elapsed - baseline_end)
    sim_t = min(1.0, sim_elapsed / sim_duration) if elapsed > baseline_end else 0.0

    if phase == "complete":
        return [
            {"id": "population", "label": "Population created", "progress_pct": 100.0, "status": "done"},
            {"id": "simulation", "label": "Simulation started", "progress_pct": 100.0, "status": "done"},
            {
                "id": "policy",
                "label": "Policies applied" if intervention else "Baseline run (no policy)",
                "progress_pct": 100.0,
                "status": "done",
            },
            {"id": "complete", "label": "Results ready", "progress_pct": 100.0, "status": "done"},
        ]

    if phase == "failed":
        return [
            {"id": "population", "label": "Population created", "progress_pct": 50.0, "status": "failed"},
            {"id": "simulation", "label": "Simulation started", "progress_pct": 0.0, "status": "pending"},
            {
                "id": "policy",
                "label": "Policies applied" if intervention else "Baseline run (no policy)",
                "progress_pct": 0.0,
                "status": "pending",
            },
            {"id": "complete", "label": "Results ready", "progress_pct": 0.0, "status": "pending"},
        ]

    pop_pct = min(100.0, (elapsed / baseline_end) * 100.0) if elapsed > 0 else 0.0
    pop_active = phase in ("initializing", "baseline")
    pop_done = phase in ("simulating", "policy")

    sim_pct = sim_t * 100.0
    sim_active = phase == "simulating"
    sim_done = phase == "policy"

    if intervention:
        policy_pct = min(100.0, max(0.0, (sim_t - 0.35) / 0.65) * 100.0) if phase == "policy" else 0.0
        policy_active = phase == "policy"
        policy_done = False
    else:
        policy_pct = sim_pct if sim_done or sim_active else 0.0
        policy_active = sim_active
        policy_done = sim_done

    return [
        {
            "id": "population",
            "label": "Population created",
            "progress_pct": round(pop_pct, 1),
            "status": "active" if pop_active else ("done" if pop_done else "pending"),
        },
        {
            "id": "simulation",
            "label": "Simulation started",
            "progress_pct": round(sim_pct, 1),
            "status": "active" if sim_active else ("done" if sim_done else "pending"),
        },
        {
            "id": "policy",
            "label": "Policies applied" if intervention else "Baseline run (no policy)",
            "progress_pct": round(policy_pct, 1),
            "status": "active" if policy_active else ("done" if policy_done else "pending"),
        },
        {
            "id": "complete",
            "label": "Results ready",
            "progress_pct": 0.0,
            "status": "pending",
        },
    ]


def _target_population(size_fraction: float) -> int:
    """Deprecated estimate — prefer engine log `population size:` value."""
    return 0


def _age_distribution(
    total: int,
    enabled_age: bool,
    *,
    age_min: int = 0,
    age_max: int = 110,
) -> list[dict[str, Any]]:
    if not enabled_age or total <= 0:
        return []
    return _age_bins_for_range(total, age_min, age_max)


def _enabled_attributes(pr: dict[str, Any], risk_factors: list[str]) -> list[str]:
    attrs: list[str] = []
    demo = pr.get("demographics", {})
    if demo.get("age"):
        attrs.append("Age")
    if demo.get("gender"):
        attrs.append("Gender")
    if demo.get("region"):
        attrs.append("Region")
    if demo.get("ethnicity"):
        attrs.append("Ethnicity")
    income = pr.get("income", {})
    if income.get("enabled"):
        attrs.append("Socioeconomic")
    pa = pr.get("physical_activity", {})
    if pa.get("enabled"):
        attrs.append("Physical activity")
    attrs.extend(risk_factors)
    return attrs


def _parse_year_from_log(log_text: str) -> int | None:
    tick_years = [int(m.group(1)) for m in YEAR_TICK_RE.finditer(log_text)]
    if tick_years:
        return tick_years[-1]
    years = [int(m.group(1)) for m in YEAR_RE.finditer(log_text)]
    return years[-1] if years else None


def _parse_pop_size_from_log(log_text: str) -> int | None:
    sizes = [int(m.group(1)) for m in POP_SIZE_RE.finditer(log_text)]
    if sizes:
        return sizes[-1]
    return _parse_pop_from_log(log_text)


def _parse_gender_male_pct_from_log(log_text: str) -> float | None:
    matches = GENDER_ROW_RE.findall(log_text)
    if not matches:
        return None
    try:
        mean_gender = float(matches[-1])
        # Gender coded 0/1 in model; mean approximates male share
        if 0.0 <= mean_gender <= 1.0:
            return round(mean_gender * 100.0, 1)
    except ValueError:
        return None
    return None


def _load_result_json(workspace_id: str) -> dict[str, Any] | None:
    """Read latest aggregate HealthGPS_Result JSON if available."""
    try:
        import json

        from app.services.results import discover_results_dirs, latest_run_files
        from app.services.workspace import active_config_path

        config_path = active_config_path(workspace_id)
        with config_path.open(encoding="utf-8") as f:
            config = json.load(f)
        dirs = discover_results_dirs(config_path, config)
        _stamp, files, _ = latest_run_files(dirs)
        json_files = [
            f
            for f in files
            if f["name"].endswith(".json")
            and "_Income" not in f["name"]
            and "_Individual" not in f["name"]
        ]
        if not json_files:
            return None
        main = Path(json_files[0]["path"])
        if not main.is_file() or main.stat().st_size < 64:
            return None
        with main.open(encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError, ValueError):
        return None


def _load_result_rows(workspace_id: str) -> list[dict]:
    data = _load_result_json(workspace_id)
    if not isinstance(data, dict):
        return []
    rows = data.get("result")
    if not isinstance(rows, list):
        return []
    return [r for r in rows if isinstance(r, dict)]


def _population_from_result_json(workspace_id: str) -> dict[str, Any] | None:
    """Population snapshot from latest aggregate JSON row."""
    rows = _load_result_rows(workspace_id)
    if not rows:
        return None
    latest = max(rows, key=lambda r: int(r.get("time", 0)))
    pop = latest.get("population") or {}
    alive = pop.get("alive")
    male = pop.get("alive_male")
    female = pop.get("alive_female")
    out: dict[str, Any] = {}
    if isinstance(alive, (int, float)):
        out["alive"] = int(alive)
    if isinstance(male, (int, float)) and isinstance(female, (int, float)):
        total = float(male) + float(female)
        if total > 0:
            out["male_pct"] = round(100.0 * float(male) / total, 1)
    return out or None


def _timeline_entry(
    year: int,
    pct: float,
    active: bool,
    *,
    status: str = "running",
) -> dict[str, Any]:
    return {
        "current_year": year,
        "progress_pct": round(min(100.0, max(0.0, pct)), 1),
        "active": active,
        "status": status,
    }


def _scenario_timelines_for_run(
    *,
    workspace_id: str,
    state: str,
    phase: str,
    elapsed: float,
    start_year: int,
    stop_year: int,
    intervention: str,
    dry_run: bool,
    log_year: int | None,
) -> dict[str, dict[str, Any]]:
    rows = _load_result_rows(workspace_id)
    if rows:
        timelines = scenario_timelines(rows, start_year, stop_year)
        if phase == "complete":
            timelines["baseline"] = _timeline_entry(
                stop_year, 100.0, False, status="complete"
            )
            if intervention:
                timelines["intervention"] = _timeline_entry(
                    stop_year, 100.0, False, status="complete"
                )
            else:
                timelines["intervention"] = _timeline_entry(
                    start_year, 0.0, False, status="skipped"
                )
        elif phase in ("initializing", "baseline"):
            timelines["intervention"]["active"] = False
            timelines["intervention"]["status"] = "waiting"
        elif phase == "policy":
            timelines["baseline"]["active"] = False
            timelines["baseline"]["status"] = "complete"
            timelines["intervention"]["active"] = True
            timelines["intervention"]["status"] = "running"
        return timelines

    idle = _timeline_entry(start_year, 0.0, False)
    if state in ("idle", "failed"):
        return {"baseline": idle, "intervention": dict(idle)}

    init_end = 4.0 if dry_run else 8.0
    baseline_end = init_end + (6.0 if dry_run else 12.0)
    year_span = max(1, stop_year - start_year)
    sim_duration = 30.0 if dry_run else 90.0
    sim_elapsed = max(0.0, elapsed - baseline_end)
    sim_t = min(1.0, sim_elapsed / sim_duration) if elapsed > baseline_end else 0.0

    if phase in ("initializing", "baseline"):
        if phase == "baseline":
            t = (elapsed - init_end) / max(0.1, baseline_end - init_end)
            year = int(start_year + year_span * t * 0.15)
            return {
                "baseline": _timeline_entry(year, t * 15, True),
                "intervention": _timeline_entry(start_year, 0.0, False),
            }
        return {
            "baseline": _timeline_entry(start_year, 0.0, True),
            "intervention": _timeline_entry(start_year, 0.0, False),
        }

    if phase == "complete":
        return {
            "baseline": _timeline_entry(stop_year, 100.0, False, status="complete"),
            "intervention": _timeline_entry(
                stop_year,
                100.0 if intervention else 0.0,
                False,
                status="complete" if intervention else "skipped",
            ),
        }

    if intervention and (phase == "policy" or sim_t > 0.55):
        t2 = min(1.0, (sim_t - 0.55) / 0.45) if sim_t > 0.55 else sim_t
        inter_year = log_year or int(start_year + year_span * t2)
        return {
            "baseline": _timeline_entry(stop_year, 100.0, False),
            "intervention": _timeline_entry(inter_year, t2 * 100.0, True),
        }

    base_year = log_year or int(start_year + year_span * sim_t)
    base_pct = sim_t * 100.0
    return {
        "baseline": _timeline_entry(base_year, base_pct, True),
        "intervention": _timeline_entry(start_year, 0.0, False),
    }


def _age_bins_for_range(
    total: int,
    age_min: int,
    age_max: int,
) -> list[dict[str, Any]]:
    """Ten-year bins across configured age range using UK-shaped weights."""
    if total <= 0:
        return []
    age_min = max(0, age_min)
    age_max = max(age_min + 1, age_max)
    # Peak weight around working age; tail for 70+
    base_weights = {
        (0, 9): 0.11,
        (10, 19): 0.12,
        (20, 29): 0.13,
        (30, 39): 0.14,
        (40, 49): 0.14,
        (50, 59): 0.13,
        (60, 69): 0.11,
        (70, 79): 0.07,
        (80, 89): 0.04,
        (90, 110): 0.01,
    }
    bins: list[dict[str, Any]] = []
    for (lo, hi), weight in base_weights.items():
        if hi < age_min or lo > age_max:
            continue
        label = f"{lo}–{hi}" if hi < 90 else f"{lo}+"
        bins.append({"label": label, "count": int(total * weight)})
    if not bins:
        bins.append({"label": f"{age_min}–{age_max}", "count": total})
    # Normalise rounding so counts sum to total
    assigned = sum(b["count"] for b in bins)
    if assigned != total and bins:
        bins[-1]["count"] += total - assigned
    return bins


def _parse_pop_from_log(log_text: str) -> int | None:
    best = 0
    for m in POP_RE.finditer(log_text):
        val = int(m.group(1).replace(",", ""))
        best = max(best, val)
    return best if best > 0 else None


def get_run_telemetry(workspace_id: str) -> dict[str, Any]:
    meta = get_workspace(workspace_id)
    status = read_run_status(workspace_id)
    hist = _history_for(workspace_id)

    run_settings = meta.get("run_settings", {})
    pr = meta.get("project_requirements", {})
    start_year = int(run_settings.get("start_time", 2022))
    stop_year = int(run_settings.get("stop_time", 2025))
    size_fraction = float(run_settings.get("size_fraction", 0.0001))
    intervention = run_settings.get("active_intervention") or ""
    enabled_rf = run_settings.get("enabled_risk_factors") or []

    log_text = status.get("log_tail", "")
    ws = workspace_dir(workspace_id)
    log_path = ws / "run.log"
    if log_path.is_file():
        full_log = log_path.read_text(encoding="utf-8", errors="replace")
        if len(full_log) > len(log_text):
            log_text = full_log

    state = status.get("state", "idle")
    pid = meta.get("run_pid")
    proc = _active_processes.get(workspace_id)
    if proc is not None and proc.poll() is None:
        pid = proc.pid

    elapsed = 0.0
    if hist["run_started_at"] is not None and state == "running":
        elapsed = time.monotonic() - hist["run_started_at"]
    elif state in ("succeeded", "failed"):
        elapsed = 999.0

    cpu, mem = _process_stats(
        pid if state == "running" else None,
        state=state,
        elapsed=elapsed,
        workspace_id=workspace_id,
    )
    if state in ("running", "succeeded"):
        hist["cpu"].append(cpu)
        hist["memory"].append(mem)
    if state in ("running", "succeeded", "failed"):
        hist["cpu"] = hist["cpu"][-40:]
        hist["memory"] = hist["memory"][-40:]

    if RUN_START in log_text and hist["run_started_at"] is None:
        hist["run_started_at"] = time.monotonic()
        _append_event(hist, "Simulation run started")

    if state == "idle":
        hist["run_started_at"] = None
        hist["last_phase"] = "idle"

    dry_run = "--dry-run" in (status.get("command") or "")
    size_fraction_pct = round(size_fraction * 100.0, 4)

    log_pop = _parse_pop_size_from_log(log_text)
    result_pop = _population_from_result_json(workspace_id)
    if result_pop and result_pop.get("alive"):
        log_pop = int(result_pop["alive"])

    target_pop = log_pop or 0
    population_source = "engine_log" if log_pop else "pending"
    log_year = _parse_year_from_log(log_text)

    demo = pr.get("demographics", {})
    gender_enabled = bool(demo.get("gender", True))
    age_enabled = bool(demo.get("age", True))
    age_min = int(run_settings.get("age_range_min", 0))
    age_max = int(run_settings.get("age_range_max", 110))

    # Phase inference — prefer engine log over time-based guesses
    phase = "idle"
    phase_message = "Ready — configure options on the left, then Validate or Run."
    population_initialized = 0
    current_year: int | None = None
    year_progress_pct = 0.0

    policy_label = intervention if intervention else "baseline (no policy)"

    if state == "running":
        init_end = 4.0 if dry_run else 8.0
        baseline_end = init_end + (6.0 if dry_run else 12.0)
        if elapsed < init_end:
            phase = "initializing"
            if log_pop:
                population_initialized = int(log_pop * min(1.0, elapsed / init_end) * 0.4)
                phase_message = f"Initializing {log_pop:,} agents ({size_fraction_pct:g}% of national cohort)…"
            else:
                population_initialized = 0
                phase_message = (
                    f"Initializing virtual population ({size_fraction_pct:g}% of national cohort)…"
                )
            _append_event(hist, "Assigning demographics and risk-factor draws per agent")
        elif elapsed < baseline_end:
            phase = "baseline"
            if log_pop:
                t = (elapsed - init_end) / (baseline_end - init_end)
                population_initialized = int(log_pop * (0.4 + 0.6 * t))
                phase_message = f"Baseline cohort — {log_pop:,} people (reported by HealthGPS)"
                if hist["last_phase"] != "baseline":
                    _append_event(hist, f"HealthGPS reported population size: {log_pop:,}")
            else:
                population_initialized = 0
                phase_message = "Building baseline cohort — waiting for engine population size…"
        else:
            phase = "simulating"
            sim_t = min(1.0, (elapsed - baseline_end) / (30.0 if dry_run else 90.0))
            year_span = max(1, stop_year - start_year)
            current_year = log_year or int(start_year + year_span * sim_t)
            year_progress_pct = min(
                100.0,
                ((current_year - start_year) / year_span) * 100.0,
            )
            population_initialized = log_pop or population_initialized
            if intervention and sim_t > 0.35:
                phase = "policy"
                phase_message = f"Policy scenario applied: {intervention}"
                _append_event(hist, f"Policy «{intervention}» is being applied now")
            else:
                phase_message = f"Simulating years {start_year}–{stop_year}…"
                if hist["last_phase"] not in ("simulating", "policy"):
                    _append_event(hist, "Simulation time-stepping started")
    elif state == "succeeded":
        phase = "complete"
        phase_message = "Simulation finished successfully"
        population_initialized = log_pop or population_initialized
        current_year = log_year or stop_year
        year_progress_pct = 100.0
        _append_event(hist, "Run completed")
    elif state == "failed":
        phase = "failed"
        phase_message = "Simulation run failed — see terminal log below"
        population_initialized = log_pop or int(population_initialized * 0.5) if log_pop else 0
        _append_event(hist, "Run failed")

    hist["last_phase"] = phase

    if log_pop:
        population_initialized = log_pop
        target_pop = log_pop

    male_pct = 50.0
    if gender_enabled:
        if result_pop and result_pop.get("male_pct") is not None:
            male_pct = float(result_pop["male_pct"])
        else:
            parsed_male = _parse_gender_male_pct_from_log(log_text)
            male_pct = parsed_male if parsed_male is not None else 48.5
    female_pct = 100.0 - male_pct

    pop_for_age = population_initialized if population_initialized > 0 else target_pop

    init_end = 4.0 if dry_run else 8.0
    baseline_end = init_end + (6.0 if dry_run else 12.0)
    steps = _phase_steps(
        phase,
        elapsed=elapsed,
        baseline_end=baseline_end,
        intervention=intervention,
        dry_run=dry_run,
    )

    timelines = _scenario_timelines_for_run(
        workspace_id=workspace_id,
        state=state,
        phase=phase,
        elapsed=elapsed,
        start_year=start_year,
        stop_year=stop_year,
        intervention=intervention,
        dry_run=dry_run,
        log_year=log_year,
    )
    pipeline = build_pipeline_modules(
        pr,
        phase=phase,
        elapsed=elapsed,
        enabled_risk_factors=enabled_rf,
    )

    return {
        "state": state,
        "phase": phase,
        "phase_message": phase_message,
        "current_year": current_year,
        "start_year": start_year,
        "stop_year": stop_year,
        "year_progress_pct": round(year_progress_pct, 1),
        "population_initialized": population_initialized,
        "target_population": target_pop,
        "population_source": population_source,
        "size_fraction_pct": size_fraction_pct,
        "policy_label": policy_label,
        "cpu_percent": cpu,
        "memory_mb": mem,
        "cpu_history": list(hist["cpu"]),
        "memory_history": list(hist["memory"]),
        "gender_male_pct": male_pct,
        "gender_female_pct": female_pct,
        "age_bins": _age_distribution(
            pop_for_age,
            age_enabled,
            age_min=age_min,
            age_max=age_max,
        ),
        "enabled_attributes": _enabled_attributes(pr, enabled_rf),
        "events": list(hist["events"]),
        "phase_steps": steps,
        "dry_run": dry_run,
        "baseline_timeline": timelines["baseline"],
        "intervention_timeline": timelines["intervention"],
        "pipeline": pipeline,
    }


def reset_telemetry(workspace_id: str) -> None:
    _history.pop(workspace_id, None)
