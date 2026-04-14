---
name: Income quintile factor means
overview: "Add an optional, config-driven pipeline: adjust continuous income using the existing two overall factors-mean CSVs; assign five rank-based income quintiles; initialise and adjust risk factors (then PA) using quintile-specific factors-mean tables; finally assign final income categories (e.g. four quartile buckets) from continuous income for the rest of the model. Mirror the same sequence in yearly updates. Keep the legacy single-table path when the feature is off."
todos:
  - id: config-schema
    content: Add toggle + optional quintile file entries under modelling.baseline_adjustments (not project_requirements); extend modelling.json schema; document Finch naming convention
    status: pending
  - id: load-tables
    content: Load 10 quintile RiskFactorSexAgeTable instances when enabled; wire into StaticLinearModel
    status: pending
  - id: stratified-adjust
    content: Implement quintile-filtered simulated means + per-stratum delta; income-only pass on overall table
    status: pending
  - id: reorder-init
    content: "Refactor initialise_risk_factors / update_risk_factors: quintile assign → factors → stratified RF adjust → PA init → stratified PA adjust → final equal-split categories"
    status: pending
  - id: sync-message
    content: Extend baseline↔intervention sync for stratified adjustment payloads; align intervention apply loop
    status: pending
  - id: tests
    content: Add unit/regression tests for off path and small stratified examples
    status: pending
isProject: false
---

# Income-quintile factors-mean adjustment (optional)

## Does the feature make sense?

Yes. Today `[calculate_adjustments](c:/healthgps/src/HealthGPS/risk_factor_adjustable_model.cpp)` compares **one** simulated mean per `(sex, age, factor)` to **one** expected value from the baseline table. That implicitly assumes a single population reference distribution. Your economists want **income** calibrated to the **overall** marginal mean (same as now), but **nutrients / PA** calibrated to **income-stratum** reference means (quintiles from external data). Doing quintiles for adjustment and **separately** assigning final **quartile** buckets from the same continuous income matches the constraint that reference tables are quintile-based while downstream (e.g. Kevin Hall) stays on a 4-way `core::Income` split.

You will indeed compute “income categories” twice in a sense: a **5-way** stratum for calibration, then a **final** 3/4-way `person.income` used elsewhere. The continuous values in `person.risk_factors["income"]` / `person.income_continuous` remain the single source of truth for the second step.

## Current anchor points (what changes)

- **Generation** (`[StaticLinearModel::initialise_risk_factors](c:/healthgps/src/HealthGPS/static_linear_model.cpp)`): continuous income → (optional) extended list → `[adjust_risk_factors](c:/healthgps/src/HealthGPS/risk_factor_adjustable_model.cpp)` → for continuous models, `[assign_income_categories_equal_split](c:/healthgps/src/HealthGPS/static_linear_model.cpp)` with `income_categories_` `"3"` or `"4"`.
- **Yearly update** (`[StaticLinearModel::update_risk_factors](c:/healthgps/src/HealthGPS/static_linear_model.cpp)`): same adjustment + equal-split pattern; newborns run `initialise_income` → `initialise_factors` → `initialise_physical_activity` before adjustment.
- **Expected tables** are loaded once in `[load_risk_factor_expected](c:/healthgps/src/HealthGPS.Input/model_parser.cpp)` from `modelling.baseline_adjustments.file_names` (`factorsmean_male` / `factorsmean_female`). The **toggle** for this feature and the **quintile CSV paths** also live here (see Configuration below), not in `project_requirements`.

## Proposed behaviour when the new mode is enabled

Scope: **continuous income** projects only (`is_continuous_income_model_`); when disabled, keep today’s behaviour unchanged.

1. **Income-only adjustment (overall tables)**
  Call adjustment with **only** `income` in the factor list (and `req.income.adjust_to_factors_mean` semantics unchanged), using the **existing** shared `expected_` table (your 2 files). Do **not** include base risk factors or PA in this pass.
2. **Assign income quintiles (5 buckets)**
  Add something equivalent to `assign_income_categories_equal_split` but with `bucket_count = 5`, based on `person.risk_factors["income"]` after step 1. Persist stratum on each person (new field, see below).
3. **Initialise risk factors**
  Same as today: `[initialise_factors](c:/healthgps/src/HealthGPS/static_linear_model.cpp)` for everyone (generation) or only new paths in update.
4. **Risk-factor adjustment (quintile tables)**
  For each quintile `q`, compute simulated means **restricted to people with `person.income_quintile == q`** (and still by sex/age). Expected value for `(sex, age, factor)` comes from quintile `q` male/female CSV. Build deltas and apply per person using their quintile. Factors: `names_` only (not income in this pass unless you explicitly want income adjusted again—default **no**).
5. **Initialise physical activity**
  Run `[initialise_physical_activity](c:/healthgps/src/HealthGPS/static_linear_model.cpp)` **after** risk adjustment (reorder vs today when flag is on).
6. **PA adjustment (same quintile tables)**
  Second stratified adjustment pass for `PhysicalActivity` only, same mechanics as step 4.
7. **Final income categories for the rest of the model**
  Keep continuous income as assigned/adjusted; set `person.income` using the **existing** final bucket count from config (`income.categories` `"3"` or `"4"`) via `assign_income_categories_equal_split` (this is your “quartiles from continuous income” when `categories == "4"`).

**Trended path** (`[req.trend.enabled](c:/healthgps/schemas/v1/config/project_requirements.json)` + `risk_factors.trended`): repeat the same **sequence** with trend applied to **expected** values consistently (overall table for any income-only pass; quintile tables for risk/PA passes). Mirror in `update_risk_factors` after trends are updated.

## Configuration (toggle + file wiring)

**Where it lives:** `modelling.baseline_adjustments` in the main `config.json` (same section that already lists overall factors-mean files). **Not** in `project_requirements`—that block stays for behavioural flags (adjust_to_factors_mean, trend, etc.); the **project-specific** choice of “use quintile-stratified adjustment + which files” is colocated with the baseline CSV wiring.

**Toggle:** Add a boolean (or small object with `enabled`) under `baseline_adjustments`, e.g. `income_quintile_stratified_factors_mean: { "enabled": false }`, default **off** so existing projects are unchanged.

**Overall (existing) files — FINCH example** (unchanged role: income adjustment + `get_expected` for linear models):

- `Finch.FactorsMean.Female.csv`
- `Finch.FactorsMean.Male.csv`

Mapped today as `factorsmean_female` / `factorsmean_male` in `file_names` (see e.g. `[KevinHall_FINCH/config.json](c:/healthgps/input-data/data/KevinHall_FINCH/config.json)`).

**Quintile-specific (10 files) — FINCH naming pattern:**

- Female: `Finch.FactorsMean.Female.Quintile1.csv` … `Finch.FactorsMean.Female.Quintile5.csv`
- Male: `Finch.FactorsMean.Male.Quintile1.csv` … `Finch.FactorsMean.Male.Quintile5.csv`

These are listed explicitly in `baseline_adjustments.file_names` (new optional keys—e.g. `factorsmean_female_quintile1` … `factorsmean_female_quintile5` and `factorsmean_male_quintile1` … `factorsmean_male_quintile5`, or another stable key scheme the parser documents). When the toggle is **on**, validation requires all 10 paths; when **off**, omit them or ignore.

**Schema:** Extend `[schemas/v1/config/modelling.json](c:/healthgps/schemas/v1/config/modelling.json)` `baseline_adjustments` so it allows the toggle and optional quintile `file_names` keys (today the schema only allows the two overall keys and `additionalProperties: false`—implementation must relax or nest the new fields so configs validate).

**Content expectations:** The **overall** two files still supply **income** (and anything else used for income-only adjustment and `get_expected`). The **quintile** files supply expected means for **risk factors** and **PhysicalActivity** in stratified passes (same CSV schema as current factors-mean files, per `[load_baseline_from_csv](c:/healthgps/src/HealthGPS.Input/model_parser.cpp)`).

## Core implementation notes

### Stratified adjustment math

Reuse the pattern in `[calculate_adjustments](c:/healthgps/src/HealthGPS/risk_factor_adjustable_model.cpp)` and `[calculate_simulated_mean](c:/healthgps/src/HealthGPS/risk_factor_adjustable_model.cpp)`, but:

- Filter people by `income_quintile` when accumulating moments.
- If a stratum has no people at a given `(sex, age)`, keep delta 0 or NaN-safe behaviour (same as today for empty means).

### `get_expected` / linear models

Keep `[StaticLinearModel](c:/healthgps/src/HealthGPS/static_linear_model.h)` using the **overall** `expected`_ table for `[get_expected](c:/healthgps/src/HealthGPS/risk_factor_adjustable_model.cpp)` in **linear model** and PA **sampling** unless economists later want stratum-specific priors—your description only changes **adjustment**, not the regression’s baseline expectation.

### Person state

Add a small field on `[Person](c:/healthgps/src/HealthGPS/person.h)`, e.g. `std::uint8_t income_quintile{255}` (sentinel) or `std::optional`-like convention, set during quintile assignment and read during stratified adjustment.

### Baseline / intervention sync (important)

Today each `adjust_risk_factors` sends one `[SyncDataMessage<RiskFactorSexAgeTable>](c:/healthgps/src/HealthGPS/risk_factor_adjustable_model.cpp)`. Stratified deltas are **not** representable as a single `(sex, factor, age)` vector.

Options (pick one in implementation):

- **A (recommended):** New message type, e.g. `SyncDataMessage<StratifiedRiskFactorAdjustments>` holding either:
  - `std::array<RiskFactorSexAgeTable, 5>` per adjustment phase, or
  - a small struct with `quintile` index inside the key space.
- **B:** Send multiple messages with an agreed ordering (fragile; needs strict pairing in intervention).

Intervention applies **baseline-computed** deltas per `(sex, age, quintile)` so each person uses **their** quintile index when applying the delta. **Order of calls** on baseline and intervention must stay identical.

**Subtlety:** Quintile membership must be **consistent** across scenarios if policies change income ranking. Safer approach: compute **quintile breakpoints** on the **baseline** population each year and broadcast thresholds (or send quintile index per slot if your runner already guarantees aligned IDs—`[Person::id](c:/healthgps/src/HealthGPS/person.h)` is intended for tracking). Spell this out in design review; worst case, document that stratified adjustment is validated for **baseline-only** runs first.

### Loading extra tables

- In `[model_parser.cpp](c:/healthgps/src/HealthGPS.Input/model_parser.cpp)` (or wherever `StaticLinearModel` is constructed), if the flag is set, load 10 tables into e.g. `std::shared_ptr<std::array<RiskFactorSexAgeTable, 5>>` per sex or a merged structure referenced by `StaticLinearModel`.

### Code organisation

- Keep the legacy path as a single combined `adjust_risk_factors` when the flag is false.
- When true, implement private helpers on `StaticLinearModel` or a small collaborator class to avoid duplicating large blocks in `initialise_risk_factors` and `update_risk_factors`.

## Testing / validation

- Unit tests for stratified `calculate_simulated_mean` (known small populations, hand-computed means and deltas).
- Regression test: flag off → bitwise/close match to existing golden outputs on a small scenario.
- With flag on: sanity checks (each quintile ~20% of people; after adjustment, stratum means move toward quintile CSVs).

## Files likely touched

- `[static_linear_model.cpp](c:/healthgps/src/HealthGPS/static_linear_model.cpp)` / `[static_linear_model.h](c:/healthgps/src/HealthGPS/static_linear_model.h)` — orchestration, reorder init when flag on.
- `[risk_factor_adjustable_model.cpp](c:/healthgps/src/HealthGPS/risk_factor_adjustable_model.cpp)` / `.h` — stratified adjustment + sync types, or new `.cpp` helper.
- `[model_parser.cpp](c:/healthgps/src/HealthGPS.Input/model_parser.cpp)` / `[poco.h](c:/healthgps/src/HealthGPS.Input/poco.h)` — optional file list + loading.
- `[person.h](c:/healthgps/src/HealthGPS/person.h)` — quintile field.
- `[schemas/v1/config/project_requirements.json](c:/healthgps/schemas/v1/config/project_requirements.json)` (+ main `config` schema for `baseline_adjustments` if file keys are added there).
- Example FINCH `[config.json](c:/healthgps/input-data/data/KevinHall_FINCH/config.json)` — optional commented example for the new keys.

## Summary

The feature is coherent and aligns with how adjustment is implemented today; the main engineering cost is **stratified adjustment + extended baseline/intervention messaging**, plus a **clear config kill-switch** so other projects keep the current single-table pipeline.