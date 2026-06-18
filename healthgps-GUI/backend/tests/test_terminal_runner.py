"""Tests for terminal command construction."""

import os
from pathlib import Path
from unittest.mock import patch

import pytest

from app.services.terminal_runner import TerminalRunnerError, build_command


def test_build_command_includes_dry_run():
    with patch.dict(os.environ, {"HEALTHGPS_CONSOLE": r"C:\bin\HealthGPS.Console.exe"}):
        from app.config import get_settings

        get_settings.cache_clear()
        cmd = build_command(
            Path(r"C:\ws\config.json"),
            thread_count=4,
            dry_run=True,
        )
    assert "HealthGPS.Console.exe" in cmd
    assert "--dry-run" in cmd
    assert "-T4" in cmd
    assert "-o" not in cmd


def test_build_command_missing_console_raises():
    env = {k: v for k, v in os.environ.items() if k != "HEALTHGPS_CONSOLE"}
    with patch.dict(os.environ, env, clear=True):
        from app.config import get_settings

        get_settings.cache_clear()
        with pytest.raises(TerminalRunnerError):
            build_command(Path("a.json"), 4)
