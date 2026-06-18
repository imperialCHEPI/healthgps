# HealthGPS Studio

Local web GUI for configuring and running HealthGPS microsimulation projects on your machine.

**HealthGPS Studio** lives in `healthgps-GUI/` inside the [healthgps](https://github.com/imperialCHEPI/healthgps) repository. It does not modify the C++ simulation engine; it writes workspace `config.json` files and launches `HealthGPS.Console` in your visible terminal.

## Prerequisites

- Python 3.10+
- Node.js 18+
- A built `HealthGPS.Console` binary
- Windows (v1 terminal launcher is Windows-first)

## Environment variables

| Variable | Description |
|----------|-------------|
| `HEALTHGPS_CONSOLE` | Path to `HealthGPS.Console.exe` (required for Validate/Run) |
| `HEALTHGPS_EXAMPLES_ROOT` | Path to `healthgps-examples` checkout (default: sibling of healthgps repo, e.g. `C:\healthgps-examples`) |
| `HEALTHGPS_ROOT` | Path to healthgps repo root (auto-detected if omitted) |
| `HEALTHGPS_WORKSPACES_ROOT` | Where workspace metadata is stored (default: `%USERPROFILE%/healthgps-workspaces`) |

## Quick start

```powershell
# Set path to your built console
$env:HEALTHGPS_CONSOLE = "C:\healthgps\build\...\HealthGPS.Console.exe"

# Backend (from repo root or healthgps-GUI)
cd healthgps-GUI\backend
pip install -e ".[dev]"
uvicorn app.main:app --reload --port 8000

# Frontend (separate terminal)
cd healthgps-GUI\frontend
npm install
npm run dev
```

Open <http://localhost:5173> — the Vite dev server proxies `/api` to the backend.

## Workflow

1. Pick a project (FINCH, India, or PIF).
2. Adjust all `project_requirements` toggles and run settings.
3. Choose the **config file** (`config.json`, `new_config.json`, etc.) — same as your `-c` argument.
4. **Validate** or **Run** — runs `HealthGPS.Console.exe -c <config> -T4` (output streams in the Run monitor).

## Tests

```powershell
cd healthgps-GUI\backend
pytest
```
