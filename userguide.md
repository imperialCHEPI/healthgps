## Global Health Policy Simulation model (Health GPS).

| [Home](index) | [Quick Start](getstarted) | [Software Architecture](architecture) | [Data Model](datamodel) | [Development](development) | User Guide | [License](index#license) |

# User Guide
The *Health-GPS* microsimulation is a data driven modelling framework, combining many disconnected data sources to support the various interacting modules during a typical simulation experiment run. The framework provides a pre-populated backend data storage to minimise the learning curve for simple use cases, however advance users are likely to need a more in-depth knowledge of the full workflows. A high-level representation of the *Health-GPS workflow* is shown below, it is crucial that users have a good appreciation for the general dataflow and processes to better design experiments and quantify the results.

|![Health GPS Workflow](/assets/image/workflow_diagram.png)|
|:--:|
|*Health GPS Workflow Diagram*|

As with any simulation model, it is the user's responsibility to process and analyse input data, define the model’s hierarchy, and fit parameters to data. A configuration file (JSON) is used control the simulation running settings and map the *Health-GPS* expected parameters to the user input data and fitted values. Likewise, it is the user's responsibility to and analyse and quantify the model results, which are saved to a chosen output folder in `JSON` format.

|![Health GPS Execution](/assets/image/execution_diagram.png)|
|:--:|
|*Health GPS Execution Diagram*|

---
> **_UNDER DEVELOPMENT:_**  More content coming soon.
---
