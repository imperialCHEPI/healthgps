# Project requirements: config-driven behaviour (no project hacks)

## Goal

Drive all project-specific behaviour from a single **`project_requirements`** section in the config. The code should never branch on "if India" / "if FINCH" / "if PIF"; it should branch on "if config says region=false" / "if config says income.adjust_to_factors_mean=true", etc.

## Schema: `project_requirements`

Place at the **start** of each project config for clarity. All fields are explicit so each project is self-documenting.

### Top-level structure

```json
{
  "project_requirements": {
    "demographics": { ... },
    "income": { ... },
    "physical_activity": { ... },
    "risk_factors": { ... },
    "trend": { ... },
    "two_stage": { ... }
  }
}
```

### 1. `demographics`

| Field     | Type    | Description |
|----------|---------|-------------|
| `age`    | boolean | Use age (always true in practice). |
| `gender` | boolean | Use gender (always true). |
| `region` | boolean | Use region (FINCH/PIF: true, India: false). |
| `ethnicity` | boolean | Use ethnicity (FINCH/PIF: true, India: false). |

- Code: initialise/adjust region only if `region === true`; same for ethnicity. No throw when data missing if flag is false.

### 2. `income`

| Field                      | Type   | Description |
|---------------------------|--------|-------------|
| `enabled`                 | boolean | Use income in the model. |
| `type`                    | string | `"continuous"` \| `"categorical"`. |
| `categories`              | string | `"3"` \| `"4"` (only when type is categorical, or for continuous→category mapping). |
| `adjust_to_factors_mean`  | boolean | Include income in factors-mean adjustment (initial). |
| `trended`                 | boolean | Include income in trended adjustment. |

- **FINCH:** type=continuous, adjust_to_factors_mean=true, trended=true (if trend enabled).
- **India:** type=categorical, adjust_to_factors_mean=false, trended=false.
- **PIF:** as per project (can match India or a variant).

- Code: use these flags instead of inferring from `is_continuous_income_model_` and hacks; `build_extended_factors_list(for_trended_adjustment)` and initial adjustment both read from here.

### 3. `physical_activity`

| Field                      | Type    | Description |
|---------------------------|---------|-------------|
| `enabled`                 | boolean | Use physical activity (have PA models). |
| `type`                    | string  | `"simple"` \| `"continuous"` (which PA model). |
| `adjust_to_factors_mean`  | boolean | Include PA in factors-mean adjustment (initial). |
| `trended`                 | boolean | Include PA in trended adjustment. |

- **FINCH:** enabled=true, type=continuous, adjust_to_factors_mean=true, trended=false (per your “do not include in trended” rule).
- **India:** enabled=true, type=simple, adjust_to_factors_mean=true, trended=false.

- Code: include PA in extended factors (initial/trended) only when these flags say so; no `has_physical_activity_models_`-only logic.

### 4. `risk_factors`

| Field                      | Type    | Description |
|---------------------------|---------|-------------|
| `adjust_to_factors_mean`  | boolean | Apply factors-mean adjustment to base risk factors. |
| `trended`                 | boolean | Apply trended adjustment to base risk factors. |

- “Base” = the list from the static model (e.g. food nutrients). Income/PA are governed by their own sections.

### 5. `trend`

| Field     | Type   | Description |
|----------|--------|-------------|
| `enabled`| boolean | Use any trend (UPF or income trend). |
| `type`   | string | `"null"` \| `"trend"` \| `"income_trend"` (when enabled). |

- Can keep current top-level `trend_type` in config and derive `trend.enabled` from it, or move everything under `project_requirements.trend`.

### 6. `two_stage`

| Field           | Type    | Description |
|----------------|---------|-------------|
| `use_logistic` | boolean | Use logistic (Stage 1) for zero vs non-zero. |
| `logistic_file`| string  | Optional; CSV name when use_logistic=true. |

- Code: “Set 0 logistic factors” vs “Set N logistic factors” and simulated-mean exclusion of zeros come from here; no inferring from file presence only.

---

## Example config snippets

### FINCH (`KevinHall_FINCH/config.json` or `new_config.json`)

```json
{
  "project_requirements": {
    "demographics": {
      "age": true,
      "gender": true,
      "region": true,
      "ethnicity": true
    },
    "income": {
      "enabled": true,
      "type": "continuous",
      "categories": "4",
      "adjust_to_factors_mean": true,
      "trended": false
    },
    "physical_activity": {
      "enabled": true,
      "type": "continuous",
      "adjust_to_factors_mean": true,
      "trended": false
    },
    "risk_factors": {
      "adjust_to_factors_mean": true,
      "trended": true
    },
    "trend": {
      "enabled": true,
      "type": "income_trend"
    },
    "two_stage": {
      "use_logistic": false
    }
  },
  "trend_type": "income_trend",
  "income_categories": "4",
  ...
}
```

### India (`KevinHall_India/config.json`)

```json
{
  "project_requirements": {
    "demographics": {
      "age": true,
      "gender": true,
      "region": false,
      "ethnicity": false
    },
    "income": {
      "enabled": true,
      "type": "categorical",
      "categories": "3",
      "adjust_to_factors_mean": false,
      "trended": false
    },
    "physical_activity": {
      "enabled": true,
      "type": "simple",
      "adjust_to_factors_mean": true,
      "trended": false
    },
    "risk_factors": {
      "adjust_to_factors_mean": true,
      "trended": true
    },
    "trend": {
      "enabled": true,
      "type": "income_trend"
    },
    "two_stage": {
      "use_logistic": false
    }
  },
  "trend_type": "income_trend",
  "income_categories": "3",
  ...
}
```

### PIF (`KevinHall_PIF/config.json`)

Set according to PIF choices (e.g. similar to India: no region/ethnicity if not used, categorical income, etc.).

---

## Implementation phases

### Phase 1: Schema and config (this PR / first step)

1. Add **`schemas/v1/config/project_requirements.json`** with the full JSON Schema for `project_requirements`.
2. In **`schemas/v1/config.json`**, add an optional property **`project_requirements`** with `$ref` to that schema (so existing configs without it still validate).
3. Add **`project_requirements`** blocks to:
   - **KevinHall_FINCH/config.json** (or new_config.json),
   - **KevinHall_India/config.json**,
   - **KevinHall_PIF/config.json**.

No code behaviour change yet; configs and schema are in place and validated.

### Phase 2: Load and expose in code

1. Extend **ModelInput** (or equivalent) to parse and hold a **project_requirements** struct (demographics, income, physical_activity, risk_factors, trend, two_stage).
2. Defaults: if `project_requirements` is absent, keep current behaviour (e.g. infer from existing `trend_type`, income model type, presence of PA models, etc.) so old configs still work.

### Phase 3: Replace hacks with requirement flags

1. **Demographics:** In `DemographicModule::initialise_region` / `initialise_ethnicity`, skip or require based on `project_requirements.demographics.region` / `ethnicity` (no throw for “newborn needs region” when region=false).
2. **Income:** In `build_extended_factors_list` and trended path, use `project_requirements.income.adjust_to_factors_mean` and `project_requirements.income.trended` instead of only `is_continuous_income_model_` and `for_trended_adjustment`.
3. **Physical activity:** Same: use `project_requirements.physical_activity.adjust_to_factors_mean` and `.trended` instead of only `has_physical_activity_models_`.
4. **Trend:** Use `project_requirements.trend.enabled` and `.type` where `trend_type_` is used (or map from existing `trend_type` when requirements are missing).
5. **Two-stage:** Use `project_requirements.two_stage.use_logistic` to decide whether to load logistic CSV and to exclude zeros in simulated mean.

After Phase 3, no code path should branch on “India” / “FINCH” / “PIF”; only on the flags in `project_requirements`.

---

## File checklist

- [ ] `schemas/v1/config/project_requirements.json` (new)
- [ ] `schemas/v1/config.json` (add optional `project_requirements` ref)
- [ ] `input-data/data/KevinHall_FINCH/config.json` (or new_config.json) – add `project_requirements`
- [ ] `input-data/data/KevinHall_India/config.json` – add `project_requirements`
- [ ] `input-data/data/KevinHall_PIF/config.json` – add `project_requirements`
- [ ] C++: config parsing and struct for `project_requirements` (Phase 2)
- [ ] C++: demographic / income / PA / trend / two-stage logic use flags (Phase 3)
