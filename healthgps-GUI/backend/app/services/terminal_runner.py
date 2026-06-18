"""Run HealthGPS.Console and stream output to workspace log (single output surface)."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

from app.config import get_settings
from app.services.results import normalize_output_folder
from app.services.workspace import (
    active_config_path,
    get_workspace,
    set_run_status,
    workspace_dir,
)

RUN_START = "=== HealthGPS Studio run started ==="
RUN_FINISH_RE = re.compile(
    r"=== HealthGPS Studio run finished: exit (-?\d+) ==="
)

_active_processes: dict[str, subprocess.Popen] = {}


class TerminalRunnerError(Exception):
    pass


def build_command(
    config_path: Path,
    thread_count: int,
    dry_run: bool = False,
) -> str:
    settings = get_settings()
    console = settings["healthgps_console"]
    if not console:
        raise TerminalRunnerError(
            "HEALTHGPS_CONSOLE is not set. Point it to your HealthGPS.Console.exe build."
        )

    parts = [
        f'"{console}"',
        f'-c "{config_path}"',
        f"-T{thread_count}",
    ]
    if dry_run:
        parts.append("--dry-run")
    return " ".join(parts)


def launch_run(workspace_id: str, dry_run: bool = False) -> dict:
    """Run HealthGPS.Console in the background; output only in run.log (GUI tail)."""
    settings = get_settings()
    console = settings["healthgps_console"]
    if not console:
        raise TerminalRunnerError("HEALTHGPS_CONSOLE is not set.")

    meta = get_workspace(workspace_id)
    config_path = active_config_path(workspace_id)
    ws = workspace_dir(workspace_id)
    log_path = ws / "run.log"
    thread_count = meta.get("thread_count", 4)

    if workspace_id in _active_processes:
        proc = _active_processes[workspace_id]
        if proc.poll() is None:
            raise TerminalRunnerError("A run is already in progress for this workspace.")

    log_path.write_text("", encoding="utf-8")
    display_command = build_command(config_path, thread_count, dry_run=dry_run)
    (ws / "run.command").write_text(display_command + "\n", encoding="utf-8")

    with log_path.open("a", encoding="utf-8") as log:
        log.write(f"{RUN_START}\n")
        log.write(f"Command: {display_command}\n")
        log.write(f"Working directory: {config_path.parent}\n\n")

    args = [
        console,
        "-c",
        str(config_path),
        f"-T{thread_count}",
    ]
    if dry_run:
        args.append("--dry-run")

    creationflags = 0
    if sys.platform == "win32":
        creationflags = subprocess.CREATE_NO_WINDOW  # type: ignore[attr-defined]

    env = os.environ.copy()
    home = str(Path.home())
    env.setdefault("HOME", home)
    env.setdefault("USERPROFILE", home)

    try:
        with config_path.open(encoding="utf-8") as cfg_file:
            cfg = json.load(cfg_file)
        out_folder = cfg.get("output", {}).get("folder", "")
        if out_folder:
            resolved = Path(normalize_output_folder(str(out_folder)))
            resolved.mkdir(parents=True, exist_ok=True)
            if "${HOME}" in str(out_folder):
                cfg.setdefault("output", {})
                cfg["output"]["folder"] = str(resolved)
                with config_path.open("w", encoding="utf-8") as cfg_file:
                    json.dump(cfg, cfg_file, indent=4)
    except (OSError, json.JSONDecodeError):
        pass

    log_handle = log_path.open("a", encoding="utf-8")
    proc = subprocess.Popen(
        args,
        cwd=str(config_path.parent),
        stdout=log_handle,
        stderr=subprocess.STDOUT,
        env=env,
        creationflags=creationflags,
    )
    _active_processes[workspace_id] = proc
    set_run_status(workspace_id, "running", run_pid=proc.pid)

    return {
        "state": "running",
        "terminal_launched": False,
        "command": display_command,
    }


def read_run_status(workspace_id: str, tail_lines: int = 120) -> dict:
    ws = workspace_dir(workspace_id)
    log_path = ws / "run.log"
    meta = get_workspace(workspace_id)

    log_tail = ""
    exit_code: int | None = meta.get("last_exit_code")
    state = meta.get("last_run_status", "idle")

    proc = _active_processes.get(workspace_id)
    if proc is not None:
        code = proc.poll()
        if code is not None:
            with log_path.open("a", encoding="utf-8") as log:
                log.write(f"\n=== HealthGPS Studio run finished: exit {code} ===\n")
            state = "succeeded" if code == 0 else "failed"
            exit_code = code
            set_run_status(workspace_id, state, exit_code)
            del _active_processes[workspace_id]

    if log_path.is_file():
        lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
        log_tail = "\n".join(lines[-tail_lines:])

        if state not in ("succeeded", "failed"):
            found_finish = False
            for line in reversed(lines):
                match = RUN_FINISH_RE.search(line)
                if match:
                    exit_code = int(match.group(1))
                    state = "succeeded" if exit_code == 0 else "failed"
                    set_run_status(workspace_id, state, exit_code)
                    found_finish = True
                    break
            if not found_finish and RUN_START in "\n".join(lines):
                state = "running"

    command = None
    cmd_file = ws / "run.command"
    if cmd_file.is_file():
        command = cmd_file.read_text(encoding="utf-8").strip()

    return {
        "state": state,
        "exit_code": exit_code,
        "log_tail": log_tail,
        "terminal_launched": False,
        "command": command,
    }
