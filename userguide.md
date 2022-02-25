## Global Health Policy Simulation model

| [Home](index) | [Quick Start](getstarted) | User Guide | [Software Architecture](architecture) | [Data Model](datamodel) | [Developer Guide](development) |

# User Guide

---
> **_UNDER DEVELOPMENT:_** New content is coming soon.
---

The **Health GPS** microsimulation is a *data driven* modelling framework, combining many disconnected data sources to support the various interacting modules during a typical simulation experiment run. The framework provides a pre-populated *backend data storage* to minimise the learning curve for simple use cases, however advance users are likely to need a more in-depth knowledge of the full modelling workflow. A high-level representation of the microsimulation user workflow is shown below, it is crucial for users to have a good appreciation for the general dataflows and processes to better design experiments, configure the tool, and quantify the results.

|![Health GPS Workflow](/assets/image/workflow_diagram.png)|
|:--:|
|*Health GPS Workflow Diagram*|

As with any simulation model, the workflow starts with data, it is the user's responsibility to process and analyse the input data, define the model’s hierarchy, fit parameters to data, and design intervention. A *configuration* file is used to provide the user's datasets, model parameters, and control the simulation experiment runtime. The model is invoked via a *Command Line Interface* (CLI), which validates the configuration contents, loads associated files to create the model inputs, assembles the microsimulation with the required modules, evaluates the experiment, and writes the results to a chosen output *file* and *folder*. Likewise, it is the user's responsibility to analyse and quantify the model results and draw conclusions on the cost-effectiveness of the intervention.

> See [Quick Start](getstarted) to get started using the microsimulation with a working example.

# 1.0 Configuration

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
        "seed": [123456789],
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

## 1.1 Inputs
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

## 1.2 Modelling
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
The *risk factor model* and *baseline adjustment* files have their own schemas and formats requirements, these files structure are defined separately below, after all the *configuration file* sections.

## 1.3 Experiment
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

## 1.4 Output

Finally, ***output*** section repeated below, defines the results output *folder and file name*. The configuration parser can handle *folder* name with *environment variables* such as `HOME` on Linux shown above, however care must be taken when running the model cross-platform with same configuration file. Likewise, *file name* can have a `TIMESTAMP` *token*, to enable multiple experiments to write results to the same folder on disk without file name crashing.

```json
...
"output": {
    "folder": "${HOME}/healthgps/results",
    "file_name": "HealthGPS_Result_{TIMESTAMP}.json"
}
...
```

## 1.5 Risk Factor Models

### 1.5.1 Static

### 1.5.2 Dynamic

### 1.5.3 Baseline Adjustments

# 2.0 Backend Storage
Health GPS by default uses a *file-based backend storage*, which implements the [Data Model](datamodel) to provides a reusable, *reference dataset* using a [standardised](datamodel) format for improved usability, the dataset can easily be expanded with new data without code changes. The contents of the file-based storage is defined using the [index.json][datastore] file, which must live at the *root* of the storage's *folder structure* as shown below.

|![File-based Datastore](/assets/image/file_based_storage.png)|
|:--:|
|*File-based Backend Datastore example*|

The *index file* defines data groups, physical storage folders relative to the root folder, file names pattern, and stores metadata to identify the data sources, licences, and data limits for consistency validation. The data is indexed and stored by country files taking advantage of the model's workflow, this minimises data reading time and enables the storage to scale with many countries.

The data model defines many data hierarchies, which have been flatten for file-based storage, *file names* are defined for consistency, avoid naming clashes, and ultimately to store processed data for machines consumption, not necessarily for humans. The high-level structure of the file-based storage index file, in `JSON` format, is shown below.

```json
{
    "country": {
        "format": "csv",
        "delimiter": ",",
        "encoding": "UTF8",
        "description": "ISO 3166-1 country codes.",
        "source": "International Organization for Standardization",
        "url": "https://www.iso.org/iso-3166-country-codes.html",
        "license": "N/A",
        "path": "",
        "file_name": "countries.csv"
    },
    "demographic": {},
    "diseases": {
        "relative_risk":{ },
        "parameters":{ },
        "registry":[ ]
    },
    "analysis":{ }
}
```

The ***country*** data file lives at the *root* of the folder structure as shown above, therefore the *path* is left empty, and respective *file name* provided. The *metadata* is used only for documentation purpose, current *Data API* contracts have no support for surfacing metadata, therefore the metadata contents will be removed from the following sections for brevity and clarity.

> The *path* and *file_name* properties are required and must be defined, all *paths* are relative to the *parent* folder.

## 2.1 Demographic

Defines the storage for country specific data containing historic estimates and projections for various demographics measures by *time, age* and *gender*. The datasets limits for *age* and *time* variables are stored for validation, the *projections* property marks the starting point, in time, for projected values. The data is stored by country file, the COUNTRY_CODE token is subtitled with a given country's identifier to identify the respective country's data file as shown below.

```json
...
"demographic": {
    ...
    "path": "undb",
    "age_limits": [0, 100],
    "time_limits": [1950, 2100],
    "projections": 2020,
    "population": {
        "description": "Total population by sex, annually from 1950 to 2100.",
        "path": "population",
        "file_name": "P{COUNTRY_CODE}.csv"
    },
    "mortality": {
        "description": "Number of deaths by age and gender.",
        "path": "mortality",
        "file_name": "M{COUNTRY_CODE}.csv"
    },
    "indicators": {
        "description": "Several indicators, e.g. births, deaths and life expectancy.",
        "path": "indicators",
        "file_name": "Pi{COUNTRY_CODE}.csv"
    }
}
...
```

## 2.2 Diseases

```json
...
"diseases": {
    ...
    "path": "diseases",
    "file_name": "D{COUNTRY_CODE}.csv",
    "age_limits": [1, 100],
    "time_year": 2019,
    "relative_risk":{
        "path": "relative_risk",
        "to_disease":{
            "path":"disease",
            "file_name": "{DISEASE_TYPE}_{DISEASE_TYPE}.csv",
            "default_value": 1.0
        },
        "to_risk_factor":{
            "path":"risk_factor",
            "file_name": "{GENDER}_{DISEASE_TYPE}_{RISK_FACTOR}.csv"
        }
    },
    "parameters":{
        "path":"P{COUNTRY_CODE}",
        "files":{
            "distribution": "prevalence_distribution.csv",
            "survival_rate": "survival_rate_parameters.csv",
            "death_weight": "death_weights.csv"
        }
    },
    "registry":[
        {"group": "other", "id": "asthma", "name": "Asthma"},
        {"group": "other", "id": "diabetes", "name": "Diabetes mellitus"},
        {"group": "other",  "id": "lowbackpain", "name": "Low back pain"},
        {"group": "cancer", "id": "colorectum", "name": "Colorectum  cancer"}
    ]
}
...
```

## 2.3 Analysis

```json
...
"analysis":{
    ...
    "path": "analysis",
    "age_limits": [1, 100],
    "time_year": 2019,
    "file_name": "disability_weights.csv",
    "cost_of_disease": {
        "path":"cost",
        "file_name": "BoD{COUNTRY_CODE}.csv"
    }
}
...
```

# 4.0 Running

Health GPS has been designed to be portable, producing stable, and comparable results cross-platform. Only minor and insignificant rounding errors should be noticeable, these errors are attributed to the C++ application binary interface (ABI), which is not guaranteed to compatible between two binary programs cross-platform, or even on same platform when using different versions of the C++ standard library.

The code repository provides `x64` binaries for `Windows` and `Linux` Operating Systems (OS), unfortunately, these binaries have been created using tools and libraries specific to each platform, and consequently have very different runtime requirements. The following step by step guide illustrates how to run the Health GPS application on each platform using the include example model and reference dataset.
## Windows
You may need to install the latest [Visual C++ Redistributable](https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-160) on the machine, the application requires the `2019 x64` version or newer to be installed.

1. Download the latest [release](https://github.com/imperialCHEPI/healthgps/releases) binaries for Windows from the repository.
2. Unzip the file contents into a local directory of your choice (xxx).
3. Open a command terminal, e.g. [Windows Terminal](https://www.microsoft.com/en-gb/p/windows-terminal/9n0dx20hk701?rtc=1&activetab=pivot:overviewtab), and navigate to the directory used in step 2 (xxx).
4. Run: `X:\xxx> .\HealthGPS.Console -f ".\example\France.Config.json" -s ".\data"` where `-f` gives the *configuration file* full-name and
`-s` the path to the root folder of the *backend storage* respectively.
5. The default output folder is `C:\healthgps\results`, but this can be changed in the *configuration file* `(France.Config.json)`.

## Linux
You may need to install the latest [GCC Compiler Libraries](https://gcc.gnu.org/) on the machine, the application requires the `GCC 11.1` or newer version to be installed.

1. Download the latest [release](https://github.com/imperialCHEPI/healthgps/releases) binaries for Linux from the repository.
2. Unzip the file contents into a local directory of your choice (xxx).
3. Navigate to the directory used in step 2 (xxx).
4. Run: `user@machine:~/xxx$ ./HealthGPS.Console.exe -f ./example/France.Config.json -s ./data` where `-f` gives the *configuration file* full-name and `-s` the path to the root folder of the *backend storage* respectively.
5. The default output folder is `~/healthgps/results`, but this can be changed in the *configuration file* `(France.Config.json)`.

> See [Quick Start](getstarted) for details on the example dataset and known issues.

The included model and dataset provide a complete example of the files and procedures described in this document. Users should use this example to a starting point when creating a production environment.

# 4.1 Results

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

[comment]: # (References)
[configjson]:https://github.com/imperialCHEPI/healthgps/blob/main/example/France.Config.json "Configuration file example"

[datastore]:https://github.com/imperialCHEPI/healthgps/blob/main/data/index.json "Backend file based data store index file"
