# Individual ID tracking CSV — column source key

Each row is written **once per simulation year**, after the full yearly update (demographics → SES → risk factors → diseases). Values come from `person.risk_factors` and `Person` fields at that moment (`analysis_module.cpp` → `publish_individual_tracking_if_enabled`).

Config column list: `output.individual_id_tracking.risk_factors` in `new_config.json`.

---

## A. Fixed CSV columns (not in `risk_factors`)

| Column | Source | Notes |
|--------|--------|--------|
| `run` | `RuntimeContext::current_run()` | Run index for this scenario execution |
| `time` | `RuntimeContext::time_now()` | Simulation calendar year |
| `scenario` | `RuntimeContext::identifier()` | `baseline` or `intervention` |
| `id` | `Person::id()` | Person ID (intervention uses offset rule) |
| `age` | `Person::age` | After demographic ageing this year |
| `gender` | `Person::gender_to_string()` | |
| `region` | `Person::region` | |
| `ethnicity` | `Person::ethnicity` | |
| `income_category` | `Person::income` → string | e.g. quartile label; not continuous `income` |

---

## B. Carb debug columns (food vs nutrient — different pipelines)

| Column | Layer | Set in | When in the year | Meaning |
|--------|-------|--------|------------------|---------|
| `Carb_pre_factors_mean` | **Food** (`FoodCarbohydrate`) | `static_linear_model.cpp` → `record_food_carbohydrate_pre_factors_mean` | After `update_factors` (BoxCox), **before** factors-mean adjustment | Food carb from static model, pre cohort adjustment |
| `Carb_factors_mean_delta` | **Food** | `record_food_carbohydrate_post_factors_mean` | After factors-mean passes | `Carb_post_factors_mean − Carb_pre_factors_mean` |
| `Carb_post_factors_mean` | **Food** | same | After factors-mean, **before** policy/trends | Food carb right after factors-mean |
| `FoodCarbohydrate` | **Food** | `StaticLinearModel` | End of static update (may differ from `Carb_post_*` if policy/trends run later) | Live food factor used by Kevin Hall |
| `Carb_nutrient_current` | **Nutrient** (`Carbohydrate`) | `kevin_hall_model.cpp` → `update_nutrient_intakes` | Start of KH nutrient step | Nutrient carb **before** lag + recompute (= end of last year’s nutrient) |
| `Carbohydrate_previous` | **Nutrient** | same (copied to lag) | Start of KH nutrient step | Same as `Carb_nutrient_current`; fed to KH as `CI_0` / `KH_CI_0` |
| `Carbohydrate` | **Nutrient** | `compute_nutrient_intakes` | After KH rebuilds from `Food*` | Current-year nutrient carb |
| `Carb_nutrient_after_recompute` | **Nutrient** | `update_nutrient_intakes` (after `compute_nutrient_intakes`) | After KH rebuild | Should match `Carbohydrate` same row |

**Important:** Factors-mean adjustment targets **`FoodCarbohydrate`**, not `Carbohydrate`. Nutrient carb is derived later from food via `dynamic_model.json` food→nutrient map (FINCH: `FoodCarbohydrate` → `Carbohydrate` × 1.0).

**Important:** `FoodCarbohydrate` can change **after** `Carb_post_factors_mean` when intervention policy or trends run (`static_linear_model.cpp`, after the post snapshot). Compare `FoodCarbohydrate` vs `Carb_post_factors_mean` to see post-policy drift.

---

## C. Other risk-factor columns in your export

| Column | Module | Set / updated in | Notes |
|--------|--------|------------------|--------|
| `FoodSodium` | Static | `StaticLinearModel` (`update_factors`, factors-mean, policy, trends) | Food sodium factor; KH uses for `Sodium` / `Sodium_previous` |
| `EnergyIntake` | Kevin Hall | `compute_energy_intake` | Σ (nutrient × kcal/g) from `dynamic_model.json` `energy_equation` |
| `PhysicalActivity` | Static | `StaticLinearModel` | May also be factors-mean adjusted |
| `Weight` | Kevin Hall | `kevin_hall_run` then **sex×age bucket** adjustment | Final weight in CSV includes baseline bucket adjust |
| `Height` | Kevin Hall | `update_height` / init | |
| `BMI` | Kevin Hall | `compute_bmi` at end of `KevinHallModel::update_risk_factors` | `Weight / (Height/100)²` |

---

## D. Suggested read order for one person-year (debug low carb)

1. `Carb_pre_factors_mean` → was food already low before factors-mean?
2. `Carb_factors_mean_delta` → how much did factors-mean move food carb?
3. `Carb_post_factors_mean` vs `FoodCarbohydrate` → any policy/trend change after factors-mean?
4. `Carb_nutrient_current` / `Carbohydrate_previous` → nutrient lag going into KH (`KH_CI_0`)
5. `Carbohydrate` / `Carb_nutrient_after_recompute` → nutrient after rebuild from food
6. `EnergyIntake` → energy from nutrients
7. `Weight` / `BMI` → body outcome (also check `KH_*` columns if enabled in config)

---

## E. Simulation order (why columns differ)

```
Demographics / SES
  → StaticLinearModel (Food*, Carb_pre/post/delta, policy, trends)
  → KevinHallModel (Carb_nutrient_*, Carbohydrate, EnergyIntake, Weight, Height, BMI)
  → Disease
  → Analysis writes CSV row
```

Code: `simulation.cpp` `update_population()`; risk factors: `riskfactor.cpp` (static then dynamic).
