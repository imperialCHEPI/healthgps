## Global Health Policy Simulation model

| [Home](index) | [Quick Start](getstarted) | [User Guide](userguide) | [Software Architecture](architecture) | Data Model | [Developer Guide](development) |

# Data Model

The backend *data model* defines an abstract model to organises data entities and how they relate to one another in a standardised schema and format to be used within the Health GPS systems. The backend storage provides a reference dataset that reconcile various disparate data sources required by the model, fill gaps, adjust units, etc, for easy use. The standardised format allows the reference dataset to be easily expanded to accommodate new and non-traditional data sources.

The data model is storage agnostic, the [Data API][dataapi] abstraction interface shown below, provides a contract for the minimum dataset, easy access, strong typing, and decoupling from the backend storage implementation. 

|![Health GPS Data API](/assets/image/data_api.png) "Health GPS Data API"|
|:--:|
|*Backend Data API Interface*|

The data model defines the minimum dataset required by the model, the backend storage can hold more data to support external analysis for example. The backend minimum dataset diagram is shown below, indexed by country, green, entities representing demographics are gray, diseases are blue, analysis are red, and enumeration types are yellow respectively.

|![Health GPS Data Model](/assets/image/data_model.png)|
|:--:|
|*Data Model Entity-Relation Diagram*|



---
> **_UNDER DEVELOPMENT:_**  More content coming soon.
---

[dataapi]: https://github.com/imperialCHEPI/healthgps/blob/main/source/HealthGPS.Core/datastore.h "Health GPS Data API definition."