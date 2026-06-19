# Technical documentation

Implementation notes, project reports, and modeller guides for Health-GPS (FINCH, India, and shared codebase work).

**Engineering contact:** Mahima — reach out for code paths, config, or debugging.

---

## Guides (modellers and economists)

Finished reference documents — start here if you are calibrating or reviewing FINCH inputs.

| Document | Description |
|----------|-------------|
| [FINCH linear models and income-stratum adjustment](guides/finch-linear-models-and-income-adjustment.md) | Policy equations, predictor names (`log_income`, `gender2`), income-quintile factors-mean adjustment, colleague Q&A |

---

## Reports (project status)

| Document | Description |
|----------|-------------|
| [HealthGPS update report — 20 Feb 2026](reports/healthgps-update-report-2026-02-20.md) | Integrated codebase changes, module order, config/schema, verification notes |
| [Performance optimizations](reports/performance-optimizations.md) | Parallelisation and runtime notes |

---

## Plans (implementation design)

Design and phase plans — useful for developers; some items may already be implemented.

| Document | Topic |
|----------|-------|
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
| What shipped in Feb 2026 | [Update report](reports/healthgps-update-report-2026-02-20.md) | [Performance notes](reports/performance-optimizations.md) |

---

[← Back to documentation home](../index.md)
