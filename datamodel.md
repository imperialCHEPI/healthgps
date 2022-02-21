## Global Health Policy Simulation model

| [Home](index) | [Quick Start](getstarted) | [User Guide](userguide) | [Software Architecture](architecture) | Data Model | [Developer Guide](development) |

# Data Model

The backend *data model* defines an abstract model to organises data entities and how they relate to one another in a standardised schema and format to be used within the Health GPS systems. The backend storage provides a reference dataset that reconcile various disparate data sources required by the model, fill gaps, adjust units, etc, for easy use. The standardised format allows the reference dataset to be easily expanded to accommodate new and non-traditional data sources.

The data model is storage agnostic, the [Data API][dataapi] abstraction interface shown below, provides a contract for the minimum dataset, easy access, strong typing, and decoupling from the backend storage implementation. 

|![Health GPS Data API](/assets/image/data_api.png) "Health GPS Data API"|
|:--:|
|*Backend Data API Interface*|

The data model defines the minimum dataset required by the model, the backend storage can hold more data to support external analysis for example. The backend dataset diagram is shown below, it identifies the required entities, relationships, and fields with respective data types. The dataset is indexed by country, *green*, entities representing demographics are *gray*, diseases are *red*, analysis are *blue*, and enumeration types are *yellow* respectively. Primary key (PK) fields are shown in **bold**, the ***ID*** fields are auto-generated row identifiers for internal use and data integrity enforcement.

|![Health GPS Data Model](/assets/image/data_model.png)|
|:--:|
|*Data Model Entity-Relation Diagram*|

The *country* index entity is based on the [ISO 3166-1][iso3166] standard. All external data sources must provide some kind of *location identifier*, most likely with different values, but must enable mapping with the data storage index definition to be reconcile.

# Enumerations
The data model defines normalised enumerations, *yellow*, to provide *stable identifier* for the commonly used concepts, such as gender, and consistent dimensional data lookups. Enumerations are defined by four fields as shown below, must populated before any data entry, provide also mapping with external data sources during the reconcile process.

| Field name | Data Type | Constraint | Description             |
| :---       | :---      | :---       | :---                    |
| **XyzID**  | Integer   | PK         | Model unique identifier |
| Code       | Text      | UK         | User stable identifier  |
| ShortName  | Text      |            | User facing display name|
| Description| Text      |            | Optional documentation  |


The unique constraint (UK) may include multiple fields within the entity definition, *ShortName* fields are the user facing name for the *code* identifier and must always be provided. It is very **important** to be consistent when populating the enumerations *code* field to provide users and applications stable lookups, the following list is a suggested guide:

* start with a letter 
* use only letters, numbers, and/or the underscore character, no spaces
* be consistent with casing, prefer lower case, avoid mixing
* keep it short, but meaningful and recognisable

The same recommendation applies to *folders* and *file* names definitions in cross-platform applications, operating system like *Linux* are case-sensitive by default, adopt a consistent naming convention that works everywhere. Following are the enumerations defined by the Health GPS model:

## Gender

| GenderID | Code   | ShortName | Description      |
| :---     | :---   | :---      | :---             |
| 1        | male   | Male      |                  |
| 2        | female | Female    |                  |

## Disease Group

| GroupID  | Code   | ShortName | Description|
| :---     | :---   | :---      | :---       |
| 0        | other  | Other     | General noncommunicable diseases|
| 1        | cancer | Cancer    | Cancer type diseases|

## Disease Measure Type

| MeasureID | Code   | ShortName | Description|
| :---      | :---   | :---      | :---       |
| 5         | prevalence | Prevalence | |
| 6         | incidence  | Incidence  | |
| 7         | remission  | Remission| | |
| 15        | mortality  | Mortality  | |

## BoD Measure Type

| MeasureID | Code   | ShortName | Description|
| :---      | :---   | :---      | :---       |
| 2         | daly | DALY | Disability adjusted life years |
| 3         | yld  | YLD  | Years lived with disability |
| 4         | yll  | YLL  | Years of life lost|

## Cancer Parameter Type

| ParameterID | Code         | ShortName  | Description              |
| :---        | :---         | :---       | :---                     |
| 0           | deathweight  | Deaths     | Death weight             |
| 1           | prevalence   | Prevalence | Prevalence distribution  |
| 2           | survivalrate | Survival   | Survival rate parameters |

## Registries
The *DiseaseType* and *RiskFactorType* are *dynamic enumerations*, providing a consistent *Registry* for available *diseases* and relative *risk factors* respectively. These enumerations are populated on demand, when defining a new diseases within the Health GPS ecosystem. Following are the examples of dynamic enumerations defined in the Health GPS model:

## Disease Type

| DiseaseID | Code        | GroupID | ShortName         | Description              |
| :---      | :---        | :---:   | :---              | :---                     |
| Auto      | asthma      | 0       | Asthma            | |
| Auto      | diabetes    | 0       | Diabetes          | Diabetes mellitus type 2 |
| Auto      | lowbackpain | 0       | Low back pain     | |
| Auto      | colorectum  | 1       | Colorectal  cancer| |

## Risk Factor Type

| ParameterID | Code | ShortName | Description     |
| :---      | :---   | :---      | :---            |
| Auto      | bmi    | BMI       | Body Mass Index |

>The risk factor *code* must be consistent, and exact match the risk factor naming convention used in the external models definition. Only risk factors with relative effects on diseases data should be registered to minimise the constraint on external modelling.

# Data Entities

All entities in the model have a *time* and/or *age* dimension associated with the *measures* being stored. The following notation is used to represent these two dimensions across the data model:

| Field Name| Data Type | Description             |
| :---      | :---      | :---                    |
| AtTime    | Integer   | The time in years       |
| WithAge   | Integer   | The age at time in years|

Entities with a single measure associated with gender, e.g. Population, store the values for each enumeration as column, while entities with higher dimensionality, e.g. disease, represent *Gender* and *Measure* independent dimensions.

## Demographics

## Diseases

### Relative Risks

## Analysis

---
> **_UNDER DEVELOPMENT:_**  More content coming soon.
---


[comment]: # (References)
[dataapi]: https://github.com/imperialCHEPI/healthgps/blob/main/source/HealthGPS.Core/datastore.h "Health GPS Data API definition."

[iso3166]: https://www.iso.org/iso-3166-country-codes.html "ISO 3166 Country Codes"