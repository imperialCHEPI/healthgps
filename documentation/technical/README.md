# Technical documentation

Implementation notes, project reports, and modeller guides for Health-GPS (FINCH, India, and shared codebase work).

**Engineering contact:** Mahima Ghosh — reach out for code paths, config, or debugging.

---

## Guides and reports

Reference documents for modellers, economists, and developers — start here for FINCH inputs and project status.

| Document | Description |
| -------- | ----------- |
| [FINCH linear models and income-stratum adjustment](guides/finch-linear-models-and-income-adjustment.md) | Policy equations, predictor names (`log_income`, `gender2`), income-quintile factors-mean adjustment, colleague Q&A |
| [HealthGPS update report — 20 Feb 2026](guides/healthgps-update-report-2026-02-20.md) | Integrated codebase changes, module order, config/schema, verification notes |
| [Performance optimizations](guides/performance-optimizations.md) | Parallelisation and runtime notes |

---

## Developer notes (tooling)

Practical Windows / MSVC / CMake notes that sit next to the formal [Developer Guide](../developer/development.md).

| Document | Description |
| -------- | ----------- |
| [Technical developer notes index](developer/README.md) | Hub for tooling / environment docs |
| [Windows MSVC / Ninja build troubleshooting](developer/msvc-windows-build-troubleshooting.md) | Missing `cstdint`, broken `MSVCRTD.lib`, toolset mismatch — diagnosis and recovery |

---

## Plans (implementation design)

Design and phase plans — useful for developers; some items may already be implemented.

| Document | Topic |
| -------- | ----- |
| [Income quintile factor means](plans/income-quintile-factor-means-plan.md) | Income-stratum factors-mean adjustment (schema + C++ phases) |
| [Dynamic income categories](plans/dynamic-income-categories-plan.md) | 3/4/5 income category layout |
| [Weight quintile plan](plans/weight-quintile-plan.md) | Kevin Hall weight by income stratum |
| [Height CSV quintile plan](plans/height-csv-quintile-plan.md) | Height curves by quintile |
| [Individual ID tracking CSV](plans/individual-id-tracking-csv-plan.md) | Per-person output tracking |
| [Same person ID baseline/intervention](plans/same-person-id-baseline-intervention-plan.md) | Stable IDs across scenarios |
| [Parallelize output writes](plans/parallelize-output-writes-plan.md) | Result dispatch threading |
| [Project requirements plan](plans/project-requirements-plan.md) | `project_requirements` schema design |
| [Schema migration plan](plans/schema-migration-plan.md) | Config v1 → v2 migration |

### Related clusters

| If you are working on… | Start with… | Then see… |
| ---------------------- | ----------- | --------- |
| FINCH policy CSVs / `gender2` | [FINCH guide](guides/finch-linear-models-and-income-adjustment.md) | [Project requirements plan](plans/project-requirements-plan.md) |
| Income quintile adjustment | [FINCH guide §2](guides/finch-linear-models-and-income-adjustment.md#2-income-stratum-factors-mean-adjustment) | [Income quintile plan](plans/income-quintile-factor-means-plan.md) |
| Kevin Hall height/weight | [Height quintile plan](plans/height-csv-quintile-plan.md) | [Weight quintile plan](plans/weight-quintile-plan.md) |
| Output / person IDs | [Individual ID tracking](plans/individual-id-tracking-csv-plan.md) | [Same person ID plan](plans/same-person-id-baseline-intervention-plan.md) |
| What shipped in Feb 2026 | [Update report](guides/healthgps-update-report-2026-02-20.md) | [Performance notes](guides/performance-optimizations.md) |
| Windows build / MSVC / Ninja | [MSVC troubleshooting](developer/msvc-windows-build-troubleshooting.md) | [Developer Guide](../developer/development.md) |

---

## Documentation map

| Area | Index |
| ---- | ----- |
| Documentation home | [documentation/index.md](../index.md) |
| User docs | [user/](../user/) |
| Formal developer docs | [developer/](../developer/) |
| Technical developer notes | [developer/](developer/) |

---

[← Back to documentation home](../index.md) · **Maintainer:** Mahima Ghosh
