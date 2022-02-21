## Global Health Policy Simulation model

| [Home](index) | [Quick Start](getstarted) | User Guide | [Software Architecture](architecture) | [Data Model](datamodel) | [Developer Guide](development) |


# User Guide

The *Health-GPS* microsimulation is a data driven modelling framework, combining many disconnected data sources to support the various interacting modules during a typical simulation experiment run. The framework provides a pre-populated backend data storage to minimise the learning curve for simple use cases, however advance users are likely to need a more in-depth knowledge of the full workflows. A high-level representation of the microsimulation user workflow is shown below, it is crucial for users to have a good appreciation for the general dataflows and processes to better design experiments, configure the tool, and quantify the results.

|![Health GPS Workflow](/assets/image/workflow_diagram.png)|
|:--:|
|*Health GPS Workflow Diagram*|

As with any simulation model, it is the user's responsibility to process and analyse input data, define the model’s hierarchy, and fit parameters to data. A [configuration][configjson] file (`JSON` format) is used to control the simulation running settings, experiment country, virtual population size, map internal parameters to user's input data and external fitted models, define intervention scenario, and output file location. Likewise, it is the user's responsibility to analyse and quantify the model results, which are saved to the  chosen output folder in a `JSON` format.

> See [Quick Start](getstarted) to get started using the microsimulation on your platform.

The *file data storage* provides a reusable, reference dataset in the model's [standardised](datamodel) format for improved usability, the dataset can easily be expanded with new data sources. The [index.json][datastore] file defines the storage structure, file names, diseases definition, metadata to identify the data sources and respective limits for consistency validation.

---
> **_UNDER DEVELOPMENT:_**  More content coming soon.
---

This is a permalink example:
[healthgps/source/HealthGPS.Console/program.cpp](
https://github.com/imperialCHEPI/healthgps/blob/main/source/HealthGPS.Console/program.cpp#L75-L146)

```cpp
	auto model_config = create_model_input(input_table, target.value(), config, diseases);

	// Create event bus and monitor
	auto event_bus = DefaultEventBus();
	auto result_file_logger = ResultFileWriter{ 
		create_output_file_name(config.result),
		ModelInfo{.name = "Health-GPS", .version = "1.0.0.0"}
	};
	auto event_monitor = EventMonitor{ event_bus, result_file_logger };

    ...
```

[comment]: # (References)
[configjson]:https://github.com/imperialCHEPI/healthgps/blob/main/example/France.Config.json "Configuration file example"

[datastore]:https://github.com/imperialCHEPI/healthgps/blob/main/data/index.json "Backend file based data store index file"
