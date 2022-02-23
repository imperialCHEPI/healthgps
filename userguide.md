## Global Health Policy Simulation model

| [Home](index) | [Quick Start](getstarted) | User Guide | [Software Architecture](architecture) | [Data Model](datamodel) | [Developer Guide](development) |


# User Guide

The **Health GPS** microsimulation is a *data driven* modelling framework, combining many disconnected data sources to support the various interacting modules during a typical simulation experiment run. The framework provides a pre-populated *backend data storage* to minimise the learning curve for simple use cases, however advance users are likely to need a more in-depth knowledge of the full modelling workflow. A high-level representation of the microsimulation user workflow is shown below, it is crucial for users to have a good appreciation for the general dataflows and processes to better design experiments, configure the tool, and quantify the results.

|![Health GPS Workflow](/assets/image/workflow_diagram.png)|
|:--:|
|*Health GPS Workflow Diagram*|

As with any simulation model, the workflow starts with data, it is the user's responsibility to process and analyse the input data, define the model’s hierarchy, fit parameters to data, and design intervention. A *configuration* file is used to provide the user's datasets, model parameters, and control the simulation experiment runtime. The model is invoked via a *Command Line Interface* (CLI), which validates the configuration contents, loads associated files to create the model inputs, assembles the microsimulation with the required modules, evaluates the experiment, and writes the results to a chosen output *file* and *folder*. Likewise, it is the user's responsibility to analyse and quantify the model results and draw conclusions on the cost-effectiveness of the intervention.

> See [Quick Start](getstarted) to get started using the microsimulation with a working example.

## Configuration

The high-level structure of the [configuration][configjson] file used to create the model inputs, in `JSON` format, is shown below. The structure defines the schema version, the user's dataset file, target country with respective virtual population settings, external models' definition with respective fitted parameters, simulation experiment options including diseases selection, and intervention scenario, and finally the results output location.

```json
{
    "version": 2,
    "inputs": {
        "file": {},
        "settings": {
            "country_code": "FRA",
            "size_fraction": 0.0001,
            "age_range": [ 0, 100 ]
        }
    },
    "modelling": {
        "ses_model": {},
        "risk_factors": [],
        "dynamic_risk_factor": "",
        "risk_factor_models": {},
        "baseline_adjustments": {}
    },
    "experiment": {
        "seed": [123456789], // optional for reproducibility
        "start_time": 2010,
        "stop_time": 2050,
        "trial_runs": 1,
        "sync_timeout_ms": 5000,
        "diseases": [ "asthma", "diabetes", "lowbackpain", "colorectum"],
        "interventions": {
            "active_type_id": "",
            "types": {}
        }
    },
    "output": {
        "folder": "${HOME}/healthgps/results",
        "file_name": "HealthGPS_Result_{TIMESTAMP}.json"
    }
}
```

The ***inputs*** section sets the target country information, the *file* sub-section provides details of data used to fit the model, including file name, format, and variable data type mapping with the model internal types: *string, integer, float, double*. The user's data is required only to provide a visual validation of the initial virtual population created by the model, this feature should be made optional in the future, the following example illustrates the data file definition:

```json
...
"inputs": {
    "file": {
        "name": "france.data_file.csv",
        "format": "csv",
        "delimiter": ",",
        "encoding": "ASCII",
        "columns": {
            "Year": "integer",
            "Gender": "integer",
            "Age": "integer",
            "Age2": "integer",
            "Age3": "integer",
            "SES": "double",
            "PA": "double",
            "Protein": "double",
            "Sodium": "double",
            "Fat": "double",
            "Energy": "double",
            "BMI": "double"
        }
    }
}
...
```

The ***modelling*** section, defines the *SES* model, and the *risk factor* model with factors identifiers, hierarchy level, data range and mapping with the model's virtual individual properties (proxy). The *risk_factor_models* sub-section provides the fitted parameters file, `JSON` format, for each hierarchical *model type*, the *dynamic risk factor*, if exists, can be identified by the respective property. Finally, the *baseline adjustments* sub-section provides the *adjustment files* for each hierarchical *model type* and *gender*.

```json
...
"modelling": {
    "ses_model": {
        "function_name":"normal",
        "function_parameters":[0.0, 1.0]
    },
    "risk_factors": [
        {"name":"Gender",   "level":0, "proxy":"gender", "range":[0,1]},
        {"name":"Age",      "level":0, "proxy":"age",    "range":[1,87]},
        {"name":"Age2",     "level":0, "proxy":"",       "range":[1,7569]},
        {"name":"Age3",     "level":0, "proxy":"",       "range":[1,658503]},
        {"name":"SES",      "level":0, "proxy":"ses",    "range":[-2.314484485,2.310181]},
        {"name":"Sodium",   "level":1, "proxy":"",       "range":[1.146601,8.675425]},
        {"name":"Protein",  "level":1, "proxy":"",       "range":[42.09252483,238.4145]},
        {"name":"Fat",      "level":1, "proxy":"",       "range":[44.78128,381.666]},
        {"name":"PA",       "level":2, "proxy":"",       "range":[400,9769.83669]},
        {"name":"Energy",   "level":2, "proxy":"",       "range":[1315.532125,7545.832301]},
        {"name":"BMI",      "level":3, "proxy":"",       "range":[13.53255,45.26488]}
    ],
    "dynamic_risk_factor": "",
    "risk_factor_models": {
        "static": "france_HLM.json",
        "dynamic": "france_EBHLM.json"
    },
    "baseline_adjustments": {
        "format": "csv",
        "delimiter": ",",
        "encoding": "UTF8",
        "files_name": {
            "static_male":"france_initial_adjustment_male.csv",
            "static_female":"france_initial_adjustment_female.csv",
            "dynamic_male":"france_update_adjustment_male.csv",
            "dynamic_female":"france_update_adjustment_female.csv"
        }
    }
}
...
```

The ***experiment*** section defines simulation *runtime* period, *start/stop time* respectively in years, pseudorandom generator seed for reproducibility or empty for auto seeding, the number of *trials to run* for the experiment, and scenarios data synchronisation *timeout* in milliseconds. The model supports a dynamic list of diseases in the backend storage, the *diseases* array contains the selected *disease identifiers* to include in the experiment. The *interventions* sub-section defines zero or more *interventions types*, however, only *one intervention at most* can be *active* per configuration, the *active_type_id* property identifies the *active intervention*, leave it *empty* for a *baseline* scenario only experiment.

```json
...
"experiment": {
    "seed": [123456789],
    "start_time": 2010,
    "stop_time": 2050,
    "trial_runs": 1,
    "sync_timeout_ms": 5000,
    "diseases": [ "asthma", "diabetes", "lowbackpain", "colorectum"],
    "interventions": {
        "active_type_id": "marketing",
        "types": {
            "simple": {
                "active_period": {
                    "start_time": 2021,
                    "finish_time": 2021
                },
                "impact_type": "absolute",
                "impacts": [
                    { "risk_factor":"BMI", "impact_value": -1.0, "from_age":0, "to_age":null}
                ]
            },
            "marketing": {
                "active_period": {
                    "start_time": 2021,
                    "finish_time": 2050
                },
                "impacts": [
                    { "risk_factor":"BMI", "impact_value": -0.12, "from_age": 5, "to_age": 12 },
                    { "risk_factor":"BMI", "impact_value": -0.31, "from_age": 13, "to_age": 18 },
                    { "risk_factor":"BMI", "impact_value": -0.155, "from_age": 19, "to_age": null }
                ]
            }
        }
    }
}
...
```

Unlike the diseases *data driven* definition, *interventions* can have specific data requirements, rules for selecting the target population to intervening, intervention period transition, etc, consequently the definition usually require supporting code implementation. Health GPS provides an *interface* abstraction for implementing new interventions and easily use at runtime, however the implementation must also handle the required input data.

Finally, ***output*** section repeated below, defines the results output *folder and file name*. The configuration parser can handle *folder* name with *environment variables* such as `HOME` on Linux shown above, however care must be taken when running the model cross-platform with same configuration file. Likewise, *file name* can have a `TIMESTAMP` *token*, to enable multiple experiments to write results to the same folder on disk without file name crashing.

```json
...
"output": {
    "folder": "${HOME}/healthgps/results",
    "file_name": "HealthGPS_Result_{TIMESTAMP}.json"
}
...
```

### Hierarchical Models

#### Static Schema

#### Dynamic Schema


## Backend Storage
The *file data storage* provides a reusable, reference dataset in the model's [standardised](datamodel) format for improved usability, the dataset can easily be expanded with new data sources. The [index.json][datastore] file defines the storage structure, file names, diseases definition, metadata to identify the data sources and respective limits for consistency validation.

## Results

The model results file structure is composed of two parts: *experiment metadata* and *result array* respectively, each entry in the *result* array contains the *results* of a complete simulation run for a *scenario* as shown below. The simulation results are unique identified the source scenario, run number and time for each result row, the *id* property identifies the *message type* within the message bus implementation.

```json
{
    "experiment": {
        "model": "Health-GPS",
        "version": "1.0.0.0",
        "time_of_day": "2022-02-01 11:51:43.129 GMT Standard Time",
        "intervention": "marketing"
    },
    "result": [
        {
            "id": 2,
            "source": "baseline",
            "run": 1,
            "time": 2010,
            ...
        },
        {
            "id": 2,
            "source": "intervention",
            "run": 1,
            "time": 2010,
            ...
        },
        ...
    ]
}
```
The content of each message is implementation dependent, JSON (JavaScript Object Notation), an open standard file format designed for data interchange in human-readable text, can represent complex data structures. It is language-independent; however, all programming language and major data science tools supports JSON format because they have libraries and functions to read/write JSON structures. To read the model results in [R](https://www.r-project.org/), for example, you need the [`jsonlite`](https://cran.r-project.org/web/packages/jsonlite/vignettes/json-aaquickstart.html) package:
```R
require(jsonlite)
data <- fromJSON(result_filename)
View(data)
```
The above script reads the results data from file and makes the data variable available in R for analysis as shown below, it is equally easy to write a R structure to a JSON string or file.

|![Health GPS Results](/assets/image/model_results.png)|
|:--:|
|*Health GPS results in R data frame example*|

The results file contains the output of all simulations in the experiment, *baseline*, and *intervention* scenarios over one or more runs. The user should not assume data order during analysis of experiments with intervention scenarios, the results are published by both simulations running in parallel *asynchronously* via messages, the order in which the messages arrive at the destination queue, before being written to file is not guaranteed. A robust method to tabulate the results shown above, is to always group the data by: ```data.result(source, run, time)```, to ensure that the analysis algorithms work for both types of simulation experiments. For example, using the results data above in R, the following script will tabulate and plot the experiment's BMI projection.

```R
require(dplyr)
require(ggplot2)

# create groups frame
groups <- data.frame(data$result$source, data$result$run, data$result$time)
colnames(groups) <- c("scenario", "run", "time")

# create dataset
risk_factor <- "BMI"
sim_data <- cbind(groups, data$result$risk_factors_average[[risk_factor]])

# pivot data
info <- sim_data %>% group_by(scenario, time) %>% 
  summarise(runs = n(),
            avg_male = mean(male, na.rm = TRUE),
            sd_male = sd(male, na.rm = TRUE),
            avg_female = mean(female, na.rm = TRUE),
            sd_female = sd(female, na.rm = TRUE),
            .groups = "keep")

# reshape data
df <- data.frame(scenario = info$scenario, time = info$time, runs = info$runs,
      bmi = c(info$avg_male, info$avg_female),
      sd = c(info$sd_male, info$sd_female),
      se = c(info$sd_male / sqrt(info$runs), info$sd_female) / sqrt(info$runs),
      gender = c(rep('male', nrow(info)), rep('female', nrow(info))))

# Plot BMI projection
p <- ggplot(data=df, aes(x=time, y=bmi, group=interaction(scenario, gender))) +
  geom_line(size=0.6, aes(linetype=scenario, color=gender)) + theme_light() +
  scale_linetype_manual(values=c("baseline"="solid","intervention"="longdash")) +
  scale_color_manual(values=c("male"="blue","female"="red")) +
  scale_x_continuous(breaks = pretty(df$time, n = 10)) +
  scale_y_continuous(breaks = pretty(df$bmi, n = 10)) +
  ggtitle(paste(risk_factor, " projection under two scenarios")) +
  xlab("Year") + ylab("Average")

show(p)
```

|![Experiment BMI Projection](/assets/image/bmi_projection.png)|
|:--:|
|*Experiment BMI projection example*|

In a similar manner, the resulting dataset `df`, can be re-created and expanded to summarise other variables of interest, create results tables and plots to better understand the experiment.

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
