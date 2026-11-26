# Developer documentation

## Global Health Policy Simulation model

| [Home](../index.md) | [Quick Start](../user/getstarted.md) | [User Guide](../user/userguide.md) | [Schemas](../user/schemas.md) | [Models](../user/models-overview.md) | [Architecture](architecture.md) | [Data Model](datamodel.md) | [Developer Guide](development.md) | [Technical docs](../technical/README.md) | [API](https://imperialchepi.github.io/healthgps/api/) |

Docs for building Health-GPS, reading the architecture, and contributing.

| Document | Description |
| -------- | ----------- |
| [Software & Architecture](architecture.md) | Full engineer-facing software architecture: components, host, modules, pipeline, bus, scenarios, parallelism |
| [Data Model](datamodel.md) | Backend Data API and entity schema |
| [Developer Guide](development.md) | C++20, CMake presets, vcpkg, tests, HPC |
| [vcpkg baseline, November 2026](../technical/guides/vcpkg-baseline-2026-11.md) | Port versions after the November 2026 baseline, and the source edits |
| [Windows MSVC / Ninja troubleshooting](msvc-windows-build-troubleshooting.md) | `cstdint`, `MSVCRTD.lib`, broken toolsets |
| [How the website updates](pages-publish.md) | Push to `main` publishes the site; docs-only pushes reuse the API |
| [GitHub Pages docs deploy troubleshooting](docs-deploy-troubleshooting.md) | Jekyll OK but Configure HealthGPS fails |
| [GitHub Flow](github-flow.md) | Branching and pull requests |
| [When a run stops](../technical/guides/failure-reporting.md) | Why the console now prints the failing step and the exception |

## If you are working on

| Topic | Start here | Then |
| ----- | ---------- | ---- |
| Build / CMake / vcpkg | [Developer Guide](development.md) | [vcpkg baseline, November 2026](../technical/guides/vcpkg-baseline-2026-11.md) |
| Docs site / Pages deploy | [How the website updates](pages-publish.md) | [Docs deploy troubleshooting](docs-deploy-troubleshooting.md) |
| Module / software design | [Software & Architecture](architecture.md) | [Update report](../technical/guides/healthgps-update-report-2026-02-20.md) |
| Datastore | [Data Model](datamodel.md) | [User Guide](../user/userguide.md) |
| Pull requests | [GitHub Flow](github-flow.md) | [Developer Guide](development.md) |
| FINCH / income / predictors | [FINCH guide](../technical/guides/finch-linear-models-and-income-adjustment.md) | [technical/README.md](../technical/README.md) |

---

| Area | Link |
| ---- | ---- |
| Documentation root | [documentation/README.md](../README.md) |
| User docs | [user/](../user/) |
| Technical guides and plans | [technical/](../technical/) |

---

**Author:** Mahima Ghosh
