## Global Health Policy Simulation model

| [Home](index) | [Quick Start](getstarted) | [User Guide](userguide) | [Software Architecture](architecture) | Data Model | [Developer Guide](development) |

# Data Model

The backend *data model* defines an abstract model to organises data entities and how they relate to one another in a standardised schema and format to be used within the Health GPS systems. The backend storage provides a reference dataset that reconcile various disparate data sources required by the model, fill gaps, adjust units, etc, for easy use. The standardised format allows the reference dataset to be easily expanded to accommodate new and non-traditional data sources.

The data model is storage agnostic, the [Data API][dataapi] abstraction interface shown below, provides a contract for the minimum dataset, easy access, strong typing, and decoupling from the backend storage implementation. 

|![Health GPS Data API](/assets/image/data_api.png) "Health GPS Data API"|
|:--:|
|*Backend Data API Interface*|

The data model defines the minimum dataset required by the model, the backend storage can hold more data to support external analysis for example. The backend minimum dataset diagram is shown below, identify required entities, relationships, and fields with respective data types. The dataset is indexed by country, green, entities representing demographics are gray, diseases are blue, analysis are red, and enumeration types are yellow respectively. Primary key fields are shown in **bold**, the ***ID*** fields are auto-generated row identifiers for internal data integrity enforcement.

|![Health GPS Data Model](/assets/image/data_model.png)|
|:--:|
|*Data Model Entity-Relation Diagram*|

The *country* index table is based on the [ISO 3166-1][iso3166] standard. All external data sources must provide some kind of *location identifier*, most likely with different values, but must enable mapping with the data storage index definition to be reconcile.

# Enumerations
To provide *stable identifier* to commonly used concepts, the data model provides normalised enumerations tables, yellow, which must be populated and possible mapped to external data source before data entry. It is very **important** to be consistent when populating the enumerations *code* field to provide a stable lookups, suggested guide:

* Start with a letter 
* Use only letters, numbers, and/or the underscore character, no spaces
* Be consistent with casing, prefer lower case, avoid mixing
* Keep it short, but meaningful and recognisable 

The same recommendation applies to folders and file names definitions in cross-platform applications, operating system like linux are by default case-sensitive, adopting a consistent naming convention works everywhere.

The *ShortName* field is intended to provide a user facing display name for the *code* identifier, and the *Description* field allows optional documentation. Following are example enumerations defined in Health GPS model:

## Gender

| GenderID | Code   | ShortName | Description      |
| :---     | :---   | :---      |---               |
| 1        | male   | Male      |                  |
| 2        | female | Female    |                  |

## Disease Group

| GroupID  | Code   | ShortName | Description|
| :---     | :---   | :---      |---         |
| 0        | other  | Other     | General noncommunicable diseases|
| 1        | cancer | Cancer    | Cancer type diseases|

## Disease Measure Type
| MeasureID | Code   | ShortName | Description|
| :---      | :---   | :---      |---         |
| 5         | prevalence | Prevalence | |
| 6         | incidence  | Incidence  | |
| 7         | remission  | Remission| | |
| 15        | mortality  | Mortality  | |

## BoD Measure Type
| MeasureID | Code   | ShortName | Description|
| :---      | :---   | :---      |---         |
| 2         | daly | DALY | Disability adjusted life years |
| 3         | yld  | YLD  | Years lived with disability |
| 4         | yll  | YLL  | Years of life lost|

## Cancer Parameter Type
| ParameterID | Code   | ShortName | Description|
| :---      | :---   | :---      |---         |
| 0         | deathweight | Deaths | Death weight |
| 1         | prevalence | Prevalence | Prevalence distribution |
| 2         | survivalrate | Survival | Survival rate parameters |

Entities *DiseaseType* and *RiskFactorType* provide a central **Registry** for available *diseases* and related *risk factors* respectively. The risk factor *code* must be consistent and exact match the external risk factor models definition.

[comment]: # (References)
[dataapi]: https://github.com/imperialCHEPI/healthgps/blob/main/source/HealthGPS.Core/datastore.h "Health GPS Data API definition."

[iso3166]: https://www.iso.org/iso-3166-country-codes.html "ISO 3166 Country Codes"