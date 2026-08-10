## Global Health Policy Simulation model

| [Home](../index.md) | [Quick Start](getstarted.md) | [User Guide](userguide.md) | [Schemas](schemas.md) | [Models](models-overview.md) | [Architecture](../developer/architecture.md) | [Data Model](../developer/datamodel.md) | [Developer Guide](../developer/development.md) | [Technical docs](../technical/README.md) | [API](https://imperialchepi.github.io/healthgps/api/) |

# Models in Health-GPS

Health-GPS is built from **top-level simulation modules** that act on each **person** every simulated year. Inside the risk-factor host module, JSON/CSV packs registered under `modelling.risk_factor_models` select a **static** implementation (set risk factors at the start) and a **dynamic** implementation (update them as time advances).

This page explains what each piece does in plain language. For file formats, coefficients, and FINCH-specific pipelines, see the **[Simulation models reference](../technical/guides/simulation-models-reference.md)** and the [User Guide](userguide.md).

---

## Top-level simulation modules

Modules run in a fixed order. Policy **scenarios** (baseline vs intervention) change parameters and interventions but use the same module stack.

| ![Top-level simulation modules](../images/simulation_modules.png) |
|:----------------------------------------------------------------:|
| *Health-GPS top-level modules: demographics → SES → risk factors → diseases → outputs* |

| Stage | Primary inputs | Primary outputs |
| ----- | -------------- | --------------- |
| **Demographics** | Country datastore (population, births, deaths), `inputs.settings` | Ages, births, deaths, immigration; alive / emigrated flags |
| **SES** | `modelling.ses_model`, RNG | `Person.ses` (continuous noise; separate from income categories) |
| **Risk factors** | Static + dynamic model files, optional factors-mean CSVs, `project_requirements` | `Person.risk_factors` map; optional PA, height, weight, nutrients |
| **Diseases** | Disease definitions from datastore + selection in config | `Person.diseases`; incidence/prevalence drivers |
| **Analysis** | Population state, scenario label | `ResultEventMessage` aggregates (and optional individual tracking events) |
| **Host output** | Analysis messages | Files under `output.folder` (see [Results](userguide.md#results)) |

Architecture diagrams (SVG): [modules](../images/modules_diagram.svg), [simulation engine](../images/simulation_engine.svg). Code-oriented detail: [Software Architecture](../developer/architecture.md).

---

## Static vs dynamic risk-factor models

This is the main distinction reviewers ask about.

| | **Static** risk-factor model | **Dynamic** risk-factor model |
| - | ---------------------------- | ----------------------------- |
| **When it runs** | Once at **initialisation** (and for newborns when they enter) | **Every simulated year** for people already in the population |
| **Job** | Create the starting risk-factor profile for each person | Evolve those risk factors through time |
| **Typical outputs** | Baseline nutrients, income, PA, BMI-related starting values | Yearly updates to weight, intake, height, or other RF trajectories |
| **Config key** | `modelling.risk_factor_models.static` | `modelling.risk_factor_models.dynamic` |
| **Example names** | `hlm`, `staticlinear`, `dummy` | `ebhlm`, `kevinhall`, `dummy` |

In short: **static = starting values**; **dynamic = how those values change over years**. Both are chosen in `config.json`. They are separate files and can be different implementations (for example FINCH often pairs `staticlinear` with `kevinhall`).

```json
"modelling": {
  "risk_factor_models": {
    "static": "static_model.json",
    "dynamic": "dynamic_model.json"
  }
}
```

Each of those JSON files has a **`ModelName`** field (`hlm`, `staticlinear`, `kevinhall`, …). That name selects the C++ implementation. Schemas: [`static.json`](https://github.com/imperialCHEPI/healthgps/blob/main/schemas/v1/config/models/static.json), [`dynamic.json`](https://github.com/imperialCHEPI/healthgps/blob/main/schemas/v1/config/models/dynamic.json).

| ![config.json modelling](../images/config_modelling.svg) |
|:--------------------------------------------------------:|
| *Static file sets `Person.risk_factors` at t0; dynamic file updates them each year* |

---

## What each model is (plain language)

### Static models

| Model | What it is | When you use it |
| ----- | ---------- | --------------- |
| **`hlm`** | Hierarchical linear model. Risk factors are drawn level-by-level from fitted regressions plus correlated residuals (STOP / France-style HLM packs). | France-style / STOP projects where the static pack is an HLM hierarchy. |
| **`staticlinear`** | CSV-driven linear / Box-Cox baseline model. Builds correlated nutrients and related factors, then initialises income, PA, policies/trends, and optional mean adjustments (including income-stratum tables). | FINCH and India-style packs; the usual “rich” baseline initialiser. |
| **`dummy`** | Test stub. Sets chosen factors to fixed values (optional simple policy shift in intervention). | Unit tests, smoke runs, minimal demos — not a scientific model. |

### Dynamic models

| Model | What it is | When you use it |
| ----- | ---------- | --------------- |
| **`ebhlm`** | Dynamic hierarchical / equation-based updater. Each year computes factor deltas from age/sex equations, adds bounded noise, applies intervention effects on deltas, then adjusts means toward expected tables. | Legacy dynamic HLM-style yearly updates (often paired with static `hlm`). |
| **`kevinhall`** | Energy-balance model (Kevin Hall). Updates nutrient/energy intake, weight, height where configured, and BMI, with newborn handling and baseline/intervention sync. | FINCH and Kevin Hall India packs; physiological weight/intake trajectories. |
| **`dummy`** | Same constant/policy stub as static, but run in the yearly update phase. | Tests that need a no-op or fixed dynamic update. |

---

## Inputs, behaviour and outputs (detail)

### Static models

| Model JSON name | Stage | Required inputs (high level) | What the code does | Outputs / side-effects |
| --------------- | ----- | ---------------------------- | ------------------ | ---------------------- |
| **`hlm`** (`StaticHierarchicalLinearModel`) | Static | `models`, `levels` (hierarchical level definitions with transition / inverse / correlation / residual-distribution matrices) | Initialises baseline risk factors level-by-level using deterministic regression + stochastic residual sampling from hierarchical matrices. Also used for newborn initialisation in updates. | Writes factor values into `person.risk_factors[<factor>]` for mapped factors. |
| **`staticlinear`** (`StaticLinearModel`) | Static | Factors-mean tables by sex/age; risk-factor correlation + policy covariance; BoxCox params (`lambda`, `stddev`, coefficients); ranges; policy models; income/PA settings; optional logistic coeffs (2-stage); optional trend / income-trend inputs; optional stratum expected tables | Core baseline initialiser used in India/FINCH flows. Computes residual-correlated factors, optional two-stage zero/non-zero generation, income initialisation (categorical or continuous), PA initialisation, policy initialisation, trend initialisation, and mean-adjustment passes (including income-stratum adjustment when enabled). | Writes many fields: risk factors (food/nutrients/etc.), residual terms, income (`person.income` + `risk_factors["income"]`), PA, policy/trend terms, and related initialised state. |
| **`dummy`** (`DummyModel`) | Static or dynamic (same model, chosen by type) | `ModelParameters` with per-factor `Value`, `Policy`, `PolicyStart` | Sets configured factors to constant values; in intervention scenario applies simple additive policy after policy start year. | Writes configured factor constants into `person.risk_factors`; deterministic behaviour for tests / smoke runs. |

### Dynamic models

| Model JSON name | Stage | Required inputs (high level) | What the code does | Outputs / side-effects |
| --------------- | ----- | ---------------------------- | ------------------ | ---------------------- |
| **`ebhlm`** (`DynamicHierarchicalLinearModel`) | Dynamic | Expected mean table, `BoundaryPercentage`, `Equations` (by age-group and sex with coefficients + residual stddev), `Variables` mapping | Yearly update model: for active non-newborns computes delta from regression equations, applies scenario intervention effect on deltas, adds bounded noise, updates factors, then performs factors-mean adjustment to keep simulated means aligned to expected values. | Updates existing `person.risk_factors[<factor>]` over time; keeps population means aligned via adjustment step. |
| **`kevinhall`** (`KevinHallModel`) | Dynamic | Expected table; nutrient specs (`Nutrients` with ranges/energy); food nutrient equations (`Foods`); food prices; weight quantiles (M/F); energy-PA quantiles; height params (`HeightStdDev`, `HeightSlope`); optional trends (`ExpectedTrend`, `TrendSteps`) | Energy-balance dynamic model (FINCH-style): initialises/updates nutrient intake, energy intake, weight dynamics, height updates where applicable, newborn handling, baseline/intervention adjustment sync, and BMI recomputation. | Updates `Weight`, nutrient/energy-related factors, height-related state, BMI, and Kevin Hall internal state in `person.risk_factors`. |
| **`dummy`** (`DummyModel`) | Dynamic alternative | Same as static dummy | Same constant/policy logic but executed in the dynamic update phase. | Same: controlled deterministic updates to selected risk factors. |

---

## Other configured “models”

These are not `ModelName` types but belong in the same mental model:

| Area | Config / data | Role |
| ---- | ------------- | ---- |
| **Income & demographics** | `project_requirements`, CSVs under `modelling` | Region, ethnicity, income category, quintile adjustment |
| **Baseline adjustments** | `modelling.baseline_adjustments` | Factors-mean calibration to stratum tables (FINCH) |
| **Interventions** | `running` scenarios + policy CSVs | Changes RF or policy levers in intervention run only |
| **PIF** | `population_impact_fraction` | Optional population impact fraction on incidence |
| **Datastore diseases** | Backend `data_index` + `running` disease list | Country-specific rates and relative risks |

---

## Where to go next

| Need | Document |
| ---- | -------- |
| How a person is built and updated | [How Health-GPS models a person](../technical/guides/how-healthgps-models-a-person.md) |
| Full inputs/outputs per model | [Simulation models reference](../technical/guides/simulation-models-reference.md) |
| FINCH linear models & income | [FINCH guide](../technical/guides/finch-linear-models-and-income-adjustment.md) |
| Static/dynamic JSON examples | [User Guide — Risk factor models](userguide.md#risk-factor-models) |
| Config layout | [Configuration schemas](schemas.md) |
| Example packs | [HealthGPS-examples](https://github.com/imperialCHEPI/healthgps-examples) |

---

**Author:** Mahima Ghosh
