## Global Health Policy Simulation model

{% include nav-developer.md %}

# Data Model

**Interfaces:** `hgps::core::Datastore` in `src/HealthGPS.Core/datastore.h`. File-backed implementation: `hgps::input::DataManager` in `src/HealthGPS.Input/datamanager.h`.

The backend *data model* is an abstract description of the country-indexed reference datasets Health-GPS modules read through the Datastore API. The physical store reconciles disparate sources (units, gaps, country codes) so the engine can stay storage-agnostic.

The diagram below is a **conceptual** entity-relationship view. In C++, many of these tables map to POCOs in `src/HealthGPS.Core` (for example `BirthItem`, `PopulationItem`, `DiseaseEntity`). Field names on the diagram are not always 1:1 with member names in code.

| ![Health-GPS Data API](../images/data_api.png) |
|:-------------------------------------------:|
|        *Backend Data API Interface*         |

The data model defines the minimum dataset required by the model, the backend storage can hold more data to support external analysis for example. The backend dataset diagram is shown below, it identifies the required entities, relationships, and fields with respective data types. The dataset is indexed by country, *green*, entities representing demographics are *gray*, diseases are *red*, analysis are *blue*, and enumeration types are *yellow* respectively. Primary key (PK) fields are shown in **bold**, the ***ID*** fields are auto-generated row identifiers for internal use and data integrity enforcement.

| ![Health-GPS Data Model](../images/data_model.png) |
|:-----------------------------------------------:|
|    *Data Model Entity-Relationship Diagram*     |

The *country* index entity is based on the [ISO 3166-1][iso3166] standard. All external data sources must provide some kind of *location identifier*, most likely with different values, but must enable mapping with the data storage index definition to be reconcile.

## Enumerations

The data model defines normalised enumerations, *yellow*, to provide *stable identifier* for the commonly used concepts, such as gender, and consistent dimensional data lookups. Enumerations are defined by four fields as shown below, must populated before any data entry, provide also mapping with external data sources during the reconcile process.

| Field name  | Data Type | Constraint | Description              |
|:------------|:----------|:-----------|:-------------------------|
| **XyzID**   | Integer   | PK         | Model unique identifier  |
| Code        | Text      | UQ         | User stable identifier   |
| ShortName   | Text      |            | User facing display name |
| Description | Text      |            | Optional documentation   |

The unique constraint (UQ) may include multiple fields within the entity definition, *ShortName* fields are the user facing name for the *code* identifier and must always be provided. It is very **important** to be consistent when populating the enumerations *code* field to provide users and applications stable lookups, the following list is a suggested guide:

* start with a letter
* use only letters, numbers, and/or the underscore character, no spaces
* be consistent with casing, prefer lower case, avoid mixing
* keep it short, but meaningful and recognisable

The same recommendation applies to *folders* and *file names* definitions in cross-platform applications, operating system like *Linux* is case-sensitive by default, adopt a consistent naming convention that works everywhere. Following are enumerations defined by the Health-GPS model:

### Gender

| GenderID | Code   | ShortName | Description |
|:---------|:-------|:----------|:------------|
| 1        | male   | Male      |             |
| 2        | female | Female    |             |

### Disease Group

| GroupID | Code   | ShortName | Description                      |
|:--------|:-------|:----------|:---------------------------------|
| 0       | other  | Other     | General noncommunicable diseases |
| 1       | cancer | Cancer    | Cancer type diseases             |

### Disease Measure Type

| MeasureID | Code       | ShortName  | Description |
|:----------|:-----------|:-----------|:------------|
| 5         | prevalence | Prevalence |             |
| 6         | incidence  | Incidence  |             |
| 7         | remission  | Remission  |             |
| 15        | mortality  | Mortality  |             |

### BoD Measure Type

| MeasureID | Code | ShortName | Description                    |
|:----------|:-----|:----------|:-------------------------------|
| 2         | daly | DALY      | Disability adjusted life years |
| 3         | yld  | YLD       | Years lived with disability    |
| 4         | yll  | YLL       | Years of life lost             |

### Cancer Parameter Type

| ParameterID | Code         | ShortName  | Description              |
|:------------|:-------------|:-----------|:-------------------------|
| 0           | deathweight  | Deaths     | Death weight             |
| 1           | prevalence   | Prevalence | Prevalence distribution  |
| 2           | survivalrate | Survival   | Survival rate parameters |

## Registries

The *DiseaseType* and *RiskFactorType* are *dynamic enumerations*, providing a consistent *Registry* for available *diseases* and relative *risk factors* respectively. These enumerations are populated on demand, when defining new diseases within the Health-GPS ecosystem. Following are the examples of dynamic enumerations defined in the Health-GPS model:

### Disease Type

| DiseaseID | Code        | GroupID | ShortName          | Description              |
|:----------|:------------|:-------:|:-------------------|:-------------------------|
| Auto      | asthma      |    0    | Asthma             |                          |
| Auto      | diabetes    |    0    | Diabetes           | Diabetes mellitus type 2 |
| Auto      | lowbackpain |    0    | Low back pain      |                          |
| Auto      | colorectum  |    1    | Colorectal  cancer |                          |

### Risk Factor Type

| ParameterID | Code | ShortName | Description     |
|:------------|:-----|:----------|:----------------|
| Auto        | bmi  | BMI       | Body Mass Index |

>The risk factor *code* must be consistent, and exact match the risk factor naming convention used in the external model's definition. Only risk factors with relative effects on diseases data should be registered to minimise the constraint on external modelling.

# Data Entities

All entities in the model have a *time* and/or *age* dimension associated with the *measures* being stored. The following notation is used to represent these two dimensions across the data model:

| Field Name | Data Type | Description                     |
|:-----------|:----------|:--------------------------------|
| AtTime     | Integer   | The time reference in years     |
| WithAge    | Integer   | Time reference at time in years |

Entities with a single measure associated with gender, e.g. Population, store the values for each enumeration as column, while entities with higher dimensionality, e.g. disease, represent *Gender* and *Measure* independent dimensions. All data stored in the model should have a consistent unit, with all unit's conversion performed outside prior to data ingestion.

## Demographics

Country specific demographics data containing historic estimates and projections are modelled using one entity per measure, representing a two-dimensional series, *time x age*, with expanded *gender* enumeration columns. The following entities provide the demographics module data, all fields are required for a row definition.

### Population

Stores the number of *males* and *females* measure for a location at each *time* and *age* combination.

| Field name | Data Type | Constraint | Description                          |
|:-----------|:----------|:-----------|:-------------------------------------|
| **ID**     | Integer   | PK         | Model unique identifier              |
| LocationID | Integer   | UQ         | Location unique identifier           |
| AtTime     | Integer   | UQ         | Time reference of the measure values |
| WithAge    | Integer   | UQ         | Age reference of the measure values  |
| PopMale    | Real      |            | Number of males in population        |
| PopFemale  | Real      |            | Number of female in population       |

### Mortality

Stores the number for *male* and *female* deaths for a location at each *time* and *age* combination.

| Field name  | Data Type | Constraint | Description                           |
|:------------|:----------|:-----------|:--------------------------------------|
| **ID**      | Integer   | PK         | Model unique identifier               |
| LocationID  | Integer   | UQ         | Location unique identifier            |
| AtTime      | Integer   | UQ         | Time reference of the measure values  |
| WithAge     | Integer   | UQ         | Age reference of the measure values   |
| DeathMale   | Real      |            | Number of males deaths in population  |
| DeathFemale | Real      |            | Number of female deaths in population |

### Indicators (births)

Loaded via `Datastore::get_birth_indicators` into `BirthItem` (`src/HealthGPS.Core/indicator.h`). The file-backed CSV columns are typically `Time`, `Births`, and `SRB`.

| Field name | Data Type | Constraint | Maps to (`BirthItem`) | Description |
|:-----------|:----------|:-----------|:----------------------|:------------|
| LocationID | Integer   | UQ         | (via `Country`)       | Location unique identifier |
| AtTime     | Integer   | UQ         | `at_time`             | Time reference of the indicator values |
| Births     | Real      |            | `number`              | Number of births, both sexes combined |
| SRB        | Real      |            | `sex_ratio`           | Sex ratio at birth (males per 100 female births) |

Life expectancy (`LEx`, `LExMale`, `LExFemale`) is **not** part of `BirthItem`. It is loaded with disease analysis into `LifeExpectancyItem` inside `DiseaseAnalysisEntity` (`get_disease_analysis`).

## Diseases

Countries disease specific estimates are modelled using a multi-dimensional entity to represent a two dimensional series, *time x age*, for *gender* and *measure type* combinations. The following entities provide the *diseases model* required data, all fields are required for a row definition.

### Disease

Diseases can be dynamic defined within the *Health-GPS framework* using data only. The *disease* entity models the common measures required to define all diseases.

| Field name | Data Type | Constraint | Description                          |
|:-----------|:----------|:-----------|:-------------------------------------|
| **ID**     | Integer   | PK         | Model unique identifier              |
| LocationID | Integer   | UQ         | Location unique identifier           |
| DiseaseID  | Integer   | UQ         | Disease type unique identifier       |
| MeasureID  | Integer   | UQ         | Measure type unique identifier       |
| GenderID   | Integer   | UQ         | Gender type unique identifier        |
| AtTime     | Integer   | UQ         | Time reference of the measure values |
| WithAge    | Integer   | UQ         | Age reference of the measure values  |
| Mean       | Real      |            | The measure mean value               |

### Cancer Parameter

In addition to the common data above, *cancers* definition requires extra parameters, which are modelled using a multi-dimensional entity, storing *time-based* parameter values using expanded *gender* enumeration as columns.

| Field name  | Data Type | Constraint | Description                          |
|:------------|:----------|:-----------|:-------------------------------------|
| **ID**      | Integer   | PK         | Model unique identifier              |
| LocationID  | Integer   | UQ         | Location unique identifier           |
| DiseaseID   | Integer   | UQ         | Disease type unique identifier       |
| ParameterID | Integer   | UQ         | Parameter type unique identifier     |
| AtTime      | Integer   | UQ         | Time reference of the measure values |
| ValueMale   | Real      |            | The parameter value for males        |
| ValueFemale | Real      |            | The parameter value for females      |

### Relative Risks

The disease relative risk measure represents the association of risk factors and diseases, how exposures to risk factors affects the probabilities of developing the disease, the incidence of diseases in the population.

#### Relative risk to Disease (DiseaseRiskDisease)

The diseases relative risk to other diseases is modelled to represent the relative risk values by *age* using expanded *gender* enumeration as columns.

| Field name  | Data Type | Constraint | Description                                |
|:------------|:----------|:-----------|:-------------------------------------------|
| **ID**      | Integer   | PK         | Model unique identifier                    |
| DiseaseID   | Integer   | UQ         | Disease type unique identifier             |
| ToDiseaseID | Integer   | UQ         | Relative to disease type unique identifier |
| WithAge     | Integer   | UQ         | Age reference of the risk values           |
| RiskMale    | Real      |            | The relative risk value for males          |
| RiskFemale  | Real      |            | The relative risk value for females        |

#### Relative Risk due to Risk Factor (DiseaseRiskFactor)

The risk factors relative risk to diseases is modelled as a two-dimensional entity with *age* x *factor value* lookups value, stored for the relevant diseases by *gender*.

| Field name   | Data Type | Constraint | Description                               |
|:-------------|:----------|:-----------|:------------------------------------------|
| **ID**       | Integer   | PK         | Model unique identifier                   |
| DiseaseID    | Integer   | UQ         | Disease type unique identifier            |
| RiskFactorID | Integer   | UQ         | Relative to risk factor unique identifier |
| GenderID     | Integer   | UQ         | Gender type unique identifier             |
| WithAge      | Integer   | UQ         | Age reference of the risk values          |
| WithFactor   | Real      | UQ         | Factor reference of the risk values       |
| RiskValue    | Real      |            | The relative risk values                  |

## Analysis

Defines reference data used by analysis modules when computing burden-of-disease style indicators (death and health loss due to diseases, injuries, and risk factors) for the simulated population.

In C++, analysis datasets for a country are returned together as `DiseaseAnalysisEntity` from `Datastore::get_disease_analysis` (`src/HealthGPS.Core/analysis.h`): disability weights, life expectancy, and cost of disease tables.

### Disability Weight

Stores disease-specific disability weight estimates (magnitude of health loss), used when calculating years lived with disability (YLD). In code this is `DiseaseAnalysisEntity::disability_weights` (`std::map<std::string, float>` keyed by disease code).

| Field name | Data Type | Constraint | Description                    |
|:-----------|:----------|:-----------|:-------------------------------|
| Disease code | Text     | PK         | Disease identifier (map key)   |
| Weight     | Real      |            | The disease weight value       |

### Life expectancy

Part of `DiseaseAnalysisEntity::life_expectancy` (`LifeExpectancyItem`). Typical CSV columns: `Time`, `LEx`, `LExMale`, `LExFemale`.

| Field name | Data Type | Maps to | Description |
|:-----------|:----------|:--------|:------------|
| AtTime     | Integer   | `at_time` | Reference year |
| LEx        | Real      | `both`    | Life expectancy at birth, both sexes (years) |
| LExMale    | Real      | `male`    | Male life expectancy at birth (years) |
| LExFemale  | Real      | `female`  | Female life expectancy at birth (years) |

### LMS Parameters

Lambda-Mu-Sigma (LMS) parameters for converting childhood BMI risk-factor values to z-scores. Loaded via `Datastore::get_lms_parameters` into `LmsDataRow` (CSV columns typically `age`, `gender_id`, `lambda`, `mu`, `sigma`).

| Field name | Data Type | Constraint | Maps to (`LmsDataRow`) | Description |
|:-----------|:----------|:-----------|:-----------------------|:------------|
| GenderID   | Integer   | UQ         | `gender`               | Gender enumeration |
| WithAge / age | Integer | UQ         | `age`                  | Age reference of the parameter |
| Lambda     | Real      |            | `lambda`               | Lambda parameter |
| Mu         | Real      |            | `mu`                   | Mu parameter |
| Sigma      | Real      |            | `sigma`                | Sigma parameter |

### Cost of disease / BoD tables

Cost-of-disease lookup data sits in `DiseaseAnalysisEntity::cost_of_diseases` (age Ã— gender). The older ERD also showed a separate Burden of Disease measure table (*time* Ã— *age* Ã— *gender* Ã— *measure*). Treat that diagram as conceptual; the live `Datastore` contract is the methods and POCOs in `datastore.h` / `analysis.h`.

---

This document describes the **country reference Datastore** surface. It is not a claim that every Health-GPS input lives here. Experiment JSON, risk-factor model packs, FINCH policy equations, income stratum tables, height/weight curves, PIF CSVs, and similar project inputs are loaded through configuration and `HealthGPS.Input`, then held on the `Repository` / `ModelInput` path. See the [FINCH guide](../technical/guides/finch-linear-models-and-income-adjustment.md) and [Developer Guide](development.md).

Different [Data API][dataapi] implementations can be injected at construction; the file-backed one is `input::DataManager`.

---

### Related documentation

| Topic | Document |
| ----- | -------- |
| Developer docs index | [developer/README.md](README.md) |
| Architecture | [Software Architecture](architecture.md) |
| Build guide | [Developer Guide](development.md) |
| FINCH / income inputs | [FINCH guide](../technical/guides/finch-linear-models-and-income-adjustment.md) |
| User guide | [User Guide](../user/userguide.md) |
| Technical docs | [Technical documentation index](../technical/README.md) |
| Documentation home | [documentation/README.md](../README.md) |

---

[dataapi]: ../../src/HealthGPS.Core/datastore.h "Health-GPS Data API definition (Datastore)."

[iso3166]: https://www.iso.org/iso-3166-country-codes.html "ISO 3166 Country Codes"

---

**Author:** Mahima Ghosh
