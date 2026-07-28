# Technical documentation

Implementation notes, project reports, and modeller guides (FINCH, India, shared codebase).

Reach out if you need a code path, config walkthrough, or help debugging.

---

## Guides and reports

| Document | Description |
| -------- | ----------- |
| [FINCH linear models and income-stratum adjustment](guides/finch-linear-models-and-income-adjustment.md) | Policy equations, predictor names (`log_income`, `gender2`), income-quintile factors-mean, Q&A |
| [HealthGPS update report (20 Feb 2026)](guides/healthgps-update-report-2026-02-20.md) | Integrated changes, module order, config/schema, verification |
| [Simulation models reference](guides/simulation-models-reference.md) | Per-module and per-ModelName inputs, outputs, schemas, code pointers |
| [Performance optimizations](guides/performance-optimizations.md) | Parallelisation and runtime notes |

---

## Software plans

Engineering design notes for developers. Written as ordinary markdown plans (summary, goals, work items). Some items may already be in the tree.

| Document | Topic |
| -------- | ----- |
| [Income quintile factor means](plans/income-quintile-factor-means-plan.md) | Income-stratum factors-mean (schema + C++) |
| [Dynamic income categories](plans/dynamic-income-categories-plan.md) | 3/4/5 income category layout |
| [Weight quintile plan](plans/weight-quintile-plan.md) | Kevin Hall weight by income stratum |
| [Height CSV quintile plan](plans/height-csv-quintile-plan.md) | Height curves by quintile |
| [Individual ID tracking CSV](plans/individual-id-tracking-csv-plan.md) | Per-person output tracking |
| [Same person ID baseline/intervention](plans/same-person-id-baseline-intervention-plan.md) | Stable IDs across scenarios |
| [Parallelize output writes](plans/parallelize-output-writes-plan.md) | Result dispatch threading |
| [Project requirements plan](plans/project-requirements-plan.md) | `project_requirements` schema |
| [Schema migration plan](plans/schema-migration-plan.md) | Config v1 to v2 |

## If you are working on

| Topic | Start here | Then |
| ----- | ---------- | ---- |
| FINCH policy CSVs / `gender2` | [FINCH guide](guides/finch-linear-models-and-income-adjustment.md) | [Project requirements plan](plans/project-requirements-plan.md) |
| Income quintile adjustment | [FINCH guide](guides/finch-linear-models-and-income-adjustment.md) | [Income quintile plan](plans/income-quintile-factor-means-plan.md) |
| Kevin Hall height/weight | [Height quintile plan](plans/height-csv-quintile-plan.md) | [Weight quintile plan](plans/weight-quintile-plan.md) |
| Output / person IDs | [Individual ID tracking](plans/individual-id-tracking-csv-plan.md) | [Same person ID plan](plans/same-person-id-baseline-intervention-plan.md) |
| What shipped in Feb 2026 | [Update report](guides/healthgps-update-report-2026-02-20.md) | [Performance notes](guides/performance-optimizations.md) |
| Windows build / MSVC | [MSVC troubleshooting](../developer/msvc-windows-build-troubleshooting.md) | [Developer Guide](../developer/development.md) |

---

| Area | Link |
| ---- | ---- |
| Documentation root | [documentation/README.md](../README.md) |
| User docs | [user/](../user/) |
| Developer docs (build, MSVC) | [developer/](../developer/) |

---

**Author:** Mahima Ghosh
