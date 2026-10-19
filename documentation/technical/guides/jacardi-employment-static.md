# JACARDI Slovenia employment (static package, 16 Oct 2026)

**Author:** Mahima Ghosh

| [Home](../../index.md) | [Technical docs](../README.md) | [JACARDI plan](../plans/JACARDI-7-countries-implementation-plan.md) | [Slovenia education](jacardi-slovenia-education.md) |

Partner algorithm (examples pack, not this repo): `healthgps-examples/Jacardi_Slovenia/EMPLOYMENT_STATIC_ALGORITHM.md`.

**These files are not education.** They are the next ladder step after Education. Keep all three education CSVs.

---

## What you received vs what stays

```mermaid
flowchart LR
  Edu[Education Part A/B<br/>KEEP three CSVs]
  Emp[Employment static<br/>NEW four files]
  Yearly[Employment yearly<br/>NOT arrived yet]
  Edu --> Emp
  Emp -.-> Yearly
```

| Keep (education - 15 Oct rules) | New (employment static - not coded yet) | Later |
| -------------------------------- | ---------------------------------------- | ----- |
| `education_lookup.csv` | `employment_static_coefs.csv` | Yearly transitions package |
| `education_draw_at_19_ssp2.csv` | `employment_4cat_recode.csv` | Replaces Part B step 3 hold |
| `education_upgrade_transitions_ssp2.csv` | `employment_static_check.csv` | |
| | `EMPLOYMENT_STATIC_ALGORITHM.md` | |

**Do not delete** the education files.

---

## Plain-language algorithm

Employment needs **age, gender, and education_id** (so it runs **after** education).

1. Ages **0-18**: no employment (`missing` / NaN), both ids.
2. Ages **19+**: compute multinomial scores from `employment_static_coefs.csv` (age capped at 80, RCS splines, education dummies, `SchoolFade` for 19-21).
3. Softmax → 6 probabilities → **DRAW** one `employment_id` (1-6).
4. Recode with `employment_4cat_recode.csv` → `employment4_id` (1-4). Downstream health models use the 4-cat id.
5. Part B until yearly package: under 19 missing; first time at 19+ draw like Part A; **everyone else keep last year** (temporary hold - partner said swap this out later).

| employment_id | Detailed | employment4_id | 4-cat |
| ------------- | -------- | -------------- | ----- |
| 1 | Employed | 1 | Employed |
| 2 | Unemployed | 2 | Unemployed |
| 3 | Retired | 4 | Not in labour force |
| 4 | Unable to work (health) | 4 | Not in labour force |
| 5 | Student | 3 | Student |
| 6 | Domestic / other inactive | 4 | Not in labour force |

Check file: SCORE probs for age × gender × education_id must match `employment_static_check.csv` (`p_1`…`p_6`, `q_1`…`q_4`).

---

## What we are supposed to do (engine work - not started)

| Step | Action |
| ---- | ------ |
| 1 | Keep education as-is (P1a done). |
| 2 | Wire **P1b employment static** into `JacardiModel` / `JacardiModelUpdate` after education. |
| 3 | Point `jacardi_model.json` Employment slot at the new filenames (today it still says `employment_coefs.csv` + binary `logistic` - that plan is outdated). |
| 4 | Store both `employment_id` and `employment4_id` on the person; export `employment4` (or both) via `modelling.risk_factors`. |
| 5 | Implement Part B step 3 as a **swappable hold** so the yearly package can replace it without rewriting Parts A / B1-2. |
| 6 | Gtest SCORE against `employment_static_check.csv` (woman 19 ISCED2 → student ≈ 0.879). |
| 7 | Wait for yearly employment package before trusting employment trends over time. |

Hierarchy note: older notes said `Employment = age + age² + sex + education`. The delivered static model is **multinomial logit + 5 age RCS terms + SchoolFade**, not age².

Partner source of truth in the examples pack: `EMPLOYMENT_STATIC_ALGORITHM.md` (16 Oct 2026).

---

**Author:** Mahima Ghosh
