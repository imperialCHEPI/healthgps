# Simulation models reference

## Global Health Policy Simulation model

| [Home](../../index.md) | [Quick Start](../../user/getstarted.md) | [User Guide](../../user/userguide.md) | [Schemas](../../user/schemas.md) | [Models](../../user/models-overview.md) | [Architecture](../../developer/architecture.md) | [Data Model](../../developer/datamodel.md) | [Developer Guide](../../developer/development.md) | [Technical docs](../README.md) | [API](https://imperialchepi.github.io/healthgps/api/) |

**Related:** [How Health-GPS models a person](how-healthgps-models-a-person.md) | [Models overview (site summary)](../../user/models-overview.md) | [User Guide](../../user/userguide.md) | [Architecture](../../developer/architecture.md) | [FINCH guide](finch-linear-models-and-income-adjustment.md) | [Configuration schemas](../../user/schemas.md)

Detailed reference for **simulation modules** and **risk-factor model implementations** in Health-GPS: configuration keys, typical input files, what is updated on `Person`, and what reaches experiment output.

For a shorter pipeline diagram and comparison table, start at [Models overview](../user/models-overview.md).

---

## Table of contents

1. [Module execution order](#1-module-execution-order)
2. [Demographics module](#2-demographics-module)
3. [SES module](#3-ses-module)
4. [Risk-factor host module](#4-risk-factor-host-module)
5. [Static model: `hlm`](#5-static-model-hlm)
6. [Static model: `staticlinear`](#6-static-model-staticlinear)
7. [Dynamic model: `ebhlm`](#7-dynamic-model-ebhlm)
8. [Dynamic model: `kevinhall`](#8-dynamic-model-kevinhall)
9. [Dynamic model: `dummy`](#9-models-dummy)
10. [Disease module](#10-disease-module)
11. [Analysis module and outputs](#11-analysis-module-and-outputs)
12. [Scenarios and interventions](#12-scenarios-and-interventions)
13. [Schema and code pointers](#13-schema-and-code-pointers)

---

## 1. Module execution order

On **`Simulation.init`** (initialise population):

1. Demographic
2. SES
3. Risk factor — **both** configured packs: initialisation-slot `generate`, then update-slot `generate` (config keys `static` / `dynamic`)
4. Disease
5. Analysis (initial statistics)

On each **`Simulation.update`** (one simulated year):

1. Demographic update
2. Net immigration
3. SES update
4. Risk factor — **both** packs again: initialisation-slot `update`, then update-slot `update`
5. Disease update
6. Analysis update (publish results)

The words `static` / `dynamic` are slot names, not “protein becomes dynamic in year 2”. See [Models overview](../../user/models-overview.md#the-confusing-words-static-and-dynamic).

See [Update report](healthgps-update-report-2026-02-20.md) for diagrams aligned with `program.cpp` / `Simulation`.

**Inputs (shared):** `config.json`, backend datastore (via `data` + `data_index`), modelling CSV/JSON paths, `project_requirements`, RNG seeds in `running`.

**Outputs (shared):** Updated `Population` of `Person` entities; analysis events consumed by the Console host.

---

## 2. Demographics module

| | |
| --- | --- |
| **Purpose** | Age the population, apply births/deaths/residual mortality, net migration vs target population tables. |
| **Config** | `inputs.settings` (`country_code`, `size_fraction`, `age_range`), datastore demographic series. |
| **Optional** | `modelling.demographic_models` (region/ethnicity assignment probabilities). |
| **Person fields** | `age`, `gender`, `is_alive`, `time_of_death`, migration flags; optional `region`, `ethnicity`, `sector`. |
| **Outputs to files** | Aggregated counts in analysis CSV/JSON (not per-module files). |

---

## 3. SES module

| | |
| --- | --- |
| **Purpose** | Assign continuous socio-economic noise used as a predictor in hierarchical models. |
| **Config** | `modelling.ses_model` (`function_name`, `function_parameters`). |
| **Person fields** | `ses` (fixed after assignment in simple setups). |
| **Note** | **Income** (categories, continuous value, quintiles) is configured via `project_requirements` and FINCH/India pipelines — not the same as `ses`. See [FINCH guide](finch-linear-models-and-income-adjustment.md). |

---

## 4. Risk-factor host module

| | |
| --- | --- |
| **Purpose** | Load and run registered **static** and **dynamic** model definitions on each person. |
| **Config** | `modelling.risk_factor_models` map (`"static"` / `"dynamic"` → file path), `modelling.risk_factors` hierarchy, `dynamic_risk_factor` name, `baseline_adjustments`. |
| **Registration** | `register_risk_factor_model_definitions()` in `model_parser.cpp` reads each file’s **`ModelName`**. |
| **Person fields** | `risk_factors` map (and model-specific fields such as `physical_activity`, height/weight where implemented). |
| **Outputs** | Risk-factor distributions feed disease incidence; analysis aggregates RF exposure. |

Supported **`ModelName`** values are listed in `schemas/v1/config/models/static.json` and `dynamic.json`.

---

## 5. Static model: `hlm`

| | |
| --- | --- |
| **Typical use** | STOP / HLM France — hierarchical ICA-based initialisation. |
| **Schema** | [`schemas/v1/config/models/hlm.json`](https://github.com/imperialCHEPI/healthgps/blob/main/schemas/v1/config/models/hlm.json) |
| **Input file** | JSON: per-RF regressions (`models`), `levels` with ICA matrices (`m`, `w`, `s`, variances), large residual arrays. |
| **When it runs** | Population initialisation only (`RiskFactorModelType::Static`). |
| **Outputs** | Initial values in `Person.risk_factors` preserving cross-sectional correlation structure. |
| **Example pack** | [HLM_France](https://github.com/imperialCHEPI/healthgps-examples/tree/main/HLM_France) |
| **User doc** | [User Guide — Static](user/userguide.md#static) |

**Memory note:** Residual arrays scale with fitting sample size *N*; prefer `staticlinear` / lite dynamic stacks for large fitting datasets.

---

## 6. Static model: `staticlinear`

| | |
| --- | --- |
| **Typical use** | FINCH, India-style configs — CSV-driven linear predictors, optional region/ethnicity files. |
| **Schema** | [`schemas/v1/config/models/staticlinear.json`](https://github.com/imperialCHEPI/healthgps/blob/main/schemas/v1/config/models/staticlinear.json) |
| **Input files** | JSON wrapper pointing at CSVs (coefficients, factors means, ethnicity, region, etc.); structure in schema and [FINCH guide](finch-linear-models-and-income-adjustment.md). |
| **When it runs** | Initialisation; may also supply data loaded for region/ethnicity tables. |
| **Outputs** | Initial RF and related person attributes; feeds dynamic `kevinhall` / adjustment passes. |
| **Example pack** | [KevinHall_FINCH](https://github.com/imperialCHEPI/healthgps-examples/tree/main/KevinHall_FINCH), [KevinHall_India](https://github.com/imperialCHEPI/healthgps-examples/tree/main/KevinHall_India) |

---

## 7. Dynamic model: `ebhlm`

| | |
| --- | --- |
| **Typical use** | Classic HLM **dynamic** projection (delta updates, hierarchy in JSON). |
| **Schema** | [`schemas/v1/config/models/ebhlm.json`](https://github.com/imperialCHEPI/healthgps/blob/main/schemas/v1/config/models/ebhlm.json) |
| **Input file** | JSON: country metadata, `RiskFactors` hierarchy with levels/ranges, regression coefficients, residual SDs (compact vs static HLM). |
| **When it runs** | Each simulated year after demographics/SES. |
| **Outputs** | Updated `Person.risk_factors` within configured bounds. |
| **User doc** | [User Guide — Dynamic](user/userguide.md#dynamic) |

---

## 8. Dynamic model: `kevinhall`

| | |
| --- | --- |
| **Typical use** | Energy-balance trajectories (BMI, intake, PA) for FINCH / Kevin Hall studies. |
| **Schema** | [`schemas/v1/config/models/kevinhall.json`](https://github.com/imperialCHEPI/healthgps/blob/main/schemas/v1/config/models/kevinhall.json) (v2 schema version in code for latest fields). |
| **Input files** | JSON + CSVs: `RiskFactorModels` (boxcox, policy, logistic regression keys), height/weight quantile files, energy/PA tables, policy effect models as configured. |
| **When it runs** | Yearly dynamic update; interacts with `project_requirements` (income, PA, trends). |
| **Outputs** | Updated energy-balance-related risk factors and person-level PA; drives downstream disease and analysis metrics. |
| **Deep dive** | [FINCH linear models and income adjustment](finch-linear-models-and-income-adjustment.md) |

---

## 9. Models: `dummy`

| | |
| --- | --- |
| **Purpose** | Minimal definitions for tests or scaffolding. |
| **Schema** | [`dummy.json`](https://github.com/imperialCHEPI/healthgps/blob/main/schemas/v1/config/models/dummy.json) (referenced from static and dynamic `anyOf`). |
| **Production use** | Not used in published example packs. |

---

## 10. Disease module

| | |
| --- | --- |
| **Purpose** | Incidence, prevalence, remission, cancer-specific pathways using datastore rates and person RF exposure. |
| **Config** | Disease selection under `running`, relative risks from datastore, optional `population_impact_fraction`. |
| **Person fields** | `diseases` map (`DiseaseStatus`, onset times). |
| **Outputs** | Disease counts and BoD inputs to analysis. |
| **User doc** | [User Guide — Diseases](user/userguide.md#diseases) |

---

## 11. Analysis module and outputs

| | |
| --- | --- |
| **Purpose** | Aggregate population statistics each time step; optional individual tracking events. |
| **Inputs** | Full population, scenario id (baseline/intervention), run number, clock. |
| **Bus messages** | `ResultEventMessage`; optional `IndividualTrackingEventMessage` when tracking enabled. |
| **Host writers** | `ResultFileWriter` → JSON + main CSV + optional income-stratum CSVs; `IndividualIDTrackingWriter` → filtered per-person CSV. |
| **Config** | `output.folder`, `output.file_name`, `output.individual_id_tracking`, `project_requirements.income.income_based_csv_output`. |
| **User doc** | [User Guide — Analysis](user/userguide.md#analysis), [Results](user/userguide.md#results) |

Same **person ID** in baseline and intervention for the initial cohort enables joining tracking rows across scenarios; see [same-person ID plan](../plans/same-person-id-baseline-intervention-plan.md).

---

## 12. Scenarios and interventions

| | |
| --- | --- |
| **Purpose** | Baseline vs intervention: same module stack, different `Scenario` implementation (fiscal, marketing, food labelling, physical activity, etc.). |
| **Config** | `running` intervention blocks, `modelling.policy_start_year`, policy CSVs in FINCH packs. |
| **Sync** | Aggregate tables (e.g. net migration, residual mortality) can flow baseline → intervention; not person-level clones. |
| **Outputs** | Result rows tagged by **source** (baseline/intervention) in JSON/CSV. |

---

## 13. Schema and code pointers

| Item | Location |
| ---- | -------- |
| Model name → loader | `src/HealthGPS.Input/model_parser.cpp` (`load_risk_model_definition`, `get_model_schema_version`) |
| Module interfaces | `src/HealthGPS/interfaces.h`, `risk_factor_model.h` |
| Simulation order | `src/HealthGPS/simulation.cpp` |
| Console registration | `src/HealthGPS.Console/program.cpp` |
| JSON schemas | `schemas/v1/config/models/*.json`, `schemas/v1/config/modelling.json` |

When adding a new **`ModelName`**, extend the appropriate `static.json` / `dynamic.json` `anyOf`, implement loader + `RiskFactorModelDefinition`, and update this reference and [Models overview](../user/models-overview.md).

---

**Author:** Mahima Ghosh
